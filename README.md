# ft_shield
This document explain the objective, features design, procedure of the ft_shield trojan.

> Disclaimer: this project is for educational purposes only

## Installation
For MacOS users, `binutils` and `libelf` needs to be installed manually to build this project

TODO: change the IP addresses accordingly

to configure native build files
```
cmake -B build
```

to build

```
cmake --build build --clean-first
```

to run
```
./build/ft_shield/ft_shield
```

to run the payload in foreground
```
./build/ft_shield/packed_payload
```

NOTE: the shellcode for the packer is not considered as build dependency, updates to it will require re-configuration.
```
cmake -S . -B build && cmake --build build
```

## Objective
The objective of this program is to spawn a service which executes a remote shell in the victims machine in the background by exposing a port, allowing for remote attackers to connect to the victims machine and execute the said shell program

## Design
TODO: diagram here

## Features
- packing without compression
- disguising via payload binary placement
- disguising via service name change
- background service
- auth mechanism

## How it works
