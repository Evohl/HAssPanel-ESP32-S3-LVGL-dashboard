// ============================================================
//  OTA + Web Management Interface
//
//  Routes:
//    GET  /          → Status-Dashboard (IP, Uptime, Heap, MQTT, RSSI)
//    GET  /config    → config.txt direkt im Browser bearbeiten
//    POST /config    → Config speichern & Neustart
//    GET  /log       → Live-Log-Viewer (JS-Polling)
//    GET  /log/data  → Log-Daten als JSON (seit Index N)
//    GET  /restart   → Fernstart
//    GET  /upload    → Datei-Upload Formular
//    POST /upload    → SD-Karten Datei-Upload
//    GET  /firmware  → Firmware-Update Formular
//    POST /firmware  → Firmware flashen (HTTP OTA)
//
//  OTA Firmware:
//    pio run -e ha_panel_ota -t upload
//    Hostname: hassPanel.local
// ============================================================

#include <ArduinoOTA.h>
#include <WebServer.h>
#include <ESPmDNS.h>

static WebServer httpServer(80);

// ─── Log Ring-Buffer ─────────────────────────────────────────
#define LOG_BUF_SIZE 80
static String _log[LOG_BUF_SIZE];
static int    _logIdx = 0;   // absoluter Schreibzähler

void webLog(const String& msg) {
  Serial.println(msg);
  _log[_logIdx % LOG_BUF_SIZE] = msg;
  _logIdx++;
}

// ─── Shared CSS (Dark Theme) ─────────────────────────────────
static const char* CSS =
  "body{font-family:monospace;background:#0d1117;color:#e6edf3;margin:0;padding:0;font-size:16px}"
  "nav{background:#161b22;padding:.55em 1.2em;border-bottom:1px solid #30363d;"
       "display:flex;gap:1em;align-items:center;flex-wrap:wrap}"
  "nav .t{color:#e6edf3;font-weight:bold;margin-right:auto;font-size:1.05em}"
  "nav a,nav button,nav .nbtn{color:#58a6ff;text-decoration:none;font-size:.95em;"
    "background:none;border:1px solid #30363d;border-radius:5px;"
    "padding:.3em .85em;cursor:pointer;white-space:nowrap}"
  "nav a:hover,nav button:hover,nav .nbtn:hover{background:#21262d}"
  "nav .red{color:#f85149;border-color:#6e2020}"
  "nav .red:hover{background:#3d1212}"
  "main{padding:1.5em;font-size:1em}"
  "h2{color:#58a6ff;margin-top:0;font-size:1.3em}"
  ".card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:1.1em 1.5em;margin-bottom:1em}"
  ".row{display:flex;gap:2em;flex-wrap:wrap}"
  ".kv{display:flex;flex-direction:column;min-width:150px}"
  ".kv .k{color:#8b949e;font-size:.82em;margin-bottom:.25em}"
  ".kv .v{font-size:1.1em}"
  ".ok{color:#3fb950}.warn{color:#fea020}.err{color:#f85149}"
  "input[type=submit]{background:#238636;color:#fff;border:none;"
    "padding:.5em 1.3em;border-radius:6px;cursor:pointer;font-size:1em;margin-top:.6em}"
  "input[type=submit]:hover{background:#2ea043}"
  "textarea{width:100%;box-sizing:border-box;background:#0d1117;color:#e6edf3;"
    "border:1px solid #30363d;border-radius:6px;padding:.8em;font-family:monospace;"
    "font-size:.95em;line-height:1.5;resize:vertical}"
  "#log{background:#0d1117;border:1px solid #30363d;border-radius:6px;padding:.9em;"
    "height:440px;overflow-y:auto;font-size:.9em;line-height:1.7}"
  ".ll{white-space:pre-wrap;color:#8b949e}.ln{color:#e6edf3}"
  "input[type=file]{color:#e6edf3;margin:.5em 0;display:block;font-size:1em}"
  ".hint{color:#8b949e;font-size:.82em;margin:.2em 0 .9em}"
  ".field{display:flex;flex-direction:column;gap:.3em;min-width:160px;flex:1}"
  ".field label{color:#8b949e;font-size:.8em}"
  ".field input,.field select,.field textarea{background:#0d1117;color:#e6edf3;border:1px solid #30363d;border-radius:6px;padding:.5em .6em;font-size:.95em}"
  ".rowwrap{display:flex;gap:1em;flex-wrap:wrap;align-items:end;margin-bottom:.8em}"
  ".field-color{flex:0 0 auto;min-width:0}"
  ".field input[type=color]{width:52px;height:38px;padding:2px;cursor:pointer}"
  ".colorwrap{display:flex;gap:.4em;align-items:center}"
  ".hexbox{width:5.5em;text-transform:uppercase;font-family:monospace;letter-spacing:.03em}"
  ".tile-card{border:1px solid #30363d;border-radius:8px;padding:1em;margin-bottom:.8em;background:#0d1117}"
  ".tile-card h4{margin:0 0 .6em 0;color:#8b949e;font-size:.85em;font-weight:normal;text-transform:uppercase;letter-spacing:.04em}"
  ".subs-block{display:none;margin-top:.6em;padding-top:.6em;border-top:1px dashed #30363d}"
  ".subs-block.active{display:block}"
  ".hidden-field{display:none}"
  ".sub-row{display:flex;gap:.6em;flex-wrap:wrap;margin-bottom:.5em;align-items:end}"
  ".sub-row .field{min-width:110px}"
  ".sub-row .field-color{min-width:0}"
  "#previewPanel{position:absolute;left:0;top:0;right:0;bottom:0;box-sizing:border-box;"
    "border:1px solid #30363d;border-radius:8px;overflow:hidden;background:#0d1117;"
    "font-family:'Segoe UI',Roboto,Helvetica,Arial,-apple-system,sans-serif}"
  "#previewRatioBox{position:relative;width:100%;max-width:840px}"
  "#previewRatioBox::before{content:'';display:block;padding-top:60%}"
  "#previewHeaderBar{position:absolute;left:0;top:0;width:100%;box-sizing:border-box;"
    "display:flex;align-items:center;justify-content:space-between;"
    "border-bottom:1px solid #30363d;padding:0 1.6%}"
  "#previewHeaderBar .htitle{font-weight:bold;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
  "#previewHeaderBar .hicons{color:#8b949e;white-space:nowrap}"
  ".ptile{position:absolute;box-sizing:border-box;border:1px solid #30363d;overflow:hidden}"
  ".ptile-inner{position:relative;width:100%;height:100%;box-sizing:border-box}"
  ".t-name{font-weight:bold;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
  ".t-header-row{display:flex;justify-content:space-between;align-items:baseline;gap:.4em}"
  ".t-mainval{font-weight:600;white-space:nowrap}"
  ".t-mainval-center{position:absolute;left:0;right:0;top:50%;transform:translateY(-50%);text-align:center;font-weight:600}"
  ".t-divider{height:1px;background:#30363d}"
  ".t-switch{width:15%;max-width:44px;aspect-ratio:2/1;border-radius:999px;background:#30363d;margin:.4em auto}"
  ".t-state{position:absolute;left:0;right:0;bottom:6%;text-align:center}"
  ".subs{display:flex;flex-direction:column;gap:.15em}"
  ".sub-row-preview{display:flex;justify-content:space-between;gap:.5em}"
  ".sub-row-preview .subname{opacity:.85}";

