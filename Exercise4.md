# Exercise 4 — Scheduling (Round-Robin)

## Setup

| Process | Arrival | Burst (ms) |
|---------|---------|------------|
| P1      | 0       | 10         |
| P2      | 2       | 3          |
| P3      | 3       | 5          |
| P4      | 7       | 6          |
| P5      | 8       | 4          |

Round-Robin, time quantum **q = 4 ms**.

Convention used (matches the lecture's treatment, Lecture 8 slide 25):
* When a running process is preempted at the end of its quantum, it goes
  to the **tail** of the ready queue.
* New arrivals are appended to the tail of the ready queue at the moment
  they arrive.
* If an arrival occurs at the same instant a running process is appended
  to the tail, the **new arrival is appended first**, then the preempted
  one (so the preempted process gets re-queued behind the freshly
  arrived one). This is the standard textbook convention.

## Part 1 — Trace and average waiting time

Walking through tick-by-tick:

| Time window | Running | Remaining after | Events during window | Ready queue at window end |
|-------------|---------|-----------------|----------------------|---------------------------|
| 0  → 4      | P1      | 10 - 4 = 6      | P2 arrives at 2; P3 arrives at 3 | P2, P3, P1 |
| 4  → 7      | P2      | 3 - 3 = 0  ✓    | (none)               | P3, P1            |
| 7  → 11     | P3      | 5 - 4 = 1       | P4 arrives at 7; P5 arrives at 8 | P1, P4, P5, P3 |
| 11 → 15     | P1      | 6 - 4 = 2       | (none)               | P4, P5, P3, P1   |
| 15 → 19     | P4      | 6 - 4 = 2       | (none)               | P5, P3, P1, P4   |
| 19 → 23     | P5      | 4 - 4 = 0  ✓    | (none)               | P3, P1, P4       |
| 23 → 24     | P3      | 1 - 1 = 0  ✓    | (none)               | P1, P4            |
| 24 → 26     | P1      | 2 - 2 = 0  ✓    | (none)               | P4                |
| 26 → 28     | P4      | 2 - 2 = 0  ✓    | (none)               | (empty)           |

**Gantt chart:**

```
| P1 | P2 | P3 | P1 | P4 | P5 | P3 | P1 | P4 |
0    4    7    11   15   19   23   24   26   28
```

**Completion times:** P1 = 26, P2 = 7, P3 = 24, P4 = 28, P5 = 23.

Waiting time = Completion − Arrival − Burst:

| Process | Completion | Arrival | Burst | Waiting time |
|---------|-----------:|--------:|------:|-------------:|
| P1      | 26         | 0       | 10    | **16**       |
| P2      | 7          | 2       | 3     | **2**        |
| P3      | 24         | 3       | 5     | **16**       |
| P4      | 28         | 7       | 6     | **15**       |
| P5      | 23         | 8       | 4     | **11**       |

```
Average waiting time = (16 + 2 + 16 + 15 + 11) / 5 = 60 / 5 = 12 ms
```

**Average waiting time = 12 ms.**

## Part 2 — Context-switch interval for 5 % overhead

From the Gantt chart there are **9 run segments** and therefore
**8 context switches** between them (no switch is needed after the very
last process terminates).

Let `x` be the duration of one context switch.

* Total useful CPU time = 10 + 3 + 5 + 6 + 4 = **28 ms**
* Total context-switch time = `8 · x`
* Total elapsed time = `28 + 8 · x`

The condition "context switches consume 5 % of the CPU time" means the
context-switch time is 5 % of the total elapsed time:

```
            8x
         ────────  =  0.05
         28 + 8x

    ⇒    8x  =  0.05 · (28 + 8x)
    ⇒    8x  =  1.4 + 0.4x
    ⇒  7.6x  =  1.4
    ⇒    x   =  1.4 / 7.6  ≈  0.184 ms   (≈ 184 µs)
```

**Each context switch may consume at most ≈ 0.184 ms (184 µs) for the
total context-switching overhead to stay at 5 %.**
