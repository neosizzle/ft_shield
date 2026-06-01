"""
Requires: pip install textual rich
"""

from __future__ import annotations

import socket
import sys
import json
import time
import threading
import os
from collections import deque
from datetime import datetime
from typing import Callable, Optional

from rich import box
from rich.panel import Panel
from rich.table import Table
from rich.text import Text

from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.widgets import Footer, Header, Static
from textual.containers import ScrollableContainer, Vertical


# ─────────────────────────────────────────────
# Constants & globals
# ─────────────────────────────────────────────

SPARKCHARS  = " ▁▂▃▄▅▆▇█"
BAR_WIDTH   = 24
HISTORY_LEN = 60

_history: dict[str, deque] = {}


# ─────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────

def _hist(key: str) -> deque:
    if key not in _history:
        _history[key] = deque(maxlen=HISTORY_LEN)
    return _history[key]


def _human(n: float) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return f"{n:6.1f} {unit}/s"
        n /= 1024
    return f"{n:6.1f} TB/s"


def _human_bytes(n: float) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return f"{n:6.1f} {unit}"
        n /= 1024
    return f"{n:6.1f} TB"


def _spark(values: list[float]) -> str:
    if not values:
        return ""
    max_v = max(values) or 1
    return "".join(SPARKCHARS[int(v / max_v * 8)] for v in values)


def _dual_bar(read: float, write: float, max_val: float,
              width: int = BAR_WIDTH) -> Text:
    half = width // 2
    r = int((read  / max_val) * half) if max_val > 0 else 0
    w = int((write / max_val) * half) if max_val > 0 else 0
    r, w = min(r, half), min(w, half)
    t = Text()
    t.append("█" * r + "░" * (half - r), style="green")
    t.append(" │ ", style="dim")
    t.append("█" * w + "░" * (half - w), style="red")
    return t


# ─────────────────────────────────────────────
# History recording
# ─────────────────────────────────────────────

def record(data: dict) -> None:
    for iface, d in data.get("network", {}).items():
        _hist(f"net_rx_{iface}").append(d["rx_bytes_sec"] / 1024)
        _hist(f"net_tx_{iface}").append(d["tx_bytes_sec"] / 1024)
    for dev, d in data.get("disk", {}).items():
        _hist(f"disk_r_{dev}").append(d["read_bytes_sec"]  / 1024)
        _hist(f"disk_w_{dev}").append(d["write_bytes_sec"] / 1024)


# ─────────────────────────────────────────────
# Panel builders  (return rich renderables)
# ─────────────────────────────────────────────

def _build_network(net: dict) -> Panel:
    table = Table(box=box.SIMPLE, show_header=True, header_style="bold cyan",
                  expand=True, pad_edge=False)
    table.add_column("Interface", style="bold", width=12)
    table.add_column("RX",        justify="right", width=13)
    table.add_column("TX",        justify="right", width=13)
    table.add_column("RX ▸ TX  (sparkline / 60s)", no_wrap=True)

    if not net:
        table.add_row("—", "—", "—", "[dim]no data[/dim]")
    else:
        for iface, d in net.items():
            rx_h = list(_hist(f"net_rx_{iface}"))
            tx_h = list(_hist(f"net_tx_{iface}"))
            spark = Text(_spark(rx_h), style="cyan") + Text(" ▸ ") + Text(_spark(tx_h), style="yellow")
            table.add_row(iface, _human(d["rx_bytes_sec"]), _human(d["tx_bytes_sec"]), spark)

    return Panel(table, title="[bold cyan]● NETWORK[/bold cyan]",
                 border_style="cyan", padding=(0, 1))


def _build_disk(disk: dict) -> Panel:
    table = Table(box=box.SIMPLE, show_header=True, header_style="bold green",
                  expand=True, pad_edge=False)
    table.add_column("Device", style="bold", width=12)
    table.add_column("Read",   justify="right", width=13)
    table.add_column("Write",  justify="right", width=13)
    table.add_column("Read ░░░░ │ ░░░░ Write", no_wrap=True)

    if not disk:
        table.add_row("—", "—", "—", "[dim]no data[/dim]")
    else:
        max_val = max(max(d["read_bytes_sec"], d["write_bytes_sec"]) for d in disk.values()) / 1024 or 1
        for dev, d in disk.items():
            table.add_row(
                dev,
                _human(d["read_bytes_sec"]),
                _human(d["write_bytes_sec"]),
                _dual_bar(d["read_bytes_sec"] / 1024, d["write_bytes_sec"] / 1024, max_val),
            )

    return Panel(table, title="[bold green]● DISK[/bold green]",
                 border_style="green", padding=(0, 1))


