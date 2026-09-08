#include "minime.h"

WebSocketsClient gatewayWS;
DynamicJsonDocument* gwDoc = nullptr;
bool gatewayConnected     = false;
bool identified           = false;
int  heartbeatIntervalMs   = 0;
unsigned long lastHeartbeatMillis = 0;
int lastSeq               = 0;
unsigned long lastBotActivityMillis = 0;
uint8_t botDiscordStatus = 0;

void gwSendJson(JsonDocument& doc) {
  String payload;
  serializeJson(doc, payload);
  gatewayWS.sendTXT(payload);
}

void requestTrackedUserPresences() {
  if (cachedGuildCount == 0) return;
  bool any = false;
  for (uint8_t i = 0; i < MAX_TRACKED_USERS; i++) {
    if (trackedUsers[i].active && trackedUsers[i].userId.length()) {
      any = true;
      break;
    }
  }
  if (!any) return;

  for (uint8_t g = 0; g < cachedGuildCount; g++) {
    if (cachedGuildIds[g].length() < 16) continue;
    StaticJsonDocument<768> doc;
    doc["op"] = 8;
    JsonObject d = doc.createNestedObject("d");
    d["guild_id"] = cachedGuildIds[g];
    d["limit"] = 0;
    d["presences"] = true;
    JsonArray ids = d.createNestedArray("user_ids");
    for (uint8_t i = 0; i < MAX_TRACKED_USERS; i++) {
      if (trackedUsers[i].active && trackedUsers[i].userId.length()) {
        ids.add(trackedUsers[i].userId);
      }
    }
    gwSendJson(doc);
  }
}

void sendBotPresence(const char* status, bool afk) {
  if (!gatewayConnected || !identified) return;
  StaticJsonDocument<256> doc;
  doc["op"] = 3;
  JsonObject d = doc.createNestedObject("d");
  d["since"] = nullptr;
  d.createNestedArray("activities");
  d["status"] = status;
  d["afk"] = afk;
  gwSendJson(doc);
}

void noteBotActivity() {
  lastBotActivityMillis = millis();
  if (!gatewayConnected || !identified) return;
  if (botDiscordStatus == 2) return;
  botDiscordStatus = 2;
  sendBotPresence("online", false);
}

void updateBotPresenceIdle() {
  if (!gatewayConnected || !identified) return;
  if (lastBotActivityMillis == 0) {
    lastBotActivityMillis = millis();
    return;
  }
  if (botDiscordStatus == 1) return;
  if (millis() - lastBotActivityMillis < BOT_PRESENCE_IDLE_MS) return;
  botDiscordStatus = 1;
  sendBotPresence("idle", true);
}

// 80 MHz only when OLED off and Discord Idle. Wi-Fi needs >= 80 MHz.
void applyCpuForIdleState() {
  bool slow = displayAsleep && botDiscordStatus == 1 && identified;
  uint32_t want = slow ? CPU_MHZ_OLED_OFF_BOT_IDLE : CPU_MHZ_ACTIVE;
  if (getCpuFrequencyMhz() == want) return;
  setCpuFrequencyMhz(want);
}

void sendIdentify() {
  StaticJsonDocument<1024> doc;
  doc["op"] = 2;
  JsonObject d = doc.createNestedObject("d");
  d["token"] = BOT_TOKEN;
  JsonObject props = d.createNestedObject("properties");
  props["os"]     = "linux";
  props["browser"] = "esp32";
  props["device"]  = "esp32";
  d["compress"]         = false;
  d["large_threshold"] = 250;
  d["intents"] = 37635; // GUILDS + MEMBERS + PRESENCES + MESSAGES + DMs + MESSAGE_CONTENT
  JsonObject presence = d.createNestedObject("presence");
  presence["since"] = nullptr;
  presence.createNestedArray("activities");
  presence["status"] = "online";
  presence["afk"] = false;
  gwSendJson(doc);
  lastBotActivityMillis = millis();
  botDiscordStatus = 2;
}

void sendHeartbeat() {
  StaticJsonDocument<256> doc;
  doc["op"] = 1;
  if (lastSeq == 0) {
    doc["d"] = nullptr;
  } else {
    doc["d"] = lastSeq;
  }
  gwSendJson(doc);
}