static String nav() {
  return "<nav>"
         "<span class='t'>&#9632; " + CL_hostname + "</span>"
         "<a href='/'>Status</a>"
         "<a href='/config'>Config</a>"
         "<a href='/wizard'>Wizard</a>"
         "<a href='/log'>Log</a>"
         "<a href='/firmware'>Firmware</a>"
         "<form style='margin:0' action='/restart' method='GET'>"
         "<button class='red' onclick=\"return confirm('Neustart?')\">&#8635; Neustart</button>"
         "</form>"
         "</nav>";
}

static String page(const String& title, const String& body) {
  // Wichtig: NICHT per verketteten '+' bauen – bei sehr großen Strings (>100KB)
  // erzeugt jeder '+' einen kompletten temporären Kopie-String, was auf dem ESP32
  // Heap-Fragmentierung/OOM verursacht und den String-Inhalt still korrumpiert
  // (fehlende/verschobene Teile). Stattdessen vorab reservieren und per '+=' anhängen.
  String h;
  h.reserve(body.length() + strlen(CSS) + title.length() + 1024);
  h += "<!DOCTYPE html><html><head>"
       "<meta charset='utf-8'>"
       "<meta name='viewport' content='width=device-width,initial-scale=1'>"
       "<title>HAssPanel – ";
  h += title;
  h += "</title>"
       "<style>";
  h += CSS;
  h += "</style>"
       "</head><body>";
  h += nav();
  h += "<main>";
  h += body;
  h += "</main></body></html>";
  return h;
}

// ─── Chunked Streaming (für sehr große Seiten wie den Wizard) ─
// Baut die Seite NICHT als einen riesigen String im RAM auf (der ESP32 hat oft
// nur ~130-150KB freien Heap), sondern schreibt jedes Fragment sofort per
// sendContent() auf die TCP-Verbindung. So bleibt der Speicherbedarf auf die
// Größe des jeweils aktuellen Fragments begrenzt statt auf die Gesamtseite.
static void sendPageHead(const String& title) {
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send(200, "text/html; charset=utf-8", "");
  httpServer.sendContent("<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>HAssPanel \u2013 ");
  httpServer.sendContent(title);
  httpServer.sendContent("</title><style>");
  httpServer.sendContent(CSS);
  httpServer.sendContent("</style></head><body>");
  httpServer.sendContent(nav());
  httpServer.sendContent("<main>");
}

static void sendPageFoot() {
  httpServer.sendContent("</main></body></html>");
  httpServer.sendContent(""); // beendet die Chunked-Übertragung
}

static String escapeHtml(const String& s) {
  String out = s;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  return out;
}

static String hex6(uint32_t v) {
  char buf[7];
  snprintf(buf, sizeof(buf), "%06X", (unsigned int)(v & 0xFFFFFF));
  return String(buf);
}

static String makeLayoutValue() {
  if (g_layout_rows > 0) {
    String v = "";
    for (int i = 0; i < g_layout_rows; i++) {
      if (i > 0) v += ",";
      v += String(g_layout_row_count[i]);
    }
    return v;
  }
  return "auto";
}

static String colorField(const String& fieldLabel, const String& name, uint32_t val) {
  String h = hex6(val);
  return "<div class='field field-color'><label>" + fieldLabel + "</label>"
         "<div class='colorwrap'>"
         "<input type='color' id='" + name + "_c' name='" + name + "' value='#" + h + "' oninput=\"syncColorToHex(this)\">"
         "<input type='text' id='" + name + "_h' class='hexbox' value='" + h + "' maxlength='7' spellcheck='false' "
         "oninput=\"syncHexToColor(this)\"></div></div>";
}

static String fontSizeField(const String& fieldLabel, const String& name, uint8_t val) {
  String s = "<div class='field'><label>" + fieldLabel + "</label><select name='" + name + "'>";
  s += "<option value='0'" + String(val == 0 ? " selected" : "") + ">Auto</option>";
  const uint8_t sizes[] = {12, 14, 16, 20, 24, 28};
  for (uint8_t sz : sizes) {
    s += "<option value='" + String(sz) + "'" + (val == sz ? " selected" : "") + ">" + String(sz) + " pt</option>";
  }
  s += "</select></div>";
  return s;
}

