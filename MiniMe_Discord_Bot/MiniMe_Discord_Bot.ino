/*
 MiniMe Discord bot (ESP32-S3 + SSD1327). Command list: Discord !help.

 Sketch map:
  secrets.h / secrets.example.h  -- Wi-Fi, tokens, IDs (gitignored)
  config.h                       -- pins, buffer sizes, timing constants
  minime.h                         -- shared declarations and globals
  time_util.cpp                    -- NTP, Pacific DST, updateLocalTime
  users.cpp                        -- tracked users, guild cache, presence
  display.cpp                      -- OLED dashboard, sleep, transient overlay
  touch.cpp                        -- touch wake, USB VBUS compensation
  hardware.cpp                     -- servo, NeoPixel, DS18B20, GPIO
  discord_rest.cpp                 -- HTTPS REST, sendDiscordMessage, members
  discord_gateway.cpp              -- websocket, heartbeat, identify, events
  commands.cpp                     -- handleCommand, APIs, DeepSeek, scheduled
  MiniMe_Discord_Bot.ino           -- setup / loop only

 OLED sleep dims then blanks the panel only; ESP32 and Wi-Fi stay up.
 Touch is polled in loop() (no interrupt). Bot status: online on activity,
 idle after 5 min quiet; touch does not set Online.
*/
#include "minime.h"

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  showTransient("WiFi", "Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  showTransient("WiFi", "Connected");
}

void connectGateway() {
  gatewayWS.beginSSL("gateway.discord.gg", 443, "/?v=10&encoding=json");
  gatewayWS.onEvent(gatewayEvent);
  gatewayWS.setReconnectInterval(5000);
}

void setup() {
  delay(500);
  gwDoc = new DynamicJsonDocument(GW_DOC_PSRAM);
  initTrackedUsers();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  u8g2.begin();
  u8g2.setContrast(DISPLAY_CONTRAST_FULL);
  lastDisplayActivityMillis = millis();
  showTransient("Booting...", "ESP32-S3 Discord bot");
  setupPins();
  sensors.begin();
  connectWiFi();
  timeClient.begin();
  showTransient("Discord", "Loading users...");
  if (fetchGuildMembersAtStartup()) {
    String n0 = trackedUsers[0].userName.length() ? trackedUsers[0].userName : "ok";
    showTransient("Users loaded", n0);
  } else {
    showTransient("Users", "Fetch failed");
  }
  delay(1200);
  connectGateway();
  setServoAngle(45);
  lastDashMillis = 0;
  setupTouch(); // after WiFi/I2C
  showTransient("Ready", "Idle presence");
  drawDashboard();
}

void loop() {
  pumpGateway();
  backgroundTasks();
  pollTouchWake();
  updateBotPresenceIdle();
  updateDisplay();
  applyCpuForIdleState();
  delay(5);
}
