#!/usr/bin/env python3
"""Tiny TCP debug client for the AREX PC simulator."""

from __future__ import annotations

import queue
import socket
import threading
import tkinter as tk
from tkinter import messagebox, scrolledtext, ttk


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 7623


class TcpDebugApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("AREX TCP Debug")
        self.root.geometry("760x480")
        self.root.minsize(520, 320)

        self.sock: socket.socket | None = None
        self.reader: threading.Thread | None = None
        self.stop_event = threading.Event()
        self.rx_queue: queue.Queue[str] = queue.Queue()

        self.host_var = tk.StringVar(value=DEFAULT_HOST)
        self.port_var = tk.StringVar(value=str(DEFAULT_PORT))
        self.send_var = tk.StringVar(value="state")
        self.status_var = tk.StringVar(value="Disconnected")

        self._build_ui()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self.root.after(50, self._drain_rx_queue)

    def _build_ui(self) -> None:
        root = self.root
        root.columnconfigure(0, weight=1)
        root.rowconfigure(1, weight=1)

        top = ttk.Frame(root, padding=(8, 8, 8, 4))
        top.grid(row=0, column=0, sticky="ew")
        top.columnconfigure(1, weight=1)

        ttk.Label(top, text="Host").grid(row=0, column=0, sticky="w")
        ttk.Entry(top, textvariable=self.host_var, width=16).grid(row=0, column=1, sticky="ew", padx=(4, 8))

        ttk.Label(top, text="Port").grid(row=0, column=2, sticky="w")
        ttk.Entry(top, textvariable=self.port_var, width=7).grid(row=0, column=3, padx=(4, 8))

        self.connect_btn = ttk.Button(top, text="Connect", command=self.connect)
        self.connect_btn.grid(row=0, column=4, padx=(0, 4))

        self.disconnect_btn = ttk.Button(top, text="Disconnect", command=self.disconnect, state=tk.DISABLED)
        self.disconnect_btn.grid(row=0, column=5)

        self.log = scrolledtext.ScrolledText(root, wrap=tk.WORD, state=tk.DISABLED)
        self.log.grid(row=1, column=0, sticky="nsew", padx=8, pady=4)

        bottom = ttk.Frame(root, padding=(8, 4, 8, 4))
        bottom.grid(row=2, column=0, sticky="ew")
        bottom.columnconfigure(0, weight=1)

        send_entry = ttk.Entry(bottom, textvariable=self.send_var)
        send_entry.grid(row=0, column=0, sticky="ew", padx=(0, 8))
        send_entry.bind("<Return>", lambda _event: self.send_line())

        self.send_btn = ttk.Button(bottom, text="Send", command=self.send_line, state=tk.DISABLED)
        self.send_btn.grid(row=0, column=1)

        quick = ttk.Frame(root, padding=(8, 0, 8, 4))
        quick.grid(row=3, column=0, sticky="ew")
        for i, cmd in enumerate(("help", "state", "12.3", "depth 0", "manual on", "auto on")):
            ttk.Button(quick, text=cmd, command=lambda text=cmd: self._quick_send(text)).grid(
                row=0, column=i, padx=(0, 4)
            )

        status = ttk.Label(root, textvariable=self.status_var, anchor="w", padding=(8, 2))
        status.grid(row=4, column=0, sticky="ew")

    def connect(self) -> None:
        if self.sock is not None:
            return

        host = self.host_var.get().strip() or DEFAULT_HOST
        try:
            port = int(self.port_var.get().strip())
        except ValueError:
            messagebox.showerror("Bad port", "Port must be a number.")
            return

        try:
            sock = socket.create_connection((host, port), timeout=3.0)
        except OSError as exc:
            self._append_log(f"[APP] Connect failed: {exc}\n")
            self.status_var.set("Disconnected")
            return

        sock.settimeout(0.2)
        self.sock = sock
        self.stop_event.clear()
        self.reader = threading.Thread(target=self._read_loop, daemon=True)
        self.reader.start()
        self._set_connected(True)
        self._append_log(f"[APP] Connected to {host}:{port}\n")

    def disconnect(self) -> None:
        self.stop_event.set()
        sock = self.sock
        self.sock = None
        if sock is not None:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                sock.close()
            except OSError:
                pass
        self._set_connected(False)

    def send_line(self) -> None:
        text = self.send_var.get()
        if not text.strip():
            return
        sock = self.sock
        if sock is None:
            self._append_log("[APP] Not connected.\n")
            return
        if not text.endswith(("\n", "\r")):
            text += "\r\n"
        try:
            sock.sendall(text.encode("utf-8"))
            self._append_log(f"> {text}")
        except OSError as exc:
            self._append_log(f"[APP] Send failed: {exc}\n")
            self.disconnect()

    def _quick_send(self, text: str) -> None:
        self.send_var.set(text)
        self.send_line()

    def _read_loop(self) -> None:
        sock = self.sock
        while not self.stop_event.is_set():
            if sock is None:
                break
            try:
                data = sock.recv(4096)
            except socket.timeout:
                continue
            except OSError as exc:
                if not self.stop_event.is_set():
                    self.rx_queue.put(f"[APP] Receive failed: {exc}\n")
                break
            if not data:
                self.rx_queue.put("[APP] Remote closed connection.\n")
                break
            self.rx_queue.put(data.decode("utf-8", errors="replace"))
        if self.sock is sock:
            self.sock = None
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass
        self.root.after(0, self._set_connected, False)

    def _drain_rx_queue(self) -> None:
        while True:
            try:
                text = self.rx_queue.get_nowait()
            except queue.Empty:
                break
            self._append_log(text)
        self.root.after(50, self._drain_rx_queue)

    def _append_log(self, text: str) -> None:
        self.log.configure(state=tk.NORMAL)
        self.log.insert(tk.END, text)
        self.log.see(tk.END)
        self.log.configure(state=tk.DISABLED)

    def _set_connected(self, connected: bool) -> None:
        if connected:
            self.status_var.set("Connected")
            self.connect_btn.configure(state=tk.DISABLED)
            self.disconnect_btn.configure(state=tk.NORMAL)
            self.send_btn.configure(state=tk.NORMAL)
        else:
            self.status_var.set("Disconnected")
            self.connect_btn.configure(state=tk.NORMAL)
            self.disconnect_btn.configure(state=tk.DISABLED)
            self.send_btn.configure(state=tk.DISABLED)

    def _on_close(self) -> None:
        self.disconnect()
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    app = TcpDebugApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
