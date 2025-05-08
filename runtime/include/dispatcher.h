#pragma once

#include "dispatcher_options.h"

static inline char *
dispatcher_print(enum DISPATCHER variant)
{
        switch (variant) {
        case DISPATCHER_EDF_INTERRUPT:
                return "EDF_INTERRUPT";
        case DISPATCHER_DARC:
                return "DARC"; 
        case DISPATCHER_SHINJUKU:
                return "SHINJUKU";
	case DISPATCHER_TO_GLOBAL_QUEUE:
		return "DISPATCHER_TO_GLOBAL_QUEUE";
	case DISPATCHER_RR:
		return "DISPATCHER_RR";
	case DISPATCHER_JSQ:
		return "DISPATCHER_JSQ";
	case DISPATCHER_LLD:
		return "DISPATCHER_LLD";
        }
}

