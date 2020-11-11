# Admissions Control

## Discussion of Implementation

The admissions control subsystem seeks to ensure that the system does not accept more work than it can execute while meeting the relative deadline defined in a module's JSON specification.

The system maintains an integral value expressing the capacity of the system as millionths of a worker core. This assumes that the runtime has "pinned" these workers to underlying processors and has no contention with other workloads.

The system maintains a second integral value expressing the total accepted work.

The module specification provides a relative deadline, an expected execution time, and a percentile target expressing the pXX latency that the admissions control system should use when making admissions decisions (tunable from 50% to 99%). Tuning this percentile expresses how conservative the system should be with regard to scheduling. Selecting a lower value, such as 50%, reserves less processor time and results in a higher likelihood that the relative deadline is not met. Selecting a higher value, such as 99%, reserves more processor time and provides a higher likelihood that that the relative deadline will be met. The provided expected execution time is assumed to match the percentile provided.

Dividing the expected execution time by the relative deadline yields the fraction of a worker needed to meet the deadline.

If the existing accepted workload plus the required work of this new workload is less than the system capacity, the workload is accepted, and the integral value expressing the total accepted work is increased. The resulting sandbox request is tagged with the fraction of a worker it was calculated to use, and when the request completes, the total accepted work is decreased by this amount.

If the existing accepted workload plus the required work of this new workload is greater than the system capacity, the request is rejected and the runtime sends the client an HTTP 503 response.

While the module specification provides an expected execution time, the system does not trust this value and only uses it in the absence of better information. Each sandbox is profiled as it runs through the system, and the end-to-end execution time of successful sandbox requests are added to a specialized performance window data structure that stores the last N execution times sorted in order of execution time. This structure optimizes for quick lookups of a specific ppXX percentile

Once data is seeded into this data structure, the initial execution estimate provided in the module specification is ignored, and the pXX target is instead used to lookup the actual pXX performance metric.

Future Work:

Currently, the scheduler takes no actual when an executing sandbox exceeds its pXX execution time or deadline.

In the case of the pXX workload, this means that a workload configured to target p50 during admissions control decisions with exceptionally poor p99 performance causes system-wide overheads that can cause other systems to miss their deadlines.

Even worse, when executing beyond the relative deadline, the request might be too stale for the client.

In the absolute worst case, one can imagine a client workload caught in an infinite loop that causes permanent head of line blocking because its deadline is earlier than the current time, such that nothing can possibly preempt the executing workload.

## Question

- Does Admissions Control guarantee that deadlines are met?

## Independent Variable

Deadline is disabled versus deadline is enabled

## Invariants

Single workload
Use FIFO policy

## Dependent Variables

End-to-end execution time of a workload measured from a client measured relative to its deadline
