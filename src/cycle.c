#include "cycle.h"

cycle_t *cycle;

cycle_t *init_cycle()
{
	pool_t *pool;
	pool = create_pool(DEFAULT_POOL_SIZE);
	if (pool == NULL) {
		return NULL;
	}

	cycle = pcalloc(pool, sizeof(cycle_t));
	if (cycle == NULL) {
		destroy_pool(pool);
		return NULL;
	}

	cycle->pool = pool;
	cycle->connection_n = MAX_CONN_N;

	queue_init(&cycle->reusable_connections_queue);
	return cycle;
}