// Sendet die Wizard-Seite direkt gechunkt (siehe sendPageHead) statt sie als
// einen einzigen sehr großen String im RAM aufzubauen – bei 12 Tiles x 8 Subs
// wird die Seite leicht >150KB groß, was den ESP32-Heap sprengen und den
// Seiteninhalt lautlos korrumpieren würde (fehlende/verschobene Teile).
static void sendWizardPage() {
  sendPageHead("Wizard");

  httpServer.sendContent(
    "<h2>MQTT-Panel Wizard</h2>"
    "<p class='hint'>W\u00e4hle MQTT-Topics, Labels, Farben und Layout \u2013 die Vorschau unten zeigt direkt, wie das Panel aussehen wird.</p>"
    "<form method='POST' action='/wizard'>");

  httpServer.sendContent("<div class='card'><h3 style='margin-top:0'>Allgemein</h3><div class='rowwrap'>");
  httpServer.sendContent("<div class='field'><label>Layout (leer/auto oder z.B. 2,3)</label><input name='layout' value='" + (makeLayoutValue() == "auto" ? String("") : makeLayoutValue()) + "' placeholder='auto'></div>");
  httpServer.sendContent("<div class='field'><label>Panel Titel</label><input name='panel_title' value='" + escapeHtml(g_panel_title) + "'></div>");
  httpServer.sendContent("</div></div>");

  httpServer.sendContent("<div class='card'><h3 style='margin-top:0'>Vorschau</h3>");
  httpServer.sendContent("<p class='hint'>Ma\u00dfstabsgetreue Vorschau \u2013 800\u00d7480, gleiches Seitenverh\u00e4ltnis &amp; Schriftgr\u00f6\u00dfen wie am echten Panel.</p>");
  httpServer.sendContent("<div id='previewRatioBox'><div id='previewPanel'>");
  httpServer.sendContent("<div id='previewHeaderBar'><span class='htitle' id='previewTitle'>" + escapeHtml(g_panel_title) + "</span><span class='hicons' id='previewHicons'>&#9679; &#8635;</span></div>");
  httpServer.sendContent("</div></div>");
  httpServer.sendContent("</div>");

  httpServer.sendContent("<div class='card'><h3 style='margin-top:0'>Farben</h3>");
  httpServer.sendContent(
    "<div class='rowwrap'><div class='field'><label>Farbschema (Vorauswahl)</label>"
    "<select id='themeSelect' onchange=\"applyTheme(this.value)\">"
    "<option value=''>-- w\u00e4hlen --</option>"
    "<option value='dark_blue'>Dunkel &middot; Blau</option>"
    "<option value='dark_green'>Dunkel &middot; Gr\u00fcn</option>"
    "<option value='dark_warm'>Dunkel &middot; Warm</option>"
    "<option value='light_classic'>Hell &middot; Klassisch</option>"
    "<option value='light_warm'>Hell &middot; Warm</option>"
    "</select></div></div>");
  httpServer.sendContent("<div class='rowwrap'>");
  httpServer.sendContent(colorField("Bildschirm-Hintergrund", "bg_color", g_bg_color));
  httpServer.sendContent(colorField("Kopfzeile-Hintergrund", "header_bg_color", g_header_bg_color));
  httpServer.sendContent(colorField("Akzent / Panel-Titel", "accent_color", g_accent_color));
  httpServer.sendContent(colorField("Kachel-Hintergrund", "tile_bg_color", g_tile_bg_color));
  httpServer.sendContent(colorField("Kachel-Titelfarbe", "title_color", g_title_color));
  httpServer.sendContent(colorField("Sub-Label-Farbe", "sub_label_color", g_sub_label_color));
  httpServer.sendContent("</div></div>");

  httpServer.sendContent("<div class='card'><h3 style='margin-top:0'>Verf\u00fcgbare MQTT-Topics</h3>");
  if (discovered_topic_count == 0) {
    httpServer.sendContent("<p class='hint'>Noch keine MQTT-Topics erkannt. Nach dem ersten MQTT-Update erscheinen sie hier.</p>");
  } else {
    httpServer.sendContent("<p class='hint'>MQTT-Topics erscheinen automatisch, sobald Nachrichten eintreffen.</p>");
  }
  httpServer.sendContent("<datalist id='mqtt-topics'>");
  for (int i = 0; i < discovered_topic_count && i < MAX_DISCOVERED_TOPICS; i++) {
    String payload = discovered_payloads[i];
    payload.replace("\"", "&quot;");
    payload.replace("&", "&amp;");
    String topic = escapeHtml(discovered_topics[i]);
    httpServer.sendContent("<option value='" + topic + "'>" + topic + " \u00b7 " + payload + "</option>");
  }
  httpServer.sendContent("</datalist>");
  httpServer.sendContent("</div>");

  // ── Tile-Konfigurationskarten (Typ, Label, Topic, Einheit, Farbe, Sub-Items) ──
  httpServer.sendContent("<div class='card'><h3 style='margin-top:0'>Tile-Konfiguration</h3>");
  for (int i = 1; i <= MAX_ENTITIES; i++) {
    bool exists = (i <= entity_count);
    String label = exists ? escapeHtml(entities[i - 1].label) : "";
    String topic = exists ? escapeHtml(entities[i - 1].state_topic) : "";
    String unit  = exists ? escapeHtml(entities[i - 1].unit) : "";
    String cmd   = exists ? escapeHtml(entities[i - 1].cmd_topic) : "";
    uint8_t fsize = exists ? entities[i - 1].font_size : 0;
    String type  = "sensor";
    if (exists) {
      type = (entities[i - 1].type == ENTITY_SWITCH) ? "switch"
           : (entities[i - 1].type == ENTITY_GROUP)  ? "group" : "sensor";
    }
    bool isGroup  = (type == "group");
    bool isSwitch = (type == "switch");
    int subCount = exists ? entities[i - 1].sub_count : 0;

    httpServer.sendContent("<div class='tile-card'>");
    httpServer.sendContent("<h4>Tile " + String(i) + "</h4>");
    httpServer.sendContent("<div class='rowwrap'>");
    httpServer.sendContent(
      "<div class='field'><label>Typ</label><select name='entity" + String(i) + "_type' onchange=\"toggleSubs(" + String(i) + ");toggleMainFields(" + String(i) + ");updatePreview();\">"
      "<option value='sensor'" + (type == "sensor" ? " selected" : "") + ">Sensor</option>"
      "<option value='switch'" + (type == "switch" ? " selected" : "") + ">Switch</option>"
      "<option value='group'"  + (type == "group"  ? " selected" : "") + ">Gruppe</option>"
      "</select></div>");
    httpServer.sendContent("<div class='field'><label>Label</label><input name='entity" + String(i) + "_label' value='" + label + "'></div>");
    httpServer.sendContent("<div class='field' style='flex:2'><label>MQTT Topic (Hauptwert)</label><input list='mqtt-topics' name='entity" + String(i) + "_state_topic' value='" + topic + "'></div>");
    httpServer.sendContent("<div class='field main-unit-field" + String(isSwitch ? " hidden-field" : "") + "' id='unit-field-" + String(i) + "'><label>Einheit</label><input name='entity" + String(i) + "_unit' value='" + unit + "'></div>");
    httpServer.sendContent("<div class='field main-cmd-field" + String(isSwitch ? "" : " hidden-field") + "' style='flex:2' id='cmd-field-" + String(i) + "'><label>Cmd Topic (Schalten)</label><input list='mqtt-topics' name='entity" + String(i) + "_cmd_topic' value='" + cmd + "'></div>");
    httpServer.sendContent(colorField("Farbe", "entity" + String(i) + "_color", exists ? entities[i - 1].color : 0xFFFFFF));
    httpServer.sendContent(fontSizeField("Schriftgr\u00f6\u00dfe", "entity" + String(i) + "_font_size", fsize));
    httpServer.sendContent("</div>");

    // Sub-Items (nur für Typ=Gruppe sichtbar, Umschaltung per JS)
    httpServer.sendContent("<div class='subs-block" + String(isGroup ? " active" : "") + "' id='subs-block-" + String(i) + "'>");
    httpServer.sendContent("<label style='color:#8b949e;font-size:.8em'>Sub-Werte (von oben nach unten ausf\u00fcllen, leeres Label = Ende)</label>");
    httpServer.sendContent(
      "<div class='field' style='flex:0 0 auto;margin-bottom:.4em'><label style='display:flex;align-items:center;gap:.4em;white-space:nowrap;color:#c9d1d9'>"
      "<input type='checkbox' name='entity" + String(i) + "_hide_title' value='1'" + String((exists && entities[i - 1].hide_title) ? " checked" : "") + " onchange='updatePreview()'> "
      "Titel ausblenden (nur Sub-Werte anzeigen)</label></div>");
    for (int s = 1; s <= MAX_SUBS; s++) {
      bool subExists = exists && (s <= subCount);
      String sLabel = subExists ? escapeHtml(entities[i - 1].sub_label[s - 1]) : "";
      String sTopic = subExists ? escapeHtml(entities[i - 1].sub_topic[s - 1]) : "";
      String sUnit  = subExists ? escapeHtml(entities[i - 1].sub_unit[s - 1])  : "";
      String sCmd   = subExists ? escapeHtml(entities[i - 1].sub_cmd_topic[s - 1]) : "";
      uint32_t sColorVal = (subExists && entities[i - 1].sub_color[s - 1] != 0xFFFFFFFF) ? entities[i - 1].sub_color[s - 1] : 0xFFFFFF;
      uint8_t  sFsize    = subExists ? entities[i - 1].sub_font_size[s - 1] : 0;
      String p = "entity" + String(i) + "_sub" + String(s) + "_";
      httpServer.sendContent("<div class='sub-row'>");
      httpServer.sendContent("<div class='field'><label>Sub " + String(s) + " Label</label><input name='" + p + "label' value='" + sLabel + "'></div>");
      httpServer.sendContent("<div class='field' style='flex:2'><label>Sub Topic</label><input list='mqtt-topics' name='" + p + "topic' value='" + sTopic + "'></div>");
      httpServer.sendContent("<div class='field'><label>Einheit</label><input name='" + p + "unit' value='" + sUnit + "'></div>");
      httpServer.sendContent("<div class='field' style='flex:2'><label>Cmd Topic (leer = nur lesen)</label><input list='mqtt-topics' name='" + p + "cmd' value='" + sCmd + "'></div>");
      httpServer.sendContent(colorField("Farbe", p + "color", sColorVal));
      httpServer.sendContent(fontSizeField("Schriftgr\u00f6\u00dfe", p + "font_size", sFsize));
      httpServer.sendContent("</div>");
    }
    httpServer.sendContent("</div>");

    httpServer.sendContent("</div>");
  }
  httpServer.sendContent("</div>");

  httpServer.sendContent("<input type='submit' value='Konfiguration speichern &amp; Neustart'>");
  httpServer.sendContent("</form>");

  httpServer.sendContent("<script>");
  httpServer.sendContent("var MAX_ENTITIES=" + String(MAX_ENTITIES) + ",MAX_SUBS=" + String(MAX_SUBS) + ";");
  httpServer.sendContent("var PANEL_W=" + String(screenWidth) + ",PANEL_H=" + String(screenHeight) + ",HEADER_H=58,TILE_GAP=4;"); // muss zu HEADER_H/TILE_GAP in ui.ino passen
  httpServer.sendContent("var liveTopics={};");
  httpServer.sendContent("function hex(v){var s=(v||'#FFFFFF').toString().trim().replace(/^#/,'').toUpperCase();if(s.length<6)s=(s+'000000').slice(0,6);return '#'+s.slice(0,6);}");
  httpServer.sendContent("function fieldVal(name){var el=document.querySelector('[name='+name+']');return el?el.value:'';}");
  httpServer.sendContent("function fieldChecked(name){var el=document.querySelector('[name='+name+']');return el?el.checked:false;}");
  httpServer.sendContent("function syncColorToHex(colorEl){var h=document.getElementById(colorEl.id+'_h');if(h)h.value=colorEl.value.replace('#','').toUpperCase();}");
  httpServer.sendContent("function syncHexToColor(hexEl){var v=hexEl.value.replace('#','').trim();if(!/^[0-9A-Fa-f]{6}$/.test(v))return;var c=document.getElementById(hexEl.id.slice(0,-2)+'_c');if(c)c.value='#'+v.toUpperCase();}");
  httpServer.sendContent("function toggleSubs(i){var block=document.getElementById('subs-block-'+i);if(!block)return;var t=fieldVal('entity'+i+'_type');block.className='subs-block'+(t==='group'?' active':'');}");
  httpServer.sendContent("function toggleMainFields(i){var t=fieldVal('entity'+i+'_type');var u=document.getElementById('unit-field-'+i);var c=document.getElementById('cmd-field-'+i);if(u)u.classList.toggle('hidden-field',t==='switch');if(c)c.classList.toggle('hidden-field',t!=='switch');}");
  httpServer.sendContent("function liveOf(topic){if(!topic||liveTopics[topic]===undefined)return null;var v=liveTopics[topic];if(/^(unavailable|unknown|none)$/i.test(v))return null;return v;}");
  // Anzahl Tiles ergibt sich aus den von oben nach unten ausgefuellten Tile-Labels (leeres Label = Ende), analog zu den Sub-Werten
  httpServer.sendContent("function getCount(){var c=0;for(var i=1;i<=MAX_ENTITIES;i++){if(!fieldVal('entity'+i+'_label'))break;c++;}return c;}");
  // Reihen-Zuordnung fuer benutzerdefiniertes Layout (z.B. "2,3") – null = automatisch
  httpServer.sendContent("function customRows(layout){layout=(layout||'').trim();if(!layout||layout==='auto')return null;var arr=layout.split(',').map(function(s){return parseInt(s.trim(),10)||0;}).filter(function(n){return n>0;});return arr.length?arr:null;}");
  // Exakte Nachbildung von build_tiles() aus ui.ino: liefert {x,y,w,h} je Kachel im 800x480-Koordinatensystem
  httpServer.sendContent(
    "function computeLayout(count,layout){"
    "var rects=[];if(count<=0)return rects;"
    "var body_y=HEADER_H+TILE_GAP;var body_h=PANEL_H-body_y-TILE_GAP;"
    "var rowsArr=customRows(layout);"
    "if(rowsArr){"
      "var rows=rowsArr.length;var row_h=(body_h-TILE_GAP*(rows-1))/rows;var idx=0;"
      "for(var r=0;r<rows&&idx<count;r++){var n=rowsArr[r];if(n<=0)continue;"
        "var tw=(PANEL_W-TILE_GAP*(n+1))/n;var th=row_h;var y=body_y+r*(row_h+TILE_GAP);"
        "for(var c=0;c<n&&idx<count;c++,idx++){var x=TILE_GAP+c*(tw+TILE_GAP);rects.push({x:x,y:y,w:tw,h:th});}"
      "}"
      "return rects;"
    "}"
    "var split=(count>=3)&&(count%2===1);"
    "if(split){"
      "var top_n=Math.floor(count/2);var bot_n=count-top_n;"
      "var bot_h=(body_h-TILE_GAP)*2/5;var top_h=body_h-TILE_GAP-bot_h;"
      "var top_w=(PANEL_W-TILE_GAP*(top_n+1))/top_n;var bot_w=(PANEL_W-TILE_GAP*(bot_n+1))/bot_n;"
      "for(var i=0;i<count;i++){"
        "if(i<top_n){rects.push({x:TILE_GAP+i*(top_w+TILE_GAP),y:body_y,w:top_w,h:top_h});}"
        "else{var j=i-top_n;rects.push({x:TILE_GAP+j*(bot_w+TILE_GAP),y:body_y+top_h+TILE_GAP,w:bot_w,h:bot_h});}"
      "}"
    "}else{"
      "var cols=count<=2?count:(count<=4?2:4);var rowsN=Math.ceil(count/cols);"
      "var tile_w=(PANEL_W-TILE_GAP*(cols+1))/cols;var tile_h=(body_h-TILE_GAP*(rowsN-1))/rowsN;"
      "for(var i2=0;i2<count;i2++){var col=i2%cols,row=Math.floor(i2/cols);"
        "rects.push({x:TILE_GAP+col*(tile_w+TILE_GAP),y:body_y+row*(tile_h+TILE_GAP),w:tile_w,h:tile_h});}"
    "}"
    "return rects;"
    "}");
  // Kachel-Inhalt – spiegelt build_sensor_tile / build_switch_tile / build_group_tile aus ui.ino
  httpServer.sendContent(
    "function tileHTML(i,th,scale){"
    "var typeVal=fieldVal('entity'+i+'_type')||'sensor';"
    "var labelVal=fieldVal('entity'+i+'_label')||('Tile '+i);"
    "var unitVal=fieldVal('entity'+i+'_unit');"
    "var colorVal=hex(fieldVal('entity'+i+'_color'));"
    "var titleColor=hex(fieldVal('title_color'));"
    "var topicVal=fieldVal('entity'+i+'_state_topic');"
    "var liveVal=liveOf(topicVal);"
    "var large=th>=200;"
    "var customSize=parseInt(fieldVal('entity'+i+'_font_size')||'0',10);"
    "var fTitle=(customSize||(large?20:16))*scale;"
    "var fSensorMain=(customSize||28)*scale;"
    "var fGroupMain=(customSize||(large?28:20))*scale;"
    "var fSub=(large?16:14)*scale;"
    "var fSwitchState=(customSize||14)*scale;"
    "var pad=(14*scale)+'px';"
    "if(typeVal==='switch'){"
      "var on=liveVal!==null&&/^(on|true|1)$/i.test(liveVal);"
      "var stateTxt=liveVal!==null?(on?'AN':'AUS'):'-';"
      "return `<div class=\"ptile-inner\" style=\"padding:${pad}\">`+"
        "`<div class=\"t-name\" style=\"color:${titleColor};font-size:${fTitle}px\">${labelVal}</div>`+"
        "`<div class=\"t-switch\" style=\"background:${on?'#3FB950':'#30363D'}\"></div>`+"
        "`<div class=\"t-state\" style=\"font-size:${fSwitchState}px;color:${liveVal!==null?'#3FB950':'#8B949E'}\">${stateTxt}</div>`+"
      "`</div>`;"
    "}"
    "if(typeVal==='group'){"
      "var hasMain=topicVal&&topicVal.length>0;"
      "var hasTitle=!fieldChecked('entity'+i+'_hide_title');"
      "var hasTopRow=hasTitle||hasMain;"
      "var mainHtml='';"
      "if(hasMain){var mv=liveVal!==null?(liveVal+(unitVal?(' '+unitVal):'')):(unitVal?('-- '+unitVal):'--');"
        "var mainColor=liveVal!==null?colorVal:'#8B949E';"
        "mainHtml=`<span class=\"t-mainval\" style=\"font-size:${fGroupMain}px;color:${mainColor}\">${mv}</span>`;}"
      "var titleHtml=hasTitle?`<span class=\"t-name\" style=\"color:${titleColor};font-size:${fTitle}px\">${labelVal}</span>`:'';"
      "var subsHtml='';"
      "for(var s=1;s<=MAX_SUBS;s++){var sLabel=fieldVal('entity'+i+'_sub'+s+'_label');if(!sLabel)continue;"
        "var sUnit=fieldVal('entity'+i+'_sub'+s+'_unit');var sTopic=fieldVal('entity'+i+'_sub'+s+'_topic');var sLive=liveOf(sTopic);"
        "var sColor=hex(fieldVal('entity'+i+'_sub'+s+'_color'));var sCustomSize=parseInt(fieldVal('entity'+i+'_sub'+s+'_font_size')||'0',10);"
        "var fSubFinal=(sCustomSize||(large?16:14))*scale;"
        "var sColorFinal=sColor;"
        "subsHtml+=`<div class=\"sub-row-preview\" style=\"font-size:${fSubFinal}px\"><span class=\"subname\">${sLabel}</span><span style=\"color:${sColorFinal}\">${sLive!==null?sLive:'--'}${sUnit?(' '+sUnit):''}</span></div>`;}"
      "return `<div class=\"ptile-inner\" style=\"padding:${pad}\">`+"
        "(hasTopRow?(`<div class=\"t-header-row\">${titleHtml}${mainHtml}</div>`+`<div class=\"t-divider\" style=\"margin:${4*scale}px 0\"></div>`):'')+"
        "`<div class=\"subs\" style=\"color:${hex(fieldVal('sub_label_color'))}\">${subsHtml}</div>`+"
      "`</div>`;"
    "}"
    "var mv=liveVal!==null?(liveVal+(unitVal?(' '+unitVal):'')):(unitVal?('-- '+unitVal):'--');"
    "var valColor=liveVal!==null?colorVal:'#8B949E';"
    "return `<div class=\"ptile-inner\" style=\"padding:${pad}\">`+"
      "`<div class=\"t-name\" style=\"color:${titleColor};font-size:${fTitle}px\">${labelVal}</div>`+"
      "`<div class=\"t-mainval-center\" style=\"font-size:${fSensorMain}px;color:${valColor}\">${mv}</div>`+"
    "`</div>`;"
    "}");
  httpServer.sendContent(
    "function updatePreview(){"
    "var count=getCount();"
    "var layout=fieldVal('layout');"
    "var rects=computeLayout(count,layout);"
    "var panel=document.getElementById('previewPanel');"
    "panel.querySelectorAll('.ptile').forEach(function(el){el.remove();});"
    "var scale=panel.getBoundingClientRect().width/PANEL_W;"
    "panel.style.background=hex(fieldVal('bg_color'));"
    "var hdr=document.getElementById('previewHeaderBar');"
    "hdr.style.height=(HEADER_H/PANEL_H*100)+'%';"
    "hdr.style.background=hex(fieldVal('header_bg_color'));"
    "var title=document.getElementById('previewTitle');"
    "title.style.color=hex(fieldVal('accent_color'));"
    "title.textContent=fieldVal('panel_title')||'HOME ASSISTANT PANEL';"
    "title.style.fontSize=(20*scale)+'px';"
    "document.getElementById('previewHicons').style.fontSize=(20*scale)+'px';"
    "rects.forEach(function(r,idx){"
      "var i=idx+1;var div=document.createElement('div');div.className='ptile';"
      "div.style.left=(r.x/PANEL_W*100)+'%';div.style.top=(r.y/PANEL_H*100)+'%';"
      "div.style.width=(r.w/PANEL_W*100)+'%';div.style.height=(r.h/PANEL_H*100)+'%';"
      "div.style.borderRadius=(10*scale)+'px';"
      "div.style.background=hex(fieldVal('tile_bg_color'));"
      "div.innerHTML=tileHTML(i,r.h,scale);"
      "panel.appendChild(div);"
    "});"
    "}");
  httpServer.sendContent("function scheduleUpdate(){requestAnimationFrame(updatePreview);}");
  httpServer.sendContent("window.addEventListener('resize',scheduleUpdate);");
  httpServer.sendContent(
    "var THEMES={"
    "dark_blue:{bg:'0D1117',header_bg:'161B22',accent:'58A6FF',tile_bg:'161B22',title:'E6EDF3',sub_label:'8B949E',value:'58A6FF'},"
    "dark_green:{bg:'0D1512',header_bg:'11201A',accent:'3FB950',tile_bg:'11201A',title:'E6EDF3',sub_label:'8B949E',value:'3FB950'},"
    "dark_warm:{bg:'1A1210',header_bg:'2B1D18',accent:'FF9800',tile_bg:'241A16',title:'FFD8A8',sub_label:'C9A88A',value:'FF9800'},"
    "light_classic:{bg:'F5F5F5',header_bg:'FFFFFF',accent:'1A73E8',tile_bg:'FFFFFF',title:'202124',sub_label:'5F6368',value:'1A73E8'},"
    "light_warm:{bg:'FFF8F0',header_bg:'FFEFE0',accent:'E8710A',tile_bg:'FFFFFF',title:'3C2A1E',sub_label:'8A6D5B',value:'E8710A'}"
    "};"
    "function setColor(name,v){var el=document.querySelector('[name='+name+']');if(el)el.value='#'+v;var h=document.getElementById(name+'_h');if(h)h.value=v.toUpperCase();}"
    "function applyTheme(name){var t=THEMES[name];if(!t)return;"
      "setColor('bg_color',t.bg);setColor('header_bg_color',t.header_bg);setColor('accent_color',t.accent);"
      "setColor('tile_bg_color',t.tile_bg);setColor('title_color',t.title);setColor('sub_label_color',t.sub_label);"
      "for(var i=1;i<=MAX_ENTITIES;i++){setColor('entity'+i+'_color',t.value);"
        "for(var s=1;s<=MAX_SUBS;s++){setColor('entity'+i+'_sub'+s+'_color',t.value);}}"
      "updatePreview();}");
  httpServer.sendContent("function refreshLiveTopics(){fetch('/api/topics').then(function(r){return r.json();}).then(function(d){liveTopics={};(d.topics||[]).forEach(function(o){liveTopics[o.t]=o.v;});updatePreview();}).catch(function(){});}");
  httpServer.sendContent("for(var i=1;i<=MAX_ENTITIES;i++){toggleSubs(i);toggleMainFields(i);}");
  httpServer.sendContent("document.querySelector('form').addEventListener('input',updatePreview);");
  httpServer.sendContent("document.querySelector('form').addEventListener('change',updatePreview);");
  httpServer.sendContent("setInterval(refreshLiveTopics,2000);");
  httpServer.sendContent("refreshLiveTopics();");
  httpServer.sendContent("updatePreview();");
  httpServer.sendContent("</script>");

  sendPageFoot();
}

