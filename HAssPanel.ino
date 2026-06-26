// ============================================================
//  HOME ASSISTANT PANEL  v1.0
//  Universelle LVGL-Anzeigetafel für Home Assistant
//  7" ESP32-8048S070 (800×480 RGB-Panel, ESP32-S3)
//
//  FEATURES:
//    - Bis zu 8 konfigurierbare Entities (Sensoren & Schalter)
//    - LVGL-basierte UI mit Dark Theme
//    - Touch-Support (GT911, auto-erkannt)
//    - Konfiguration per SD-Karte (/config.txt)
//    - MQTT Subscribe (Sensoren) & Publish (Schalter)
//
//  SD-KARTE (config.txt):
//    WiFi_ssid=MeinNetz
//    WiFi_password=MeinPasswort
//    MQTT_server=192.168.1.x
//    MQTT_port=1883
//    entity_count=4
//    entity1_type=sensor
//    entity1_label=Solar
//    entity1_topic=homeassistant/sensor/xxx/state
//    entity1_unit=W
//    entity1_color=FEA020
//    entity2_type=switch
//    entity2_label=Licht
//    entity2_state_topic=homeassistant/switch/licht/state
//    entity2_cmd_topic=homeassistant/switch/licht/set
// ============================================================

#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "SD.h"
#include <TAMC_GT911.h>
#include <lvgl.h>
#include <time.h>

// ─── DISPLAY ──────────────────────────────────────────────────
#define TFT_BL 2
const uint16_t screenWidth  = 800;
const uint16_t screenHeight = 480;

Arduino_ESP32RGBPanel* rgbpanel = new Arduino_ESP32RGBPanel(
  41 /* DE */, 40 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
  14 /* R0 */, 21 /* R1 */, 47 /* R2 */, 48 /* R3 */, 45 /* R4 */,
  9  /* G0 */, 46 /* G1 */, 3  /* G2 */, 8  /* G3 */, 16 /* G4 */, 1 /* G5 */,
  15 /* B0 */, 7  /* B1 */, 6  /* B2 */, 5  /* B3 */, 4  /* B4 */,
  0 /* hsync_polarity */, 210 /* hsync_front_porch */, 30 /* hsync_pulse_width */, 16 /* hsync_back_porch */,
  0 /* vsync_polarity */,  22 /* vsync_front_porch */, 13 /* vsync_pulse_width */, 10 /* vsync_back_porch */,
  1 /* pclk_active_neg */, 16000000 /* prefer_speed */);

Arduino_RGB_Display* gfx = new Arduino_RGB_Display(screenWidth, screenHeight, rgbpanel);

// ─── TOUCH ────────────────────────────────────────────────────
#define TOUCH_GT911_SCL      20
#define TOUCH_GT911_SDA      19
#define TOUCH_GT911_INT      -1
#define TOUCH_GT911_RST      38
#define TOUCH_GT911_ROTATION ROTATION_NORMAL
#define TOUCH_MAP_X1         800
#define TOUCH_MAP_X2         0
#define TOUCH_MAP_Y1         480
#define TOUCH_MAP_Y2         0

TAMC_GT911 ts = TAMC_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL,
                            TOUCH_GT911_INT, TOUCH_GT911_RST,
                            max(TOUCH_MAP_X1, TOUCH_MAP_X2),
                            max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));
bool touch_available = false;

// ─── WIFI ─────────────────────────────────────────────────────
String CL_wifissid     = "";
String CL_wifipassword = "";
String CL_hostname     = "hassPanel";  // Überschrieben durch hostname= in config.txt

// ─── MQTT ─────────────────────────────────────────────────────
String   HASS_SERVER     = "";
uint16_t HASS_SERVERPORT = 1883;
String   HASS_USERNAME   = "";
String   HASS_KEY        = "";
String   clientId        = "HAssPanel-" + String((uint32_t)ESP.getEfuseMac(), HEX);

bool mqtt_connected = false;

WiFiClient   espClient;
PubSubClient mqttClient(espClient);

// ─── NTP ──────────────────────────────────────────────────────
struct tm tm_info;
String ntpServer = "pool.ntp.org";
String timezone  = "CET-1CEST,M3.5.0/02,M10.5.0/03";

