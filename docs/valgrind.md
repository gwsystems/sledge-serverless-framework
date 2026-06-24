# Running SLEdge under Valgrind

This document explains how to run the SLEdge runtime under Valgrind/Memcheck and
why its output needs interpretation.

## TL;DR

- Valgrind reports many "Invalid read/write" errors against SLEdge that are
  **false positives** — artifacts of how the runtime preempts, switches stacks,
  and maps sandbox memory, none of which Memcheck can model.
- For finding **real** memory bugs, use **control-plane mode** (preemption off,
  one worker). This is what the per-test `make valgrind` targets already do.
- To exercise the **preemptive** path (multiple workers + SIGALRM) under
  Valgrind, use the suppression file that filters the known false positives.

Both modes are wrapped by `runtime/tools/valgrind/run-valgrind.sh`.

## Quick start

```sh
# Build the runtime + sample apps first (Docker route recommended; see README.md)

# Control-plane mode: clean output, best for hunting real leaks/errors.
runtime/tools/valgrind/run-valgrind.sh -d tests/multi-tenancy-sample

# Preemptive mode: 8 workers + preemption, with false positives suppressed.
runtime/tools/valgrind/run-valgrind.sh -m preemptive -w 8 -d tests/multi-tenancy-sample
```

Send requests from another shell while the runtime is up (e.g.
`curl localhost:55555/admin?40`), then stop the runtime with Ctrl-C so Valgrind
prints its summary.

## Why Valgrind struggles with SLEdge

Memcheck models a normal process. SLEdge violates three of its assumptions, and
each produces a flood of bogus "Invalid read/write" reports:

1. **Signal-based preemption.** A SIGALRM fires while a worker runs on a
   sandbox's stack. The handler
   (`software_interrupt_handle_signals` → `propagate_sigalrm` → `pthread_kill`)
   reads the interrupted `ucontext` and the glibc thread descriptor (TCB).
   Because the handler runs after a user-level stack switch, Valgrind has lost
   track of those regions and flags every read.

2. **User-level context switching** (`swapcontext` / `arch_context`). Valgrind
   prints `client switching stacks? SP change ...` and can no longer tell valid
   stack memory from garbage.

3. **Direct `mmap` of sandbox linear memory.** The runtime maps large guard
   regions that Valgrind marks `noaccess`
   (`set address range perms: large range ... (noaccess)`); the JIT-compiled
   wasm module then legitimately accesses them, producing **millions** of bogus
   errors inside `*.wasm.so` and in the WASI/syscall shims that touch guest
   memory.

This is why the existing `make valgrind` targets pin
`SLEDGE_DISABLE_PREEMPTION=true SLEDGE_NWORKERS=1`: with preemption off and a
single worker, problems #1 and #2 disappear, and Memcheck output becomes
trustworthy for the control plane.

## A worked example of a false positive

The signal path produces reports like:

```
Thread 3:
Invalid read of size 4
   at pthread_kill (pthread_kill.c:40)
   by propagate_sigalrm
   by software_interrupt_handle_signals
   ...
   by worker_thread_main
 Address 0x97d32d0 is in a rw- anonymous segment
```

This looks alarming but is benign. The `pthread_t` handed to `pthread_kill` is a
valid, mapped thread descriptor (the runtime `calloc`s
`runtime_worker_threads[]`, asserts each slot is non-zero before use via
`assert(runtime_worker_threads[i] != 0)` in `propagate_sigalrm`, and arms the
interval timer only after every worker has been created). The read itself is
correct; what is wrong is Valgrind's addressability map, because the signal was
delivered on a switched stack (reason #1 above). The `... is in a rw- anonymous
segment` verdict — as opposed to `... is not stack'd, malloc'd or free'd` —
confirms the target memory is genuinely mapped.

## The suppression file

`runtime/tools/valgrind/sledge.supp` filters the architectural false positives so
that any *remaining* report is worth investigating. `run-valgrind.sh` applies it
in both modes — it only targets provably-false-positive classes, none of which is
where a real control-plane bug would hide. It suppresses:

1. The signal handler reading the interrupted context
   (`software_interrupt_handle_signals`, `scheduler_idle_loop`,
   `scheduler_cooperative_sched`, `__sigsetjmp`, `sigprocmask`).
2. The SIGALRM broadcast (`propagate_sigalrm`, `pthread_kill`).
3. TCB reads after a stack switch (`pthread_self`, `pthread_kill`,
   `__pthread_{en,dis}able_asynccancel`).
4. JIT-compiled guest code (`obj:*wasm.so`).
5. Per-worker run/timeout queues read after a context switch
   (`local_runqueue_minheap_get_next`, `priority_queue_*`).
6. Host WASI/syscall shims dereferencing pointers into guest linear memory
   (`sledge_abi__*`, `sandbox_syscall*`, `sandbox_return`).

With this file, a representative run drops from hundreds of millions of reported
errors to a small, well-understood residual (see below), and **none** of the
remaining errors are in SLEdge's own runtime code — the signal/preemption,
scheduler, run-queue, and sandbox-lifecycle paths are all silent.

### Known residual noise in preemptive mode

A small residue remains that is *not* suppressed, because doing so safely is not
possible (it would require suppressing generic libc functions and would hide real
bugs):

- `memset` / `memmove` zeroing freshly mapped sandbox memory or copying a
  module's data image into it at instantiation. Valgrind captures no caller
  frame for these, so they cannot be anchored without blanket-suppressing all
  `memset`/`memmove`.
- Host stdio (`__vfprintf_internal`, `_IO_*`) invoked by WASI `fd_write` reading
  guest buffers.

Both are the same "Valgrind can't model sandbox memory" class as #4/#6 above.
They scale with the amount of guest code executed, so keep request volume low
when leak-hunting.

`run-valgrind.sh` sets `LD_BIND_NOW=1` so the dynamic linker resolves the
dlopen'd wasm module eagerly; this avoids a separate class of false positives in
`_dl_fixup`/`do_lookup_x` against the module's PLT/GOT at lazy-bind time.

## Reproducing manually

```sh
cd tests/multi-tenancy-sample
export LD_LIBRARY_PATH=../../runtime/bin
export SLEDGE_NWORKERS=8

# Preemptive, WITHOUT suppressions -> millions of (false) errors.
valgrind --max-stackframe=11150456 ../../runtime/bin/sledgert spec.json

# Preemptive, WITH suppressions -> signal/preemption path is silent.
valgrind --max-stackframe=11150456 \
  --suppressions=../../runtime/tools/valgrind/sledge.supp \
  ../../runtime/bin/sledgert spec.json
```