// ─── GET / – Status Dashboard ────────────────────────────────
static void handleRoot() {
  unsigned long up = millis() / 1000;
  char upStr[32];
  snprintf(upStr, sizeof(upStr), "%lud %02luh %02lum %02lus",
           up / 86400, (up % 86400) / 3600, (up % 3600) / 60, up % 60);

  String mqtt = mqtt_connected
    ? "<span class='ok'>&#10003; verbunden</span>"
    : "<span class='err'>&#10007; getrennt</span>";

  String body =
    "<h2>Status</h2>"
    "<div class='card'><div class='row'>"
    "<div class='kv'><span class='k'>IP</span><span class='v'>" + WiFi.localIP().toString() + "</span></div>"
    "<div class='kv'><span class='k'>Hostname</span><span class='v'>" + CL_hostname + ".local</span></div>"
    "<div class='kv'><span class='k'>Uptime</span><span class='v'>" + String(upStr) + "</span></div>"
    "<div class='kv'><span class='k'>Freier Heap</span><span class='v'>" + String(ESP.getFreeHeap() / 1024) + " KB</span></div>"
    "<div class='kv'><span class='k'>WiFi RSSI</span><span class='v'>" + String(WiFi.RSSI()) + " dBm</span></div>"
    "<div class='kv'><span class='k'>MQTT</span><span class='v'>" + mqtt + "</span></div>"
    "</div></div>";

  httpServer.send(200, "text/html; charset=utf-8", page("Status", body));
}

