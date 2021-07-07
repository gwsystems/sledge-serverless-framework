#pragma once

#include "module.h"
#include "ck_ht.h" 

extern ck_ht_t g_module_ht;
extern void init_module_ht();
extern void insert_module_to_ht(const uint32_t port, const struct module* module);
extern struct module* get_module_from_ht(const uint32_t port);
