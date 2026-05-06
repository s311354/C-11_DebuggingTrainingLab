# [Bug Type: Segmentation Fault]

## Buggy Code

File: `buggy.cpp`

The program simulates a telemetry router. Known sensors are mapped to delivery
routes. The crash occurs when a packet arrives from an unknown source and the
dispatcher dereferences a null `Route*`.

Relevant fault:

```cpp
const Route* route = find_route(routes, packet.source_id);
std::cout << "dispatching " << packet.source_id << " through "
          << route->queue_name << "\n";
```

`find_route` can return `NULL`, but `dispatch_packet` assumes it always returns
a valid route.

## Build

```sh
cd segmentation_fault
g++ -std=c++11 -g -O0 -Wall -Wextra -pedantic buggy.cpp -o segfault_buggy
g++ -std=c++11 -g -O0 -Wall -Wextra -pedantic fixed.cpp -o segfault_fixed
g++ -std=c++11 -g -O0 -Wall -Wextra -pedantic -fsanitize=address -fno-omit-frame-pointer buggy.cpp -o segfault_asan
```

## Reproduce

Crash with the default unknown sensor:

```sh
./segfault_buggy
```

Expected behavior:

```text
Segmentation fault (core dumped)
```

Show that the program works for a known sensor:

```sh
./segfault_buggy pump-A 95
```

Expected successful output:

```text
dispatching pump-A through telemetry/pumps
alarm threshold exceeded
delivered pump-A=95 to telemetry/pumps
```

## Debugging Strategy

1. Hypothesis: the failure depends on input source ID because `pump-A` works and
   the default `heater-X` crashes.
2. Isolate the branch: run with `pump-A`, `valve-B`, `fan-C`, then `heater-X`.
   The crash follows unknown source IDs.
3. Inspect the lookup contract: `find_route` returns `NULL` when the map lookup
   misses.
4. Break at `dispatch_packet` in `gdb`, step over `find_route`, and print
   `route`.
5. Confirm the bad dereference happens before any route validity check.
6. Fix the boundary: handle unknown routes where the nullable pointer is first
   consumed.

Key signals:

- `route == 0x0`
- Crash line is the first `route->...` access
- Known IDs do not crash

## Tools & Logs

Use `gdb`:

```sh
gdb --args ./segfault_buggy heater-X 91
(gdb) run
(gdb) bt
(gdb) frame 0
(gdb) print packet.source_id
(gdb) print route
```

Representative `gdb` output:

```text
Program received signal SIGSEGV, Segmentation fault.
dispatch_packet (...) at buggy.cpp:52
52                << route->queue_name << "\n";
(gdb) print packet.source_id
$1 = "heater-X"
(gdb) print route
$2 = (const Route *) 0x0
```

Use AddressSanitizer:

```sh
./segfault_asan heater-X 91
```

Representative ASan signal:

```text
AddressSanitizer: SEGV on unknown address 0x000000000008
    #0 ... in dispatch_packet ... buggy.cpp:52
    #1 ... in main ... buggy.cpp:63
```

Interpretation: ASan is reporting a read through a near-zero address, which is
typical of dereferencing a null pointer plus a field offset.

Use Valgrind:

```sh
valgrind --tool=memcheck --track-origins=yes ./segfault_buggy heater-X 91
```

Representative Valgrind signal:

```text
Invalid read of size 8
   at ... dispatch_packet ... buggy.cpp:52
 Address 0x8 is not stack'd, malloc'd or free'd
```

Interpretation: `0x8` is not a real object address. It is a null pointer plus an
offset into `Route`.

## Fix

File: `fixed.cpp`

Build and run:

```sh
./segfault_fixed heater-X 91
```

Expected output:

```text
dropping packet from unknown source: heater-X
```

Why it works:

- `dispatch_packet` checks `route == NULL` immediately after lookup.
- Unknown input is converted into a controlled failure path.
- Valid input still follows the normal dispatch path.
- The nullable pointer is never dereferenced without validation.
