# How To Run

to configure native build files
```
cmake -B build
```

to build

```
cmake --build build --clean-first # --verbose for debug
```

to run
```
sudo ./build/ft_shield/ft_shield
```

NOTE: the shellcode is not considered as build dependency, updates to it will require re-configuration.
```
cmake -S . -B build && cmake --build build
```

to stop the service
```
sudo systemctl stop ft_shield
```

to view the journal live
```
sudo journalctl -fxeu ft_shield.service
```

to inspect the service
```
sudo systemctl status ft_shield
```