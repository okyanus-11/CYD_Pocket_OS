# CYD Pocket OS

Unified launcher for the ESP32-2432S028 with Groq AI, Snake, clock, Wi-Fi
switcher, ASCII animation player and PC messenger.

## Appearance

Tap **THEME** on the home screen to cycle between Graphite, Midnight and Light.
The selected palette is saved in ESP32 Preferences and restored after reboot.
The same semantic colours are used across the launcher and apps.

## Build

Verified with PlatformIO: RAM 55,448 bytes (16.9%), flash 1,026,637 bytes (32.6% of the 3 MB app partition).
Bad Apple frames live on the SD card, so the firmware fits the 3 MB app
partition.

## Configuration

Copy `firmware/CYD_Pocket_OS/config.example.h` to `config.h` and fill in the
local Wi-Fi profiles, Groq settings and PC server address. Set
`GROQ_ROOT_CA` to the verified PEM root CA for `api.groq.com`; the firmware
rejects HTTPS connections if the CA or synchronized clock is missing.
Replace the example `PC_SERVER_TOKEN` with a long random secret and use the
same value as `CYD_API_TOKEN` on the PC server. `config.h` is ignored by Git.

## Bad Apple

The player expects `/badapple.txt` on the SD card with 26 lines per frame and
a `---FRAME---` separator. The asset is not committed. While it plays, the SD
card owns VSPI; hold BOOT (GPIO0) to restore touch.

## Server

`server/server.py` is a Flask message queue with `/health`, `/message` and
`/messages`. Install its requirements, set `CYD_API_TOKEN` to the same secret
as `PC_SERVER_TOKEN`, and run it on the trusted LAN. LAN requests require a
Bearer token; tokenless mode is loopback-only.

## Limits

Not tested on the physical board. Spotify is a separate project. The clock is
fixed to UTC+3. The Wi-Fi switcher only uses saved profiles.

## Installation

Requires an ESP32-2432S028 CYD, PlatformIO, Wi-Fi and a Groq API key. Configure `firmware/CYD_Pocket_OS/config.h` from the example, including Wi-Fi profiles, Groq trust root and the PC server token/address; build and upload from `firmware/`. The optional PC messenger uses `server/requirements.txt` and `server/server.py`. See the [full installation and flashing guide](INSTALLATION.md).
