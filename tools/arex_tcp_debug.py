#!/usr/bin/env python3
"""Minimal TCP debug client for the AREX PC simulator.

Usage:
    python tools/arex_tcp_debug.py
    python tools/arex_tcp_debug.py 127.0.0.1 7623

Type a command and press Enter to send it. Type quit/exit to close.
"""

from __future__ import annotations

import socket
import sys
import threading


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 7623


def receive_loop(sock: socket.socket, stop_event: threading.Event) -> None:
    while not stop_event.is_set():
        try:
            data = sock.recv(4096)
        except OSError:
            break
        if not data:
            print("\n[APP] Remote closed connection.")
            break
        print(data.decode("utf-8", errors="replace"), end="", flush=True)
    stop_event.set()


def main() -> int:
    host = sys.argv[1] if len(sys.argv) >= 2 else DEFAULT_HOST
    port_text = sys.argv[2] if len(sys.argv) >= 3 else str(DEFAULT_PORT)

    try:
        port = int(port_text)
    except ValueError:
        print(f"[APP] Bad port: {port_text}")
        return 2

    print(f"[APP] Connecting to {host}:{port} ...")
    try:
        sock = socket.create_connection((host, port), timeout=5.0)
    except OSError as exc:
        print(f"[APP] Connect failed: {exc}")
        print("[APP] Please start the simulator first, then run this tool again.")
        return 1

    stop_event = threading.Event()
    reader = threading.Thread(target=receive_loop, args=(sock, stop_event), daemon=True)
    reader.start()

    print("[APP] Connected. Type commands and press Enter. Type quit to exit.")
    try:
        while not stop_event.is_set():
            try:
                line = input()
            except (EOFError, KeyboardInterrupt):
                print()
                break

            if line.strip().lower() in ("quit", "exit"):
                break
            if not line.endswith(("\n", "\r")):
                line += "\r\n"

            try:
                sock.sendall(line.encode("utf-8"))
            except OSError as exc:
                print(f"[APP] Send failed: {exc}")
                break
    finally:
        stop_event.set()
        try:
            sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        sock.close()

    print("[APP] Disconnected.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
