#include "minime.h"

// Logging goes to Serial. With USB CDC On Boot enabled (recommended / CI),
// Serial is the USB-C CDC port. Optional mirror to UART0 (Serial0) when CDC is on.

size_t MmLogClass::write(uint8_t c) {
  return write(&c, 1);
}

size_t MmLogClass::write(const uint8_t* buffer, size_t size) {
  if (!buffer || size == 0) return 0;
  Serial.write(buffer, size);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
  Serial0.write(buffer, size);
#endif
  return size;
}

void MmLogClass::flushAll() {
  Serial.flush();
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
  Serial0.flush();
#endif
}

MmLogClass MmLog;

void mmSerialBegin() {
  Serial.begin(115200);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
  Serial.setTxTimeoutMs(0); // USBCDC only
  Serial0.begin(115200);
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
  MmLog.println("[GW] build flag: USB CDC On Boot = DISABLED (enable it for USB-C Serial)");
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
