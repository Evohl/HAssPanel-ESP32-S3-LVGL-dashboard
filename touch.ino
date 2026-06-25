// ─── Touch GT911 – Auto-Erkennung ─────────────────────────────

void touch_init() {
  Wire.begin(TOUCH_GT911_SDA, TOUCH_GT911_SCL);

  // GT911 auf I2C-Adresse 0x5D oder 0x14 suchen
  Wire.beginTransmission(0x5D);
  bool found = (Wire.endTransmission() == 0);
  if (!found) {
    Wire.beginTransmission(0x14);
    found = (Wire.endTransmission() == 0);
  }

  if (found) {
    ts.begin();
    ts.setRotation(TOUCH_GT911_ROTATION);
    touch_available = true;
    Serial.println("Touch: GT911 erkannt – aktiviert");
  } else {
    Serial.println("Touch: GT911 nicht gefunden – deaktiviert");
  }
}
