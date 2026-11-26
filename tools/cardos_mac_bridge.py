#!/usr/bin/env python3
"""CardOS MacLink Bridge Server.

Run this on your Mac after connecting to the Cardputer's `CardOS-AP` Wi-Fi network
(or on the same local Wi-Fi network) so CardOS's MacLink and Shell apps can offload
heavy computation, run scripts/terminal commands, query CPU/RAM/battery, and control media.

Usage:
    python3 cardos_mac_bridge.py [--port 8765]
"""

import argparse
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
import platform
import subprocess
import urllib.parse


def run_shell(cmd: str, timeout: int = 20) -> str:
    try:
        res = subprocess.run(
            cmd,
            shell=True,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=os.path.expanduser("~"),
        )
        out = (res.stdout or "") + (res.stderr or "")
        out = out.strip()
        if not out:
            out = f"(exit {res.returncode})"
        return out[:3500]
    except subprocess.TimeoutExpired:
        return "Timed out after 20s"
    except Exception as exc:
        return f"Error: {exc}"


def get_mac_stats() -> dict:
    host = platform.node() or "Mac"
    batt = run_shell("pmset -g batt | grep -Eo '[0-9]+%' | head -n 1", timeout=3)
    load = run_shell("sysctl -n vm.loadavg", timeout=3)
    mem = run_shell("top -l 1 -s 0 | grep PhysMem | head -n 1", timeout=5)
    return {
        "host": host,
        "battery": batt,
        "load": load,
        "mem": mem,
    }


def trigger_media(action: str) -> str:
    scripts = {
        "playpause": 'tell application "Music" to playpause',
        "next": 'tell application "Music" to next track',
        "voldown": "set volume output volume ((output volume of (get volume settings)) - 10)",
        "volup": "set volume output volume ((output volume of (get volume settings)) + 10)",
        "lock": 'tell application "System Events" to keystroke "q" using {control down, command down}',
    }
    if action not in scripts:
        return "Unknown action"
    try:
        subprocess.run(["osascript", "-e", scripts[action]], timeout=5, check=False)
        return f"OK: {action}"
    except Exception as exc:
        return f"Err: {exc}"


class BridgeHandler(BaseHTTPRequestHandler):
    def _send(self, code: int, body: str, ctype: str = "text/plain; charset=utf-8"):
        data = body.encode("utf-8", errors="replace")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        qs = urllib.parse.parse_qs(parsed.query)
        if parsed.path == "/ping":
            self._send(200, "CardOS MacBridge Ready")
        elif parsed.path == "/stats":
            self._send(200, json.dumps(get_mac_stats()), "application/json")
        elif parsed.path == "/exec":
            cmd = qs.get("cmd", [""])[0]
            self._send(200, run_shell(cmd) if cmd else "Missing cmd")
        else:
            self._send(404, "Not found")

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        qs = urllib.parse.parse_qs(parsed.query)
        length = int(self.headers.get("Content-Length", "0") or 0)
        raw = self.rfile.read(length).decode("utf-8", errors="replace") if length > 0 else ""
        if parsed.path == "/exec":
            cmd = raw.strip() or qs.get("cmd", [""])[0]
            self._send(200, run_shell(cmd) if cmd else "Empty command")
        elif parsed.path == "/media":
            act = qs.get("a", [""])[0] or raw.strip()
            self._send(200, trigger_media(act))
        else:
            self._send(404, "Not found")

    def log_message(self, fmt, *args):
        print(f"[MacLink] {self.client_address[0]} - {fmt % args}")


def main():
    parser = argparse.ArgumentParser(description="CardOS MacLink Wireless Bridge")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    srv = HTTPServer(("0.0.0.0", args.port), BridgeHandler)
    print(f"CardOS MacLink Bridge listening on 0.0.0.0:{args.port}")
    print("Connect your Mac to CardOS-AP (pass: cardos1234), then open MacLink on Cardputer.")
    srv.serve_forever()


if __name__ == "__main__":
    main()

