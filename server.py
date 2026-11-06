"""
CYD local LAN server.
Keeps the Spotify controller routes used by the earlier CYD Spotify project,
and adds POST /message for short text messages from the CYD.

Run:
  py -m pip install flask
  py server.py

The computer and CYD must be on the same home Wi-Fi.
"""
from collections import deque
from datetime import datetime
from flask import Flask, jsonify, request

app = Flask(__name__)
messages = deque(maxlen=100)

@app.get("/")
def home():
    return "CYD SPOTIFY + PC MESSENGER SERVER OK"

# Existing Spotify controller endpoints (keep these for the old CYD app).
@app.get("/play")
def play():
    print("[SPOTIFY] play")
    return "OK"

@app.get("/previous")
def previous():
    print("[SPOTIFY] previous")
    return "OK"

@app.get("/next")
def next_track():
    print("[SPOTIFY] next")
    return "OK"

@app.get("/volume/down")
def volume_down():
    print("[SPOTIFY] volume down")
    return "OK"

@app.get("/volume/up")
def volume_up():
    print("[SPOTIFY] volume up")
    return "OK"

@app.get("/now-playing")
def now_playing():
    # Keep the legacy format expected by the old CYD Spotify controller.
    return "Unknown song|Unknown artist|"

@app.get("/spotify-test")
def spotify_test():
    return "Spotify route OK"

@app.post("/message")
def receive_message():
    data = request.get_json(silent=True) or {}
    message = str(data.get("message", "")).strip()
    source = str(data.get("source", "CYD"))[:32]
    if not message:
        return jsonify(ok=False, error="message is required"), 400
    if len(message) > 240:
        return jsonify(ok=False, error="message must be 240 characters or fewer"), 400

    item = {
        "time": datetime.now().astimezone().isoformat(timespec="seconds"),
        "source": source,
        "message": message,
    }
    messages.appendleft(item)
    print(f"\n[{item['time']}] MESSAGE FROM {source}: {message}\n", flush=True)
    return jsonify(ok=True, received=True, item=item)

@app.get("/messages")
def get_messages():
    return jsonify(list(messages))

if __name__ == "__main__":
    # 0.0.0.0 makes the server reachable by other devices on your home LAN.
    # Do not expose this port directly to the public internet.
    app.run(host="0.0.0.0", port=5000, debug=False)

