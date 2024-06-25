#ifndef MESSAGE_H
#define MESSAGE_H

#include "runtime.h"

typedef enum
{
	MESSAGE_CFW_PULLED_NEW_SANDBOX,
	MESSAGE_CFW_REDUCE_DEMAND, 
	MESSAGE_CFW_DELETE_SANDBOX, /* normal mode for deleting new sandbox demands */
	MESSAGE_CFW_EXTRA_DEMAND_REQUEST,
	MESSAGE_CFW_WRITEBACK_PREEMPTION,
	MESSAGE_CFW_WRITEBACK_OVERSHOOT,

	MESSAGE_CTW_SHED_CURRENT_JOB
} message_type_t;

struct message {
	uint64_t                 sandbox_id;
	uint64_t                 adjustment;
	uint64_t                 total_running_duration;
	uint64_t                 remaining_exec;
	uint64_t                 timestamp;
	struct sandbox          *sandbox;
	struct sandbox_metadata *sandbox_meta;
	message_type_t           message_type;
	int                      sender_worker_idx;
	uint8_t                  state;
	bool                     exceeded_estimation;
}; // PAGE_ALIGNED;


#endif /* MESSAGE_H */
