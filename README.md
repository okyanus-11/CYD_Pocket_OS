# CYD Pocket OS

Multi-app launcher for the ESP32-2432S028 CYD: boot animation, Groq AI, Snake, clock, Wi-Fi network switcher, Bad Apple ASCII playback, and PC messaging. The repository also contains the LAN Flask companion server and the recovered ASCII asset.

## Dependencies

- ESP32 Arduino core (WiFi, WiFiClientSecure, HTTPClient, SPI, SD, time)
- ArduinoJson 7 (the sketch uses the `JsonDocument` API)
- TFT_eSPI
- XPT2046_Touchscreen

Configure TFT_eSPI for the target CYD board and verify the SD-card wiring, especially `SD_CS`.

## Setup

1. Replace the Wi-Fi, Groq, and `PC_SERVER` placeholders locally in `CYD_Pocket_OS.ino`.
2. Copy `assets/badapple.txt` to `/badapple.txt` on the SD card. The player expects 26 text lines per frame separated by `---FRAME---`.
3. Start the companion server with `py -m pip install -r requirements.txt` and `py server.py`.

Never commit credentials. The sketch uses `setInsecure()` for the Groq HTTPS client and requires certificate verification before sensitive use.

## Verification

Source-reviewed; not compiled or hardware-tested because no Arduino toolchain or CYD hardware was available.