def _build_processes(procs: list[dict], top_n: int = 12) -> Panel:
    active = [p for p in procs if p["total_bytes_sec"] > 0][:top_n]
    fallback = not active
    if fallback:
        active = sorted(procs,
                        key=lambda p: p.get("total_rchar", 0) + p.get("total_wchar", 0),
                        reverse=True)[:top_n]

    title_suffix = "  [dim](cumulative — idle)[/dim]" if fallback else ""

    table = Table(box=box.SIMPLE, show_header=True, header_style="bold magenta",
                  expand=True, pad_edge=False)
    table.add_column("PID",   justify="right", width=7)
    table.add_column("Name",  style="bold",    width=16)
    table.add_column("Read",  justify="right", width=13)
    table.add_column("Write", justify="right", width=13)
    table.add_column("Read ░░░░ │ ░░░░ Write", no_wrap=True)

    if not active:
        table.add_row("—", "[dim]idle[/dim]", "—", "—", "[dim]no activity[/dim]")
    elif fallback:
        max_val = max(p.get("total_rchar", 0) + p.get("total_wchar", 0) for p in active) or 1
        for p in active:
            r, w = p.get("total_rchar", 0), p.get("total_wchar", 0)
            table.add_row(str(p["pid"]), p["name"][:15], _human_bytes(r), _human_bytes(w),
                          _dual_bar(r / 1024, w / 1024, max_val / 1024))
    else:
        max_val = max(p["total_bytes_sec"] for p in active) / 1024 or 1
        for p in active:
            r_kb = p["read_bytes_sec"]  / 1024
            w_kb = p["write_bytes_sec"] / 1024
            table.add_row(str(p["pid"]), p["name"][:15],
                          _human(p["read_bytes_sec"]), _human(p["write_bytes_sec"]),
                          _dual_bar(r_kb, w_kb, max_val))

    return Panel(table,
                 title=f"[bold magenta]● TOP PROCESSES[/bold magenta]{title_suffix}",
                 border_style="magenta", padding=(0, 1))


def _build_open_files(files: list[dict]) -> Panel:
    seen: set = set()
    deduped = []
    for f in files:
        key = (f["pid"], f["path"])
        if key not in seen:
            seen.add(key)
            deduped.append(f)

    table = Table(box=box.SIMPLE, show_header=True, header_style="bold yellow",
                  expand=True, pad_edge=False)
    table.add_column("PID",     justify="right", width=7)
    table.add_column("Process", style="bold",    width=14)
    table.add_column("Path")

    if not deduped:
        table.add_row("—", "[dim]—[/dim]", "[dim]no interesting files open[/dim]")
    else:
        for entry in deduped:
            table.add_row(str(entry["pid"]), entry["name"][:13], entry["path"])

    return Panel(table,
                 title=f"[bold yellow]● OPEN FILES[/bold yellow]  [dim]({len(deduped)} total)[/dim]",
                 border_style="yellow", padding=(0, 1))


# ─────────────────────────────────────────────
# Textual widget
# ─────────────────────────────────────────────

class RichPanel(Static):
    DEFAULT_CSS = """
    RichPanel {
        height: auto;
        margin: 0 0 1 0;
    }
    """


# ─────────────────────────────────────────────
# Textual app
# ─────────────────────────────────────────────

class IOMonitorApp(App):
    """Scrollable live I/O monitor."""

    CSS = """
    Screen {
        background: #0d0d0d;
    }
    Vertical {
        height: auto;
    }
    #scroll_main {
        height: 1fr;
        scrollbar-color: #444444;
        scrollbar-background: #1a1a1a;
    }
    Header {
        background: #8b0000;
        color: white;
        text-style: bold;
    }
    Footer {
        background: #1a1a1a;
        color: #666666;
    }
    """

    BINDINGS = [
        Binding("q",      "quit",          "Quit"),
        Binding("ctrl+c", "quit",          "Quit"),
        Binding("p",      "pause",         "Pause/Resume"),
        Binding("r",      "reset_history", "Reset history"),
    ]

    TITLE = "VM I/O MONITOR"

    def __init__(self, fetch_fn: Callable, interval: float = 2.0, **kwargs):
        super().__init__(**kwargs)
        self.fetch_fn   = fetch_fn
        self.interval   = interval
        self.sample     = 0
        self.paused     = False
        self._data: dict = {}
        self._lock      = threading.Lock()
        self._stop_evt  = threading.Event()

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        with ScrollableContainer(id="scroll_main"):
            with Vertical():
                yield RichPanel(id="panel_network")
                yield RichPanel(id="panel_disk")
                yield RichPanel(id="panel_processes")
                yield RichPanel(id="panel_files")
        yield Footer()

    def on_mount(self) -> None:
        self._refresh_ui()
        self._fetch_thread = threading.Thread(target=self._fetch_loop, daemon=True)
        self._fetch_thread.start()
        self.set_interval(self.interval, self._refresh_ui)

    def _fetch_loop(self) -> None:
        while not self._stop_evt.is_set():
            if not self.paused:
                try:
                    result = self.fetch_fn()
                    if result:
                        record(result)
                        with self._lock:
                            self._data = result
                            self.sample += 1
                except Exception:
                    pass
            self._stop_evt.wait(timeout=self.interval)

    def _refresh_ui(self) -> None:
        with self._lock:
            data  = dict(self._data)
            sample = self.sample

        self.sub_title = f"sample #{sample}  •  {'PAUSED' if self.paused else 'LIVE'}"
        self.query_one("#panel_network",    RichPanel).update(_build_network(data.get("network",    {})))
        self.query_one("#panel_disk",       RichPanel).update(_build_disk(data.get("disk",          {})))
        self.query_one("#panel_processes",  RichPanel).update(_build_processes(data.get("processes",[])))
        self.query_one("#panel_files",      RichPanel).update(_build_open_files(data.get("open_files", [])))

    def action_pause(self) -> None:
        self.paused = not self.paused

    def action_reset_history(self) -> None:
        _history.clear()

    def on_unmount(self) -> None:
        # signal the fetch thread to stop before we return control
        self._stop_evt.set()