// ─── GET /wizard – Konfigurations-Wizard ───────────────────
static void handleWizardGet() {
  sendWizardPage();
}

// ─── POST /wizard – Konfiguration speichern ─────────────────
static void handleWizardPost() {
  String panelTitle = httpServer.hasArg("panel_title") ? httpServer.arg("panel_title") : g_panel_title;
  String layout = httpServer.hasArg("layout") ? httpServer.arg("layout") : makeLayoutValue();
  layout.trim();
  // Anzahl Tiles ergibt sich aus den von oben nach unten ausgefuellten Tile-Labels (leeres Label = Ende)
  int count = 0;
  while (count < MAX_ENTITIES && httpServer.hasArg("entity" + String(count + 1) + "_label") &&
         httpServer.arg("entity" + String(count + 1) + "_label").length() > 0) {
    count++;
  }

  auto colorArg = [&](const String& name, uint32_t fallback) -> String {
    if (!httpServer.hasArg(name)) return hex6(fallback);
    String v = httpServer.arg(name);
    v.replace("#", "");
    if (v.length() != 6) return hex6(fallback);
    return v;
  };

  String cfg = "";
  cfg.reserve(16384); // Heap-Fragmentierung bei vielen kleinen += vermeiden
  cfg += "WiFi_ssid=" + CL_wifissid + "\n";
  cfg += "WiFi_password=" + CL_wifipassword + "\n";
  cfg += "hostname=" + CL_hostname + "\n";
  cfg += "MQTT_server=" + HASS_SERVER + "\n";
  cfg += "MQTT_port=" + String(HASS_SERVERPORT) + "\n";
  cfg += "MQTT_user=" + HASS_USERNAME + "\n";
  cfg += "MQTT_passwd=" + HASS_KEY + "\n";
  cfg += "NTP_server=" + ntpServer + "\n";
  cfg += "NTP_timezone=" + timezone + "\n";
  cfg += "panel_title=" + panelTitle + "\n";
  cfg += "title_color=" + colorArg("title_color", g_title_color) + "\n";
  cfg += "sub_label_color=" + colorArg("sub_label_color", g_sub_label_color) + "\n";
  cfg += "bg_color=" + colorArg("bg_color", g_bg_color) + "\n";
  cfg += "header_bg_color=" + colorArg("header_bg_color", g_header_bg_color) + "\n";
  cfg += "tile_bg_color=" + colorArg("tile_bg_color", g_tile_bg_color) + "\n";
  cfg += "accent_color=" + colorArg("accent_color", g_accent_color) + "\n";
  if (layout.length() > 0 && layout != "auto") cfg += "layout=" + layout + "\n";
  cfg += "entity_count=" + String(count) + "\n";
  for (int i = 1; i <= count; i++) {
    String prefix = "entity" + String(i) + "_";
    String type = httpServer.hasArg(prefix + "type") ? httpServer.arg(prefix + "type") : "sensor";
    String label = httpServer.hasArg(prefix + "label") ? httpServer.arg(prefix + "label") : "";
    String topic = httpServer.hasArg(prefix + "state_topic") ? httpServer.arg(prefix + "state_topic") : "";
    String cmdTopic = httpServer.hasArg(prefix + "cmd_topic") ? httpServer.arg(prefix + "cmd_topic") : "";
    String unit = httpServer.hasArg(prefix + "unit") ? httpServer.arg(prefix + "unit") : "";
    String color = colorArg(prefix + "color", 0xFFFFFF);
    String fontSize = httpServer.hasArg(prefix + "font_size") ? httpServer.arg(prefix + "font_size") : "0";
    if (label.length() == 0) label = "Tile " + String(i);
    cfg += prefix + "type=" + type + "\n";
    cfg += prefix + "label=" + label + "\n";
    cfg += prefix + "state_topic=" + topic + "\n";
    cfg += prefix + "unit=" + unit + "\n";
    cfg += prefix + "color=" + color + "\n";
    if (type == "switch" && cmdTopic.length() > 0) cfg += prefix + "cmd_topic=" + cmdTopic + "\n";
    if (fontSize.length() > 0 && fontSize != "0") cfg += prefix + "font_size=" + fontSize + "\n";

    if (type == "group") {
      if (httpServer.hasArg(prefix + "hide_title")) cfg += prefix + "hide_title=1\n";
      int subCount = 0;
      for (int s = 1; s <= MAX_SUBS; s++) {
        String subLabel = httpServer.hasArg(prefix + "sub" + String(s) + "_label") ? httpServer.arg(prefix + "sub" + String(s) + "_label") : "";
        if (subLabel.length() == 0) break;
        subCount++;
      }
      if (subCount > 0) {
        cfg += prefix + "sub_count=" + String(subCount) + "\n";
        for (int s = 1; s <= subCount; s++) {
          String sp = prefix + "sub" + String(s) + "_";
          String subLabel = httpServer.hasArg(sp + "label") ? httpServer.arg(sp + "label") : "";
          String subTopic = httpServer.hasArg(sp + "topic") ? httpServer.arg(sp + "topic") : "";
          String subUnit  = httpServer.hasArg(sp + "unit")  ? httpServer.arg(sp + "unit")  : "";
          String subCmd   = httpServer.hasArg(sp + "cmd")   ? httpServer.arg(sp + "cmd")   : "";
          String subColor = colorArg(sp + "color", 0xFFFFFF);
          String subFontSize = httpServer.hasArg(sp + "font_size") ? httpServer.arg(sp + "font_size") : "0";
          cfg += sp + "label=" + subLabel + "\n";
          if (subTopic.length()) cfg += sp + "topic=" + subTopic + "\n";
          if (subUnit.length())  cfg += sp + "unit="  + subUnit  + "\n";
          if (subCmd.length())   cfg += sp + "cmd="   + subCmd   + "\n";
          if (httpServer.hasArg(sp + "color"))     cfg += sp + "color=" + subColor + "\n";
          if (subFontSize.length() > 0 && subFontSize != "0") cfg += sp + "font_size=" + subFontSize + "\n";
        }
      }
    }
  }

  SD.remove("/config.txt.bak");
  SD.rename("/config.txt", "/config.txt.bak");
  File f = SD.open("/config.txt", FILE_WRITE);
  bool ok = false;
  if (f) { f.print(cfg); f.close(); ok = true; }

  webLog(ok ? "Wizard config gespeichert" : "Wizard: SD-Schreibfehler");
  httpServer.send(ok ? 200 : 500, "text/html; charset=utf-8",
    page("Wizard",
      "<h2 class='" + String(ok ? "ok" : "err") + "'>" + (ok ? "&#10003; Gespeichert" : "&#10007; Fehler") + "</h2>"
      "<p>Display startet in 3 Sekunden neu...</p>"
      "<a href='/wizard' style='color:#58a6ff'>Zurück</a>"));
  if (ok) { delay(3000); ESP.restart(); }
}