// ─── ENTITY MODEL ─────────────────────────────────────────────
#define MAX_ENTITIES   8
#define MAX_SUBS       5
#define MAX_THRESHOLDS 3
#define MAX_ROWS       4

enum EntityType { ENTITY_SENSOR, ENTITY_SWITCH, ENTITY_GROUP };

struct HAEntity {
  EntityType type          = ENTITY_SENSOR;
  String     label         = "";
  String     unit          = "";
  String     state_topic   = "";
  String     cmd_topic     = "";
  uint32_t   color         = 0xFFFFFF;  // 0xRRGGBB
  // Laufzeit-Werte
  float      sensor_value  = 0.0f;
  bool       switch_state  = false;
  bool       valid         = false;
  // Gruppe: bis zu MAX_SUBS Sub-Sensoren
  int        sub_count                = 0;
  String     sub_label[MAX_SUBS];
  String     sub_topic[MAX_SUBS];
  String     sub_cmd_topic[MAX_SUBS];   // leer = read-only; gesetzt = Toggle-Switch
  String     sub_unit[MAX_SUBS];
  float      sub_value[MAX_SUBS]      = {};
  bool       sub_valid[MAX_SUBS]      = {};
  String     sub_raw[MAX_SUBS];          // Rohwert-String (z.B. "on"/"off")
  // Farb-Schwellwerte (aufsteigend sortiert)
  int      thresh_count                   = 0;
  float    thresh_val[MAX_THRESHOLDS]     = {};
  uint32_t thresh_color[MAX_THRESHOLDS]   = {};
  // LVGL Widgets
  lv_obj_t*  tile                     = nullptr;
  lv_obj_t*  value_label              = nullptr;
  lv_obj_t*  unit_label               = nullptr;
  lv_obj_t*  sw                       = nullptr;
  lv_obj_t*  sub_val_lbl[MAX_SUBS]    = {};
  lv_obj_t*  sub_sw[MAX_SUBS]         = {};  // Toggle-Switches für Sub-Cmd-Topics
  // Dirty-Flag: UI-Update ausstehend
  bool       dirty                    = false;
};

HAEntity entities[MAX_ENTITIES];
int      entity_count = 0;

// ─── LVGL Header-Widgets ──────────────────────────────────────
lv_obj_t* lbl_time = nullptr;
lv_obj_t* lbl_wifi = nullptr;
lv_obj_t* lbl_mqtt = nullptr;

// ─── Globale UI-Einstellungen ─────────────────────────────────
uint32_t g_title_color     = 0x8B949E;  // Überschrift-Farbe aller Kacheln (grau)
uint32_t g_sub_label_color = 0x8B949E;  // Farbe der Sub-Entity-Namen (grau)
String   g_panel_title     = "HOME ASSISTANT PANEL";  // Titel in der Kopfzeile

// Benutzerdefiniertes Layout: layout=2,4 → 2 Tiles oben, 4 unten
// g_layout_rows == 0 → automatisches Layout
int g_layout_rows = 0;
int g_layout_row_count[MAX_ROWS] = {};

// ─── LVGL Mutex (Core-0 / Core-1 Synchronisation) ─────────────
SemaphoreHandle_t g_lvgl_mutex = NULL;
#define LVGL_LOCK()   xSemaphoreTake(g_lvgl_mutex, portMAX_DELAY)
#define LVGL_UNLOCK() xSemaphoreGive(g_lvgl_mutex)

// ─── OTA-State ───────────────────────────────────────────────
volatile bool          g_ota_active        = false;
volatile bool          g_ota_show_req      = false;
volatile unsigned long g_ota_error_time    = 0;
TaskHandle_t           g_lvgl_task_handle  = NULL;
TaskHandle_t           g_mqtt_task_handle  = NULL;

