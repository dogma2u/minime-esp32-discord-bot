#ifndef MINIME_SECRETS_H
#define MINIME_SECRETS_H

// Copy this file to secrets.h in the same folder, then fill in real values.
// secrets.h is gitignored — never commit real tokens or passwords.

const char* WIFI_SSID     = "ssid";
const char* WIFI_PASSWORD = "password";
const char* BOT_TOKEN     = "bot token";
const char* WEATHER_API_KEY = "WEATHER_API_KEY";
const char* NASA_API_KEY    = "NASA_API_KEY";
const char* DEEPSEEK_API_KEY = "DEEPSEEK_API_KEY";

#define BOT_GUILD_ID "GUILD_ID"  // startup member fetch

const char* OWNER_ID_STR        = "OWNER_ID_STR";  // GPIO / servo
const char* TARGET_CHANNEL_ID  = "TARGET_CHANNEL_ID";  // commands + auto posts
const char* TARGET_CHANNEL_ID1 = "TARGET_CHANNEL_ID1";  // second command channel

#endif
