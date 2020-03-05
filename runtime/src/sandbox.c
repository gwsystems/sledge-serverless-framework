#include <assert.h>
#include <runtime.h>
#include <sandbox.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>
#include <uv.h>
#include <http_api.h>

static inline struct sandbox *
sandbox_memory_map(struct module *module)
{
	unsigned long mem_sz = SBOX_MAX_MEM; // 4GB
	unsigned long sb_sz = sizeof(struct sandbox) + module->max_request_or_response_size;
	unsigned long lm_sz = WASM_PAGE_SIZE * WASM_START_PAGES;

	if (lm_sz + sb_sz > mem_sz) return NULL;
	assert(round_up_to_page(sb_sz) == sb_sz);
	unsigned long rw_sz = sb_sz + lm_sz;
	void *addr = mmap(NULL, sb_sz + mem_sz + /* guard page */ PAGE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1,
	                  0);
	if (addr == MAP_FAILED) return NULL;

	void *addr_rw = mmap(addr, sb_sz + lm_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1,
	                     0);
	if (addr_rw == MAP_FAILED) {
		munmap(addr, mem_sz + PAGE_SIZE);
		return NULL;
	}

	struct sandbox *s = (struct sandbox *)addr;
	// can it include sandbox as well?
	s->linear_start = (char *)addr + sb_sz;
	s->linear_size  = lm_sz;
	s->module          = module;
	s->sb_size      = sb_sz;
	module_acquire(module);

	return s;
}

static inline void
sandbox_args_setup(i32 argc)
{
	struct sandbox *curr = sandbox_current();
	char *          args = sandbox_args();

	// whatever gregor has, to be able to pass args to a module!
	curr->args_offset = sandbox_lmbound;
	assert(sandbox_lmbase == curr->linear_start);
	expand_memory();

	i32 *array_ptr  = get_memory_ptr_void(curr->args_offset, argc * sizeof(i32));
	i32  string_off = curr->args_offset + (argc * sizeof(i32));

	for (int i = 0; i < argc; i++) {
		char * arg    = args + (i * MOD_ARG_MAX_SZ);
		size_t str_sz = strlen(arg) + 1;

		array_ptr[i] = string_off;
		// why get_memory_ptr_for_runtime??
		strncpy(get_memory_ptr_for_runtime(string_off, str_sz), arg, strlen(arg));

		string_off += str_sz;
	}
	stub_init(string_off);
}

static inline void
sb_read_callback(uv_stream_t *s, ssize_t nr, const uv_buf_t *b)
{
	struct sandbox *c = s->data;

	if (nr > 0) {
		if (http_request_parse_sb(c, nr) != 0) return;
		c->rr_data_len += nr;
		struct http_request *rh = &c->rqi;
		if (!rh->message_end) return;
	}

	uv_read_stop(s);
	sandbox_wakeup(c);
}

static inline void
sb_close_callback(uv_handle_t *s)
{
	struct sandbox *c = s->data;
	sandbox_wakeup(c);
}

static inline void
sb_shutdown_callback(uv_shutdown_t *req, int status)
{
	struct sandbox *c = req->data;
	sandbox_wakeup(c);
}

static inline void
sb_write_callback(uv_write_t *w, int status)
{
	struct sandbox *c = w->data;
	if (status < 0) {
		c->cuvsr.data = c;
		uv_shutdown(&c->cuvsr, (uv_stream_t *)&c->cuv, sb_shutdown_callback);
		return;
	}
	sandbox_wakeup(c);
}

static inline void
sb_alloc_callback(uv_handle_t *h, size_t suggested, uv_buf_t *buf)
{
	struct sandbox *c = h->data;
	size_t l  = (c->module->max_request_or_response_size - c->rr_data_len);
	buf->base = (c->req_resp_data + c->rr_data_len);
	buf->len  = l > suggested ? suggested : l;
}

static inline int
sandbox_client_request_get(void)
{
	struct sandbox *curr = sandbox_current();
	curr->rr_data_len = 0;
#ifndef USE_HTTP_UVIO
	int r = 0;
	r     = recv(curr->csock, (curr->req_resp_data), curr->module->max_request_size, 0);
	if (r <= 0) {
		if (r < 0) perror("recv1");
		return r;
	}
	while (r > 0) {
		if (http_request_parse(r) != 0) return -1;
		curr->rr_data_len += r;
		struct http_request *rh = &curr->rqi;
		if (rh->message_end) break;

		r = recv(curr->csock, (curr->req_resp_data + curr->rr_data_len),
		         curr->module->max_request_size - curr->rr_data_len, 0);
		if (r < 0) {
			perror("recv2");
			return r;
		}
	}
#else
	int r = uv_read_start((uv_stream_t *)&curr->cuv, sb_alloc_callback, sb_read_callback);
	sandbox_block_http();
	if (curr->rr_data_len == 0) return 0;
#endif
	return 1;
}

/**
 * Sends Response Back to Client
 * @return RC. -1 on Failure
 **/