// ─── GET /config – Config Editor ─────────────────────────────
static void handleConfigGet() {
  String content = "";
  File f = SD.open("/config.txt", FILE_READ);
  if (f) { content = f.readString(); f.close(); }
  content.replace("&", "&amp;");
  content.replace("<", "&lt;");
  content.replace(">", "&gt;");

  String body =
    "<h2>config.txt bearbeiten</h2>"
    "<form method='POST' action='/config'>"
    "<textarea name='cfg' rows='38'>" + content + "</textarea><br>"
    "<input type='submit' value='Speichern &amp; Neustart'>"
    "</form>"
    "<p style='color:#8b949e;font-size:.82em'>Backup wird als /config.txt.bak angelegt.</p>"
    "<hr style='border-color:#30363d;margin:1.5em 0'>"
    "<h2>config.txt hochladen</h2>"
    "<form method='POST' action='/upload' enctype='multipart/form-data'>"
    "<input type='file' name='config' accept='.txt'>"
    "<input type='submit' value='Datei hochladen &amp; Neustart'>"
    "</form>"
    "<p style='color:#8b949e;font-size:.82em'>Alternative zum Editor &ndash; Backup wird als /config.txt.bak angelegt.</p>";

  httpServer.send(200, "text/html; charset=utf-8", page("Config", body));
}

