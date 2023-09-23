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
        }
}

