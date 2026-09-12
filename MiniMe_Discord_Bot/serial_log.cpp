#include "minime.h"

#if CONFIG_IDF_TARGET_ESP32S3
#include "USB.h"
#endif

static bool mmUsbCdcExtra = false;

size_t MmLogClass::write(uint8_t c) {
  return write(&c, 1);
}

size_t MmLogClass::write(const uint8_t* buffer, size_t size) {
  if (!buffer || size == 0) return 0;
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
  // Tools: CDC On Boot = Enabled -> Serial is USB; also mirror to UART0
  Serial.write(buffer, size);
  Serial0.write(buffer, size);
#else
  // Tools: CDC Off -> Serial is UART0; also drive native USB CDC so USB-C Monitor works
  Serial.write(buffer, size);
#if CONFIG_IDF_TARGET_ESP32S3
  if (mmUsbCdcExtra) USBSerial.write(buffer, size);
#endif
#endif
  return size;
}

void MmLogClass::flushAll() {
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
  Serial.flush();
  Serial0.flush();
#else
  Serial.flush();
#if CONFIG_IDF_TARGET_ESP32S3
  if (mmUsbCdcExtra) USBSerial.flush();
#endif
#endif
}

MmLogClass MmLog;

void mmSerialBegin() {
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
  // Serial is USBCDC here; setTxTimeoutMs is USB-only (not on HardwareSerial/UART)
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  Serial0.begin(115200);
#else
  // Serial is HardwareSerial (UART) — no setTxTimeoutMs on this class
  Serial.begin(115200);
#if CONFIG_IDF_TARGET_ESP32S3
  USB.begin();
  USBSerial.begin(115200);
  USBSerial.setTxTimeoutMs(0);
  mmUsbCdcExtra = true;
#endif
#endif

  // USB re-enumerates after reset; Monitor often opens late
  delay(2500);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 2000UL) delay(10);

  for (uint8_t i = 0; i < 8; i++) {
    MmLog.println("[GW] MiniMe Serial OK @115200");
    MmLog.flushAll();
    delay(80);
  }
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
  MmLog.println("[GW] build flag: USB CDC On Boot = ENABLED");
#else
  MmLog.println("[GW] build flag: USB CDC On Boot = DISABLED (UART + USBSerial)");
#endif
  MmLog.println("[GW] gateway drop log armed (5s remind / 60s full dump)");
  MmLog.flushAll();
}

bool mmSerialCdcOnBoot() {
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
  return true;
#else
  return false;
#endif
}
