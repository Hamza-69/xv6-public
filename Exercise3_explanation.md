# Exercise 3 — Synchronization (Producers / Consumers / Readers)

## Part 2 — Hypothetical execution scenario

Following the format of the assignment's time-dependent example, this
section traces a scenario time-step-by-time-step. Buffer size **N = 2**
(deliberately small so the buffer fills/empties during the trace).

The scenario is engineered to demonstrate the required
**buffer-full case where a reader is made to wait until the buffer
state changes**.

Semaphore values follow the abstract semantics of Lecture 7 slide 32:
`wait(s)` decrements unconditionally, and a negative value `−k` means
*k* processes are blocked on that semaphore.

**Initial state at t = 0:**

```
empty_s = 2     full_s = 0     wrt = 1     order_s = 1     m = 1
rc = 0          count = 0      buffer = [ . , . ]
```

`m` is held only inside the very short `rc++ / rc−−` critical
section while no other party is in that section, so it stays at 1
throughout this scenario and is omitted from the table to save space.

### Scenario assumptions (execution times)

* P, C operations take 1 time unit each (acquire wrt → modify → release).
* R operations take 2 time units (in the reading section).
* Arrivals happen at the start of a time slot.

### Time-dependent trace

The table is laid out exactly like the second example in the
assignment (rows = state, columns = time slot; the column shows the
**resulting** state after that slot's events have executed as far as
they can before blocking).

| time              |  0        |  1        |  2     |  3     |  4         |  5         |  6     |  7        |  8        |
|-------------------|-----------|-----------|--------|--------|------------|------------|--------|-----------|-----------|
| **arrival**       | P1        | P2        | R1, R2 | P3     | C1         | R3         | —      | —         | —         |
| **count**         |  1        |  2        |  2     |  2     |  2         |  2         |  1     |  1        |  1        |
| **buffer state**  | partial   | **FULL**  | FULL   | FULL   | FULL       | FULL       | partial| partial   | partial   |
| **rc**            |  0        |  0        |  2     |  2     |  2         |  2         |  0     |  0        |  1        |
| **empty_s**       |  1        |  0        |  0     |  −1    |  −1        |  −1        |  −1    |  0        |  0        |
| **full_s**        |  1        |  2        |  2     |  2     |  1         |  1         |  1     |  1        |  1        |
| **wrt**           |  1        |  1        |  0     |  0     |  −1        |  −1        |  0     |  1        |  −1       |
| **order_s**       |  1        |  1        |  1     |  1     |  0         |  −1        |  −1    |  0        |  0        |
| **W@empty_s**     |  —        |  —        |  —     | **P3** | P3         | P3         | P3     | —         | —         |
| **W@full_s**      |  —        |  —        |  —     |  —     |  —         |  —         |  —     | —         | —         |
| **W@order_s**     |  —        |  —        |  —     |  —     |  —         | **R3**     | R3     | —         | —         |
| **W@wrt**         |  —        |  —        |  —     |  —     | **C1**     | C1         | —      | —         | **P3**    |
| **operating**     | (P1 done) | (P2 done) | R1, R2 | R1, R2 | R1, R2     | R1, R2     | C1     | (C1 done) | R3        |

### Walk-through of the key steps

**t = 0 — P1 produces.** P1 runs `wait(empty_s)` (2→1),
`wait(order_s)` (1→0), `wait(wrt)` (1→0), `signal(order_s)` (0→1),
enqueues item, `signal(wrt)` (0→1), `signal(full_s)` (0→1). count = 1.

**t = 1 — P2 produces.** Same sequence. After: `empty_s = 0`,
`full_s = 2`, count = 2 → **buffer is now FULL**.

**t = 2 — R1 and R2 enter the reading section.**
R1: `wait(order_s)` (1→0), `wait(m)`, rc=1, `wait(wrt)` (1→0),
`signal(m)`, `signal(order_s)` (0→1).
R2: `wait(order_s)` (1→0), `wait(m)`, rc=2 (no `wait(wrt)` since
rc > 1), `signal(m)`, `signal(order_s)` (0→1). Both reading.

**t = 3 — P3 arrives, blocks on `empty_s`.** P3 runs `wait(empty_s)`
(0 → −1) and is added to the `empty_s` wait queue. P3 is **not** held
back by a reader — it is held back by the buffer being full. Only a
consumer can release P3.

**t = 4 — C1 arrives.** C1 runs `wait(full_s)` (2→1) — succeeds
immediately. `wait(order_s)` (1→0). Then `wait(wrt)` (0 → −1) —
**blocks** because R1 and R2 are reading. C1 is added to the `wrt`
wait queue. **Crucially, C1 still holds `order_s` while blocked on
`wrt`.**

**t = 5 — R3 arrives — and is forced to wait!** R3 runs
`wait(order_s)` (0 → −1) and is added to the `order_s` wait queue.
**This is the buffer-full case in action: a reader is waiting until
the buffer state changes.** R3 cannot enter the reading section
until C1 has consumed an item, even though readers normally have
priority in the mid case. This is exactly what the assignment asks
to demonstrate.

**t = 6 — R1 and R2 finish reading.**
R1: `wait(m)`, rc=1, `signal(m)`. R2: `wait(m)`, rc=0,
`signal(wrt)` (−1 → 0) — this wakes C1; `signal(m)`.

**t = 7 — C1 actually consumes.** Now-unblocked C1 dequeues the item
(count: 2→1), `signal(wrt)` (0→1), `signal(empty_s)` (−1 → 0)
— wakes P3, `signal(order_s)` (−1 → 0) — wakes R3. Buffer is no
longer full. C1 finishes. Note `full_s` is **not** touched by C1 at
t=7 (C1 signals `empty_s`, not `full_s`), so `full_s` stays at 1.

**t = 8 — R3 (woken) enters reading; P3 (woken) blocks behind R3.**
R3 finishes its entry section: `wait(m)`, rc=1, `wait(wrt)` (1→0),
`signal(m)`, `signal(order_s)` (0→1). R3 starts reading. P3 then
proceeds: `wait(order_s)` (1→0), `wait(wrt)` (0 → −1) — **blocks**
waiting for R3 to finish. Symmetric to C1's behaviour earlier.

### What the trace demonstrates

| Requirement from spec | Where it shows up |
|---|---|
| Multiple thread types arriving randomly | t = 0…5: P1, P2, R1, R2, P3, C1, R3 in arbitrary order |
| Reader-priority in mid case | t = 2: R1 and R2 read concurrently (rc = 2) |
| Buffer becomes full | t = 1: count = 2 = N, `empty_s` hits 0 |
| Producer waits when buffer full | t = 3: P3 in W@`empty_s`, **not** waiting on a reader |
| Consumer not held back by readers indefinitely | t = 4–6: C1 waits only for the *currently active* readers, then runs |
| **Reader made to wait until buffer state changes** | t = 5: R3 arrives, is queued in W@`order_s`. R3 cannot proceed until C1 consumes an item at t = 7 — i.e., until the *buffer state changes from full to partial* |
| No busy waiting | every blocking event in the trace is a `sem_wait` on a POSIX semaphore |