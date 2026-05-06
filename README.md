# C++11 Debugging Training Lab

This lab contains three self-contained mini-projects for practicing real-world
C++11 debugging:

1. [Segmentation Fault](segmentation_fault/README.md)
2. [Memory Leak](memory_leak/README.md)
3. [Race Condition](race_condition/README.md)

Each directory contains:

- `buggy.cpp`: minimal realistic failing program
- `fixed.cpp`: corrected implementation
- `README.md`: build, reproduce, debug, tool output, and fix notes

The examples are Linux-oriented and use only C++11. They should also compile on
most Unix-like systems with a recent GCC or Clang. Valgrind is Linux-first;
AddressSanitizer and ThreadSanitizer support depends on your compiler/runtime.

Recommended baseline packages on Ubuntu/Debian:

```sh
sudo apt-get update
sudo apt-get install -y build-essential gdb valgrind
```

Recommended workflow:

1. Build the plain debug binary with `-g -O0`.
2. Reproduce the failure without tools.
3. Form one narrow hypothesis from the observed symptom.
4. Use `gdb`, Valgrind, ASan, or TSan to collect a signal.
5. Confirm the exact ownership, pointer, or synchronization error.
6. Build and run the fixed version.