// ─── POST /config – Config speichern ─────────────────────────
static void handleConfigPost() {
  if (!httpServer.hasArg("cfg")) {
    httpServer.send(400, "text/plain", "Missing cfg"); return;
  }
  String cfg = httpServer.arg("cfg");
  SD.remove("/config.txt.bak");
  SD.rename("/config.txt", "/config.txt.bak");
  File f = SD.open("/config.txt", FILE_WRITE);
  bool ok = false;
  if (f) { f.print(cfg); f.close(); ok = true; }

  webLog(ok ? "Config gespeichert (" + String(cfg.length()) + " B)"
            : "Config: SD-Schreibfehler!");

  String body = ok
    ? "<h2 class='ok'>&#10003; Gespeichert</h2><p>Display startet in 3 Sekunden neu...</p>"
    : "<h2 class='err'>&#10007; Fehler</h2><p>SD-Karte nicht erreichbar.</p>"
      "<a href='/config' style='color:#58a6ff'>Zurück</a>";

  httpServer.send(ok ? 200 : 500, "text/html; charset=utf-8", page("Config", body));
  if (ok) { delay(3000); ESP.restart(); }
}

// ─── GET /log – Log Viewer ────────────────────────────────────
static void handleLogPage() {
  String body =
    "<h2>Live Log</h2>"
    "<div id='log'></div>"
    "<p style='color:#8b949e;font-size:.82em'>Aktualisiert jede Sekunde &bull; letzte "
    + String(LOG_BUF_SIZE) + " Zeilen</p>"
    "<script>"
    "var s=0;"
    "function poll(){"
    "  fetch('/log/data?since='+s).then(r=>r.json()).then(d=>{"
    "    var b=document.getElementById('log');"
    "    d.l.forEach(function(t){"
    "      var e=document.createElement('div');"
    "      e.className='ll ln';e.textContent=t;b.appendChild(e);"
    "    });"
    "    if(d.l.length){b.scrollTop=b.scrollHeight;}"
    "    s=d.n;"
    "  }).catch(function(){});"
    "}"
    "setInterval(poll,1000);poll();"
    "</script>";

  httpServer.send(200, "text/html; charset=utf-8", page("Log", body));
}

// ─── GET /log/data – Poll Endpoint ───────────────────────────
static void handleLogData() {
  int since  = httpServer.arg("since").toInt();
  int oldest = max(0, _logIdx - LOG_BUF_SIZE);
  since = max(since, oldest);

  String json = "{\"n\":" + String(_logIdx) + ",\"l\":[";
  bool first = true;
  for (int i = since; i < _logIdx; i++) {
    String line = _log[i % LOG_BUF_SIZE];
    line.replace("\\", "\\\\");
    line.replace("\"", "\\\"");
    line.replace("\r", "");
    line.replace("\n", " ");
    if (!first) json += ",";
    json += "\"" + line + "\"";
    first = false;
  }
  json += "]}";
  httpServer.send(200, "application/json", json);
}

// ─── GET /api/topics – Live MQTT-Werte als JSON ──────────────
static String jsonEscape(const String& s) {
  String out = s;
  out.replace("\\", "\\\\");
  out.replace("\"", "\\\"");
  out.replace("\r", "");
  out.replace("\n", " ");
  return out;
}

static void handleApiTopics() {
  String json = "{\"connected\":" + String(mqtt_connected ? "true" : "false") + ",\"topics\":[";
  bool first = true;
  for (int i = 0; i < discovered_topic_count && i < MAX_DISCOVERED_TOPICS; i++) {
    if (!first) json += ",";
    json += "{\"t\":\"" + jsonEscape(discovered_topics[i]) + "\",\"v\":\"" + jsonEscape(discovered_payloads[i]) + "\"}";
    first = false;
  }
  json += "]}";
  httpServer.send(200, "application/json", json);
}

// ─── GET /restart ─────────────────────────────────────────────
static void handleRestart() {
  httpServer.send(200, "text/html; charset=utf-8",
    page("Neustart",
      "<h2>Neustart...</h2>"
      "<p>Seite lädt in 8 Sekunden neu.</p>"
      "<meta http-equiv='refresh' content='8;url=/'>"));
  delay(500);
  ESP.restart();
}

// ─── GET /upload – Upload Formular ───────────────────────────
static void handleUploadPage() {
  String body =
    "<h2>config.txt hochladen</h2>"
    "<form method='POST' action='/upload' enctype='multipart/form-data'>"
    "<input type='file' name='config' accept='.txt'>"
    "<input type='submit' value='Hochladen &amp; Neustart'>"
    "</form>"
    "<p style='color:#8b949e;font-size:.82em'>Tipp: Einfacher geht es über "
    "<a href='/config' style='color:#58a6ff'>Config Editor</a>.</p>";
  httpServer.send(200, "text/html; charset=utf-8", page("Upload", body));
}

// ─── POST /upload – SD Datei-Upload ──────────────────────────
static File _uploadFile;
static bool _uploadOk = false;

static void handleUpload() {
  HTTPUpload& upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    _uploadOk = false;
    webLog("Upload start: " + upload.filename);
    SD.remove("/config.txt.bak");
    SD.rename("/config.txt", "/config.txt.bak");
    _uploadFile = SD.open("/config.txt", FILE_WRITE);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (_uploadFile) _uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (_uploadFile) { _uploadFile.close(); _uploadOk = true; }
    webLog(_uploadOk ? "Upload OK: " + String(upload.totalSize) + " B"
                     : "Upload: SD-Fehler");
  }
}

static void handleUploadDone() {
  String body = _uploadOk
    ? "<h2 class='ok'>&#10003; Gespeichert</h2><p>Display startet in 3 Sekunden neu...</p>"
    : "<h2 class='err'>&#10007; Fehler</h2><p>SD nicht erreichbar. Backup: /config.txt.bak</p>"
      "<a href='/' style='color:#58a6ff'>Zurück</a>";
  httpServer.send(_uploadOk ? 200 : 500, "text/html; charset=utf-8", page("Upload", body));
  if (_uploadOk) { delay(3000); ESP.restart(); }
}

