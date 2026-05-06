# [Bug Type: Memory Leak]

## Buggy Code

File: `buggy.cpp`

The program simulates a receiver that allocates a 4096-byte frame buffer before
validating an incoming message. Invalid frames return early. The accepted frames
are owned and deleted by `Receiver`, but rejected frames are leaked.

Relevant fault:

```cpp
Frame* frame = new Frame(id, 4096);

if (checksum_of(payload) != expected) {
    return false;
}
```

The early return loses the only pointer to `frame`.

## Build

```sh
cd memory_leak
g++ -std=c++11 -g -O0 -Wall -Wextra -pedantic buggy.cpp -o leak_buggy
g++ -std=c++11 -g -O0 -Wall -Wextra -pedantic fixed.cpp -o leak_fixed
g++ -std=c++11 -g -O0 -Wall -Wextra -pedantic -fsanitize=address -fno-omit-frame-pointer buggy.cpp -o leak_asan
```

## Reproduce

Run the buggy receiver with 2000 invalid frames:

```sh
./leak_buggy 2000
```

Expected output:

```text
accepted=0 rejected=2000
```

Faulty behavior: the program appears successful, but it leaks every rejected
frame allocation.

## Debugging Strategy

1. Hypothesis: memory growth is tied to rejected frames because accepted frames
   are stored and deleted by `Receiver`.
2. Reduce the input size: run `./leak_buggy 1`, then `10`, then `2000`. A leak
   proportional to the count indicates per-frame ownership loss.
3. Inspect ownership transitions in `Receiver::ingest`.
4. Mark every exit path after `new Frame`: successful path pushes the pointer
   into `accepted_`; validation failures return without `delete`.
5. Use Valgrind or LeakSanitizer to confirm direct leaks of `Frame` objects and
   indirect leaks of their `payload` buffers.
6. Fix ownership by making the local allocation RAII-managed before validation.

Key signals:

- Leak count scales with rejected input count
- `accepted_` stays at 0, so its destructor cannot free rejected frames
- Leak report points to `Receiver::ingest`

## Tools & Logs

Use Valgrind Memcheck:

```sh
valgrind --leak-check=full --show-leak-kinds=all ./leak_buggy 2000
```

Representative Valgrind output:

```text
HEAP SUMMARY:
    in use at exit: 8,272,000 bytes in 4,000 blocks

80,000 bytes in 2,000 blocks are definitely lost
   at ... operator new(unsigned long)
   by ... Receiver::ingest ... buggy.cpp:48

8,192,000 bytes in 2,000 blocks are indirectly lost
   at ... operator new[](unsigned long)
   by ... Frame::Frame ... buggy.cpp:10
```

Interpretation:

- `definitely lost` is the leaked `Frame` object.
- `indirectly lost` is the `payload` owned by each leaked `Frame`.
- The allocation stack identifies where ownership was created.

Use AddressSanitizer LeakSanitizer on Linux:

```sh
ASAN_OPTIONS=detect_leaks=1 ./leak_asan 2000
```

Representative LeakSanitizer output:

```text
ERROR: LeakSanitizer: detected memory leaks
Direct leak of 80000 byte(s) in 2000 object(s)
    #1 ... Receiver::ingest ... buggy.cpp:48
Indirect leak of 8192000 byte(s) in 2000 object(s)
    #1 ... Frame::Frame ... buggy.cpp:10
```

Use `gdb` to inspect ownership:

```sh
gdb --args ./leak_buggy 1
(gdb) break Receiver::ingest
(gdb) run
(gdb) next
(gdb) print frame
(gdb) continue
```

Procedural check: after stepping through the checksum failure, there is no
`delete frame` and no transfer into `accepted_`.

## Fix

File: `fixed.cpp`

Build and validate:

```sh
./leak_fixed 2000
valgrind --leak-check=full --show-leak-kinds=all ./leak_fixed 2000
```

Expected Valgrind summary:

```text
All heap blocks were freed -- no leaks are possible
```

Why it works:

- `std::unique_ptr<Frame>` owns the frame immediately after allocation.
- Every early return destroys the local `unique_ptr` and frees the frame.
- Accepted frames transfer ownership into `std::vector<std::unique_ptr<Frame> >`.
- Manual cleanup loops are no longer needed.
