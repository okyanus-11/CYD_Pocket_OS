# CYD Pocket OS

Unified launcher for the ESP32-2432S028 with Groq AI, Snake, clock, Wi-Fi
switcher, ASCII animation player and PC messenger.

## Build

Verified with PlatformIO: RAM 55,596 bytes, flash 1,045,845 bytes.
Bad Apple frames live on the SD card, so the firmware fits the 3 MB app
partition.

## Configuration

Copy `firmware/CYD_Pocket_OS/config.example.h` to `config.h` and fill in the
local Wi-Fi profiles, Groq settings and PC server address. `config.h` is
ignored by Git.

## Bad Apple

The player expects `/badapple.txt` on the SD card with 26 lines per frame and
a `---FRAME---` separator. The asset is not committed. While it plays, the SD
card owns VSPI; hold BOOT (GPIO0) to restore touch.

## Server

`server/server.py` is a Flask message queue with `/health`, `/message` and
`/messages`. Install its requirements and run it on a trusted LAN.

## Fixes

Removed real credentials and the hardcoded LAN address; fixed the Wi-Fi
profile initializer and BACK hitbox; added low reasoning effort for Groq;
opened SD lazily; moved TFT_eSPI to HSPI; shared keyboard geometry; added a
Wi-Fi timeout; replaced the ADC seed with `esp_random()`.

## Limits

Not tested on the physical board. Spotify is a separate project. The clock is
fixed to UTC+3. The Wi-Fi switcher only uses saved profiles.
