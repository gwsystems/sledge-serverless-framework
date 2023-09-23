#pragma once

enum DISPATCHER
{
        DISPATCHER_EDF_INTERRUPT  = 0,
        DISPATCHER_DARC           = 1,
        DISPATCHER_SHINJUKU       = 2,
};

extern enum DISPATCHER dispatcher;

