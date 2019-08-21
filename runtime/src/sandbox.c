#include <assert.h>
#include <runtime.h>
#include <sandbox.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>

static inline void
sandbox_args_setup(i32 argc)
{
	struct sandbox *curr = sandbox_current();
	char *args = sandbox_args();

	// whatever gregor has, to be able to pass args to a module!
	curr->args_offset = sandbox_lmbound;
	assert(sandbox_lmbase == curr->linear_start);
	expand_memory();

	i32 *array_ptr = get_memory_ptr_void(curr->args_offset, argc * sizeof(i32));
	i32 string_off = curr->args_offset + (argc * sizeof(i32));

	for (int i = 0; i < argc; i++) {
		char *arg = args + (i * MOD_ARG_MAX_SZ);
		size_t str_sz = strlen(arg) + 1;

		array_ptr[i] = string_off;
		// why get_memory_ptr_for_runtime??
		strncpy(get_memory_ptr_for_runtime(string_off, strlen(arg) + 1), arg, strlen(arg));

		string_off += str_sz;
	}
}

void
sandbox_entry(void)
{
	struct sandbox *curr = sandbox_current();
	// FIXME: is this right? this is the first time this sandbox is running.. so it wont
	//        return to sandbox_switch() api.. 
	//        we'd potentially do what we'd in sandbox_switch() api here for cleanup..
	if (!softint_enabled()) {
		arch_context_init(&curr->ctxt, 0, 0);
		next_context = NULL;
		softint_enable();
	}
	struct module *curr_mod = sandbox_module(curr);
	int argc = module_nargs(curr_mod);

	// for stdio
	int f = io_handle_open(0);
	assert(f == 0);
	f = io_handle_open(1);
	assert(f == 1);
	f = io_handle_open(2);
	assert(f == 2);

	alloc_linear_memory();
	
	// perhaps only initialized for the first instance? or TODO!
	//module_table_init(curr_mod);
	module_memory_init(curr_mod);

	sandbox_args_setup(argc);

	curr->retval = module_entry(curr_mod, argc, curr->args_offset);

	sandbox_exit();
}

struct sandbox *
sandbox_alloc(struct module *mod, char *args)
{
	if (!module_is_valid(mod)) return NULL;

	// FIXME: don't use malloc. huge security problem!
	// perhaps, main should be in its own sandbox, when it is not running any sandbox.
	struct sandbox *sb = (struct sandbox *)malloc(sizeof(struct sandbox));

	if (!sb) return NULL;

	memset(sb, 0, sizeof(struct sandbox));
	//actual module instantiation!
	sb->mod = mod;
	module_acquire(mod);
	sb->args = (void *)args;
	sb->stack_size = mod->stack_size;
	sb->stack_start = mmap(NULL, sb->stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN, -1, 0);
	if (sb->stack_start == MAP_FAILED) {
		perror("mmap");
		assert(0);
	}
	for (int i = 0; i < SBOX_MAX_OPEN; i++) sb->handles[i].fd = -1;
	ps_list_init_d(sb);

	arch_context_init(&sb->ctxt, (reg_t)sandbox_entry, (reg_t)(sb->stack_start + sb->stack_size));
	sandbox_run(sb);

	return sb;
}

void
sandbox_free(struct sandbox *sb)
{
	// you have to context switch away to free a sandbox.
	if (!sb || sb == sandbox_current()) return;

	// again sandbox should be done and waiting for the parent.
	// TODO: this needs to be enhanced. you may be killing a sandbox when its in any other execution states.
	if (sb->state != SANDBOX_RETURNED) return;
	module_release(sb->mod);

	free(sb->args);
	// remove stack! and also heap!
	int ret = munmap(sb->stack_start, sb->stack_size);
	if (ret) perror("munmap");

	// depending on the memory type
	free_linear_memory(sb->linear_start, sb->linear_size, sb->linear_max_size);
}
