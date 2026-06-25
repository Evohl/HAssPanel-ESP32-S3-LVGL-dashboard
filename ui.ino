// ============================================================
//  LVGL UI – Home Assistant Panel
//
//  Layout (800×480):
//  ┌──────────────────── HEADER (h=50) ─────────────────────┐
//  │                                                         │
//  │   Entity-Tiles (dynamisch, 1–8 Entities)               │
//  │   1–2:  1×1 oder 2×1   (volle Breite)                  │
//  │   3–4:  2×2                                             │
//  │   5–8:  4×2                                             │
//  │                                                         │
//  └─────────────────────────────────────────────────────────┘
// ============================================================

// ── Farben (LVGL hex) ─────────────────────────────────────────
#define COL_BG       0x0D1117
#define COL_CARD     0x161B22
#define COL_HEADER   0x0D1117
#define COL_BORDER   0x30363D
#define COL_TEXT     0xF0F6FC
#define COL_SUBTEXT  0x8B949E
#define COL_OK       0x3FB950
#define COL_WARN     0xD29922
#define COL_DANGER   0xF85149
#define COL_ACCENT   0x58A6FF

#define HEADER_H  58
#define TILE_GAP   4

// ── Stile ─────────────────────────────────────────────────────
static lv_style_t st_screen;
static lv_style_t st_header;
static lv_style_t st_card;

static bool updating_from_mqtt = false;

// ── Schalter-Callback (Tile-Switch) ───────────────────────────
static void switch_event_cb(lv_event_t* e) {
  if (updating_from_mqtt) return;
  HAEntity* ent = (HAEntity*)lv_event_get_user_data(e);
  if (!ent) return;
  bool state = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  if (mqtt_connected && ent->cmd_topic.length() > 0) {
    mqttClient.publish(ent->cmd_topic.c_str(), state ? "ON" : "OFF");
    Serial.println("Switch publish → " + ent->cmd_topic + " = " + (state ? "ON" : "OFF"));
  }
}

// ── Sub-Schalter-Callback (Toggle in Group-Kachel) ────────────
// user_data: entity_index * MAX_SUBS + sub_index (als uintptr_t)
static void sub_switch_event_cb(lv_event_t* e) {
  if (updating_from_mqtt) return;
  uintptr_t ctx = (uintptr_t)lv_event_get_user_data(e);
  int ei = (int)(ctx / MAX_SUBS);
  int si = (int)(ctx % MAX_SUBS);
  if (ei >= entity_count || si >= entities[ei].sub_count) return;
  if (entities[ei].sub_cmd_topic[si].length() == 0) return;
  bool state = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  if (mqtt_connected) {
    mqttClient.publish(entities[ei].sub_cmd_topic[si].c_str(), state ? "ON" : "OFF");
    Serial.println("SubSwitch → " + entities[ei].sub_cmd_topic[si] + " = " + (state ? "ON" : "OFF"));
  }
}

// ── Stile initialisieren ──────────────────────────────────────
static void init_styles() {
  lv_style_init(&st_screen);
  lv_style_set_bg_color(&st_screen, lv_color_hex(COL_BG));
  lv_style_set_bg_opa(&st_screen, LV_OPA_COVER);

  lv_style_init(&st_header);
  lv_style_set_bg_color(&st_header, lv_color_hex(COL_HEADER));
  lv_style_set_bg_opa(&st_header, LV_OPA_COVER);
  lv_style_set_border_width(&st_header, 0);
  lv_style_set_border_side(&st_header, LV_BORDER_SIDE_BOTTOM);
  lv_style_set_border_color(&st_header, lv_color_hex(COL_BORDER));
  lv_style_set_border_width(&st_header, 1);
  lv_style_set_pad_hor(&st_header, 16);
  lv_style_set_pad_ver(&st_header, 0);
  lv_style_set_radius(&st_header, 0);

  lv_style_init(&st_card);
  lv_style_set_bg_color(&st_card, lv_color_hex(COL_CARD));
  lv_style_set_bg_opa(&st_card, LV_OPA_COVER);
  lv_style_set_border_width(&st_card, 1);
  lv_style_set_border_color(&st_card, lv_color_hex(COL_BORDER));
  lv_style_set_radius(&st_card, 10);
  lv_style_set_pad_all(&st_card, 14);
}