void pumpGateway() {
  gatewayWS.loop();
  if (heartbeatIntervalMs > 0 && gatewayConnected && identified) {
    unsigned long now = millis();
    if (now - lastHeartbeatMillis >= (unsigned long)heartbeatIntervalMs) {
      lastHeartbeatMillis = now;
      sendHeartbeat();
    }
  }
}

void gatewayEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      gatewayConnected = false;
      identified       = false;
      botDiscordStatus = 0;
      showTransient("Gateway", "Disconnected");
      break;
    case WStype_CONNECTED:
      gatewayConnected = true;
      showTransient("Gateway", "Connected");
      break;
    case WStype_TEXT: {
      static StaticJsonDocument<384> gwFilter;
      static bool gwFilterInit = false;
      if (!gwFilterInit) {
        gwFilter["op"] = true;
        gwFilter["s"] = true;
        gwFilter["t"] = true;
        gwFilter["d"]["heartbeat_interval"] = true;
        gwFilter["d"]["status"] = true;
        gwFilter["d"]["user"]["id"] = true;
        gwFilter["d"]["user"]["username"] = true;
        gwFilter["d"]["user"]["global_name"] = true;
        gwFilter["d"]["content"] = true;
        gwFilter["d"]["channel_id"] = true;
        gwFilter["d"]["guild_id"] = true;
        gwFilter["d"]["author"]["id"] = true;
        gwFilter["d"]["author"]["username"] = true;
        gwFilter["d"]["author"]["global_name"] = true;
        gwFilter["d"]["author"]["bot"] = true;
        gwFilter["d"]["presences"][0]["user"]["id"] = true;
        gwFilter["d"]["presences"][0]["status"] = true;
        gwFilter["d"]["guilds"][0]["presences"][0]["user"]["id"] = true;
        gwFilter["d"]["guilds"][0]["presences"][0]["status"] = true;
        gwFilterInit = true;
      }

      if (!gwDoc) return;
      gwDoc->clear();
      DeserializationError err = deserializeJson(*gwDoc, payload, length, DeserializationOption::Filter(gwFilter));
      if (err) return;
      int op = (*gwDoc)["op"] | -1;
      if (gwDoc->containsKey("s") && !(*gwDoc)["s"].isNull()) {
        lastSeq = (*gwDoc)["s"].as<int>();
      }
      if (op == 10) {
        heartbeatIntervalMs = (*gwDoc)["d"]["heartbeat_interval"] | 0;
        lastHeartbeatMillis = millis();
        sendIdentify();
        identified = true;
        return;
      }
      if (op == 11) return;
      if (op == 0) {
        const char* t = (*gwDoc)["t"];
        if (!t) return;
        if (strcmp(t, "READY") == 0) {
          JsonArray guilds = (*gwDoc)["d"]["guilds"].as<JsonArray>();
          if (!guilds.isNull()) {
            for (JsonObject g : guilds) {
              applyPresencesArray(g["presences"].as<JsonArray>());
            }
          }
          requestTrackedUserPresences();
          return;
        }
        if (strcmp(t, "GUILD_CREATE") == 0) {
          applyPresencesArray((*gwDoc)["d"]["presences"].as<JsonArray>());
          requestTrackedUserPresences();
          return;
        }
        if (strcmp(t, "GUILD_MEMBERS_CHUNK") == 0) {
          applyPresencesArray((*gwDoc)["d"]["presences"].as<JsonArray>());
          return;
        }
        if (strcmp(t, "PRESENCE_UPDATE") == 0) {
          handlePresenceUpdate((*gwDoc)["d"]);
          return;
        }
        if (strcmp(t, "MESSAGE_CREATE") == 0) {
          if (httpsInUse) return; // DeepSeek holds HTTPS; defer commands until free
          JsonObject d = (*gwDoc)["d"];
          if (d["author"]["bot"] == true) return;
          String content   = d["content"].as<String>();
          String channelId = d["channel_id"].as<String>();
          String authorId  = d["author"]["id"].as<String>();
          String authorName = discordDisplayName(d["author"]);
          bool isDM = d["guild_id"].isNull();
          handleCommand(content, authorId, authorName, channelId, isDM);
        }
      }
      break;
    }
    default:
      break;
  }
}
