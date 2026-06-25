// ─── Konfiguration von SD-Karte laden ─────────────────────────
//
//  Dateiformat: /config.txt
//  Pro Zeile: Key=Wert
//  Kommentare: Zeilen mit // oder # werden ignoriert
//
//  Globale Keys:
//    WiFi_ssid, WiFi_password
//    MQTT_server, MQTT_port, MQTT_user, MQTT_passwd
//    NTP_server, NTP_timezone
//
//  Entity-Keys (i = 1..8):
//    entity_count=N
//    entityN_type=sensor|switch
//    entityN_label=Anzeigename
//    entityN_unit=Einheit              (nur sensor)
//    entityN_topic=ha/.../state        (nur sensor, oder state_topic)
//    entityN_state_topic=ha/.../state  (sensor oder switch)
//    entityN_cmd_topic=ha/.../set      (nur switch)
//    entityN_color=RRGGBB              (Hex, optional)
// ─────────────────────────────────────────────────────────────

void loadconfig() {
  File file = SD.open("/config.txt");
  if (!file) {
    Serial.println("SD: config.txt nicht gefunden.");
    return;
  }

  entity_count = 0;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.startsWith("//") || line.startsWith("#") || line.length() == 0) continue;

    int sep = line.indexOf('=');
    if (sep < 1) continue;

    String key = line.substring(0, sep);
    String val = line.substring(sep + 1);
    key.trim();
    val.trim();

    // ── Globale Einstellungen ────────────────────────────────
    if      (key == "WiFi_ssid")     CL_wifissid       = val;
    else if (key == "WiFi_password") CL_wifipassword   = val;
    else if (key == "MQTT_server")   HASS_SERVER       = val;
    else if (key == "MQTT_port")     HASS_SERVERPORT   = (uint16_t)val.toInt();
    else if (key == "MQTT_user")     HASS_USERNAME     = val;
    else if (key == "MQTT_passwd")   HASS_KEY          = val;
    else if (key == "NTP_server")    ntpServer         = val;
    else if (key == "NTP_timezone")  timezone          = val;
    else if (key == "entity_count")  entity_count      = constrain(val.toInt(), 0, MAX_ENTITIES);
    else if (key == "title_color")   g_title_color     = (uint32_t)strtoul(val.c_str(), nullptr, 16);
    else if (key == "layout") {
      // "2,4" → g_layout_rows=2, g_layout_row_count={2,4}
      g_layout_rows = 0;
      int start = 0;
      for (int j = 0; j <= (int)val.length(); j++) {
        if (j == (int)val.length() || val[j] == ',') {
          String part = val.substring(start, j);
          part.trim();
          if (g_layout_rows < MAX_ROWS && part.length() > 0)
            g_layout_row_count[g_layout_rows++] = part.toInt();
          start = j + 1;
        }
      }
    }

    // ── Entity-Keys: entityN_xxx ──────────────────────────────
    else {
      for (int i = 0; i < MAX_ENTITIES; i++) {
        String prefix = "entity" + String(i + 1) + "_";
        if (!key.startsWith(prefix)) continue;

        String field = key.substring(prefix.length());

        if (field == "type") {
          if      (val.equalsIgnoreCase("switch")) entities[i].type = ENTITY_SWITCH;
          else if (val.equalsIgnoreCase("group"))  entities[i].type = ENTITY_GROUP;
          else                                     entities[i].type = ENTITY_SENSOR;
        } else if (field == "label") {
          entities[i].label = val;
        } else if (field == "unit") {
          entities[i].unit = val;
        } else if (field == "topic" || field == "state_topic") {
          entities[i].state_topic = val;
        } else if (field == "cmd_topic") {
          entities[i].cmd_topic = val;
        } else if (field == "color") {
          entities[i].color = (uint32_t)strtoul(val.c_str(), nullptr, 16);
        } else if (field == "sub_count") {
          entities[i].sub_count = constrain(val.toInt(), 0, MAX_SUBS);
        } else if (field == "thresh_count") {
          entities[i].thresh_count = constrain(val.toInt(), 0, MAX_THRESHOLDS);
        } else {
          // Sub-Sensor-Keys: sub1_label, sub1_topic, sub1_unit, ...
          for (int s = 0; s < MAX_SUBS; s++) {
            String sp = "sub" + String(s + 1) + "_";
            if (!field.startsWith(sp)) continue;
            String sf = field.substring(sp.length());
            if      (sf == "label") entities[i].sub_label[s]     = val;
            else if (sf == "topic") entities[i].sub_topic[s]     = val;
            else if (sf == "cmd")   entities[i].sub_cmd_topic[s] = val;
            else if (sf == "unit")  entities[i].sub_unit[s]      = val;
            break;
          }
          // Farb-Schwellwert-Keys: thresh1_val, thresh1_color, ...
          for (int t = 0; t < MAX_THRESHOLDS; t++) {
            String tp = "thresh" + String(t + 1) + "_";
            if (!field.startsWith(tp)) continue;
            String tf = field.substring(tp.length());
            if      (tf == "val")   entities[i].thresh_val[t]   = val.toFloat();
            else if (tf == "color") entities[i].thresh_color[t] = (uint32_t)strtoul(val.c_str(), nullptr, 16);
            break;
          }
        }
        break;
      }
    }
  }

  file.close();

  // Defaults setzen
  for (int i = 0; i < entity_count; i++) {
    if (entities[i].color == 0) entities[i].color = 0xFFFFFF;
    entities[i].valid = false;
    for (int s = 0; s < MAX_SUBS; s++) entities[i].sub_valid[s] = false;
  }

  Serial.printf("Config geladen: %d Entities\n", entity_count);
}
