#include "sledge_abi.h"

void
awsm_abi__trap_unreachable(void)
{
	sledge_abi__wasm_trap_raise(WASM_TRAP_UNREACHABLE);
}
