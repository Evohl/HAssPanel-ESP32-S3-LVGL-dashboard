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
  "input[type=file]{color:#e6edf3;margin:.5em 0;display:block;font-size:1em}";

static String nav() {
  return "<nav>"
         "<span class='t'>&#9632; " + CL_hostname + "</span>"
         "<a href='/'>Status</a>"
         "<a href='/config'>Config</a>"
         "<a href='/log'>Log</a>"
         "<form style='margin:0' action='/restart' method='GET'>"
         "<button class='red' onclick=\"return confirm('Neustart?')\">&#8635; Neustart</button>"
         "</form>"
         "</nav>";
}

static String page(const String& title, const String& body) {
  String h = "<!DOCTYPE html><html><head>"
             "<meta charset='utf-8'>"
             "<meta name='viewport' content='width=device-width,initial-scale=1'>"
             "<title>HAssPanel – " + title + "</title>"
             "<style>" + CSS + "</style>"
             "</head><body>" + nav() + "<main>" + body + "</main></body></html>";
  return h;
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

// ─── Setup (nach WiFi-Verbindung aufrufen) ───────────────────
void ota_setup() {
  ArduinoOTA.setHostname(CL_hostname.c_str());
  ArduinoOTA.onStart([]() {
    webLog("OTA Start: " + String(ArduinoOTA.getCommand() == U_FLASH ? "Firmware" : "FS"));
  });
  ArduinoOTA.onEnd([]() { webLog("OTA: Fertig"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    static int last = -1;
    int pct = (p * 100) / t;
    int bucket = pct / 10;
    if (bucket != last) { last = bucket; webLog("OTA: " + String(pct) + "%"); }
  });
  ArduinoOTA.onError([](ota_error_t e) { webLog("OTA Fehler: " + String(e)); });
  ArduinoOTA.begin();

  // mDNS starten damit <hostname>.local auflösbar ist
  if (MDNS.begin(CL_hostname.c_str())) {
    webLog("mDNS: " + CL_hostname + ".local");
  }

  httpServer.on("/",         HTTP_GET,  handleRoot);
  httpServer.on("/config",   HTTP_GET,  handleConfigGet);
  httpServer.on("/config",   HTTP_POST, handleConfigPost);
  httpServer.on("/log",      HTTP_GET,  handleLogPage);
  httpServer.on("/log/data", HTTP_GET,  handleLogData);
  httpServer.on("/restart",  HTTP_GET,  handleRestart);
  httpServer.on("/upload",   HTTP_GET,  handleUploadPage);
  httpServer.on("/upload",   HTTP_POST, handleUploadDone, handleUpload);
  httpServer.begin();

  webLog("WebUI: http://" + CL_hostname + ".local  (" + WiFi.localIP().toString() + ")");
  webLog("OTA:   " + CL_hostname + ".local");
}

// ─── FreeRTOS Task ────────────────────────────────────────────
void ota_task(void* param) {
  while (true) {
    ArduinoOTA.handle();
    httpServer.handleClient();
    // Kürzere Pause damit OTA-Updates schnell reagieren
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
