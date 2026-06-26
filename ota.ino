// ============================================================
//  OTA – Firmware-Update (ArduinoOTA) + HTTP SD-Karten-Upload
//
//  Firmware-Update:
//    PlatformIO: pio run -e ha_panel_ota -t upload
//    Hostname:   hassPanel.local
//
//  SD-Karte (config.txt):
//    Browser: http://<IP>/
//    Formular → config.txt auswählen → Hochladen
//    Das Display startet nach dem Upload automatisch neu.
// ============================================================

#include <ArduinoOTA.h>
#include <WebServer.h>

static WebServer httpServer(80);

// ─── HTTP: Startseite mit Upload-Formular ────────────────────
static void handleRoot() {
  String ip = WiFi.localIP().toString();
  httpServer.send(200, "text/html; charset=utf-8",
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>HAssPanel</title>"
    "<style>body{font-family:sans-serif;background:#0d1117;color:#e6edf3;padding:2em}"
    "h2{color:#58a6ff}input[type=file]{margin:1em 0;display:block}"
    "button{background:#238636;color:#fff;border:none;padding:.6em 1.4em;"
    "border-radius:6px;cursor:pointer;font-size:1em}</style></head>"
    "<body><h2>HAssPanel – " + ip + "</h2>"
    "<h3>config.txt auf SD-Karte aktualisieren</h3>"
    "<form method='POST' action='/upload' enctype='multipart/form-data'>"
    "<input type='file' name='config' accept='.txt'>"
    "<button type='submit'>Hochladen &amp; Neustart</button>"
    "</form></body></html>"
  );
}

// ─── HTTP: Datei-Upload auf SD schreiben ─────────────────────
static File   _uploadFile;
static bool   _uploadOk = false;

static void handleUpload() {
  HTTPUpload& upload = httpServer.upload();

  if (upload.status == UPLOAD_FILE_START) {
    _uploadOk = false;
    Serial.println("SD Upload start: " + upload.filename);
    SD.remove("/config.txt.bak");
    SD.rename("/config.txt", "/config.txt.bak");   // Backup anlegen
    _uploadFile = SD.open("/config.txt", FILE_WRITE);
    if (!_uploadFile) Serial.println("SD: Datei konnte nicht geöffnet werden!");

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (_uploadFile) _uploadFile.write(upload.buf, upload.currentSize);

  } else if (upload.status == UPLOAD_FILE_END) {
    if (_uploadFile) {
      _uploadFile.close();
      _uploadOk = true;
      Serial.printf("SD Upload OK: %u Bytes\n", upload.totalSize);
    }
  }
}

static void handleUploadDone() {
  if (_uploadOk) {
    httpServer.send(200, "text/html; charset=utf-8",
      "<!DOCTYPE html><html><head><meta charset='utf-8'>"
      "<style>body{font-family:sans-serif;background:#0d1117;color:#e6edf3;padding:2em}"
      "h2{color:#3fb950}</style></head>"
      "<body><h2>&#10003; config.txt gespeichert</h2>"
      "<p>Display startet in 3 Sekunden neu...</p></body></html>"
    );
    delay(3000);
    ESP.restart();
  } else {
    httpServer.send(500, "text/html; charset=utf-8",
      "<!DOCTYPE html><html><head><meta charset='utf-8'>"
      "<style>body{font-family:sans-serif;background:#0d1117;color:#e6edf3;padding:2em}"
      "h2{color:#f85149}</style></head>"
      "<body><h2>&#10007; Fehler beim Schreiben</h2>"
      "<p>SD-Karte erreichbar? Backup: /config.txt.bak</p>"
      "<a href='/' style='color:#58a6ff'>Zurück</a></body></html>"
    );
  }
}

// ─── Setup (nach WiFi-Verbindung aufrufen) ───────────────────
void ota_setup() {
  // ── ArduinoOTA ──────────────────────────────────────────────
  ArduinoOTA.setHostname("hassPanel");
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "Firmware" : "Dateisystem";
    Serial.println("OTA Start: " + type);
  });
  ArduinoOTA.onEnd([]()  { Serial.println("\nOTA: Fertig – Neustart"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    Serial.printf("OTA: %u%%\r", (p * 100) / t);
  });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("OTA Fehler [%u]\n", e);
  });
  ArduinoOTA.begin();

  // ── HTTP Server ──────────────────────────────────────────────
  httpServer.on("/",       HTTP_GET,  handleRoot);
  httpServer.on("/upload", HTTP_POST, handleUploadDone, handleUpload);
  httpServer.begin();

  Serial.println("OTA/HTTP bereit: http://" + WiFi.localIP().toString());
  Serial.println("OTA Hostname: hassPanel.local");
}

// ─── FreeRTOS Task (Core 0, läuft parallel zu MQTT) ─────────
void ota_task(void* param) {
  while (true) {
    ArduinoOTA.handle();
    httpServer.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
