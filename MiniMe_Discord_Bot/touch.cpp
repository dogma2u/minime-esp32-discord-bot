#include "minime.h"

unsigned long lastTouchWakeMillis = 0;
bool touchWasActive = false;
uint32_t touchIdleBuf[TOUCH_AVG_N];
uint8_t touchIdleBufCount = 0;
uint8_t touchIdleBufIdx = 0;
uint32_t touchIdleSum = 0;
uint32_t touchIdleAvg = 0;
uint32_t usbVbusRefMv = 0;
uint32_t usbVbusMvCached = 0;
uint32_t usbVbusCompLastMv = 0;
unsigned long usbVbusLastReadMs = 0;

bool touchSampleValid(uint32_t raw) {
  return raw > 0 && raw < 150000;
}

uint32_t touchTripPoint() {
  return touchIdleAvg + TOUCH_THRESHOLD;
}

uint32_t readUsbVbusMilliVolts() {
  unsigned long now = millis();
  if (usbVbusLastReadMs != 0 && (now - usbVbusLastReadMs) < USB_VBUS_READ_MS) {
    return usbVbusMvCached;
  }
  usbVbusLastReadMs = now;
  uint32_t pinMv = analogReadMilliVolts((uint8_t)PIN_USB_VBUS_ADC);
  uint32_t inst = (uint32_t)((uint64_t)pinMv * (USB_VBUS_R_HI + USB_VBUS_R_LO) / USB_VBUS_R_LO);
  if (usbVbusMvCached == 0) usbVbusMvCached = inst;
  else usbVbusMvCached = (usbVbusMvCached * 7u + inst) / 8u;
  return usbVbusMvCached;
}

// Compensated idle samples are raw * ref / V. When V steps, rescale the window so trip stays put.
void rescaleTouchIdleForVbus(uint32_t oldV, uint32_t newV) {
  if (oldV == 0 || newV == 0 || touchIdleBufCount == 0) return;
  touchIdleSum = 0;
  for (uint8_t i = 0; i < touchIdleBufCount; i++) {
    touchIdleBuf[i] = (uint32_t)((uint64_t)touchIdleBuf[i] * oldV / newV);
    touchIdleSum += touchIdleBuf[i];
  }
  touchIdleAvg = touchIdleSum / touchIdleBufCount;
}

// Scale touch raw to boot-time USB voltage so VBUS sag/swell does not walk the gap.
uint32_t compensateTouchRaw(uint32_t raw) {
  uint32_t v = readUsbVbusMilliVolts();
  if (usbVbusRefMv < 1000 || v < 1000) return raw;
  if (usbVbusCompLastMv >= 1000 && usbVbusCompLastMv != v) {
    rescaleTouchIdleForVbus(usbVbusCompLastMv, v);
  }
  usbVbusCompLastMv = v;
  return (uint32_t)((uint64_t)raw * usbVbusRefMv / v);
}

void setupUsbVbusAdc() {
  analogSetPinAttenuation(PIN_USB_VBUS_ADC, ADC_11db);
  pinMode(PIN_USB_VBUS_ADC, INPUT);
  uint32_t acc = 0;
  for (int i = 0; i < 8; i++) {
    usbVbusLastReadMs = 0;
    acc += readUsbVbusMilliVolts();
    delay(10);
  }
  usbVbusRefMv = acc / 8;
  usbVbusCompLastMv = usbVbusMvCached;
}

void touchIdlePush(uint32_t sample) {
  if (touchIdleBufCount < TOUCH_AVG_N) {
    touchIdleBuf[touchIdleBufCount++] = sample;
    touchIdleSum += sample;
  } else {
    touchIdleSum -= touchIdleBuf[touchIdleBufIdx];
    touchIdleBuf[touchIdleBufIdx] = sample;
    touchIdleSum += sample;
    touchIdleBufIdx = (uint8_t)((touchIdleBufIdx + 1) % TOUCH_AVG_N);
  }
  if (touchIdleBufCount > 0) touchIdleAvg = touchIdleSum / touchIdleBufCount;
}

void calibrateTouchIdleAvg() {
  touchIdleBufCount = 0;
  touchIdleBufIdx = 0;
  touchIdleSum = 0;
  touchIdleAvg = 0;
  for (int i = 0; i < TOUCH_AVG_N; i++) {
    uint32_t raw = (uint32_t)touchRead(PIN_TOUCH);
    if (touchSampleValid(raw)) touchIdlePush(compensateTouchRaw(raw));
    delay(25);
  }
  if (touchIdleBufCount == 0) {
    uint32_t raw = (uint32_t)touchRead(PIN_TOUCH);
    touchIdlePush(compensateTouchRaw(raw));
  }
}

void updateTouchIdleAvg(uint32_t compensated) {
  if (!touchSampleValid(compensated)) return;
  if (compensated >= touchTripPoint()) return;
  touchIdlePush(compensated);
}

void configureTouchHardware() {
  // Arduino-ESP32 3.x: touchSetTiming. Older 2.x: touchSetCycles.
#if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
  touchSetTiming(0.5f, 100);
#else
  touchSetCycles(1, 100);
#endif
}

void setupTouch() {
  setupUsbVbusAdc();
  configureTouchHardware();
  calibrateTouchIdleAvg();
}

bool touchIsActive() {
  uint32_t raw = (uint32_t)touchRead(PIN_TOUCH);
  uint32_t compensated = compensateTouchRaw(raw);
  if (compensated >= touchTripPoint()) return true;
  updateTouchIdleAvg(compensated);
  return false;
}

// Rising edge only -- stuck trip must not keep resetting the idle timer.
void pollTouchWake() {
  bool active = touchIsActive();
  if (!active) {
    touchWasActive = false;
    return;
  }
  unsigned long now = millis();
  if (now - lastTouchWakeMillis < TOUCH_DEBOUNCE_MS) return;
  if (touchWasActive) return;
  touchWasActive = true;
  lastTouchWakeMillis = now;
  noteDisplayActivity();
}