// ── Header aufbauen ───────────────────────────────────────────
static void build_header() {
  lv_obj_t* header = lv_obj_create(lv_scr_act());
  lv_obj_set_size(header, screenWidth, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_add_style(header, &st_header, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  // App-Titel
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "HOME ASSISTANT PANEL");
  lv_obj_set_style_text_color(title, lv_color_hex(COL_ACCENT), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

  // Uhrzeit
  lbl_time = lv_label_create(header);
  lv_label_set_text(lbl_time, "--:--");
  lv_obj_set_style_text_color(lbl_time, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_20, 0);
  lv_obj_align(lbl_time, LV_ALIGN_RIGHT_MID, -82, 0);

  // WiFi-Icon
  lbl_wifi = lv_label_create(header);
  lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_color(lbl_wifi, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_20, 0);
  lv_obj_align(lbl_wifi, LV_ALIGN_RIGHT_MID, -46, 0);

  // MQTT-Icon
  lbl_mqtt = lv_label_create(header);
  lv_label_set_text(lbl_mqtt, LV_SYMBOL_REFRESH);
  lv_obj_set_style_text_color(lbl_mqtt, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_text_font(lbl_mqtt, &lv_font_montserrat_20, 0);
  lv_obj_align(lbl_mqtt, LV_ALIGN_RIGHT_MID, -12, 0);
}

// ── Sensor-Tile aufbauen ──────────────────────────────────────
static void build_sensor_tile(int i, lv_obj_t* tile, int tw, int th) {
  lv_color_t ecol = lv_color_hex(entities[i].color);
  bool large = (th >= 200);
  const lv_font_t* font_title = large ? &lv_font_montserrat_20 : &lv_font_montserrat_16;

  // Entity-Name oben links
  lv_obj_t* lbl_name = lv_label_create(tile);
  lv_label_set_text(lbl_name, entities[i].label.c_str());
  lv_obj_set_style_text_color(lbl_name, lv_color_hex(g_title_color), 0);
  lv_obj_set_style_text_font(lbl_name, font_title, 0);
  lv_obj_align(lbl_name, LV_ALIGN_TOP_LEFT, 0, 0);

  // Hauptwert – zentriert, groß (Wert + Einheit in einem Label)
  lv_obj_t* lbl_val = lv_label_create(tile);
  String placeholder = entities[i].unit.length() > 0 ? ("-- " + entities[i].unit) : "--";
  lv_label_set_text(lbl_val, placeholder.c_str());
  lv_obj_set_style_text_color(lbl_val, lv_color_hex(COL_SUBTEXT), 0);
  lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_align(lbl_val, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(lbl_val, LV_PCT(100));
  lv_obj_align(lbl_val, LV_ALIGN_CENTER, 0, 0);
  entities[i].value_label = lbl_val;
}

// ── Switch-Tile aufbauen ──────────────────────────────────────
static void build_switch_tile(int i, lv_obj_t* tile, int tw, int th) {
  lv_color_t ecol = lv_color_hex(entities[i].color);
  bool large = (th >= 200);
  const lv_font_t* font_title = large ? &lv_font_montserrat_20 : &lv_font_montserrat_16;

  // Entity-Name oben links
  lv_obj_t* lbl_name = lv_label_create(tile);
  lv_label_set_text(lbl_name, entities[i].label.c_str());
  lv_obj_set_style_text_color(lbl_name, lv_color_hex(g_title_color), 0);
  lv_obj_set_style_text_font(lbl_name, font_title, 0);
  lv_obj_align(lbl_name, LV_ALIGN_TOP_LEFT, 0, 0);

  // Toggle-Switch – zentriert
  lv_obj_t* sw = lv_switch_create(tile);
  lv_obj_set_style_bg_color(sw, lv_color_hex(COL_OK), LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_align(sw, LV_ALIGN_CENTER, 0, -8);
  lv_obj_add_event_cb(sw, switch_event_cb, LV_EVENT_VALUE_CHANGED, &entities[i]);
  entities[i].sw = sw;

  // Status-Text – unten mittig
  lv_obj_t* lbl_state = lv_label_create(tile);
  lv_label_set_text(lbl_state, "-");
  lv_obj_set_style_text_color(lbl_state, lv_color_hex(COL_SUBTEXT), 0);
  lv_obj_set_style_text_font(lbl_state, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl_state, LV_ALIGN_BOTTOM_MID, 0, 0);
  entities[i].value_label = lbl_state;
}

// ── Group-Tile aufbauen (Hauptwert + Sub-Sensor-Zeilen) ───────
static void build_group_tile(int i, lv_obj_t* tile, int tw, int th) {
  HAEntity& e = entities[i];
  lv_color_t ecol = lv_color_hex(e.color);
  bool large = (th >= 200);
  const lv_font_t* font_title = large ? &lv_font_montserrat_20 : &lv_font_montserrat_16;
  const lv_font_t* font_main  = large ? &lv_font_montserrat_28 : &lv_font_montserrat_20;
  const lv_font_t* font_sub   = large ? &lv_font_montserrat_16 : &lv_font_montserrat_14;
  const int div_y   = large ? 36 : 30;
  const int SUB_Y0  = large ? 46 : 38;
  const int SUB_STEP= large ? 30 : 26;

  bool has_main = e.state_topic.length() > 0;

  // Titel oben links
  lv_obj_t* lbl_name = lv_label_create(tile);
  lv_label_set_text(lbl_name, e.label.c_str());
  lv_obj_set_style_text_color(lbl_name, lv_color_hex(g_title_color), 0);
  lv_obj_set_style_text_font(lbl_name, font_title, 0);
  lv_obj_set_pos(lbl_name, 0, 2);

  if (has_main) {
    // Hauptwert rechtsbündig (gleiche Zeile wie Titel)
    lv_obj_t* lbl_val = lv_label_create(tile);
    lv_label_set_text(lbl_val, e.unit.length() > 0 ? ("-- " + e.unit).c_str() : "--");
    lv_obj_set_style_text_color(lbl_val, lv_color_hex(COL_SUBTEXT), 0);
    lv_obj_set_style_text_font(lbl_val, font_main, 0);
    lv_obj_set_width(lbl_val, LV_PCT(100));
    lv_obj_set_style_text_align(lbl_val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_val, 0, 0);
    entities[i].value_label = lbl_val;

    // Trennlinie
    lv_obj_t* div = lv_obj_create(tile);
    lv_obj_remove_style_all(div);
    lv_obj_set_size(div, LV_PCT(100), 1);
    lv_obj_set_pos(div, 0, div_y);
    lv_obj_set_style_bg_color(div, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
  } else {
    // Kein Hauptwert → kein Label, kein Divider
    entities[i].value_label = nullptr;
  }

  // Sub-Sensor-Zeilen: bei fehlendem Hauptwert gleichmäßig verteilt
  int sub_y0   = has_main ? SUB_Y0 : (large ? 30 : 26);
  int sub_step = has_main ? SUB_STEP
                          : (e.sub_count > 0 ? (th - sub_y0 - 4) / e.sub_count : SUB_STEP);

  for (int s = 0; s < e.sub_count && s < MAX_SUBS; s++) {
    int y = sub_y0 + s * sub_step;

    lv_obj_t* lbl_sub = lv_label_create(tile);
    lv_label_set_text(lbl_sub, e.sub_label[s].c_str());
    lv_obj_set_style_text_color(lbl_sub, lv_color_hex(COL_SUBTEXT), 0);
    lv_obj_set_style_text_font(lbl_sub, font_sub, 0);
    lv_obj_set_pos(lbl_sub, 0, y);

    if (e.sub_cmd_topic[s].length() > 0) {
      // Toggle-Switch für schaltbare Subs
      lv_obj_t* sw = lv_switch_create(tile);
      lv_obj_set_size(sw, 44, 22);
      lv_obj_set_style_bg_color(sw, lv_color_hex(COL_OK), LV_PART_INDICATOR | LV_STATE_CHECKED);
      lv_obj_set_style_bg_color(sw, lv_color_hex(COL_SUBTEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_width(sw, 44);
      // Rechtsbündig ausrichten
      lv_obj_set_pos(sw, tw - 14 - 44, y + (sub_step - 22) / 2 - 2);
      uintptr_t ctx = (uintptr_t)(i * MAX_SUBS + s);
      lv_obj_add_event_cb(sw, sub_switch_event_cb, LV_EVENT_VALUE_CHANGED, (void*)ctx);
      e.sub_sw[s]      = sw;
      e.sub_val_lbl[s] = nullptr;
    } else {
      // Wert-Label für read-only Subs
      lv_obj_t* lbl_sv = lv_label_create(tile);
      lv_label_set_text(lbl_sv, "--");
      lv_obj_set_style_text_color(lbl_sv, ecol, 0);
      lv_obj_set_style_text_font(lbl_sv, font_sub, 0);
      lv_obj_set_width(lbl_sv, LV_PCT(100));
      lv_obj_set_style_text_align(lbl_sv, LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_pos(lbl_sv, 0, y);
      e.sub_val_lbl[s] = lbl_sv;
      e.sub_sw[s]      = nullptr;
    }
  }
}

// ── Alle Entity-Tiles aufbauen ────────────────────────────────
static void build_tiles() {
  if (entity_count == 0) {
    lv_obj_t* lbl = lv_label_create(lv_scr_act());
    lv_label_set_text(lbl,
      "Keine Entities konfiguriert.\n"
      "Bitte config.txt auf SD-Karte legen.\n\n"
      "Beispiel: entity1_type=sensor\n"
      "          entity1_label=Solar\n"
      "          entity1_topic=homeassistant/sensor/xxx/state");
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_SUBTEXT), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl, screenWidth - 40);
    lv_obj_center(lbl);
    return;
  }

  int body_y = HEADER_H + TILE_GAP;
  int body_h = screenHeight - body_y - TILE_GAP;

  // ── Benutzerdefiniertes Layout (layout=2,4 etc.) ──────────────
  if (g_layout_rows > 0) {
    int row_h = (body_h - TILE_GAP * (g_layout_rows - 1)) / g_layout_rows;
    int idx   = 0;
    for (int r = 0; r < g_layout_rows && idx < entity_count; r++) {
      int n  = g_layout_row_count[r];
      if (n <= 0) continue;
      int tw = (screenWidth - TILE_GAP * (n + 1)) / n;
      int th = row_h;
      int y  = body_y + r * (row_h + TILE_GAP);
      for (int c = 0; c < n && idx < entity_count; c++, idx++) {
        int x = TILE_GAP + c * (tw + TILE_GAP);
        lv_obj_t* tile = lv_obj_create(lv_scr_act());
        lv_obj_set_size(tile, tw, th);
        lv_obj_set_pos(tile, x, y);
        lv_obj_add_style(tile, &st_card, 0);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        entities[idx].tile = tile;
        if      (entities[idx].type == ENTITY_SENSOR) build_sensor_tile(idx, tile, tw, th);
        else if (entities[idx].type == ENTITY_GROUP)  build_group_tile (idx, tile, tw, th);
        else                                           build_switch_tile(idx, tile, tw, th);
      }
    }
    return;
  }

  // ── Automatisches Layout ──────────────────────────────────────
  // Ungerade Anzahl >= 3: geteiltes Layout – oben weniger/größer, unten mehr/kleiner
  bool split = (entity_count >= 3) && (entity_count % 2 == 1);

  if (split) {
    int top_n = entity_count / 2;           // floor: 5→2, 7→3, 3→1
    int bot_n = entity_count - top_n;       // ceil:  5→3, 7→4, 3→2

    // Höhe: oben 60%, unten 40% (Verhältnis 1.5:1)
    int bot_h = (body_h - TILE_GAP) * 2 / 5;
    int top_h = body_h - TILE_GAP - bot_h;

    int top_w = (screenWidth - TILE_GAP * (top_n + 1)) / top_n;
    int bot_w = (screenWidth - TILE_GAP * (bot_n + 1)) / bot_n;

    for (int i = 0; i < entity_count; i++) {
      int tw, th, x, y;
      if (i < top_n) {
        tw = top_w; th = top_h;
        x  = TILE_GAP + i * (tw + TILE_GAP);
        y  = body_y;
      } else {
        int j = i - top_n;
        tw = bot_w; th = bot_h;
        x  = TILE_GAP + j * (tw + TILE_GAP);
        y  = body_y + top_h + TILE_GAP;
      }
      lv_obj_t* tile = lv_obj_create(lv_scr_act());
      lv_obj_set_size(tile, tw, th);
      lv_obj_set_pos(tile, x, y);
      lv_obj_add_style(tile, &st_card, 0);
      lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
      entities[i].tile = tile;
      if      (entities[i].type == ENTITY_SENSOR) build_sensor_tile(i, tile, tw, th);
      else if (entities[i].type == ENTITY_GROUP)  build_group_tile (i, tile, tw, th);
      else                                         build_switch_tile(i, tile, tw, th);
    }

  } else {
    // Gleichmäßiges Grid
    int cols = (entity_count <= 2) ? entity_count : (entity_count <= 4 ? 2 : 4);
    int rows = (entity_count + cols - 1) / cols;
    int tile_w = (screenWidth - TILE_GAP * (cols + 1)) / cols;
    int tile_h = (body_h - TILE_GAP * (rows - 1)) / rows;

    for (int i = 0; i < entity_count; i++) {
      int col = i % cols;
      int row = i / cols;
      int x   = TILE_GAP + col * (tile_w + TILE_GAP);
      int y   = body_y   + row * (tile_h + TILE_GAP);

      lv_obj_t* tile = lv_obj_create(lv_scr_act());
      lv_obj_set_size(tile, tile_w, tile_h);
      lv_obj_set_pos(tile, x, y);
      lv_obj_add_style(tile, &st_card, 0);
      lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
      entities[i].tile = tile;
      if      (entities[i].type == ENTITY_SENSOR) build_sensor_tile(i, tile, tile_w, tile_h);
      else if (entities[i].type == ENTITY_GROUP)  build_group_tile (i, tile, tile_w, tile_h);
      else                                         build_switch_tile(i, tile, tile_w, tile_h);
    }
  }
}

// ── Öffentliche Funktionen ────────────────────────────────────

void ui_build() {
  init_styles();
  lv_obj_add_style(lv_scr_act(), &st_screen, 0);
  build_header();
  build_tiles();
}

void ui_update_entity(int i) {
  if (i < 0 || i >= entity_count || !entities[i].tile) return;
  HAEntity& e = entities[i];

  // Aktuelle Farbe anhand Schwellwerte ermitteln
  auto entity_color = [&]() -> uint32_t {
    if (e.thresh_count > 0 && e.valid) {
      for (int t = 0; t < e.thresh_count && t < MAX_THRESHOLDS; t++) {
        if (e.sensor_value < e.thresh_val[t]) return e.thresh_color[t];
      }
    }
    return e.color;
  };

  updating_from_mqtt = true;

  if (e.type == ENTITY_SENSOR && e.value_label) {
    String s;
    if (e.valid) {
      s = String(e.sensor_value, (e.sensor_value == (int)e.sensor_value) ? 0 : 1);
      if (e.unit.length() > 0) s += " " + e.unit;
    } else {
      s = e.unit.length() > 0 ? "-- " + e.unit : "--";
    }
    lv_label_set_text(e.value_label, s.c_str());
    lv_obj_set_style_text_color(e.value_label,
      e.valid ? lv_color_hex(entity_color()) : lv_color_hex(COL_SUBTEXT), 0);

  } else if (e.type == ENTITY_GROUP) {
    // Hauptwert
    if (e.value_label) {
      String s = e.valid
        ? String((int)e.sensor_value) + " " + e.unit
        : (e.unit.length() > 0 ? "-- " + e.unit : "--");
      lv_label_set_text(e.value_label, s.c_str());
      lv_obj_set_style_text_color(e.value_label,
        e.valid ? lv_color_hex(entity_color()) : lv_color_hex(COL_SUBTEXT), 0);
    }
    // Sub-Werte
    for (int s = 0; s < e.sub_count && s < MAX_SUBS; s++) {
      if (e.sub_sw[s]) {
        // Toggle-Switch: Zustand aus sub_raw ("on"/"off")
        if (e.sub_valid[s]) {
          String r = e.sub_raw[s]; r.toLowerCase();
          bool on = (r == "on" || r == "true" || r == "1");
          if (on) lv_obj_add_state(e.sub_sw[s], LV_STATE_CHECKED);
          else    lv_obj_clear_state(e.sub_sw[s], LV_STATE_CHECKED);
        }
      } else if (e.sub_val_lbl[s]) {
        String sv;
        if (!e.sub_valid[s]) {
          sv = "--";
        } else {
          // Numerisch → Zahl + Einheit; sonst Rohwert (z.B. "on"/"off")
          float f = e.sub_raw[s].toFloat();
          bool is_num = (f != 0.0f) || (e.sub_raw[s] == "0" || e.sub_raw[s] == "0.0");
          if (is_num) {
            float fval = e.sub_value[s];
            sv = (fval == (int)fval) ? String((int)fval) : String(fval, 1);
            if (e.sub_unit[s].length() > 0) sv += " " + e.sub_unit[s];
          } else {
            sv = e.sub_raw[s];
            sv.toUpperCase();
          }
        }
        lv_label_set_text(e.sub_val_lbl[s], sv.c_str());
      }
    }

  } else if (e.type == ENTITY_SWITCH) {
    if (e.sw) {
      if (e.switch_state) lv_obj_add_state(e.sw, LV_STATE_CHECKED);
      else                lv_obj_clear_state(e.sw, LV_STATE_CHECKED);
    }
    if (e.value_label) {
      lv_label_set_text(e.value_label, e.valid ? (e.switch_state ? "AN" : "AUS") : "-");
      lv_obj_set_style_text_color(e.value_label,
        e.switch_state ? lv_color_hex(COL_OK) : lv_color_hex(COL_SUBTEXT), 0);
    }
  }

  updating_from_mqtt = false;
}

void ui_update_header() {
  if (lbl_time) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d",
             tm_info.tm_hour, tm_info.tm_min);
    lv_label_set_text(lbl_time, buf);
  }
  if (lbl_wifi) {
    lv_obj_set_style_text_color(lbl_wifi,
      WiFi.status() == WL_CONNECTED ? lv_color_hex(COL_OK) : lv_color_hex(COL_DANGER), 0);
  }
  if (lbl_mqtt) {
    lv_obj_set_style_text_color(lbl_mqtt,
      mqtt_connected ? lv_color_hex(COL_OK) : lv_color_hex(COL_DANGER), 0);
  }
}
