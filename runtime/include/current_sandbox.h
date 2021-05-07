#pragma once

#include "sandbox.h"

struct sandbox *current_sandbox_get(void);
void            current_sandbox_set(struct sandbox *sandbox);
void            current_sandbox_block(void);
