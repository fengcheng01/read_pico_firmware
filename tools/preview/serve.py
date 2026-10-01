#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""本地浏览器预览与截图导出。/ Local browser preview and screenshot export."""
import argparse
import base64
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
from pathlib import Path
import secrets
import struct
import subprocess
import tempfile
import zlib

from build import ROOT, OUT, build


def png_from_pgm(data):
    """保留原始 16 级灰度。/ Preserve the original sixteen gray levels."""
    header = b"P5\n684 1216\n255\n"
    if not data.startswith(header) or len(data) != len(header) + 684 * 1216:
        raise ValueError("Invalid native framebuffer")
    pixels = data[len(header):]

    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)

    scanlines = b"".join(b"\0" + pixels[y * 684:(y + 1) * 684] for y in range(1216))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 684, 1216, 8, 0, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b""))


class Preview:
    def __init__(self, binary):
        self.directory = tempfile.TemporaryDirectory(prefix="session-", dir=OUT)
        self.frame = Path(self.directory.name) / "frame.pgm"
        self.pages = json.loads((OUT / "pages.json").read_text())
        self.process = subprocess.Popen([str(binary), str(self.frame)], cwd=ROOT,
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        self.sequence = 0
        try:
            self.receive()
        except Exception:
            self.close()
            raise

    def receive(self):
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError("Native renderer stopped; inspect its terminal output")
        self.state = json.loads(line)
        self.sequence += 1
        self.png = png_from_pgm(self.frame.read_bytes())

    def command(self, command):
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        self.receive()
        return self.payload()

    def payload(self):
        return dict(self.state, pages=self.pages, sequence=self.sequence,
                    image="data:image/png;base64," + base64.b64encode(self.png).decode("ascii"))

    def close(self):
        self.process.stdin.close()
        try:
            self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            self.process.wait(timeout=3)
        self.process.stdout.close()
        self.directory.cleanup()


def action_command(action, pages):
    kind = action.get("action")

    def integer(name, low, high):
        value = action.get(name)
        if type(value) is not int or not low <= value <= high:
            raise ValueError(f"Invalid {name}")
        return value

    if kind == "tap":
        return "tap %d %d" % (integer("x", 0, 683), integer("y", 0, 1215))
    if kind == "swipe":
        return "swipe %d %d %d %d" % (integer("x0", 0, 683), integer("y0", 0, 1215),
                                      integer("x1", 0, 683), integer("y1", 0, 1215))
    if kind == "key":
        return "key %d" % integer("key", 0, 2)
    if kind == "page":
        return "page %d" % integer("page", 0, len(pages) - 1)
    if kind == "asset":
        return "asset %d" % integer("asset", 0, 1)
    if kind == "home_fixture":
        return "fixture %d" % integer("fixture", 0, 2)
    if kind in ("menu", "tick"):
        return kind
    raise ValueError("Unknown action")


def export_screens(preview, directory):
    directory.mkdir(parents=True, exist_ok=True)
    indices = {page["symbol"]: index for index, page in enumerate(preview.pages)}
    scenarios = [("menu", "menu"), ("reading", f"page {indices['app_reading']}"),
                 ("reading-next", "tap 300 700"), ("refresh", f"page {indices['app_refresh']}"),
                 ("gray16", "tap 130 1150"), ("font", f"page {indices['app_font_pick']}"),
                 ("loading", "asset 0"), ("lock", "asset 1"),
                 ("home-empty", "fixture 0"), ("home-error", "fixture 2"), ("home-reading", "fixture 1"),
                 ("product-shelf", "tap 220 1140"), ("product-shelf-2", "tap 540 1040"),
                 ("product-reader", "tap 300 400"), ("product-reader-tools", "key 1"),
                 ("product-today", f"page {indices['app_os_today']}"),
                 ("product-settings", f"page {indices['app_os_settings']}"),
                 ("product-diagnostics", f"page {indices['app_os_tools']}")]
    for name, command in scenarios:
        preview.command(command)
        (directory / (name + ".png")).write_bytes(preview.png)
    print(f"Exported {len(scenarios)} native screenshots to {directory}")


def serve(preview, port):
    token = secrets.token_urlsafe(24)

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            pass

        def local_request(self):
            # 拒绝跨站和 DNS 重绑定请求。/ Reject cross-site and DNS-rebinding requests.
            hosts = (f"127.0.0.1:{self.server.server_port}", f"localhost:{self.server.server_port}")
            origin = self.headers.get("Origin")
            if self.headers.get("Host") not in hosts or (origin and origin not in tuple("http://" + host for host in hosts)):
                self.reply(403, b"Loopback preview only", "text/plain")
                return False
            return True

        def reply(self, status, data, content_type):
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Content-Security-Policy", "default-src 'self'; img-src 'self' data:; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'")
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            if not self.local_request():
                return
            if self.path == "/":
                self.reply(200, (ROOT / "tools/preview/index.html").read_bytes(), "text/html; charset=utf-8")
            elif self.path == "/api/state":
                self.reply(200, json.dumps(dict(preview.payload(), token=token)).encode(), "application/json")
            else:
                self.reply(404, b"Not found", "text/plain")

        def do_POST(self):
            if not self.local_request():
                return
            if self.path != "/api/action":
                self.reply(404, b"Not found", "text/plain")
                return
            if self.headers.get("X-Preview-Token") != token:
                self.reply(403, b"Invalid preview session", "text/plain")
                return
            try:
                size = int(self.headers.get("Content-Length", "0"))
                if not 0 < size <= 1024:
                    raise ValueError("Invalid request size")
                action = json.loads(self.rfile.read(size))
                if not isinstance(action, dict):
                    raise ValueError("Expected an action object")
                result = preview.command(action_command(action, preview.pages))
                self.reply(200, json.dumps(result).encode(), "application/json")
            except (ValueError, TypeError) as error:
                self.reply(400, str(error).encode(), "text/plain")
            except (BrokenPipeError, RuntimeError) as error:
                self.reply(500, str(error).encode(), "text/plain")

    server = HTTPServer(("127.0.0.1", port), Handler)
    server.timeout = 30
    print(f"Read Pico UI preview: http://127.0.0.1:{server.server_port}", flush=True)
    print("Real drawing callbacks; physical refresh timing and peripherals are not emulated. Ctrl+C stops.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--export", type=Path, metavar="DIRECTORY")
    parser.add_argument("--no-build", action="store_true", help="Use the already-built host renderer")
    args = parser.parse_args()
    preview = Preview(OUT / "preview" if args.no_build else build())
    try:
        if args.export:
            export_screens(preview, args.export)
        else:
            serve(preview, args.port)
    finally:
        preview.close()
