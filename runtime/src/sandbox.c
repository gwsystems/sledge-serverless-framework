#include <assert.h>
#include <runtime.h>
#include <sandbox.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>
#include <uv.h>
#include <http_api.h>

static inline struct sandbox *
sandbox_memory_map(struct module *m)
{
	unsigned long mem_sz = SBOX_MAX_MEM; // 4GB
#ifndef STANDALONE
	unsigned long sb_sz = sizeof(struct sandbox) + m->max_rr_sz;
#else
	unsigned long sb_sz = sizeof(struct sandbox);
#endif
	unsigned long lm_sz = WASM_PAGE_SIZE * WASM_START_PAGES;

	if (lm_sz + sb_sz > mem_sz) return NULL;
	assert(round_up_to_page(sb_sz) == sb_sz);
	unsigned long rw_sz = sb_sz + lm_sz; 
	void *addr = mmap(NULL, mem_sz + /* guard page */ PAGE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); 
	if (addr == MAP_FAILED) return NULL;

	void *addr_rw = mmap(addr, sb_sz + lm_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (addr_rw == MAP_FAILED) {
		munmap(addr, mem_sz + PAGE_SIZE);
		return NULL;
	}

	struct sandbox *s = (struct sandbox *)addr;
	// can it include sandbox as well?
	s->linear_start = (char *)addr + sb_sz;
	s->linear_size = lm_sz;
	s->mod = m;
	s->sb_size = sb_sz;
	module_acquire(m);

	return s;
}

static inline void
sandbox_args_setup(i32 argc)
{
	struct sandbox *curr = sandbox_current();
	char *args = sandbox_args();
	if (!args) return;

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
		strncpy(get_memory_ptr_for_runtime(string_off, str_sz), arg, strlen(arg));

		string_off += str_sz;
	}
}

static inline void
sb_read_callback(uv_stream_t *s, ssize_t nr, const uv_buf_t *b)
{
#ifndef STANDALONE
	struct sandbox *c = s->data;

	if (nr > 0) {
		if (http_request_parse_sb(c, nr) != 0) return;
		c->rr_data_len += nr;
                struct http_request *rh = &c->rqi;
		if (!rh->message_end) return;
	}

	uv_read_stop(s);
	sandbox_wakeup(c);
#endif
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
#ifndef STANDALONE
	struct sandbox *c = w->data;
	if (status < 0) {
		c->cuvsr.data = c;
		uv_shutdown(&c->cuvsr, (uv_stream_t *)&c->cuv, sb_shutdown_callback);
		return;
	}
	sandbox_wakeup(c);
#endif
}

static inline void
sb_alloc_callback(uv_handle_t *h, size_t suggested, uv_buf_t *buf)
{
	struct sandbox *c = h->data;

#ifndef STANDALONE
	size_t l = (c->mod->max_rr_sz - c->rr_data_len);
	buf->base = (c->req_resp_data + c->rr_data_len);
	buf->len = l > suggested ? suggested : l;
#endif
}

static inline int
sandbox_client_request_get(void)
{
#ifndef STANDALONE
	struct sandbox *curr = sandbox_current();

	curr->rr_data_len = 0;
#ifndef USE_UVIO
	int r = 0;
	r = recv(curr->csock, (curr->req_resp_data), curr->mod->max_req_sz, 0);
	if (r <= 0) {
		perror("recv");
		return r;
	}
	while (r > 0) {
		if (http_request_parse(r) != 0) return -1;
		curr->rr_data_len += r;
		struct http_request *rh = &curr->rqi;
		if (rh->message_end) break;

		r = recv(curr->csock, (curr->req_resp_data + r), curr->mod->max_req_sz - r, 0);
		if (r < 0) {
			perror("recv");
			return r;
		}
	}
#else
	int r = uv_read_start((uv_stream_t *)&curr->cuv, sb_alloc_callback, sb_read_callback);
	sandbox_block();
	if (curr->rr_data_len == 0) return 0;
#endif

	return 1;
#else
	return 1;
#endif
}

