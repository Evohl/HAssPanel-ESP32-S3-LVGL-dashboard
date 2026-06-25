// ─── MQTT – Subscribe, Publish, Callback ──────────────────────

// Callback: Eingehende MQTT-Nachrichten verarbeiten
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char buf[64];
  unsigned int len = min(length, (unsigned int)(sizeof(buf) - 1));
  memcpy(buf, payload, len);
  buf[len] = '\0';
  String val = String(buf);
  val.trim();
  String t = String(topic);

  // Haupt-Topics (kein return – dasselbe Topic kann auch Sub-Topic sein)
  for (int i = 0; i < entity_count; i++) {
    if (entities[i].state_topic.length() == 0 || t != entities[i].state_topic) continue;

    if (entities[i].type == ENTITY_SENSOR || entities[i].type == ENTITY_GROUP) {
      if (val != "unavailable" && val != "unknown" && val != "None") {
        entities[i].sensor_value = val.toFloat();
        entities[i].valid = true;
        entities[i].dirty = true;
      }
    } else if (entities[i].type == ENTITY_SWITCH) {
      String v = val;
      v.toLowerCase();
      entities[i].switch_state = (v == "on" || v == "true" || v == "1");
      entities[i].valid = true;
      LVGL_LOCK();
      ui_update_entity(i);
      LVGL_UNLOCK();
    }
  }

  // Sub-Topics – alle passenden Einträge aktualisieren (kein frühes return)
  for (int i = 0; i < entity_count; i++) {
    if (entities[i].type != ENTITY_GROUP) continue;
    for (int s = 0; s < entities[i].sub_count && s < MAX_SUBS; s++) {
      if (entities[i].sub_topic[s].length() == 0 || t != entities[i].sub_topic[s]) continue;
      if (val != "unavailable" && val != "unknown" && val != "None") {
        // JSON-Payload? → "state"-Wert extrahieren (z.B. Zigbee2MQTT: {"state":"ON",...})
        String effective = val;
        int si_json = val.indexOf("\"state\"");
        if (si_json >= 0) {
          int colon = val.indexOf(':', si_json);
          if (colon >= 0) {
            int vstart = val.indexOf('"', colon + 1);
            int vend   = vstart >= 0 ? val.indexOf('"', vstart + 1) : -1;
            if (vstart >= 0 && vend > vstart) effective = val.substring(vstart + 1, vend);
          }
        }
        entities[i].sub_value[s] = effective.toFloat();
        entities[i].sub_raw[s]   = effective;
        entities[i].sub_valid[s] = true;
        entities[i].dirty = true;
      }
    }
  }
}

// Reconnect mit 5-Sekunden-Cooldown
void mqttReconnect() {
  static unsigned long lastAttempt = 0;
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqttClient.connected()) return;
  if (millis() - lastAttempt < 5000) return;
  lastAttempt = millis();

  Serial.print("MQTT verbinden... ");
  if (mqttClient.connect(clientId.c_str(), HASS_USERNAME.c_str(), HASS_KEY.c_str())) {
    Serial.println("OK");
    for (int i = 0; i < entity_count; i++) {
      if (entities[i].state_topic.length() > 0) {
        mqttClient.subscribe(entities[i].state_topic.c_str());
        Serial.println("  Subscribe: " + entities[i].state_topic);
      }
    }
    // Sub-Topics für Group-Entities
    for (int i = 0; i < entity_count; i++) {
      if (entities[i].type != ENTITY_GROUP) continue;
      for (int s = 0; s < entities[i].sub_count && s < MAX_SUBS; s++) {
        if (entities[i].sub_topic[s].length() > 0) {
          mqttClient.subscribe(entities[i].sub_topic[s].c_str());
          Serial.println("  Subscribe (sub): " + entities[i].sub_topic[s]);
        }
      }
    }
    mqtt_connected = true;
  } else {
    Serial.printf("Fehler rc=%d\n", mqttClient.state());
    mqtt_connected = false;
  }
}
