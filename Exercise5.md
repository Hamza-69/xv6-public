# Exercise 5 — Processes vs. Threads

### 1. What does it mean when we say that a process has a private address space?

Each process owns its own virtual-memory layout — its own text/code,
data, heap, and stack regions — and the kernel's page tables keep one
process's pages invisible to every other process. Two processes that
happen to use the same virtual address `0x1000` are accessing
*different* physical memory. Lecture 6 slide 5 shows this: each process
in the picture has a separate Stack/Heap/Data/Text block and the page
tables enforce that isolation.

### 2. Advantages and disadvantages of a private address space

**Advantages**

* Isolation/safety — one process cannot corrupt another's memory; a bug
  or crash in P1 cannot scribble on P2's data.
* Independent scheduling — the OS can swap, kill, or migrate a process
  without affecting any other.
* Security boundary — the kernel can enforce per-process access rights
  on files, sockets, and so on.

**Disadvantages**

* Communication is expensive — sharing data needs explicit IPC (shared
  memory, message passing, pipes) instead of just dereferencing a
  pointer.
* Process creation is heavy — Lecture 6 slide 8 notes that creating a
  thread is roughly 30× faster than creating a process, precisely
  because there is no new address space to set up.
* Larger memory footprint — every process duplicates code/library
  pages (mitigated by COW but still real).

### 3. What programming directives have we used to circumvent address-space privacy?

Mainly the IPC primitives from Lecture 5 plus thread-style sharing:

* `pipe()` and named pipes (FIFOs).
* `shmget()` / `shmat()` shared-memory segments.
* `mmap(MAP_SHARED, ...)` mapping the same backing file/object into
  multiple processes.
* `pthread_create()` — threads share their parent process's address
  space by construction (Lecture 6 slide 6).
* On Linux, `clone(CLONE_VM, ...)` directly (Lecture 6 slide 36).

### 4. Threads as "lightweight processes"

They are *processes-like* in that each thread has its own independent
flow of control: a private user stack, its own program counter, its own
register set, and its own scheduling state (Lecture 6 slide 6). The
kernel scheduler can preempt and dispatch each thread separately.

The *lightweight* qualifier comes from what they do **not** duplicate:
no separate text/data/heap, no new file descriptor table, no new
process structure tree. As Lecture 6 slide 8 says, creating one is
~30× cheaper than creating a process and switching between them is
much cheaper too.

### 5. In what sense are threads an example of virtualization?

Each thread is given the *illusion* of having its own CPU. Even with
just one physical core, multiple threads each run as if they have a
dedicated processor: their own PC, registers, and stack. The kernel
multiplexes the real hardware by saving and restoring each thread's
register file at context-switch time. So the "virtual processor" each
thread sees is a software abstraction layered on top of the few real
cores the machine has — that's exactly what virtualization means.

### 6. Advantages/disadvantages of letting threads share virtually all memory

**Advantages**

* Cheap, zero-copy data sharing — pass a pointer instead of marshalling
  through IPC.
* Natural for parallelizing one logical task across cores (the
  multithreaded server pattern from Lecture 6 slide 9).
* Smaller working set — one shared text/heap means more room in
  L2/L3 cache.

**Disadvantages**

* Race conditions and tearing on shared variables (Lecture 6 slide 26
  shows the classic `counter++` race; Lecture 7 slides 9-10 detail it
  in machine code).
* Synchronization overhead — mutexes, semaphores, monitors all add
  complexity and can themselves be misused (deadlock,
  priority inversion).
* No fault isolation — a wild pointer in one thread can corrupt
  another thread's heap or stack and bring the whole process down.
* Harder to reason about — debugging is non-deterministic because
  interleavings are not reproducible.

---

### 7. Advantages of leveraging the pid abstraction for thread ids

* One uniform namespace — kernel data structures, `/proc`, `ps`,
  `kill`, `waitpid`, scheduler bookkeeping, all already handle pid-like
  integers, so no parallel mechanism is needed for tids.
