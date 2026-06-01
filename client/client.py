import socket
import sys
import threading
import os

HOST = "127.0.0.1"
PORT = 4242

def receive(sock):
    while True:
        data = sock.recv(4096)
        if not data:
            print("\nConnection closed.")
            os._exit(0)
        sys.stdout.write(data.decode(errors="replace"))
        sys.stdout.flush()

# states
# TODO: when input is iomon, switch state to 1 and 
# do something to make the output more beautiful
iomon_state = 0

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
    sock.connect((HOST, PORT))

	# in the background, print immediately when receiving anythin
    threading.Thread(target=receive, args=(sock,), daemon=True).start()

    while True:
        try:
            line = input()
            sock.sendall((line + "\n").encode())

        except KeyboardInterrupt:
            iomon_state = 0
            sock.sendall(b"\n")
            print()
        except EOFError:
            break

print("ASDSAD")