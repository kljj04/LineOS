# Scheduling

[Documentation index](README.md) · [한국어](../ko_KR/Scheduler.md) · [English](../en_US/Scheduler.md)

LBPWRR (Load-Balanced Primary Weighted Round Robin) manages a run queue, current task and idle task per CPU. Priorities 1–6 map directly to weights. Adding a task chooses the CPU with the smallest assigned runnable-list count; that count also includes registered sleeping or blocked tasks.

## Scheduling and stack ownership

Task slots, CPU run lists and weighted chunks use growing dynamic arrays rather than the former fixed 1024-entry limits. Allocation failure and ID/capacity representation still bound growth. Each task receives a 16-page kernel stack allocated below 4 GiB, with its initial interrupt frame copied onto that stack.

`LBPWRRStart()` selects and restores a local task immediately; create/register the workload before calling it. It arms the first TSC deadline immediately before restoring the saved context. Vector 64 schedules timer ticks at nominal 5 ms intervals; explicit yield uses software interrupt 67 (`0x43`). Non-idle READY tasks are preferred, with the local idle task as fallback. BLOCKED and SLEEPING tasks are not selected.

`OnCPU` remains set while a task's stack is still in use, including the interval after selecting the next task. After changing RSP, the common ISR calls `LBPWRRCompleteSwitch()` on the next stack, then restores registers and executes `iretq`. Only then can the former task be migrated or its stack released. Idle tasks cannot migrate or enter a Wait Queue. The existing “No runnable task” panic remains an invariant failure if the idle fallback is unavailable; blocked work is never resumed just to avoid that panic.

## Wait Queues

`Kernel/Include/Scheduler/WaitQueue.h` provides reusable FIFO wait lists:

| Interface | Contract |
| --- | --- |
| `WaitQueueInit(Queue)` | Initialize before publication; Queue must outlive all waiters |
| `WaitQueueSleep(Queue, ConditionLock)` | Register current task as BLOCKED, release locks, yield, and reacquire ConditionLock before returning |
| `WaitQueueWakeOne(Queue)` | Detach and wake the oldest waiter |
| `WaitQueueWakeAll(Queue)` | Detach and wake all current waiters |
| `WaitQueueFinishTask(Task, State)` | Detach a waiter and apply the terminal state under scheduler synchronization; used by kill/terminate |

Call Sleep with IRQs disabled and the condition lock held after checking the predicate. Hold no unrelated locks. Sleep releases the condition, common WaitLock and RunQueue locks before yield; on return only the condition lock is reacquired, with IF still clear. Recheck the predicate in a loop, then restore the caller's saved IRQ flags. A missing current task, idle task, or invalid current-task state returns FALSE without releasing the condition lock. Wake operations save/restore IRQ flags and are IRQ-safe.

Input producers and consumers share the same event condition lock, so checking an empty ring and registering a waiter cannot lose a concurrent event. Publish the event before wake. Lock order is input virtqueue lock → event condition lock → common WaitLock → target RunQueue lock. RunQueue code does not acquire WaitLock or input locks while holding its own lock.

If wake precedes yield, a waiter still in CurrentTasks returns to RUNNING; otherwise it becomes READY. Current-task checks and OnCPU prevent migration while its execution or ISR stack handoff is unfinished. A remote wake uses the existing timer for scheduling; no reschedule IPI is sent.

## Chunk capacity, priority and lifetime

A Wait Queue BLOCKED task reserves its weighted Chunk entries, but READY-only selection skips it. Wake requires no insertion and can succeed when the Chunk is full. Timed SLEEPING tasks instead leave the Chunk and are reinserted when their TSC deadline expires.

Priority changes hold the task's RunQueue lock and update Priority/Weight only after the Chunk operation succeeds. A capacity failure retains the previous priority and weight, including while blocked. NotifyTaskChanged also preserves reserved entries for registered Wait Queue waiters. Killing or terminating a waiter detaches it and removes its Chunk entries before later destruction; wake cannot revive the detached terminal task.

Removal requires a terminal task that is neither current nor OnCPU. Migration is limited to registered, non-idle READY tasks that are neither current nor OnCPU; it locks the two queues in CPU-index order. Load-balancer helpers exist, but the current startup/interrupt paths do not periodically invoke them.

## CPU usage and current workload

`Usage.c` accounts TSC time as busy or idle at scheduling boundaries and samples at approximately 100 ms. `UsageGetCPU()` returns an integer percentage; `UsageGetTotal()` returns the arithmetic mean across active CPUs. TASK CPUTimeTSC and CPUUsage track per-task time and percentage. These are scheduler accounting values, not host process CPU measurements.

KMain registers keyboard/mouse consumers and an NTP worker, then starts Init. Init launches Terminal and Login; Login starts an authenticated user session. Terminal reads block on Wait Queues and sched_yield yields to the scheduler and returns zero. PROCESS/TASK separation permits multiple independently scheduled kernel threads in one user address space. See [DeviceFS/ABI](DeviceFS.md).

## Limits and source

Public interfaces are under `Kernel/Include/Scheduler/`; implementations are under `Kernel/Source/Scheduler/`, with stack restoration in `Kernel/Source/Interrupt/ISR.S`. CPU affinity policy, broader fairness analysis and hardening of task-pool/timed-sleep concurrency remain future work. Wait lists share one lock; large-scale contention has not been characterized.

## Processes and threads

PROCESS owns PID, address space, file context, descriptors, credentials, assets, mappings and the thread list. TASK owns TID, registers, kernel stack, FS base, FPU state, wait state and signal mask. Thread exit and group exit are distinct; the last thread releases process resources. The fork/exec execution-record table remains limited to 128 slots independently of dynamic scheduler arrays.
