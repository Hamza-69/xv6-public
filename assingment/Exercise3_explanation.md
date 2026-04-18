# Exercise 3 — Synchronization (Producers / Consumers / Readers)

## Part 1 — Code summary

The implementation in `Exercise3.c` is built from two patterns that come
directly out of Lecture 7:

| Concern | Pattern from slides | Where in code |
|---|---|---|
| Producer / Consumer on bounded buffer | Slide 41, semaphores `f`, `e`, `m` | `empty_s`, `full_s`, `wrt` |
| Reader / Writer | Slide 47 — Solution 3, no-starvation via `order` turnstile | `order_s`, `m`, `wrt`, `rc` |

Producers and consumers play the role of "writers" since both mutate
the buffer. Solution 3 (slide 47) was chosen over Solution 1
(slide 45) because Solution 1 lets a continuous arrival of readers
starve writers indefinitely; the `order` turnstile in Solution 3 makes
readers and writers queue in arrival order, while still allowing
multiple readers to read concurrently when no writer is queued
(effective reader-priority in the mid case).

**Thread mix:** the spawn pool contains balanced numbers of producers
and consumers (4 each — so the buffer arithmetic is sound: 16 produces
== 16 consumes) and a *majority* of readers (9), satisfying the spec's
"higher likeliness for readers". The spawn order is shuffled randomly
(Fisher–Yates) so the runtime interleaving is non-deterministic.

The two edge-case requirements from the spec are met structurally:

* *Buffer full → consumer must not wait by readers.* When the buffer
  is full, the consumer's `wait(full_s)` succeeds at once; it then
  takes `order_s` and blocks on `wrt`, **holding `order_s` while it
  waits**. Any reader that arrives now must `wait(order_s)` and is
  forced to wait — the consumer cannot be overtaken by a stream of
  new readers.
* *Buffer empty → producer must not wait by readers.* Symmetric.

**No busy waiting** — every blocking operation is a `sem_wait` on a
POSIX semaphore.

---

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

### Scenario narrative (assumptions for execution times)

* P, C operations take 1 time unit each (acquire wrt → modify → release).
* R operations take 2 time units (in the reading section).
* Arrivals at the start of a time slot.

### Time-dependent trace

The table is laid out exactly like the second example in the
assignment (rows = state, columns = time slot, latest event drives the
column).

| time              |  0  |  1  |  2     |  3     |  4         |  5         |  6     |  7        |  8        |
|-------------------|-----|-----|--------|--------|------------|------------|--------|-----------|-----------|
| **arrival**       | P1  | P2  | R1, R2 | P3     | C1         | R3         | —      | —         | —         |
| **count**         |  1  |  2  |  2     |  2     |  2         |  2         |  1     |  1        |  1        |
| **buffer state**  | partial | **FULL** | FULL | FULL | FULL    | FULL       | partial| partial   | partial   |
| **rc**            |  0  |  0  |  2     |  2     |  2         |  2         |  0     |  0        |  1        |
| **empty_s**       |  1  |  0  |  0     |  −1    |  −1        |  −1        |  −1    |  0        |  0        |
| **full_s**        |  1  |  2  |  2     |  2     |  1         |  1         |  1     |  0        |  0        |
| **wrt**           |  1  |  1  |  0     |  0     |  −1        |  −1        |  0     |  1        |  0        |
| **order_s**       |  1  |  1  |  1     |  1     |  0         |  −1        |  −1    |  0        |  1        |
| **W@empty_s**     |  —  |  —  |  —     | **P3** | P3         | P3         | P3     | —         | —         |
| **W@full_s**      |  —  |  —  |  —     |  —     |  —         |  —         |  —     | —         | —         |
| **W@order_s**     |  —  |  —  |  —     |  —     |  —         | **R3**     | R3     | —         | —         |
| **W@wrt**         |  —  |  —  |  —     |  —     | **C1**     | C1         | —      | —         | —         |
| **operating**     | (P1 done) | (P2 done) | R1, R2 | R1, R2 | R1, R2 | R1, R2     | C1     | (C1 done) | R3, P3*   |

\* At t = 8, R3 takes its turn first (`order_s` was released by C1);
R3 grabs `wrt`, signals `order_s`, and starts reading. P3 then runs
`wait(order_s)` (succeeds), `wait(wrt)` (blocks behind R3) — symmetric
behaviour to what C1 did at t = 4.

### Walk-through of the key steps

**t = 0 — P1 produces.** P1 runs `wait(empty_s)` (2→1), `wait(order_s)`
(1→0), `wait(wrt)` (1→0), `signal(order_s)` (0→1), enqueues item,
`signal(wrt)` (0→1), `signal(full_s)` (0→1). count = 1.

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
`signal(wrt)` (−1 → 0). The `signal(wrt)` wakes C1.

**t = 7 — C1 runs.** C1 dequeues the item (count: 2→1), `signal(wrt)`
(0→1), `signal(empty_s)` (−1 → 0) — wakes P3, `signal(order_s)`
(−1 → 0) — wakes R3. Buffer is no longer full. C1 finishes.

**t = 8 — R3 (woken) and P3 (woken) proceed.** R3 runs the rest of
its entry section, takes `wrt` (1→0), and starts reading. P3 runs
`wait(order_s)` (0 → −1)... actually, with `order_s = 0` after C1's
signal(0→1) wake-up of R3 brought it back to 0 — P3 now finds
`order_s = 0` after R3's `signal(order_s)` brings it to 1, so
P3 takes it (1→0) and proceeds to `wait(wrt)` which will block behind
R3. The system continues operating normally.

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