# Preemption

## Question

- How do mixed criticality workloads perform under the Sledge scheduler policies?
- How does the latency of a high criticality workload that triggers preemption on a system under load compare to being the only workload on the system?
- What is the slowdown on the low priority workload?
- How does this affect aggregate throughput?

## Setup

The system is configured with admission control disabled.

The driver script drives a bimodal distribution of long-running low-priority and short-running high-priority workloads

Relative Deadlines are tuned such that the scheduler should always preempt the low-priority workload for the high-priority workload.

A driver script runs the two workloads separately as a baseline

It then runs them concurrently, starting the low-priority long-running workload first such that the system begins execution and accumulates requests in the data structures. The high-priority short-running workload then begins.

## Independent Variable

The Scheduling Policy: EDF versus FIFO

## Dependent Variables

Latency of high priority workload