static inline int
sandbox_client_response_set(void)
{
#ifndef STANDALONE
	struct sandbox *curr = sandbox_current();

#ifndef USE_UVIO
	strcpy(curr->req_resp_data + curr->rr_data_len, "HTTP/1.1 200 OK\r\n");

	// TODO: response set in req_resp_data
	curr->rr_data_len += strlen("HTTP/1.1 200 OK\r\n");

	int r = send(curr->csock, curr->req_resp_data, curr->rr_data_len, 0);
	if (r < 0) perror("send");
#else
	int bodylen = curr->rr_data_len;
	if (bodylen > 0) {
		http_response_body_set(curr->req_resp_data, bodylen);
		char len[16] = { 0 };
		sprintf(len, "%d", bodylen);
		//content-length = body length
		char *key = curr->req_resp_data + curr->rr_data_len;
		int lenlen = strlen("content-length: "), dlen = strlen(len);
		strcpy(key, "content-length: ");
		strncat(key + lenlen, len, dlen);
		strncat(key + lenlen + dlen, "\r\n", 2);
		http_response_header_set(key, lenlen + dlen + 2);
		curr->rr_data_len += lenlen + dlen + 2;

		//content-type as set in the headers.
		key = curr->req_resp_data + curr->rr_data_len;
		strcpy(key, "content-type: ");
		lenlen = strlen("content-type: ");
		dlen = strlen(curr->mod->rspctype);
		if (dlen == 0) {
			int l = strlen("text/plain\r\n\r\n");
			strncat(key + lenlen, "text/plain\r\n\r\n", l);
			http_response_header_set(key, lenlen + l);
			curr->rr_data_len += lenlen + l;
		} else {
			strncat(key + lenlen, curr->mod->rspctype, dlen);
			strncat(key + lenlen + dlen, "\r\n\r\n", 4);
			http_response_header_set(key, lenlen + dlen + 4);
			curr->rr_data_len += lenlen + dlen + 4;
		}
		//TODO - other headers requested in module!
	}

	char *st = curr->req_resp_data + curr->rr_data_len;
	strcpy(st, "HTTP/1.1 200 OK\r\n");
	curr->rr_data_len += strlen("HTTP/1.1 200 OK\r\n");

	http_response_status_set(st, strlen("HTTP/1.1 200 OK\r\n"));
	uv_write_t req = { .data = curr, };
	int n = http_response_uv();
	int r = uv_write(&req, (uv_stream_t *)&curr->cuv, curr->rsi.bufs, n, sb_write_callback);
	sandbox_block();
#endif
	return r;
#else
	return 0;
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
	sandbox_args_setup(argc);

#ifndef STANDALONE
	http_parser_init(&curr->hp, HTTP_REQUEST);
	curr->hp.data = curr;
#ifdef USE_UVIO
	int r = uv_tcp_init(runtime_uvio(), (uv_tcp_t *)&curr->cuv);
	assert(r == 0);
	curr->cuv.data = curr;
	r = uv_tcp_open((uv_tcp_t *)&curr->cuv, curr->csock);
	assert(r == 0);
#endif
	if (sandbox_client_request_get() > 0)
#endif
	{
		curr->rr_data_len = 0; // TODO: do this on first write to body.
		alloc_linear_memory();

		// perhaps only initialized for the first instance? or TODO!
		//module_table_init(curr_mod);
		module_memory_init(curr_mod);
		curr->retval = module_entry(curr_mod, argc, curr->args_offset);

		sandbox_client_response_set();
	}

#ifndef STANDALONE
#ifdef USE_UVIO
	uv_close((uv_handle_t *)&curr->cuv, sb_close_callback);
	sandbox_block();
#else
	close(curr->csock);
#endif
#endif
	sandbox_exit();
}

struct sandbox *
sandbox_alloc(struct module *mod, char *args, int sock, const struct sockaddr *addr)
{
	if (!module_is_valid(mod)) return NULL;

	// FIXME: don't use malloc. huge security problem!
	// perhaps, main should be in its own sandbox, when it is not running any sandbox.
	struct sandbox *sb = (struct sandbox *)sandbox_memory_map(mod);
	if (!sb) return NULL;

	//actual module instantiation!
	sb->args = (void *)args;
	sb->stack_size = mod->stack_size;
	sb->stack_start = mmap(NULL, sb->stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN, -1, 0);
	if (sb->stack_start == MAP_FAILED) {
		perror("mmap");
		assert(0);
	}
#ifndef STANDALONE
	sb->csock = sock;
	if (addr) memcpy(&sb->client, addr, sizeof(struct sockaddr));
#endif
	for (int i = 0; i < SBOX_MAX_OPEN; i++) sb->handles[i].fd = -1;
	ps_list_init_d(sb);

	arch_context_init(&sb->ctxt, (reg_t)sandbox_entry, (reg_t)(sb->stack_start + sb->stack_size));
#ifdef STANDALONE
	sandbox_run(sb);
#else
#ifndef SBOX_SCALE_ALLOC
	sandbox_run(sb);
#endif
#endif

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

	module_release(sb->mod);

	// TODO free(sb->args);
	void *stkaddr = sb->stack_start;
	size_t stksz = sb->stack_size;

	// depending on the memory type
	free_linear_memory(sb->linear_start, sb->linear_size, sb->linear_max_size);

	// mmaped memory includes sandbox structure in there.
	ret = munmap(sb, SBOX_MAX_MEM + PAGE_SIZE);
	if (ret) perror("munmap sandbox");

	// remove stack!
	// for some reason, removing stack seem to cause crash in some cases. 
	// TODO: debug more. 
	ret = munmap(stkaddr, stksz);
	if (ret) perror("munmap stack");
}
