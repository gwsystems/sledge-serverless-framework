# Bimodal Distribution

This experiment drives a bimodal distribution of long-running low-priority and short-running high-priority workloads

Relative Deadlines are tuned such that the scheduler should always preempt the low-priority workload for the high-priority workload if preemption is disabled.

The two workloads are run separately as a baseline. They are then run concurrently, starting the low-priority long-running workload first such that the system begins execution and accumulates requests in the data structures. The high-priority short-running workload then begins.

## Independent Variable

The Scheduling Policy: EDF versus FIFO

## Dependent Variables

Latency of high priority workload
