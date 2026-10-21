# Scheduling

[Documentation index](README.md) · [한국어](../ko_KR/Scheduler.md) · [English](../en_US/Scheduler.md)

- LBPWRR (Load-Balanced Primary Weighted Round Robin) scheduler
- Per-CPU run queues and current-task tracking
- Per-CPU idle tasks
- Weighted runnable chunks with priorities 1 through 6
- Least-loaded CPU selection when adding a task
- Timer-driven scheduling through the TSC Deadline Timer
- Explicit task yield support
- Sleeping task wake-up based on TSC deadlines
- Task termination, removal from the run queue, and destruction
- Ready-task migration with ordered dual-run-queue locking

`LBPWRRStart()` selects a local runnable task and restores its saved interrupt frame. It arms a new TSC deadline
immediately before the first context restore so a BSP that reached the scheduler with interrupts disabled still receives
its first timer interrupt after task entry.

The timer interrupt saves the current task frame, updates task state, selects the next local task from the weighted
chunk, and returns that task's saved frame to the common ISR restore path. Non-idle tasks are preferred; the local idle
task is used only when no non-idle task is ready.

The scheduler is experimental. CPU affinity, blocking beyond timed sleep, fairness measurement, and broader SMP
subsystem hardening remain future work.

## Run queues and task ownership

LBPWRR uses a fixed-capacity per-CPU runnable list and a weighted chunk. A task's priority maps directly to a weight
from 1 through 6; each task appears in the chunk according to that weight. Adding a task selects the CPU with the
smallest runnable count, then updates that CPU's chunk while holding its run-queue lock.

Task state changes update the corresponding weighted chunk. Sleeping, terminated, and killed tasks do not remain
runnable. A task must be removed from scheduler ownership before its task storage and stack are destroyed.

Migration is intentionally limited to scheduled `TASK_READY` tasks that are not the current task on their source CPU. It
acquires the two run-queue locks in CPU-index order and updates both the runnable lists and weighted chunks atomically
with respect to those locks.

## Source

Public interfaces are under `Kernel/Include/Scheduler/`; implementations are under `Kernel/Source/Scheduler/`. The default startup workload is described in [Architecture](Architecture.md).