// ─── WiFi/MQTT-Task auf Core 0 ─────────────────────────────
void mqtt_task(void* param) {
  while (true) {
    if (g_ota_active) {
      vTaskSuspend(NULL);  // sicherer Suspend-Punkt: kein Mutex gehalten
      continue;
    }
    if (WiFi.status() == WL_CONNECTED) {
      if (!mqttClient.connected()) {
        mqtt_connected = false;
        mqttReconnect();
      } else {
        mqtt_connected = true;
        mqttClient.loop();
      }
    } else {
      mqtt_connected = false;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ─── LVGL-Task auf Core 1 ────────────────────────────────
void lvgl_task(void* param) {
  unsigned long last_flush = 0;
  while (true) {
    // OTA gestartet: lvgl_task suspendieren – Display friert ein, kein DMA-Konflikt
    if (g_ota_show_req) {
      g_ota_show_req = false;
      vTaskSuspend(NULL);   // kein lv_timer_handler mehr bis Resume
      // nach vTaskResume() (bei OTA-Fehler) geht es hier weiter
    }

    // LVGL intern verarbeiten (Animationen, Timers, Rendering)
    LVGL_LOCK();
    lv_timer_handler();
    LVGL_UNLOCK();

    // Nach OTA-Fehler: nach 3s zurück zum Hauptscreen
    if (g_ota_error_time > 0 && millis() - g_ota_error_time > 3000) {
      g_ota_error_time = 0;
      g_ota_active     = false;
      if (g_mqtt_task_handle) vTaskResume(g_mqtt_task_handle);
      LVGL_LOCK();
      ui_build();
      LVGL_UNLOCK();
    }

    // UI-Updates pausieren während OTA
    if (!g_ota_active && millis() - last_flush >= 1000) {
      last_flush = millis();
      LVGL_LOCK();
      getLocalTime(&tm_info);
      ui_update_header();
      for (int i = 0; i < entity_count; i++) {
        if (entities[i].dirty) {
          ui_update_entity(i);
          entities[i].dirty = false;
        }
      }
      LVGL_UNLOCK();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ─────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Home Assistant Panel ===");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  touch_init();
  display_init();
  sd_init();
  loadconfig();

  // Boot-Screen
  ui_boot_show();

  // WiFi verbinden
  WiFi.setHostname(CL_hostname.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.begin(CL_wifissid.c_str(), CL_wifipassword.c_str());
  Serial.print("WiFi verbinden mit: ");
  Serial.println(CL_wifissid);
  ui_boot_status(("WiFi: " + CL_wifissid + " ...").c_str());
  for (int i = 0; i < 120 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    lv_timer_handler();
  }
  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    Serial.println("WiFi: " + ip);
    ui_boot_status(("WiFi: " + ip).c_str());
    configTzTime(timezone.c_str(), ntpServer.c_str());
    delay(800);
    getLocalTime(&tm_info);
  } else {
    Serial.println("WiFi: Verbindung fehlgeschlagen");
    ui_boot_status("WiFi: Verbindung fehlgeschlagen");
  }

  // OTA + HTTP SD-Upload (nur wenn WiFi verbunden)
  if (WiFi.status() == WL_CONNECTED) {
    ota_setup();
    String ota_msg = "OTA bereit - http://" + CL_hostname + ".local";
    ui_boot_status(ota_msg.c_str());
    lv_timer_handler();
    delay(600);
  }

  // Haupt-UI aufbauen (ersetzt Boot-Screen) + Mutex VOR MQTT
  // (mqttCallback ruft LVGL_LOCK() auf, kommt bei retained Messages während subscribe())
  ui_build();
  lv_timer_handler();
  g_lvgl_mutex = xSemaphoreCreateMutex();

  // MQTT initialisieren
  mqttClient.setServer(HASS_SERVER.c_str(), HASS_SERVERPORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(30);
  mqttClient.setBufferSize(1024);
  mqttReconnect();
  // Mutex + Tasks starten
  // LVGL auf Core 1 (Priorität 2)
  xTaskCreatePinnedToCore(lvgl_task, "LVGL", 8192, NULL, 2, &g_lvgl_task_handle, 1);
  // WiFi/MQTT auf Core 0 (Priorität 1)
  xTaskCreatePinnedToCore(mqtt_task, "MQTT", 4096, NULL, 1, NULL, 0);
  // OTA/HTTP auf Core 0 (Priorität 1)
  xTaskCreatePinnedToCore(ota_task,  "OTA",  8192, NULL, 1, NULL, 0);
  Serial.println("LVGL-Task: Core 1 | MQTT-Task: Core 0 | OTA-Task: Core 0");
}

// ─────────────────────────────────────────────────────────────
// LOOP – leer, beide Tasks laufen als FreeRTOS-Tasks
// ─────────────────────────────────────────────────────────────
void loop() {
  vTaskDelay(portMAX_DELAY);
}
