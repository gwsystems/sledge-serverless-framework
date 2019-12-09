#include <assert.h>
#include <runtime.h>
#include <sandbox.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>
#include <uv.h>

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

static void
sandbox_uvio_init(struct sandbox *c)
{
#ifndef STANDALONE
	int ret = uv_udp_init(runtime_uvio(), &c->clientuv);
	assert(ret == 0);
	//ret = uv_udp_bind(&c->clientuv, (const struct sockaddr *)&c->mod->srvaddr, 0);
	//assert(ret >= 0);
	
	//ret = uv_udp_connect(&c->clientuv, &c->client);
	//assert(ret == 0);
	//c->clientuv.data = (void *)c;
	//struct sockaddr_in addr;
	//int len = sizeof(addr);
	//ret = uv_udp_getpeername(&c->clientuv, &addr, &len);
	//assert(ret == 0);
	//printf("Peer's IP address is: %s\n", inet_ntoa(addr.sin_addr));
        //printf("Peer's port is: %d\n", (int) ntohs(addr.sin_port));
#endif
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

	sandbox_uvio_init(curr);

	alloc_linear_memory();
	
	// perhaps only initialized for the first instance? or TODO!
	//module_table_init(curr_mod);
	module_memory_init(curr_mod);

	sandbox_args_setup(argc);

	curr->retval = module_entry(curr_mod, argc, curr->args_offset);

	sandbox_exit();
}

struct sandbox *
sandbox_alloc(struct module *mod, char *args, const struct sockaddr *addr)
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
	if (addr) memcpy(&sb->client, addr, sizeof(struct sockaddr));

	arch_context_init(&sb->ctxt, (reg_t)sandbox_entry, (reg_t)(sb->stack_start + sb->stack_size));
	sandbox_run(sb);

	return sb;
}

void
sandbox_udp_send_callback(uv_udp_send_t *req, int status)
{
	struct sandbox *c = req->data;
	c->retval = status;

	sandbox_wakeup(c);
}

void
sandbox_response(void)
{
	struct sandbox *sb = sandbox_current();
        // send response.
#ifndef STANDALONE
	int sock = -1, ret;
	char resp[SBOX_RESP_STRSZ] = { 0 };
	// sends return value only for now!
	sprintf(resp, "%d", sb->retval);
#ifdef USE_SYSCALL
	// FIXME, with USE_SYSCALL, we should not be using uv at all.
	int ret = uv_fileno((uv_handle_t *)&sb->mod->udpsrv, &sock);
	assert(ret == 0);
	// using system call here because uv_udp_t is in the "module listener thread"'s loop, cannot access here. also dnot want to mess with cross-core/cross-thread uv loop states or structures.
	ret = sendto(sock, resp, strlen(resp), 0, &sb->client, sizeof(struct sockaddr));
	assert(ret == strlen(resp));
#elif USE_UVIO
	uv_udp_send_t req = { .data = sb, };
	uv_buf_t b = uv_buf_init(resp, strlen(resp));
	ret = uv_udp_send(&req, &sb->clientuv, &b, 1, &sb->client, sandbox_udp_send_callback);
	assert(ret == 0);
	sandbox_block();
#else
	assert(0);
#endif
#endif
}

void
sandbox_free(struct sandbox *sb)
{
	int ret;

	// you have to context switch away to free a sandbox.
	if (!sb || sb == sandbox_current()) return;

	// again sandbox should be done and waiting for the parent.
	// TODO: this needs to be enhanced. you may be killing a sandbox when its in any other execution states.
	if (sb->state != SANDBOX_RETURNED) return;

	module_release(sb->mod);

	free(sb->args);
	// remove stack! and also heap!
	ret = munmap(sb->stack_start, sb->stack_size);
	if (ret) perror("munmap");

	// depending on the memory type
	free_linear_memory(sb->linear_start, sb->linear_size, sb->linear_max_size);

	free(sb);
	// sb is a danging-ptr!
}
