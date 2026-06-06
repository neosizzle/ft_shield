import socket
import sys
import os
import select
import time

def get_pass() -> str:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        # sock.connect(("192.168.1.123", 5556))
        sock.connect(("127.0.0.1", 5556)) # junhan mac attacker
        pswd = sock.recv(1024).decode("utf-8")
        sock.close()
    return pswd.strip()


HOST = "127.0.0.1"
PORT = 4242

upload_state = 0
download_state = 0

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))
sock.setblocking(False)

buffer = ""

print(f"Password from remote: {get_pass()}")

while True:
    # watch both socket + stdin
    readable, _, _ = select.select([sock], [], [])

    try:
        for r in readable:
            if r == sock:
                message_from_serv = b""
                while True:
                    time.sleep(0.05)
                    try:
                        data = sock.recv(4096)
                        if not data:
                            print("\nConnection closed.")
                            sys.exit(0)
                        message_from_serv += data
                    except BlockingIOError:
                        break

                text = data.decode(errors="replace")

                if download_state < 2:
                    sys.stdout.write(text)
                    sys.stdout.flush()

                line = None

                if upload_state == 0 and download_state == 0:
                    line = sys.stdin.readline().strip()

                # init state
                if line == "upload":
                    upload_state = 1
                    print("input file path :", end="", flush=True)
                    
                    # get file from host
                    upload_file_path = input().strip()

                    # validate file exists
                    if not os.path.isfile(upload_file_path):
                        print("Error: file does not exist")
                        os._exit(0)

                    file_size = os.path.getsize(upload_file_path)
                    sock.sendall((line + "\n").encode())
                    continue

                if line == "download":
                    download_state = 1
                    print("dest file path :", end="", flush=True)
                    download_file_path = input().strip()

                    # check if path is creatable
                    dest_dir = os.path.dirname(download_file_path) or "."
                    if not os.path.isdir(dest_dir):
                        print(f"Error: directory '{dest_dir}' does not exist")
                        os._exit(0)
                    elif not os.access(dest_dir, os.W_OK):
                        print(f"Error: directory '{dest_dir}' is not writable")
                        os._exit(0)

                    else:
                        try:
                            with open(download_file_path, "xb") as f:
                                pass 
                        except FileExistsError:
                            print(f"Warning: file '{download_file_path}' exists and will be overwritten")
                        except OSError as e:
                            print(f"Error: cannot create file '{download_file_path}': {e}")
                            os._exit(0)
                    
                    sock.sendall((line + "\n").encode())
                    continue



                # handle states
                #################
                # Uploading     #
                #################

                if upload_state == 1:
                    print(file_size)
                    sock.sendall((f"{file_size}\n").encode())
                    upload_state = 2
                    continue

                if upload_state == 2:
                    line = sys.stdin.readline().strip()
                    sock.sendall((line + "\n").encode())
                    upload_state = 3
                    continue

                if upload_state == 3:
                    try:
                        with open(upload_file_path, "rb") as f:
                            print("upload in progress...")
                            while True:
                                chunk = f.read(65536)  # 64 KB
                                if not chunk:
                                    break

                                sock.sendall(chunk)

                        sock.sendall(("\n").encode())


                    except OSError as e:
                        print(f"Upload failed: {e}")

                    upload_state = 0
                    continue

                #################
                # Downloading   #
                #################
                if download_state == 1:
                    line = sys.stdin.readline().strip()
                    sock.sendall((line + "\n").encode())
                    download_state = 2
                    continue
                if download_state == 2:
                    download_size = int(message_from_serv.decode(errors="replace").split('\n')[0])
                    sock.sendall(("\n").encode())
                    download_state = 3
                    continue
                if download_state == 3:
                    # substring until content end and print 
                    file_contents = message_from_serv[:download_size]

                    # write file_contents to download_file_path
                    with open(download_file_path, "wb") as f:
                        f.write(file_contents)

                    leftover_msg = message_from_serv[download_size:].decode(errors="replace")
                    print(leftover_msg)
                    sock.sendall(("\n").encode())
                    download_state = 0
                    continue

                sock.sendall((line + "\n").encode())

    except KeyboardInterrupt:
        if upload_state == 0:
            print("")
            sock.sendall("\n".encode())
        