static inline int
sandbox_client_response_set(void)
{
	int             sndsz       = 0;
	struct sandbox *curr        = sandbox_current();
	int             rsp_hdr_len = strlen(HTTP_RESP_200OK) + strlen(HTTP_RESP_CONTTYPE) + strlen(HTTP_RESP_CONTLEN);
	int             bodylen     = curr->rr_data_len - rsp_hdr_len;

	memset(curr->req_resp_data, 0,
	       strlen(HTTP_RESP_200OK) + strlen(HTTP_RESP_CONTTYPE) + strlen(HTTP_RESP_CONTLEN));
	strncpy(curr->req_resp_data, HTTP_RESP_200OK, strlen(HTTP_RESP_200OK));
	sndsz += strlen(HTTP_RESP_200OK);

	if (bodylen == 0) goto done;
	strncpy(curr->req_resp_data + sndsz, HTTP_RESP_CONTTYPE, strlen(HTTP_RESP_CONTTYPE));
	if (strlen(curr->module->response_content_type) <= 0) {
		strncpy(curr->req_resp_data + sndsz + strlen("Content-type: "), HTTP_RESP_CONTTYPE_PLAIN,
		        strlen(HTTP_RESP_CONTTYPE_PLAIN));
	} else {
		strncpy(curr->req_resp_data + sndsz + strlen("Content-type: "), curr->module->response_content_type,
		        strlen(curr->module->response_content_type));
	}
	sndsz += strlen(HTTP_RESP_CONTTYPE);
	char len[10] = { 0 };
	sprintf(len, "%d", bodylen);
	strncpy(curr->req_resp_data + sndsz, HTTP_RESP_CONTLEN, strlen(HTTP_RESP_CONTLEN));
	strncpy(curr->req_resp_data + sndsz + strlen("Content-length: "), len, strlen(len));
	sndsz += strlen(HTTP_RESP_CONTLEN);
	sndsz += bodylen;

done:
	assert(sndsz == curr->rr_data_len);
	// Get End Timestamp
	curr->total_time = rdtsc() - curr->start_time;
	printf("Function returned in %lu cycles\n", curr->total_time);

#ifndef USE_HTTP_UVIO
	int r = send(curr->csock, curr->req_resp_data, sndsz, 0);
	if (r < 0) {
		perror("send");
		return -1;
	}
	while (r < sndsz) {
		int s = send(curr->csock, curr->req_resp_data + r, sndsz - r, 0);
		if (s < 0) {
			perror("send");
			return -1;
		}
		r += s;
	}
#else
	uv_write_t req = {
		.data = curr,
	};
	uv_buf_t bufv = uv_buf_init(curr->req_resp_data, sndsz);
	int      r    = uv_write(&req, (uv_stream_t *)&curr->cuv, &bufv, 1, sb_write_callback);
	sandbox_block_http();
#endif
	return 0;
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
	int            argc     = module_argument_count(curr_mod);
	// for stdio
	int f = io_handle_open(0);
	assert(f == 0);
	f = io_handle_open(1);
	assert(f == 1);
	f = io_handle_open(2);
	assert(f == 2);

	http_parser_init(&curr->hp, HTTP_REQUEST);
	curr->hp.data = curr;
	// NOTE: if more headers, do offset by that!
	int rsp_hdr_len = strlen(HTTP_RESP_200OK) + strlen(HTTP_RESP_CONTTYPE) + strlen(HTTP_RESP_CONTLEN);
#ifdef USE_HTTP_UVIO
	int r = uv_tcp_init(runtime_uvio(), (uv_tcp_t *)&curr->cuv);
	assert(r == 0);
	curr->cuv.data = curr;
	r              = uv_tcp_open((uv_tcp_t *)&curr->cuv, curr->csock);
	assert(r == 0);
#endif
	if (sandbox_client_request_get() > 0)
	{
		curr->rr_data_len = rsp_hdr_len; // TODO: do this on first write to body.
		alloc_linear_memory();
		// perhaps only initialized for the first instance? or TODO!
		// module_table_init(curr_mod);
		module_globals_init(curr_mod);
		module_memory_init(curr_mod);
		sandbox_args_setup(argc);

		curr->retval = module_entry(curr_mod, argc, curr->args_offset);

		sandbox_client_response_set();
	}

#ifdef USE_HTTP_UVIO
	uv_close((uv_handle_t *)&curr->cuv, sb_close_callback);
	sandbox_block_http();
#else
	close(curr->csock);
#endif
	sandbox_exit();
}

struct sandbox *
sandbox_alloc(struct module *module, char *args, int sock, const struct sockaddr *addr, u64 start_time)
{
	if (!module_is_valid(module)) return NULL;

	// FIXME: don't use malloc. huge security problem!
	// perhaps, main should be in its own sandbox, when it is not running any sandbox.
	struct sandbox *sb = (struct sandbox *)sandbox_memory_map(module);
	if (!sb) return NULL;

	// Assign the start time from the request
	sb->start_time = start_time;

	// actual module instantiation!
	sb->args        = (void *)args;
	sb->stack_size  = module->stack_size;
	sb->stack_start = mmap(NULL, sb->stack_size, PROT_READ | PROT_WRITE,
	                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN, -1, 0);
	if (sb->stack_start == MAP_FAILED) {
		perror("mmap");
		assert(0);
	}
	sb->csock = sock;
	if (addr) memcpy(&sb->client, addr, sizeof(struct sockaddr));
	for (int i = 0; i < SBOX_MAX_OPEN; i++) sb->handles[i].fd = -1;
	ps_list_init_d(sb);

	arch_context_init(&sb->ctxt, (reg_t)sandbox_entry, (reg_t)(sb->stack_start + sb->stack_size));
	return sb;
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

	int sz = sizeof(struct sandbox);

	sz += sb->module->max_request_or_response_size;
	module_release(sb->module);

	// TODO free(sb->args);
	void * stkaddr = sb->stack_start;
	size_t stksz   = sb->stack_size;

	// depending on the memory type
	// free_linear_memory(sb->linear_start, sb->linear_size, sb->linear_max_size);

	// mmaped memory includes sandbox structure in there.
	ret = munmap(sb, sz);
	if (ret) perror("munmap sandbox");

	// remove stack!
	// for some reason, removing stack seem to cause crash in some cases.
	// TODO: debug more.
	ret = munmap(stkaddr, stksz);
	if (ret) perror("munmap stack");
}
