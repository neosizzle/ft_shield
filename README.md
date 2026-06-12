# ft_shield
This document explain the objective, features design, procedure of the ft_shield trojan.

> Disclaimer: this project is for educational purposes only

## Installation
For MacOS users, `binutils` and `libelf` needs to be installed manually to build this project

In the [key generation file](./ft_shield/payload/key/key.c), change the IP of CLOUD_IP to the public IP address of the password server

In the [client](./ft_shield/client/client.py), change the valeus of HOST, PORT to the victims public IP address and exposed port and the IP address in get_pass() to the public IP address of the password server. This server should be the same server accessed by the IP address in CLOUD_IP above

[DEVELOP.md](./DEVELOP.md)

## Objective
The objective of this program is to spawn a service which executes a remote shell in the victims machine in the background by exposing a port, allowing for remote attackers to connect to the victims machine and execute the said shell program

## Design
![](./resources/Screenshot%202026-06-08%20at%2011.56.11 AM.png)

## Features
- packing without compression
- disguising via payload binary placement
- disguising via service name change
- background service
- auth mechanism

## How it works
1. User runs trojan and copies a malicious binary that exposes a port for remote connection which will be running /bin/sh upon connection in the victims machine. The binary will run in the background as a service while the program also runs a legitimate program as a red herring.
2. The background binary sends a randomly generated password to the attackers remote server accesible by the network
3. The attacker queries the password on their local machine which will be used to connect to the victims background server program
4. Using the password, the attacker is able to authenticate their identidy and access the victims machine using the servers shell program.

## [The Injector](./ft_shield/main.c)
- Copies the payload binary stored within itself from the shellcode injection to `/var/mail` as it requires sudo access and isn't checked frequently. This location is usually not in the $PATH environment variable for most users, so it evades autocomplete detection.

- Creates the service for the payload to run on startup by writing a new `sysctl` config for linux or `launctl` config for MacOS. The service will redirect all logs to the systems log file for now, but to acheive full log stealth, any output should be redirected to `/dev/null`. The service created will also be made so that the payload runs on startup every time for persistency. There will be no duplicate services for a single machine

- Runs the ["legit"](./ft_shield/legit/legit.c) part of the trojan.

## [The Payload](./ft_shield/payload/)
- The payload binary itself is encrypted via our non compressing packer which will make the payload be more resistant towards static analysis.

- When ran will rerun itself with a different name via `execv`, this obfuscates itself when the machine is inspected using tools like `ps aux` or `htop`. The name that we have chosen is `sd_pam` which is a helper created by systemd and has low chances of duplication. The service itself was intentionally not obfuscated for the purpose of this project. 

- The payload will [generate a random password](./ft_shield/payload/key/key.c) by using /dev/random as a seed and rotate through a character set of size 81 before exfiltrating it to a [*cloud server*](./local_cloud.py).

- Spins up the [server](./ft_shield/payload/server/server.c) exposing port `4242`. Up to 3 clients can connect to the server at any given time.

- Clients can run several commands such as I/O Monitoring, a virtual shell (/bin/sh process wtihout true TTY session), and bidirectional file transfering.
