#!/usr/bin/env python3
"""Reusable Flask LAN server for CYD projects.

The CYD and the Windows PC must be on the same trusted home network.  The
server keeps the last 100 short messages in memory and exposes a tiny JSON
API.  It is deliberately dependency-light: Flask is the only requirement.

Run:
    python -m pip install -r requirements.txt
    python server.py --port 5000

Endpoints
    GET  /                 plain-text liveness string
    GET  /health           JSON status + message count
    GET  /ping             "pong"
    POST /message          {"message": "...", "source": "CYD"}
    GET  /messages?limit=  newest-first JSON list
    POST /messages/clear   drop all in-memory messages

There is no authentication: bind to a trusted LAN only, never port-forward it.
"""
from __future__ import annotations

import argparse
import os
import threading
from collections import deque
from datetime import datetime

from flask import Flask, jsonify, request

MAX_MESSAGES = int(os.getenv("CYD_MAX_MESSAGES", "100"))
MAX_MESSAGE_LENGTH = 240

app = Flask(__name__)
_messages: deque[dict] = deque(maxlen=MAX_MESSAGES)
_lock = threading.Lock()


def _timestamp() -> str:
    return datetime.now().astimezone().isoformat(timespec="seconds")


def _text(value, limit: int) -> str:
    """Coerce to a trimmed string without control characters."""
    cleaned = "".join(
        ch for ch in str(value or "") if ch == "\n" or ch == "\t" or ord(ch) >= 32
    ).strip()
    return cleaned[:limit]


@app.get("/")
def index():
    return "CYD LAN SERVER OK\n", 200, {"Content-Type": "text/plain; charset=utf-8"}


@app.get("/ping")
def ping():
    return "pong"


@app.get("/health")
def health():
    with _lock:
        count = len(_messages)
    return jsonify(
        ok=True,
        service="cyd-lan-server",
        time=_timestamp(),
        messages=count,
        capacity=MAX_MESSAGES,
    )


@app.post("/message")
def receive_message():
    if not request.is_json:
        return jsonify(ok=False, error="expected application/json"), 415

    data = request.get_json(silent=True)
    if not isinstance(data, dict):
        return jsonify(ok=False, error="body must be a JSON object"), 400

    raw = data.get("message", "")
    if not isinstance(raw, str):
        return jsonify(ok=False, error="message must be a string"), 400

    message = _text(raw, MAX_MESSAGE_LENGTH)
    if not message:
        return jsonify(ok=False, error="message is required"), 400
    if len(raw) > MAX_MESSAGE_LENGTH:
        return (
            jsonify(
                ok=False,
                error=f"message must be {MAX_MESSAGE_LENGTH} characters or fewer",
            ),
            400,
        )

    source = _text(data.get("source", "CYD"), 32) or "CYD"
    item = {"time": _timestamp(), "source": source, "message": message}

    with _lock:
        _messages.appendleft(item)

    print(f"\n[{item['time']}] MESSAGE FROM {source}: {message}\n", flush=True)
    return jsonify(ok=True, received=True, item=item), 201


@app.get("/messages")
def list_messages():
    try:
        limit = int(request.args.get("limit", "50"))
    except (TypeError, ValueError):
        limit = 50
    limit = max(0, min(limit, MAX_MESSAGES))

    with _lock:
        items = list(_messages)[:limit]

    return jsonify(ok=True, count=len(items), messages=items)


@app.post("/messages/clear")
def clear_messages():
    with _lock:
        cleared = len(_messages)
        _messages.clear()
    return jsonify(ok=True, cleared=cleared)


def main() -> None:
    parser = argparse.ArgumentParser(description="CYD LAN message server")
    parser.add_argument("--host", default=os.getenv("CYD_SERVER_HOST", "0.0.0.0"))
    parser.add_argument(
        "--port", type=int, default=int(os.getenv("CYD_SERVER_PORT", "5000"))
    )
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()

    print(f"CYD LAN server listening on http://{args.host}:{args.port}")
    print("Keep this on a trusted LAN; the API has no authentication.")
    app.run(host=args.host, port=args.port, debug=args.debug)


if __name__ == "__main__":
    main()
