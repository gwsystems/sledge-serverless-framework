# Unimodal Distribution

This experiment drives a unimodal distribution of two workloads from the same tenant, where one workload has some guarantees and the other does not.


First, the non-guaranteed workload is launched to acquire all the CPU. After a few second (e.g. 3s) the guaranteed workload begins. Depending on the amount of the reservation, the guaranteed workload should typically have a better success rate than the other one.

## Independent Variable

The Scheduling Policy: MT-EDF versus just EDF

## Dependent Variables

Replenishment Period and Max Budget of Tenants
Latency of high priority workload
