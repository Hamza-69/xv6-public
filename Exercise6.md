# Exercise 6 — Threading Short Answers

### 1. Is `i--` thread safe? Why or why not?

**No.** `i--` looks like a single statement in C but compiles to three
machine instructions (Lecture 7 slide 9 illustrates the symmetric
`count++` case explicitly):

```
r1 = i        ; load
r1 = r1 - 1   ; sub
i  = r1       ; store
```

If two threads execute `i--` concurrently and the scheduler interleaves
those three instructions, both threads can read the *same* old value of
`i`, decrement their local copy, and write back — losing one of the
decrements. This is the textbook race condition shown in the table on
Lecture 7 slide 10. Making it safe requires either an atomic
decrement instruction or a mutex/semaphore around the operation.

### 2. Difference between a mutex and a semaphore initialized to 1? Can one substitute for the other?

A binary semaphore initialized to 1 and a mutex *look* the same when
used for mutual exclusion, but they differ in their semantics:

* **Ownership.** A mutex has an *owner* — only the thread that locked
  it is allowed to unlock it (this is what enables features like
  priority inheritance and recursive locking in some implementations).
  A semaphore has no owner concept — any thread can call
  `sem_post()`/`signal()` regardless of who called `sem_wait()`.
* **Purpose / API.** A mutex is purely a mutual-exclusion primitive.
  A semaphore is a more general counting/signalling primitive that
  *happens* to reduce to mutual exclusion in the binary case (Lecture
  7 slide 34 explicitly says "Binary Semaphore … used to guarantee
  mutual exclusion").
* **Idiom.** Semaphores are also useful for signalling between
  threads — e.g. P0 signals after `<s0>` so P1 can wait and then run
  `<s1>` (Lecture 7 slide 35). Mutexes are not designed for that
  signalling pattern.

**Substitution:** for *pure* mutual exclusion you can use either, and
they behave the same. But mutexes are not appropriate when the wait
and the signal happen in *different* threads (the signalling pattern
above) — that's a job for a semaphore. Going the other way, replacing
a mutex with a binary semaphore loses ownership safety: any thread can
"unlock" it, which is fragile.

### 3. What is busy waiting? Is it ever a good idea? Does the answer depend on the number of CPUs?

**Busy waiting** (a.k.a. spinning) is when a thread sits in a tight
loop continuously testing a condition instead of blocking. Lecture 7
slide 29 calls it out as the motivation for semaphores: "If one
process is in CS, any other process trying to enter its CS continuously
loops in entry code."

**Single-CPU machine:** busy waiting is almost always bad. While the
spinning thread holds the CPU, the *other* thread — the one that
needs to release the lock — cannot run. So the spinner just wastes
its quantum and delays the resolution. You should always block
(via a semaphore) instead.

**Multi-CPU machine:** busy waiting can be acceptable for *very short*
critical sections. If the holder is running on another CPU and will
release within a few hundred cycles, spinning avoids the much higher
cost of a full block/unblock context switch. Lecture 7 slide 33 makes
this point explicitly: "In a multi-CPU environment: use spinlocks.
Rare busy waiting to wait()/signal() CS. Such operations are short."
This is exactly why kernels use spinlocks internally for protecting
their semaphore implementations.

So: usually no, but on multi-CPU systems for very short waits, yes.

### 4. A scenario where `semaphore s(-11)` (negative initial value) is sensible

Recall the abstract semaphore semantics from Lecture 7 slide 32:

```
wait(S):    S.value--;  if (S.value < 0)  block();
signal(S):  S.value++;  if (S.value <= 0) wakeup();
```

A negative initial value pre-loads "owed signals". `s(-11)` means a
single `wait(s)` will succeed only after **12 signals** have been
posted (the first 11 signals just bring the value from −11 to 0, the
12th brings it to 1, and then the wait can find a positive value to
decrement).

A clean use case: **wait for a fixed number of independent events to
have occurred before proceeding.** Imagine a coordinator thread that
must not start its summary until 12 worker threads have each finished
their part. Initialize `s` to `-11`. Each worker calls `signal(s)`
exactly once on completion. The coordinator calls `wait(s)`. The
wait can only succeed after the 12th signal, i.e. only after every
worker has finished. This avoids the alternative of looping
`wait(s); wait(s); ...` 12 times; it's a single wait against a
counter that has been seeded with the right "deficit".

This is the classic *N-arrival barrier* pattern — handy whenever you
want one synchronisation point that fires after a known number of
independent events.