* Linux's task model already treats every schedulable entity as a
  `task_struct` (Lecture 6 slide 36) — pid is just the identifier of
  that task, and a tid is the identifier of a different task that
  happens to share most of its state. Same primitive, no new code.
* Tools that already consume pids (debuggers, profilers, accounting,
  CPU affinity APIs) work on threads with no change.

### 8. What happens if you pass a thread id that isn't a process id to waitpid()?

`waitpid()` is defined to wait on a *process*. Linux historically
treats only the thread group leader (the original main thread, whose
tid equals the pid) as something `waitpid()` can wait on. Passing a
tid that is not a thread-group leader fails with `ECHILD` — the call
returns -1 because that tid is not a child *process* of the caller,
even if it lives inside a child process. The proper primitive for
joining a thread is `pthread_join()` (Lecture 6 slide 27).

### 9. What happens if you pass a thread id to sched_setaffinity()?

It works — `sched_setaffinity()` operates on a tid, not just a pid.
The CPU affinity mask of *that specific thread* is updated, while its
sibling threads in the same process keep whatever masks they had.
Lecture 8 slide 41 (Example 10) shows the per-thread version
`pthread_setaffinity_np()` doing exactly this for each thread it
spawns.

### 10. Advantages of always assigning a thread to the same CPU

This is **CPU affinity** (Lecture 8 slide 39). The benefits all come
from the cache hierarchy:

* The thread's hot data stays in that CPU's L1/L2 cache, so memory
  accesses hit cache instead of going to RAM.
* TLB entries for the thread's pages stay warm.
* No cross-CPU cache-line migration / invalidation when the thread
  resumes.
* Lower variance in per-iteration latency, important for real-time
  workloads.

Lecture 8 slide 39 explicitly calls out the cost of moving a process
to a different CPU (cache invalidation on the source, cache
re-population on the destination) and that's exactly what affinity
avoids.

---

### 11. Why prefer multithreading over multiprocessing?

* Cheaper creation/teardown (~30× per Lecture 6 slide 8) and faster
  context switches because there is no address-space switch.
* Trivial sharing of large in-memory state — no shared-memory or IPC
  setup; just dereference pointers.
* Smaller memory footprint — text/heap/data are shared.
* Natural fit when the work is one logical job that can be split into
  cooperating sub-tasks (e.g., the multithreaded server pattern,
  Lecture 6 slide 9).

### 12. Why prefer multiprocessing over multithreading?

* Fault isolation — a segfault or `abort()` in one worker doesn't
  bring the whole job down; the parent can `wait()` for the corpse and
  respawn.
* Independent address spaces avoid whole classes of races and make the
  code easier to reason about.
* Security — each process can drop privileges or be sandboxed
  independently.
* You want to call `exec()` to run a different binary — that requires
  a process boundary.
* Languages with a global interpreter lock (CPython) get real
  parallelism only through processes.

### 13. What happens if a thread within a larger process calls fork()?

Lecture 6 slide 33 covers this. There are historically two flavours:

* `forkall()` (rare) duplicates *all* threads into the child.
* `fork1()` (POSIX-style, what Linux gives you) duplicates *only the
  calling thread* in the child. The child has one thread (the one
  that called `fork()`). All locks, condition variables, and
  semaphores held by the *other* threads of the parent are now in an
  indeterminate state in the child — which is why POSIX warns that
  after `fork()` in a multithreaded program, the child is only safe to
  call async-signal-safe functions until it `exec()`s.

### 14. What happens if a thread within a larger process calls execvp()?

The slide is again explicit (Lecture 6 slide 33): `exec()` replaces
the entire process — text, data, heap, *and every thread*. The
calling thread's `execvp()` succeeds by overlaying the new program on
the whole process; all sibling threads vanish along with the old
address space. After `exec()` the process has exactly one thread
running the new program's `main()`.
