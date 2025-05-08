#pragma once

enum DISPATCHER
{
	DISPATCHER_EDF_INTERRUPT   = 0,
	DISPATCHER_DARC            = 1,
	DISPATCHER_SHINJUKU        = 2,
	DISPATCHER_TO_GLOBAL_QUEUE = 3,
	DISPATCHER_RR              = 4,
	DISPATCHER_JSQ		   = 5,
	DISPATCHER_LLD		   = 6
};

extern enum DISPATCHER dispatcher;

