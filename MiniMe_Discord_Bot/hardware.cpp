#include "minime.h"
#include <driver/ledc.h>

OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);
Adafruit_NeoPixel pixels(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

void setupServo() {
  ledc_timer_config_t timer = {
    .speed_mode       = LEDC_LOW_SPEED_MODE,
    .duty_resolution  = LEDC_TIMER_14_BIT,
    .timer_num        = LEDC_TIMER_0,
    .freq_hz          = 50,
    .clk_cfg          = LEDC_AUTO_CLK
  };
  ledc_timer_config(&timer);
  ledc_channel_config_t channel = {
    .gpio_num         = PIN_SERVO,
    .speed_mode       = LEDC_LOW_SPEED_MODE,
    .channel          = LEDC_CHANNEL_0,
    .intr_type        = LEDC_INTR_DISABLE,
    .timer_sel        = LEDC_TIMER_0,
    .duty             = 0,
    .hpoint           = 0
  };
  ledc_channel_config(&channel);
}

void setServoAngle(int angleDeg) {
  if (angleDeg < 0) angleDeg = 0;
  if (angleDeg > 90) angleDeg = 90;
  lastServoDeg = angleDeg;
  int pulseUs = 500 + (1500 * angleDeg / 90);
  uint32_t max_duty = (1 << 14) - 1;
  uint32_t duty = (pulseUs * max_duty) / 20000;
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void setupPins() {
  pinMode(RGB_LED_PIN, OUTPUT);
  digitalWrite(RGB_LED_PIN, LOW);
  pixels.begin();
  pixels.show();
  pinMode(PIN_SET1, OUTPUT);
  pinMode(PIN_SET2, OUTPUT);
  digitalWrite(PIN_SET1, LOW);
  digitalWrite(PIN_SET2, LOW);
  pinMode(PIN_DS18B20, INPUT_PULLUP);
  setupServo();
  setServoAngle(45);
}

bool readTemperature(float& tempC, float& tempF) {
  pinMode(PIN_DS18B20, INPUT_PULLUP);
  sensors.requestTemperatures();
  float c = sensors.getTempCByIndex(0);
  if (c == DEVICE_DISCONNECTED_C) {
    return false;
  }
  tempC = c;
  tempF = c * 9.0f / 5.0f + 32.0f;
  return true;
}

bool isLedByteToken(const String& s) {
  if (s.length() == 0 || s.length() > 3) return false;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (c < '0' || c > '9') return false;
  }
  int v = s.toInt();
  return v >= 0 && v <= 255;
}

void setLedRgb(uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}

bool parseRgbTriplet(const String& args, uint8_t& r, uint8_t& g, uint8_t& b) {
  String a = args;
  a.trim();
  int sp1 = a.indexOf(' ');
  int sp2 = (sp1 >= 0) ? a.indexOf(' ', sp1 + 1) : -1;
  if (sp1 <= 0 || sp2 <= sp1) return false;
  String rs = a.substring(0, sp1);
  String gs = a.substring(sp1 + 1, sp2);
  String bs = a.substring(sp2 + 1);
  bs.trim();
  int sp3 = bs.indexOf(' ');
  if (sp3 >= 0) bs = bs.substring(0, sp3);
  if (!isLedByteToken(rs) || !isLedByteToken(gs) || !isLedByteToken(bs)) return false;
  r = (uint8_t)rs.toInt();
  g = (uint8_t)gs.toInt();
  b = (uint8_t)bs.toInt();
  return true;
}

bool isOwner(const String& authorId) {
  return authorId == OWNER_ID_STR;
}
