#!/usr/bin/env python3
"""Laptop remote control, screenshot capture, & inspection CLI for CardOS over USB Serial."""

import argparse
import glob
import struct
import sys
import time
import zlib
import serial


def find_port():
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("No /dev/cu.usbmodem* port found. Is the Cardputer ADV connected?")
    return ports[0]


def write_png_rgb(path, width, height, rgb_bytes):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    rows = bytearray()
    stride = width * 3
    for y in range(height):
        rows.append(0)  # filter type 0
        rows.extend(rgb_bytes[y * stride : (y + 1) * stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(rows), 6)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def capture_screenshot(ser, out_path="screenshot.png"):
    ser.reset_input_buffer()
    ser.write(b"screenshot\n")
    ser.flush()

    header = ser.readline().decode("utf-8", errors="replace").strip()
    if not header.startswith("SCREENSHOT "):
        sys.exit(f"Unexpected screenshot header: {header!r}")
    _, w_s, h_s, n_s = header.split()
    w, h, n = int(w_s), int(h_s), int(n_s)

    raw = bytearray()
    deadline = time.time() + 5.0
    while len(raw) < n and time.time() < deadline:
        chunk = ser.read(n - len(raw))
        if chunk:
            raw.extend(chunk)
    if len(raw) != n:
        sys.exit(f"Incomplete screenshot: got {len(raw)}/{n} bytes")

    # Drain trailing OK line
    time.sleep(0.05)
    while ser.in_waiting:
        ser.readline()

    # M5Canvas 16-bit sprite stores big-endian RGB565 (swap565_t)
    rgb = bytearray(w * h * 3)
    for i in range(w * h):
        p = (raw[2 * i] << 8) | raw[2 * i + 1]
        r = ((p >> 11) & 0x1F) * 255 // 31
        g = ((p >> 5) & 0x3F) * 255 // 63
        b = (p & 0x1F) * 255 // 31
        rgb[3 * i] = r
        rgb[3 * i + 1] = g
        rgb[3 * i + 2] = b

    write_png_rgb(out_path, w, h, rgb)
    print(f"Saved {w}x{h} screenshot to {out_path}")


def send_cmd(ser, cmd, wait=0.25):
    ser.reset_input_buffer()
    ser.write((cmd.strip() + "\n").encode("utf-8"))
    ser.flush()
    deadline = time.time() + wait
    lines = []
    while time.time() < deadline:
        if ser.in_waiting:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if line:
                lines.append(line)
                deadline = min(deadline, time.time() + 0.06)
        else:
            time.sleep(0.01)
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description="Control CardOS from laptop over USB Serial")
    ap.add_argument("-p", "--port", help="Serial port (auto-detected if omitted)")
    ap.add_argument("command", nargs="*", help="Command to send (e.g. status, open notes, screenshot out.png)")
    args = ap.parse_args()

    port = args.port or find_port()
    with serial.Serial(port, 115200, timeout=0.5) as ser:
        if not args.command:
            print(f"Connected to {port}. Interactive mode (Ctrl-C to exit):")
            while True:
                try:
                    line = input("cardos> ")
                except (EOFError, KeyboardInterrupt):
                    break
                if not line.strip():
                    continue
                if line.strip().startswith("screenshot"):
                    parts = line.strip().split()
                    capture_screenshot(ser, parts[1] if len(parts) > 1 else "screenshot.png")
                else:
                    print(send_cmd(ser, line, wait=1.5 if line.startswith("wifi scan") else 0.4))
        elif args.command[0] == "screenshot":
            out = args.command[1] if len(args.command) > 1 else "screenshot.png"
            capture_screenshot(ser, out)
        else:
            cmd = " ".join(args.command)
            wait = 5.0 if cmd.startswith("wifi scan") else 0.5
            resp = send_cmd(ser, cmd, wait=wait)
            print(resp)


if __name__ == "__main__":
    main()