# ─────────────────────────────────────────────
# Socket helpers
# ─────────────────────────────────────────────

def recv_all(sock: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("socket closed")
        buf += chunk
    return buf


def fetch(sock: socket.socket) -> dict:
    raw_len = recv_all(sock, 4)
    length  = int.from_bytes(raw_len, "big")
    raw     = recv_all(sock, length)
    return json.loads(raw) if raw else {}


# ─────────────────────────────────────────────
# Entry points
# ─────────────────────────────────────────────

def run_live(fetch_fn: Callable, interval: float = 2.0) -> None:
    """
    Launch the Textual TUI.

    Reopens /dev/tty so Textual gets a clean terminal handle regardless
    of what state pycli.py's input() loop left stdin/stderr in.
    Restores everything on exit so the menu keeps working.
    """
    # Textual's LinuxDriver uses sys.__stdin__.fileno() for input
    # and sys.__stderr__ for output. After Python's input() the terminal
    # may be in a state that prevents Textual from taking raw control.
    # Reopening /dev/tty gives it a fresh handle.
    tty_r      = open("/dev/tty", "rb", buffering=0)
    tty_w_text = open("/dev/tty", "w",  buffering=1)
 
    saved = {
        "__stdin__":  sys.__stdin__,
        "__stdout__": sys.__stdout__,
        "__stderr__": sys.__stderr__,
        "stdin":      sys.stdin,
        "stdout":     sys.stdout,
        "stderr":     sys.stderr,
    }
 
    sys.__stdin__  = tty_r        # type: ignore[assignment]
    sys.__stderr__ = tty_w_text   # type: ignore[assignment]
    sys.__stdout__ = tty_w_text   # type: ignore[assignment]
    sys.stdin      = tty_r        # type: ignore[assignment]
    sys.stderr     = tty_w_text
    sys.stdout     = tty_w_text
 
    try:
        IOMonitorApp(fetch_fn=fetch_fn, interval=interval).run()
    finally:
        sys.__stdin__  = saved["__stdin__"]
        sys.__stdout__ = saved["__stdout__"]
        sys.__stderr__ = saved["__stderr__"]
        sys.stdin      = saved["stdin"]
        sys.stdout     = saved["stdout"]
        sys.stderr     = saved["stderr"]
        tty_r.close()
        tty_w_text.close()


def render_once(data: dict) -> None:
    """One-shot rich print for the 'o' snapshot command — no Textual needed."""
    from rich.console import Console
    c = Console()
    record(data)
    c.print(_build_network(data.get("network",    {})))
    c.print(_build_disk(data.get("disk",          {})))
    c.print(_build_processes(data.get("processes",[])))
    c.print(_build_open_files(data.get("open_files", [])))


def stream_response(sock: socket.socket, buffer_size: int = 4096) -> None:
    while True:
        chunk = sock.recv(buffer_size)
        if not chunk:
            break
        sys.stdout.buffer.write(chunk)
        sys.stdout.buffer.flush()


def main():
    if len(sys.argv) < 4:
        print("Usage: python pycli.py <host> <port> <password>")
        sys.exit(1)

    host     = sys.argv[1]
    port     = int(sys.argv[2])
    password = sys.argv[3]

    menu_text = (
        "\n"
        "Enter a command:\n"
        "    d - Display I/O metrics\n"
        "    o - Display snapshot of I/O metrics\n"
        "    q - Quit\n"
    )

    def show_menu() -> None:
        sys.stdout.write(menu_text)
        sys.stdout.flush()

    def redraw_prompt() -> None:
        sys.stdout.write("\x1b[1A\r\x1b[2K")
        sys.stdout.flush()

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client:
            client.settimeout(5.0)
            client.connect((host, port))
            print(f"Connected to {host}:{port}")
            client.sendall(password.encode("utf-8"))
            client.settimeout(None)

            show_menu()
            while True:
                inp = input("> ").strip().lower()
                if not inp:
                    redraw_prompt()
                    continue
                if inp == 'd':
                    run_live(lambda: fetch(client), interval=2.0)
                    redraw_prompt()
                elif inp == 'o':
                    render_once(fetch(client))
                    show_menu()
                elif inp == 'q':
                    print("Disconnecting")
                    break
                else:
                    redraw_prompt()
                    continue

    except socket.timeout:
        print(f"Connection to {host}:{port} timed out")
    except ConnectionRefusedError:
        print(f"Server refused connection on {host}:{port}")
    except KeyboardInterrupt:
        print("")
    except Exception as e:
        print(f"Error: {e}")


if __name__ == "__main__":
    main()