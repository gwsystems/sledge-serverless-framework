#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <runtime.h>
#include <sandbox.h>
#include <softint.h>
#include <util.h>

#define MOD_LINE_MAX 1024

i32 logfd = -1;

u32 ncores = 0, sbox_ncores = 0, sbox_core_st = 0;

pthread_t rtthd[SBOX_NCORES];

static void
usage(char *cmd)
{
	printf("%s <modules_file>\n", cmd);
	debuglog("%s <modules_file>\n", cmd);
}

int
main(int argc, char* argv[])
{
#ifndef STANDALONE
	int i = 0, rtthd_ret[SBOX_NCORES] = { 0 };
	memset(rtthd, 0, sizeof(pthread_t)*SBOX_NCORES);

	if (argc != 2) {
		usage(argv[0]);

		exit(-1);
	}

	ncores = sysconf(_SC_NPROCESSORS_ONLN);

	if (ncores > 1) {
		u32 x = ncores - 1;
		sbox_ncores = SBOX_NCORES;
		if (x < SBOX_NCORES) sbox_ncores = x;
		sbox_core_st = 1;
	} else {
		sbox_ncores = 1;
	}
	debuglog("Number of cores %u, sandboxing cores %u (start: %u) and module reqs %u\n", ncores, sbox_ncores, sbox_core_st, MOD_REQ_CORE);

#ifdef NOSTDIO
	fclose(stdout);
	fclose(stderr);
	fclose(stdin);
	logfd = open(LOGFILE, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU | S_IRWXG);
	if (logfd < 0) {
		perror("open");
		exit(-1);
	}
#else
	logfd = 1;
#endif

	runtime_init();
	debuglog("Parsing modules file [%s]\n", argv[1]);
	if (util_parse_modules_file_json(argv[1])) {
//	if (util_parse_modules_file_custom(argv[1])) {
		printf("failed to parse modules file[%s]\n", argv[1]);

		exit(-1);
	}

	for (i = 0; i < sbox_ncores; i++) {
		int ret = pthread_create(&rtthd[i], NULL, sandbox_run_func, (void *)&rtthd_ret[i]);
		if (ret) {
			errno = ret;
			perror("pthread_create");
			exit(-1);
		}

		cpu_set_t cs;
		CPU_ZERO(&cs);
		CPU_SET(sbox_core_st + i, &cs);
		ret = pthread_setaffinity_np(rtthd[i], sizeof(cs), &cs);
		assert(ret == 0);
	}
	debuglog("Sandboxing environment ready!\n");
	
	for (i = 0; i < sbox_ncores; i++) {
		int ret = pthread_join(rtthd[i], NULL);
		if (ret) {
			errno = ret;
			perror("pthread_join");
			exit(-1);
		}
	}

	// runtime threads run forever!! so join should not return!!
	printf("\nOh no..!! This can't be happening..!!\n");
	exit(-1);
#else /* STANDALONE */
	arch_context_init(&base_context, 0, 0);
	uv_loop_init(&uvio);

	/* in current dir! */
	struct module *m = module_alloc(argv[1], argv[1], 0, 0, 0, 0, 0, 0);
	assert(m);
	struct sandbox *s = sandbox_alloc(m, argv[1], NULL);

	exit(0);
#endif
}
