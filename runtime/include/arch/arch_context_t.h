#pragma once

#include <setjmp.h>
#include <ucontext.h>

#include "arch/reg_t.h"
#include "arch/ureg_t.h"
#include "arch/arch_context_variant_t.h"

struct arch_context {
	arch_context_variant_t variant;
	reg_t                  regs[UREG_COUNT];
	mcontext_t             mctx;
	bool                   preemptable;
	jmp_buf                start_buf;
};
