#ifndef MINIME_H
#define MINIME_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>
#include <string.h>

#include "secrets.h"
#include "config.h"

// ====== USER TRACKING ======
struct TrackedUser {
  String userId, userName;
  uint8_t status;  // 0 Off, 1 Idle, 2 On, 3 DND
  uint32_t useCount24h;
  bool active;
};

// ====== NTP / TIME ======
extern WiFiUDP ntpUDP;
extern NTPClient timeClient;
void updateLocalTime();

// ====== DISCORD GATEWAY ======
extern WebSocketsClient gatewayWS;
extern DynamicJsonDocument* gwDoc;
extern bool gatewayConnected;
extern bool identified;
extern int heartbeatIntervalMs;
extern unsigned long lastHeartbeatMillis;
extern int lastSeq;
extern unsigned long lastBotActivityMillis;
extern uint8_t botDiscordStatus;
void noteBotActivity();
void updateBotPresenceIdle();
void sendBotPresence(const char* status, bool afk);
void applyCpuForIdleState();
void sendIdentify();
void sendHeartbeat();
void pumpGateway();
void gatewayEvent(WStype_t type, uint8_t* payload, size_t length);
void requestTrackedUserPresences();
void gwSendJson(JsonDocument& doc);

// ====== DISCORD REST / HTTPS ======
extern WiFiClientSecure httpsClient;
extern bool httpsInUse;
bool httpsConnect(const char* host, uint32_t timeoutMs = 15000);
uint8_t httpsGetOpen(const char* host, const String& path, unsigned long headerTimeoutMs,
                     const char* userAgent = "MiniMeBot/1.0",
                     const char* extraHeaders = nullptr);
uint8_t httpGetOpen(WiFiClient& client, const char* host, const String& path, unsigned long headerTimeoutMs);
void setHttpOpenError(String& outReport, uint8_t err, const char* label);
bool httpsAwaitHeaders(unsigned long deadlineMs, bool pump, String& outStatus,
                       bool& chunked, int& contentLength);
bool readHttpBodyAfterHeaders(Client& client, bool chunked, int contentLength,
                              String& outBody, unsigned long deadlineMs);
bool discordIdLooksValid(const String& id);
bool discordRestGet(const String& path, String& outBody, String& outStatus);
String guildIdFromChannel(const String& channelId);
bool appendMembersFromGuild(const String& guildId, uint8_t maxToAdd);
bool fetchGuildMembersAtStartup();
bool sendDiscordMessage(const String& channelId, const String& content, bool suppressEmbeds = false);
String getSystemInfo();
void boardMemTotals(uint32_t& memFree, uint32_t& memTotal);
void uptimeDhms(unsigned long& days, unsigned long& hours, unsigned long& minutes);

// ====== DISPLAY ======
extern U8G2_SSD1327_WS_128X128_F_HW_I2C u8g2;
extern float dashTempC;
extern float dashTempF;
extern int lastServoDeg;
extern String transientLine1;
extern String transientLine2;
extern String transientLine3;
extern unsigned long transientUntilMs;
extern unsigned long lastDashMillis;
extern unsigned long lastDisplayActivityMillis;
extern bool displayAsleep;
void noteDisplayActivity();
void drawDashboard();
void updateDisplay();
void showTransient(const String& line1, const String& line2 = "", const String& line3 = "", unsigned long durationMs = 3000);

// ====== TOUCH ======
extern unsigned long lastTouchWakeMillis;
extern bool touchWasActive;
extern uint32_t touchIdleBuf[TOUCH_AVG_N];
extern uint8_t touchIdleBufCount;
extern uint8_t touchIdleBufIdx;
extern uint32_t touchIdleSum;
extern uint32_t touchIdleAvg;
extern uint32_t usbVbusRefMv;
extern uint32_t usbVbusMvCached;
extern uint32_t usbVbusCompLastMv;
extern unsigned long usbVbusLastReadMs;
uint32_t readUsbVbusMilliVolts();
void setupTouch();
void pollTouchWake();

// ====== HARDWARE ======
extern OneWire oneWire;
extern DallasTemperature sensors;
extern Adafruit_NeoPixel pixels;
void setupPins();
void setupServo();
void setServoAngle(int angleDeg);
bool readTemperature(float& tempC, float& tempF);
void setLedRgb(uint8_t r, uint8_t g, uint8_t b);
bool parseRgbTriplet(const String& args, uint8_t& r, uint8_t& g, uint8_t& b);
bool isOwner(const String& authorId);

// ====== USERS / PRESENCE ======
extern TrackedUser trackedUsers[MAX_TRACKED_USERS];
extern String cachedGuildIds[3];
extern uint8_t cachedGuildCount;
extern unsigned long usesWindowStartMillis;
const char* statusToWord(uint8_t s);
void initTrackedUsers();
void recordUserUse(const String& userId, const String& userName);
void applyPresencesArray(JsonArray presences);
void handlePresenceUpdate(JsonObject d);
String discordDisplayName(JsonVariantConst user);
int findUserIndex(const String& userId);
int findFreeTrackedSlot();
void fillTrackedSlot(uint8_t i, const String& userId, const String& userName);
void rememberGuildId(const String& gid);

// ====== COMMANDS / BACKGROUND ======
extern unsigned long lastSysInfoMillis;
extern int lastSentHour;
extern bool askNeedPost;
extern String askPendingQuestion;
extern String askPendingChannelId;
String collapseWhitespace(String s);
String truncateText(const String& s, int maxLen);
bool getWeather(const String& zip, String& outReport);
bool getScienceNews(String& outReport);
bool getPhysicsPapers(String& outReport);
bool getApod(String& outReport);
bool getIssPosition(String& outReport);
bool askDeepSeek(const String& question, String& outReport);
void runAskFromLoop();
void handleCommand(const String& content, const String& authorId, const String& authorName,
                   const String& channelId, bool isDM);
void backgroundTasks();

// ====== SETUP / LOOP (MiniMe_Discord_Bot.ino) ======
void connectWiFi();
void connectGateway();

#endif
