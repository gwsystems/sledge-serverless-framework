#include "module_manager.h"

ck_ht_t g_module_ht;

static void *
ht_malloc(size_t r)
{

        return malloc(r);
}

static void
ht_free(void *p, size_t b, bool r)
{

        (void)b;
        (void)r;
        free(p);
        return;
}

static void
ht_hash_wrapper(struct ck_ht_hash *h,
        const void *key,
        size_t length,
        uint64_t seed)
{
        //h->value = (unsigned long)MurmurHash64A(key, length, seed);
        return;
}

static struct ck_malloc my_allocator = {
        .malloc = ht_malloc,
        .free = ht_free
};

/**
 * Module Manager Hashtable initilization Function
 * Initilize the glaoble hashtable g_module_ht
 * Returns true if initlization success, false otherwise
 */

void 
init_module_ht()
{
	if (ck_ht_init(&g_module_ht, CK_HT_MODE_DIRECT, NULL, &my_allocator, 2, 6602834) == false) {
                perror("ck_ht_init");
                exit(EXIT_FAILURE);
        }

}
/**
 * Module Manager Insert Function
 * Insert a module with a key into a hashtable, the key is the port number.
 * The value is the module object pointer
 *
 * @param port - the TCP port number that the module will listen to
 * @param module - the module object pointer that will be inserted into the hashtable
 */

void 
insert_module_to_ht(const uint32_t port, const struct module* module) 
{
	assert(module != NULL);
	ck_ht_entry_t entry;
	ck_ht_hash_t h;
        ck_ht_hash_direct(&h, &g_module_ht, (uintptr_t)port);	
	ck_ht_entry_set_direct(&entry, h, (uintptr_t)port, (uintptr_t)module);
	ck_ht_put_spmc(&g_module_ht, h, &entry);
	printf("insert port %u, module=%p\n", port, module);
}

/**
 * Module Manager Get Function
 * Get a module from the hashtable with a key, the key is the port number.
 * Returns the module object pointer 
 *
 */

struct module* 
get_module_from_ht(const uint32_t port)
{
	printf("try to get module with port %u\n", port);
	ck_ht_entry_t entry;
        ck_ht_hash_t h;
	ck_ht_hash_direct(&h, &g_module_ht, (uintptr_t)port);
	ck_ht_entry_key_set_direct(&entry, (uintptr_t)port);
	if (ck_ht_get_spmc(&g_module_ht, h, &entry) == false) { //
                printf("ERROR: Found non-existing entry with port %u.\n", port);
		return NULL;
        }
	uintptr_t k, v;

        k = ck_ht_entry_key_direct(&entry);// get key from entry
        v = ck_ht_entry_value_direct(&entry);//get value from entry
	if (unlikely(k != port)) {
		printf("ERROR: key doesnt match with port %u.\n", port);
		return NULL;
	} else {
		printf("module found, address is =%p\n", v);
		return (struct module*) v;	
	}
}