// ─── GET /firmware ──────────────────────────────────────────
static void handleFirmwarePage() {
  String body =
    "<h2>Firmware Update</h2>"
    "<div class='card'>"
    "<p style='color:#fea020'>Display schaltet w&auml;hrend des Updates aus.<br>"
    "Nur .bin Dateien aus PlatformIO Build (.pio/build/ha_panel/firmware.bin).</p>"
    "<form method='POST' action='/firmware' enctype='multipart/form-data' onsubmit='go()'>"
    "<input type='file' name='firmware' accept='.bin' id='f'>"
    "<br><input type='submit' id='btn' value='Firmware flashen'>"
    "</form>"
    "<div id='wait' style='display:none;margin-top:1.2em'>"
    "<h2 class='warn'>&#9203; Flash l&auml;uft &ndash; bitte warten...</h2>"
    "<p style='color:#8b949e'>Nicht die Seite schlie&szlig;en oder das Ger&auml;t trennen.<br>"
    "Das Display startet nach dem Update automatisch neu.</p>"
    "</div>"
    "<script>"
    "function go(){"
    "var f=document.getElementById('f');"
    "if(!f.files.length){alert('Bitte eine .bin Datei ausw\u00e4hlen.');return false;}"
    "document.getElementById('btn').disabled=true;"
    "document.getElementById('btn').value='Wird hochgeladen...';"
    "document.getElementById('wait').style.display='block';"
    "}"
    "</script>"
    "</div>";
  httpServer.send(200, "text/html; charset=utf-8", page("Firmware", body));
}

// ─── POST /firmware – Firmware flashen ──────────────────────
static bool _fwOk = false;

static void handleFirmwareUpload() {
  HTTPUpload& upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    _fwOk = false;
    webLog("Firmware upload: " + upload.filename);
    // Tasks suspendieren + Backlight aus (wie ArduinoOTA)
    g_ota_active   = true;
    g_ota_show_req = true;  // lvgl_task suspendiert sich selbst
    delay(20);              // kurz warten bis Tasks suspended sind
    if (g_mqtt_task_handle) vTaskSuspend(g_mqtt_task_handle);
    digitalWrite(TFT_BL, LOW);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      webLog("Firmware: begin failed: " + String(Update.errorString()));
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      webLog("Firmware: write error: " + String(Update.errorString()));
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      _fwOk = true;
      webLog("Firmware OK: " + String(upload.totalSize) + " B - Neustart...");
    } else {
      webLog("Firmware: end failed: " + String(Update.errorString()));
    }
  }
}

static void handleFirmwareDone() {
  if (_fwOk) {
    httpServer.send(200, "text/html; charset=utf-8",
      page("Firmware", "<h2 class='ok'>&#10003; Update OK</h2><p>Neustart in 3 Sekunden...</p>"));
    delay(3000);
    ESP.restart();
  } else {
    // Fehler: Tasks wieder starten
    g_ota_active     = false;
    g_ota_error_time = millis();
    if (g_lvgl_task_handle) vTaskResume(g_lvgl_task_handle);
    if (g_mqtt_task_handle) vTaskResume(g_mqtt_task_handle);
    digitalWrite(TFT_BL, HIGH);
    httpServer.send(500, "text/html; charset=utf-8",
      page("Firmware", "<h2 class='err'>&#10007; Update fehlgeschlagen</h2>"
        "<p>" + String(Update.errorString()) + "</p>"
        "<a href='/firmware' style='color:#58a6ff'>Nochmal versuchen</a>"));
  }
}

// ─── Setup (nach WiFi-Verbindung aufrufen) ───────────────────
void ota_setup() {
  ArduinoOTA.setHostname(CL_hostname.c_str());
  ArduinoOTA.onStart([]() {
    webLog("OTA Start: " + String(ArduinoOTA.getCommand() == U_FLASH ? "Firmware" : "FS"));
    g_ota_active   = true;
    g_ota_show_req = true;
    digitalWrite(TFT_BL, LOW);  // Backlight aus – kein sichtbares LCD-Flackern
  });
  ArduinoOTA.onEnd([]() {
    webLog("OTA: Fertig");
    digitalWrite(TFT_BL, HIGH);  // Backlight an (Reboot folgt gleich)
  });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    static int last_pct = -1;
    static int last_log = -1;
    int pct    = (p * 100) / t;
    int bucket = pct / 10;
    if (pct != last_pct) {           // Bar: jedes %
      last_pct = pct;
      ui_ota_progress(pct);
    }
    if (bucket != last_log) {        // Log + webLog: alle 10%
      last_log = bucket;
      webLog("OTA: " + String(pct) + "%");
      String msg = "Uploading: " + String(pct) + " %";
      ui_ota_log(msg.c_str());
    }
  });
  ArduinoOTA.onError([](ota_error_t e) {
    webLog("OTA Error: " + String(e));
    String err = "Error (" + String(e) + "): ";
    if      (e == OTA_AUTH_ERROR)    err += "Auth failed";
    else if (e == OTA_BEGIN_ERROR)   err += "Begin failed";
    else if (e == OTA_CONNECT_ERROR) err += "Connect failed";
    else if (e == OTA_RECEIVE_ERROR) err += "Receive failed";
    else if (e == OTA_END_ERROR)     err += "End failed";
    g_ota_error_time = millis();
    if (g_lvgl_task_handle) vTaskResume(g_lvgl_task_handle);
    if (g_mqtt_task_handle) vTaskResume(g_mqtt_task_handle);
    digitalWrite(TFT_BL, HIGH);  // Backlight wieder an
  });
  ArduinoOTA.begin();

  // mDNS starten damit <hostname>.local auflösbar ist
  if (MDNS.begin(CL_hostname.c_str())) {
    webLog("mDNS: " + CL_hostname + ".local");
  }

  httpServer.on("/",         HTTP_GET,  handleRoot);
  httpServer.on("/config",   HTTP_GET,  handleConfigGet);
  httpServer.on("/config",   HTTP_POST, handleConfigPost);
  httpServer.on("/wizard",   HTTP_GET,  handleWizardGet);
  httpServer.on("/wizard",   HTTP_POST, handleWizardPost);
  httpServer.on("/log",      HTTP_GET,  handleLogPage);
  httpServer.on("/log/data", HTTP_GET,  handleLogData);
  httpServer.on("/api/topics", HTTP_GET, handleApiTopics);
  httpServer.on("/restart",  HTTP_GET,  handleRestart);
  httpServer.on("/upload",   HTTP_GET,  handleUploadPage);
  httpServer.on("/upload",   HTTP_POST, handleUploadDone, handleUpload);
  httpServer.on("/firmware", HTTP_GET,  handleFirmwarePage);
  httpServer.on("/firmware", HTTP_POST, handleFirmwareDone, handleFirmwareUpload);
  httpServer.begin();

  webLog("WebUI: http://" + CL_hostname + ".local  (" + WiFi.localIP().toString() + ")");
  webLog("OTA:   " + CL_hostname + ".local");
}

// ─── FreeRTOS Task ────────────────────────────────────────────
void ota_task(void* param) {
  while (true) {
    ArduinoOTA.handle();
    if (!g_ota_active) httpServer.handleClient();
    vTaskDelay(pdMS_TO_TICKS(g_ota_active ? 1 : 5));  // OTA: schneller pollen
  }
}
