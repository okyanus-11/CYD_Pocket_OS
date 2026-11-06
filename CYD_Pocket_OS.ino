/*
  CYD Pocket OS - ESP32-2432S028 / 320x240
  Apps: Groq AI, Snake, Clock, Wi-Fi switcher, ASCII frame player, PC Messenger.
  Hardware assumptions based on the supplied sketch:
    TFT_eSPI display, XPT2046 touch CS=33 IRQ=36 SCK=25 MISO=39 MOSI=32
    CYD SD card CS=5, default SPI pins (verify your board/wiring).
  IMPORTANT:
    1) Fill WIFI_PROFILES and GROQ_API_KEY below.
    2) The Wi-Fi switcher connects to networks listed in WIFI_PROFILES.
    3) Put an ASCII animation file at /badapple.txt on the SD card.
       It uses 30 lines per frame, each frame separated by a line containing ---FRAME---.
    4) Keep the Flask server on your computer running on the same home Wi-Fi.
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <SD.h>
#include <time.h>

// ---------------- USER CONFIG ----------------
struct WiFiProfile { const char* ssid; const char* password; };
WiFiProfile WIFI_PROFILES[] = {
  {"YOUR_WIFI_NAME", "YOUR_WIFI_PASSWORD"},
  // {"Another network", "its password"},
};
const int WIFI_PROFILE_COUNT = sizeof(WIFI_PROFILES) / sizeof(WIFI_PROFILES[0]);

const char* GROQ_API_KEY = "PASTE_GROQ_API_KEY_HERE";
const char* GROQ_MODEL = "openai/gpt-oss-20b";
const char* GROQ_URL = "https://api.groq.com/openai/v1/chat/completions";

// Set this to the computer's LAN IPv4 address. Spotify server previously used port 5000.
const char* PC_SERVER = "http://YOUR_COMPUTER_LAN_IP:5000";

// ---------------- HARDWARE ----------------
TFT_eSPI tft;
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_SCK 25
#define TOUCH_MISO 39
#define TOUCH_MOSI 32
#define SD_CS 5

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

const int SW = 320, SH = 240;
const int TOUCH_MIN_X = 200, TOUCH_MAX_X = 3900;
const int TOUCH_MIN_Y = 200, TOUCH_MAX_Y = 3900;

enum Screen { HOME, AI_CHAT, PC_MESSAGE, CLOCK_APP, WIFI_APP, SNAKE_APP, BADAPPLE_APP };
Screen screen = HOME;
String statusText = "Starting";
String inputText = "";
String lastReply = "";
bool keyboardShift = false;
bool sdReady = false;

// ---------------- TOUCH ----------------
bool getTouchPoint(int &x, int &y) {
  if (!touch.touched()) return false;
  TS_Point p = touch.getPoint();
  x = constrain(map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SW - 1), 0, SW - 1);
  y = constrain(map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SH - 1), 0, SH - 1);
  return true;
}
void waitForRelease() {
  unsigned long start = millis();
  while (touch.touched() && millis() - start < 500) delay(10);
  delay(100);
}

// ---------------- UI HELPERS ----------------
void header(const char* title) {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, SW, 22, TFT_DARKCYAN);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
  tft.setCursor(5, 7); tft.print(title);
  tft.setCursor(264, 7); tft.print(WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE");
}
void button(int x, int y, int w, int h, const char* label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 5, color);
  tft.drawRoundRect(x, y, w, h, 5, TFT_WHITE);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, color);
  int textW = strlen(label) * 6;
  tft.setCursor(x + max(3, (w - textW) / 2), y + (h - 8) / 2);
  tft.print(label);
}
void drawHome() {
  header("CYD POCKET OS");
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(7, 27); tft.print("Choose an app");
  button(7, 43, 98, 48, "GROQ AI", TFT_DARKGREEN);
  button(111, 43, 98, 48, "SNAKE", TFT_BLUE);
  button(215, 43, 98, 48, "CLOCK", TFT_PURPLE);
  button(7, 99, 98, 48, "WI-FI", TFT_DARKCYAN);
  button(111, 99, 98, 48, "BAD APPLE", TFT_MAROON);
  button(215, 99, 98, 48, "PC MESSAGE", TFT_OLIVE);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(7, 165); tft.print("CYD ESP32 | 320x240");
  tft.setCursor(7, 180); tft.print(statusText.substring(0, 50));
  tft.setCursor(7, 218); tft.print("Tap an app to open");
}
void goHome() { screen = HOME; drawHome(); }

// ---------------- BOOT ANIMATION ----------------
void bootAnimation() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(42, 60); tft.print("CYD POCKET");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(92, 84); tft.print("OS");
  tft.drawRect(30, 125, 260, 14, TFT_DARKGREY);
  for (int i = 0; i <= 252; i += 7) {
    tft.fillRect(34, 129, i, 6, TFT_GREEN);
    tft.fillCircle(34 + i, 132, 2, TFT_WHITE);
    delay(18);
  }
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(88, 154); tft.print("Starting applications...");
  delay(350);
}

// ---------------- CLOCK ----------------
void drawClock() {
  header("CLOCK");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(3);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 50)) {
    char buf[16]; strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    tft.setCursor(50, 65); tft.print(buf);
    tft.setTextSize(2);
    strftime(buf, sizeof(buf), "%d/%m/%Y", &timeinfo);
    tft.setCursor(85, 108); tft.print(buf);
    tft.setTextSize(1);
    strftime(buf, sizeof(buf), "%A", &timeinfo);
    tft.setCursor(125, 145); tft.print(buf);
  } else {
    tft.setTextSize(2); tft.setCursor(35, 75); tft.print("Waiting for NTP");
    tft.setTextSize(1); tft.setCursor(35, 105); tft.print("Connect Wi-Fi for time sync");
  }
  button(8, 202, 72, 27, "BACK", TFT_DARKGREY);
}
void updateClock() {
  if (screen != CLOCK_APP) return;
  static unsigned long last = 0;
  if (millis() - last >= 1000) { last = millis(); drawClock(); }
}

// ---------------- WIFI SWITCHER ----------------
int foundNetworks = 0;
String foundSSID[12];
int foundRSSI[12];
void scanNetworks() {
  header("WI-FI NETWORKS");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(7, 30); tft.print("Scanning...");
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks(false, true);
  foundNetworks = min(n, 12);
  for (int i = 0; i < foundNetworks; i++) {
    foundSSID[i] = WiFi.SSID(i);
    foundRSSI[i] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();
  drawWiFiList();
}
void drawWiFiList() {
  header("WI-FI SWITCHER");
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(6, 25); tft.print("Tap a network to connect");
  for (int i = 0; i < foundNetworks && i < 6; i++) {
    int y = 42 + i * 25;
    tft.fillRoundRect(5, y, 310, 22, 3, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setCursor(10, y + 7);
    String name = foundSSID[i]; if (name.length() > 25) name = name.substring(0, 25);
    tft.print(name);
    tft.setCursor(255, y + 7); tft.print(foundRSSI[i]); tft.print("dB");
  }
  button(5, 202, 88, 27, "RESCAN", TFT_DARKCYAN);
  button(100, 202, 88, 27, "BACK", TFT_DARKGREY);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(194, 211); tft.print("Saved nets only");
}
void connectProfile(int index) {
  if (index < 0 || index >= foundNetworks) return;
  String selected = foundSSID[index];
  for (int i = 0; i < WIFI_PROFILE_COUNT; i++) {
    if (selected == WIFI_PROFILES[i].ssid) {
      header("CONNECTING");
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(10, 60); tft.print(selected);
      WiFi.disconnect();
      WiFi.begin(WIFI_PROFILES[i].ssid, WIFI_PROFILES[i].password);
      unsigned long start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
        delay(250);
      }
      statusText = WiFi.status() == WL_CONNECTED ? "Wi-Fi connected" : "Wi-Fi failed";
      if (WiFi.status() == WL_CONNECTED) configTime(3 * 3600, 0, "pool.ntp.org", "time.google.com");
      drawWiFiList();
      return;
    }
  }
  tft.fillRect(5, 178, 310, 18, TFT_BLACK);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setCursor(8, 182); tft.print("Not saved: add password in WIFI_PROFILES");
}

// ---------------- TEXT ENTRY ----------------
void drawTextApp(bool pcMode) {
  header(pcMode ? "PC MESSENGER" : "GROQ AI");
  tft.drawRect(4, 27, 312, 69, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  String shown = inputText;
  if (shown.length() == 0) shown = lastReply;
  if (shown.length() > 100) shown = shown.substring(shown.length() - 100);
  tft.setCursor(8, 32);
  for (int i = 0, line = 0; i < shown.length() && line < 6; i += 50, line++) {
    tft.setCursor(8, 32 + line * 10);
    tft.print(shown.substring(i, min(i + 50, (int)shown.length())));
  }
  const char* rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm,."};
  for (int r = 0; r < 3; r++) {
    int y = 105 + r * 24;
    int count = strlen(rows[r]);
    int kw = SW / count;
    for (int c = 0; c < count; c++) {
      int x = c * kw;
      tft.fillRoundRect(x + 1, y, kw - 2, 21, 3, TFT_NAVY);
      tft.setTextColor(TFT_WHITE, TFT_NAVY);
      tft.setCursor(x + (kw - 6) / 2, y + 7);
      char ch = rows[r][c]; if (keyboardShift) ch = toupper(ch);
      tft.print(ch);
    }
  }
  button(3, 179, 48, 24, "SHIFT", TFT_MAROON);
  button(54, 179, 65, 24, "SPACE", TFT_DARKGREY);
  button(122, 179, 48, 24, "DEL", TFT_DARKGREY);
  button(173, 179, 48, 24, "CLEAR", TFT_DARKGREY);
  button(224, 179, 92, 24, pcMode ? "SEND TO PC" : "ASK GROQ", TFT_DARKGREEN);
  button(5, 210, 75, 25, "BACK", TFT_DARKGREY);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(88, 219); tft.print(statusText.substring(0, 37));
}
void addTypedChar(char c) { if (inputText.length() < 140) inputText += c; }

String askGroq(String prompt) {
  if (String(GROQ_API_KEY) == "PASTE_GROQ_API_KEY_HERE") return "Set GROQ_API_KEY in the sketch first.";
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, GROQ_URL)) return "Could not start HTTPS";
  http.setTimeout(30000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + GROQ_API_KEY);
  JsonDocument req;
  req["model"] = GROQ_MODEL;
  req["max_completion_tokens"] = 220;
  JsonArray msgs = req["messages"].to<JsonArray>();
  JsonObject sys = msgs.add<JsonObject>();
  sys["role"] = "system";
  sys["content"] = "You are a helpful tiny-device assistant. Reply in at most 2 short sentences, plain text.";
  JsonObject usr = msgs.add<JsonObject>(); usr["role"] = "user"; usr["content"] = prompt;
  String body; serializeJson(req, body);
  int code = http.POST(body); String response = http.getString(); http.end();
  if (code != 200) return "Groq HTTP " + String(code);
  JsonDocument res;
  if (deserializeJson(res, response)) return "Bad JSON from Groq";
  String answer = res["choices"][0]["message"]["content"] | "Empty AI reply";
  answer.trim(); return answer;
}
void postPCMessage(String msg) {
  if (WiFi.status() != WL_CONNECTED) { statusText = "Wi-Fi is offline"; return; }
  HTTPClient http;
  String url = String(PC_SERVER) + "/message";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  JsonDocument doc; doc["message"] = msg; doc["source"] = "CYD";
  String body; serializeJson(doc, body);
  int code = http.POST(body);
  String response = http.getString();
  http.end();
  statusText = (code >= 200 && code < 300) ? "Message delivered to PC" : "Server error " + String(code);
  Serial.println("PC message result: " + response);
}
void submitText(bool pcMode) {
  if (inputText.length() == 0) return;
  String msg = inputText; inputText = "";
  lastReply = pcMode ? ("Sent: " + msg) : "Thinking...";
  statusText = pcMode ? "Sending to computer..." : "Asking Groq...";
  drawTextApp(pcMode);
  if (pcMode) {
    postPCMessage(msg);
    lastReply = "Sent: " + msg;
  } else {
    lastReply = askGroq(msg);
    statusText = "Groq reply received";
    Serial.println("Groq: " + lastReply);
  }
  drawTextApp(pcMode);
}

// ---------------- SNAKE ----------------
const int CELL = 8, COLS = 38, ROWS = 22;
int sx[COLS * ROWS], sy[COLS * ROWS], snakeLen = 5;
int dx = 1, dy = 0, foodX = 18, foodY = 10, snakeScore = 0;
bool snakeAlive = false;
unsigned long lastSnake = 0;
void placeFood() {
  foodX = random(1, COLS - 1); foodY = random(1, ROWS - 1);
}
void drawSnake() {
  header("SNAKE");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(220, 7); tft.print("SCORE "); tft.print(snakeScore);
  tft.drawRect(7, 27, COLS * CELL, ROWS * CELL, TFT_DARKGREY);
  for (int i = 0; i < snakeLen; i++)
    tft.fillRect(8 + sx[i] * CELL, 28 + sy[i] * CELL, CELL - 1, CELL - 1, i == 0 ? TFT_GREEN : TFT_DARKGREEN);
  tft.fillRect(8 + foodX * CELL, 28 + foodY * CELL, CELL - 1, CELL - 1, TFT_RED);
  button(5, 210, 70, 25, "BACK", TFT_DARKGREY);
  button(238, 210, 77, 25, snakeAlive ? "PAUSE" : "START", TFT_DARKCYAN);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(82, 219); tft.print("Tap left/right/top/bottom to steer");
}
void startSnake() {
  snakeLen = 5; snakeScore = 0; dx = 1; dy = 0;
  for (int i = 0; i < snakeLen; i++) { sx[i] = 12 - i; sy[i] = 10; }
  placeFood(); snakeAlive = true; drawSnake();
}
void tickSnake() {
  if (screen != SNAKE_APP || !snakeAlive || millis() - lastSnake < 170) return;
  lastSnake = millis();
  for (int i = snakeLen - 1; i > 0; i--) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
  sx[0] += dx; sy[0] += dy;
  if (sx[0] < 0 || sx[0] >= COLS || sy[0] < 0 || sy[0] >= ROWS) {
    snakeAlive = false; statusText = "Game over! Tap START"; drawSnake(); return;
  }
  for (int i = 1; i < snakeLen; i++) if (sx[0] == sx[i] && sy[0] == sy[i]) {
    snakeAlive = false; statusText = "Game over! Tap START"; drawSnake(); return;
  }
  if (sx[0] == foodX && sy[0] == foodY) {
    if (snakeLen < COLS * ROWS - 1) snakeLen++;
    snakeScore++; placeFood();
  }
  drawSnake();
}

// ---------------- ASCII ANIMATION PLAYER ----------------
File frameFile;
unsigned long lastFrame = 0;
const int FRAME_LINES = 26;
void openBadApple() {
  header("ASCII PLAYER");
  if (!sdReady) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(8, 45); tft.print("SD card not detected.");
    tft.setCursor(8, 62); tft.print("Insert SD with /badapple.txt");
    tft.setCursor(8, 79); tft.print("ASCII frames are user-supplied.");
  } else {
    if (frameFile) frameFile.close();
    frameFile = SD.open("/badapple.txt", FILE_READ);
    if (!frameFile) {
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.setCursor(8, 45); tft.print("Missing /badapple.txt");
      tft.setCursor(8, 62); tft.print("Put ASCII frames on SD.");
    } else {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.setCursor(8, 45); tft.print("ASCII animation loaded.");
      tft.setCursor(8, 62); tft.print("Playing frames...");
    }
  }
  button(5, 210, 75, 25, "BACK", TFT_DARKGREY);
}
void playAsciiFrame() {
  if (screen != BADAPPLE_APP || !frameFile || millis() - lastFrame < 100) return;
  lastFrame = millis();
  tft.fillRect(0, 23, SW, 184, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int y = 25, lines = 0;
  while (frameFile.available() && lines < FRAME_LINES) {
    String line = frameFile.readStringUntil('\n'); line.replace("\r", "");
    if (line == "---FRAME---") {
      if (lines == 0) continue;
      break;
    }
    if (line.length() > 53) line = line.substring(0, 53);
    tft.setCursor(0, y); tft.print(line);
    y += 7; lines++;
  }
  if (!frameFile.available()) { frameFile.close(); frameFile = SD.open("/badapple.txt", FILE_READ); }
  button(5, 210, 75, 25, "BACK", TFT_DARKGREY);
}

// ---------------- SETUP / TOUCH ROUTER ----------------
void setup() {
  Serial.begin(115200);
  tft.init(); tft.setRotation(1);
  touchSPI.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI); touch.setRotation(1);
  randomSeed(analogRead(34));
  bootAnimation();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_PROFILES[0].ssid, WIFI_PROFILES[0].password);
  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 12000) delay(250);
  if (WiFi.status() == WL_CONNECTED) {
    configTime(3 * 3600, 0, "pool.ntp.org", "time.google.com");
    statusText = "Wi-Fi connected: " + WiFi.localIP().toString();
  } else statusText = "Wi-Fi offline; open Wi-Fi app";
  // SD support is optional. If your SD and TFT share SPI pins, test SD separately.
  sdReady = SD.begin(SD_CS);
  drawHome();
}
void loop() {
  updateClock();
  tickSnake();
  playAsciiFrame();
  int x, y;
  if (!getTouchPoint(x, y)) { delay(10); return; }

  if (screen == HOME) {
    if (y >= 43 && y < 91) {
      if (x < 108) { screen = AI_CHAT; inputText = ""; drawTextApp(false); }
      else if (x < 212) { screen = SNAKE_APP; startSnake(); }
      else { screen = CLOCK_APP; drawClock(); }
    } else if (y >= 99 && y < 147) {
      if (x < 108) { screen = WIFI_APP; scanNetworks(); }
      else if (x < 212) { screen = BADAPPLE_APP; openBadApple(); }
      else { screen = PC_MESSAGE; inputText = ""; lastReply = ""; drawTextApp(true); }
    }
  } else if (screen == CLOCK_APP) {
    if (y > 195 && x < 95) goHome();
  } else if (screen == WIFI_APP) {
    if (y > 198 && x < 95) scanNetworks();
    else if (y > 198 && x < 190) goHome();
    else if (y >= 42 && y < 192) connectProfile((y - 42) / 25);
  } else if (screen == SNAKE_APP) {
    if (y > 205 && x < 80) { snakeAlive = false; goHome(); }
    else if (y > 205 && x > 225) { if (!snakeAlive) startSnake(); else snakeAlive = false; drawSnake(); }
    else if (y < 105) { if (dy != 1) { dx = 0; dy = -1; } }
    else if (y < 155) { if (dx != 1) { dx = -1; dy = 0; } }
    else if (x > 160) { if (dx != -1) { dx = 1; dy = 0; } }
    else { if (dy != -1) { dx = 0; dy = 1; } }
  } else if (screen == BADAPPLE_APP) {
    if (y > 205 && x < 90) { if (frameFile) frameFile.close(); goHome(); }
  } else if (screen == AI_CHAT || screen == PC_MESSAGE) {
    bool pc = screen == PC_MESSAGE;
    if (y > 205 && x < 90) { goHome(); }
    else if (y >= 105 && y < 177) {
      int row = (y - 105) / 24;
      const char* rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm,."};
      int count = strlen(rows[row]); int idx = x / (SW / count);
      if (idx < count) { char c = rows[row][idx]; if (keyboardShift) c = toupper(c); addTypedChar(c); keyboardShift = false; }
      drawTextApp(pc);
    } else if (y >= 179 && y < 204) {
      if (x < 53) keyboardShift = !keyboardShift;
      else if (x < 121) addTypedChar(' ');
      else if (x < 172) { if (inputText.length()) inputText.remove(inputText.length()-1); }
      else if (x < 223) inputText = "";
      else submitText(pc);
      drawTextApp(pc);
    }
  }
  waitForRelease();
}

