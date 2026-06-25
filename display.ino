// ─── Display + LVGL Driver-Initialisierung ───────────────────

static lv_disp_draw_buf_t lvgl_draw_buf;

// LVGL Flush-Callback: überträgt LVGL-Buffer → Display
static void lvgl_flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)color_p, w, h);
  lv_disp_flush_ready(disp);
}

// LVGL Touch-Callback: liest GT911-Koordinaten
static void lvgl_touch_cb(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
  if (touch_available) {
    ts.read();
    if (ts.isTouched) {
      data->state   = LV_INDEV_STATE_PR;
      data->point.x = map(ts.points[0].x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, screenWidth  - 1);
      data->point.y = map(ts.points[0].y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, screenHeight - 1);
    } else {
      data->state = LV_INDEV_STATE_REL;
    }
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void display_init() {
  gfx->begin();
  gfx->fillScreen(0x0000);

  // LVGL initialisieren
  lv_init();

  // LVGL-Draw-Buffer in Internal-SRAM (kein Cache-Coherency-Problem wie bei PSRAM)
  // 20 Zeilen à 800 px × 2 Byte = 32 KB – passt in den freien Internal-SRAM des ESP32-S3
  const size_t buf_lines = 20;
  static lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(
    screenWidth * buf_lines * sizeof(lv_color_t),
    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (!buf1) {
    Serial.println("Internal SRAM zu klein – Fallback auf PSRAM");
    buf1 = (lv_color_t*)ps_malloc(screenWidth * buf_lines * sizeof(lv_color_t));
  }
  lv_color_t* buf2 = nullptr;  // Single-Buffer reicht für unseren Update-Rhythmus

  Serial.printf("LVGL Draw-Buffer: %s, %u Zeilen\n",
    heap_caps_get_allocated_size(buf1) ? "Internal" : "PSRAM", buf_lines);

  lv_disp_draw_buf_init(&lvgl_draw_buf, buf1, buf2, screenWidth * buf_lines);

  // Display-Treiber registrieren
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res      = screenWidth;
  disp_drv.ver_res      = screenHeight;
  disp_drv.flush_cb     = lvgl_flush_cb;
  disp_drv.draw_buf     = &lvgl_draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Touch-Eingabetreiber registrieren
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lvgl_touch_cb;
  lv_indev_drv_register(&indev_drv);

  Serial.println("Display: LVGL initialisiert");
}

void sd_init() {
  if (!SD.begin()) {
    Serial.println("SD: Karte nicht gefunden oder Fehler.");
    return;
  }
  Serial.println("SD: OK");
}
