// Copy to config.h and fill in locally. config.h is git-ignored.
#pragma once

// Up to four saved Wi-Fi profiles. Leave unused SSIDs empty.
#define WIFI_PROFILE_1_SSID      "YOUR_WIFI_SSID"
#define WIFI_PROFILE_1_PASSWORD  "YOUR_WIFI_PASSWORD"
#define WIFI_PROFILE_2_SSID      ""
#define WIFI_PROFILE_2_PASSWORD  ""
#define WIFI_PROFILE_3_SSID      ""
#define WIFI_PROFILE_3_PASSWORD  ""

// Groq key: https://console.groq.com/keys
#define GROQ_API_KEY   "YOUR_GROQ_API_KEY"
#define GROQ_MODEL     "openai/gpt-oss-20b"

// Windows PC running SHARED/LAN_SERVER/server.py (or server/server.py).
// Use the PC's current LAN IPv4 address; do not reuse an old address.
#define PC_SERVER_HOST  "192.168.1.100"
#define PC_SERVER_PORT  5000
