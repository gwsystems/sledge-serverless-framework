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
#include <sys/time.h>
#include <sys/resource.h>

// TODO: I think this define is unused
#define MOD_LINE_MAX 1024

i32 logfd = -1; 	  // Log File Descriptor
u32 ncores = 0; 	  // Number of cores
u32 sbox_ncores = 0;  // Number of Sandboxing Cores
u32 sbox_core_st = 0; // First Sandbox Core 

pthread_t rtthd[SBOX_NCORES]; // An array of runtime threads

static unsigned long long
get_time()
{
	struct timeval Tp;
	int            stat;
	stat = gettimeofday(&Tp, NULL);
	if (stat != 0) printf("Error return from gettimeofday: %d", stat);
	return (Tp.tv_sec * 1000000 + Tp.tv_usec);
}


static void
usage(char *cmd)
{
	printf("%s <modules_file>\n", cmd);
	debuglog("%s <modules_file>\n", cmd);
}

int
main(int argc, char **argv)
{
#ifndef STANDALONE
	int i = 0, rtthd_ret[SBOX_NCORES] = { 0 };
	memset(rtthd, 0, sizeof(pthread_t) * SBOX_NCORES);

	if (argc != 2) {
		usage(argv[0]);
		exit(-1);
	}

	struct rlimit r;
	if (getrlimit(RLIMIT_DATA, &r) < 0) {
		perror("getrlimit RLIMIT_DATA");
		exit(-1);
	}
	r.rlim_cur = r.rlim_max;
	if (setrlimit(RLIMIT_DATA, &r) < 0) {
		perror("setrlimit RLIMIT_DATA");
		exit(-1);
	}
	if (getrlimit(RLIMIT_NOFILE, &r) < 0) {
		perror("getrlimit RLIMIT_NOFILE");
		exit(-1);
	}
	r.rlim_cur = r.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &r) < 0) {
		perror("setrlimit RLIMIT_NOFILE");
		exit(-1);
	}

	ncores = sysconf(_SC_NPROCESSORS_ONLN);
	if (ncores > 1) {
		u32 x       = ncores - 1;
		sbox_ncores = SBOX_NCORES;
		if (x < SBOX_NCORES) sbox_ncores = x;
		sbox_core_st = 1;
	} else {
		sbox_ncores = 1;
	}
	debuglog("Number of cores %u, sandboxing cores %u (start: %u) and module reqs %u\n", ncores, sbox_ncores,
	         sbox_core_st, MOD_REQ_CORE);

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
		printf("failed to parse modules file[%s]\n", argv[1]);

		exit(-1);
	}
	runtime_thd_init();

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

	int   ac   = 1;
	char *args = argv[1];
	if (argc - 1 > 1) {
		ac        = argc - 1;
		char **av = argv + 1;
		args      = malloc(sizeof(char) * MOD_ARG_MAX_SZ * ac);
		memset(args, 0, sizeof(char) * MOD_ARG_MAX_SZ * ac);

		for (int i = 0; i < ac; i++) {
			char *a = args + (i * MOD_ARG_MAX_SZ * sizeof(char));
			strcpy(a, av[i]);
		}
	}

	/* in current dir! */
	struct module *m = module_alloc(args, args, ac, 0, 0, 0, 0, 0, 0);
	assert(m);

	// unsigned long long st = get_time(), en;
	struct sandbox *s = sandbox_alloc(m, args, 0, NULL);
	// en = get_time();
	// fprintf(stderr, "%llu\n", en - st);

	exit(0);
#endif
}
