# [Bug Type: Race Condition]

## Buggy Code

File: `buggy.cpp`

The program simulates two concurrent withdrawals from a shared account. Each
worker reads the balance, reports that observation to the test harness, then
waits until both workers are allowed to commit. This harness forces the bad
interleaving so the race is reproducible.

Relevant fault:

```cpp
int observed = account.read_balance_for_approval();
...
account.commit_withdrawal(observed, amount);
```

The check and update are not protected by a mutex or atomic read-modify-write.
Both threads approve a 60-unit withdrawal from a 100-unit account.

## Build

```sh
cd race_condition
g++ -std=c++11 -g -O0 -Wall -Wextra -pedantic -pthread buggy.cpp -o race_buggy
g++ -std=c++11 -g -O0 -Wall -Wextra -pedantic -pthread fixed.cpp -o race_fixed
g++ -std=c++11 -g -O1 -Wall -Wextra -pedantic -pthread -fsanitize=thread -fno-omit-frame-pointer buggy.cpp -o race_tsan
```

Build the sanitizer binary separately. ThreadSanitizer and AddressSanitizer are
not normally combined in one executable.

## Reproduce

```sh
./race_buggy
```

Expected faulty output:

```text
observed_balances=100,100
approved_withdrawals=2 final_balance=40
BUG: two withdrawals were approved from a 100-unit account
```

Expected exit status:

```sh
echo $?
2
```

The failure is deterministic because the test harness forces both workers to
read `balance_` before either worker writes it.

## Debugging Strategy

1. Define the invariant: a 100-unit account cannot approve two 60-unit
   withdrawals.
2. Confirm the invariant failure is repeatable: run `./race_buggy` several
   times and observe `approved_withdrawals=2`.
3. Isolate shared state: `approvals` is atomic, so focus on `Account::balance_`.
4. Inspect the critical section: the balance check and write are separate
   operations with no mutex.
5. Use TSan to confirm a data race on `balance_`.
6. Fix by making the account balance invariant protected by one mutex.

Key signals:

- Both threads use the same observed balance of 100
- Final balance reflects one write, but external state shows two approvals
- TSan reports conflicting accesses in `Account::commit_withdrawal`

## Tools & Logs

Use ThreadSanitizer:

```sh
TSAN_OPTIONS=halt_on_error=0 ./race_tsan
```

Representative TSan output:

```text
WARNING: ThreadSanitizer: data race
  Write of size 4 at ... worker(...) ... buggy.cpp:44
  Previous write of size 4 at ... worker(...) ... buggy.cpp:44
  Location is stack of main thread
```

Interpretation:

- Two threads access `balance_` without a happens-before relationship.
- At least one access is a write.
- The balance invariant is not protected by synchronization.

Use `gdb` to inspect the interleaving:

```sh
gdb --args ./race_buggy
(gdb) break Account::commit_withdrawal
(gdb) run
(gdb) info threads
(gdb) thread apply all bt
```

Optional watchpoint:

```text
(gdb) watch account.balance_
```

If your compiler prevents direct access to the private field by name, break in
`worker` and print `observed` after stepping over the approval read. Both
workers will observe `100`.

## Fix

File: `fixed.cpp`

Build and run:

```sh
./race_fixed
```

Expected output:

```text
approved_withdrawals=1 final_balance=40
```

Why it works:

- `Account::withdraw` holds a `std::mutex` for the entire check-update sequence.
- The account balance invariant has one synchronization boundary.
- Only one worker can observe and modify the balance at a time.
- `balance()` also locks, so reads use the same ownership rule as writes.
