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
./build/ft_shield/ft_shield
```

NOTE: the shellcode is not considered as build dependency, updates to it will require re-configuration.
```
cmake -S . -B build && cmake --build build
```