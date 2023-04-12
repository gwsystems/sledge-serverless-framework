#pragma once
extern _Atomic uint64_t worker_queuing_cost[1024];

static inline void
worker_queuing_cost_initialize()
{
        for (int i = 0; i < 1024; i++) atomic_init(&worker_queuing_cost[i], 0);
}

static inline void
worker_queuing_cost_increment(int index, uint64_t cost)
{
        atomic_fetch_add(&worker_queuing_cost[index], cost);
}

static inline void
worker_queuing_cost_decrement(int index, uint64_t cost)
{
        assert(index >= 0 && index < 1024);
        atomic_fetch_sub(&worker_queuing_cost[index], cost);
	assert(worker_queuing_cost[index] >= 0);
}


void local_runqueue_minheap_initialize();
