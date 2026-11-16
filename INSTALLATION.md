# Installation and flashing

## Requirements
- ESP32-2432S028 / ESP32-2432S028R CYD and USB data cable
- microSD card for the optional Bad Apple asset
- Python 3.10+, PlatformIO Core, 2.4 GHz Wi-Fi
- Groq API key for AI features

Install PlatformIO with `python -m pip install --upgrade platformio`, then clone:

```bash
git clone https://github.com/okyanus-11/CYD_Pocket_OS.git
cd CYD_Pocket_OS
```

Copy `firmware/CYD_Pocket_OS/config.example.h` to `firmware/CYD_Pocket_OS/config.h`. Set Wi-Fi profiles, Groq key/model, the verified PEM root CA for `api.groq.com`, PC server address and a strong `PC_SERVER_TOKEN`. HTTPS requests fail closed while the trusted CA or synchronized clock is unavailable. Keep the populated config private.

Build and upload:

```bash
cd firmware
pio run
pio run -t upload --upload-port COMx
```

Replace `COMx` with the board's actual serial port.

## Optional PC messenger

From the repository root, install server dependencies and start the Flask service:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r server/requirements.txt
$env:CYD_API_TOKEN = "same-long-random-token-as-PC_SERVER_TOKEN"
python server/server.py --host 0.0.0.0 --port 5000
```

LAN requests require the same bearer token. HTTP is unencrypted; use a trusted private LAN and never port-forward it.

## SD asset

The player expects `/badapple.txt` at the SD card root, containing 26 lines per frame separated by `---FRAME---`. The asset is not included. See README for controls and known hardware limitations.
