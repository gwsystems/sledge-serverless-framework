#ifndef ARCH_CONTEXT_H
#define ARCH_CONTEXT_H

#if defined(AARCH64) || defined(aarch64)
#include "aarch64/context.h"
#elif defined(X86_64) || defined(x86_64)
#include "x86_64/context.h"
#else
#warning "Architecture not set. Using x86_64"
#define X86_64
#include "x86_64/context.h"
#endif

#endif /* ARCH_CONTEXT_H */
