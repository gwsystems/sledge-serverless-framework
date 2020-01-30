#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>

#define BUF_MAX  44
#define RDWR_MAX 1
#if RDWR_MAX > 1
int
main(int argc, char **argv) __attribute__((optnone))
{
	char buf[RDWR_MAX][BUF_MAX] = { 0 };

	printf("%s enter [in:%s, out:%s]\n", argv[0], argv[1], argv[2]);
	int fdr = open(argv[1], O_RDONLY, S_IRUSR | S_IRGRP);
	if (fdr < 0) {
		perror("fopen");
		return -1;
	}
	int fdw = creat(argv[2], S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (fdw < 0) {
		perror("creat");
		return -1;
	}

	int          n             = 0;
	struct iovec iov[RDWR_MAX] = { 0 };
	for (int i = 0; i < RDWR_MAX; i++) {
		iov[i].iov_base = buf[i];
		iov[i].iov_len  = BUF_MAX;
	}
	while ((n = readv(fdr, iov, RDWR_MAX)) > 0) {
		int wvcnt = n / BUF_MAX;
		if (n % BUF_MAX) {
			iov[wvcnt].iov_len = n % BUF_MAX;
			wvcnt++;
		}
		if (writev(fdw, iov, wvcnt) != n) perror("writev");

		memset(buf, 0, RDWR_MAX * BUF_MAX);
		for (int i = 0; i < RDWR_MAX; i++) {
			iov[i].iov_base = buf[i];
			iov[i].iov_len  = BUF_MAX;
		}
		n = 0;
	}

	close(fdr);
	close(fdw);

	printf("%s done\n", argv[0]);

	return 0;
}

#else
int
main(int argc, char **argv) __attribute__((optnone))
{
	char buf[BUF_MAX] = { 0 };

	printf("%s enter [in:%s, out:%s]\n", argv[0], argv[1], argv[2]);
#ifdef USE_OPEN
	int fdr = open(argv[1], O_RDONLY, S_IRUSR | S_IRGRP);
	if (fdr < 0) {
		perror("fopen");
		return -1;
	}
	int fdw = creat(argv[2], S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (fdw < 0) {
		perror("creat");
		return -1;
	}

	int n = 0;
	while ((n = read(fdr, buf, BUF_MAX)) > 0) {
		if (write(fdw, buf, n) != n) perror("write");
		memset(buf, 0, BUF_MAX);
		n = 0;
	}
	if (n < 0) perror("read");

	close(fdr);
	close(fdw);

#else
	FILE *fpr = fopen(argv[1], "r");
	if (!fpr) {
		perror("fopen");
		return -1;
	}
	FILE *fpw = fopen(argv[2], "w");
	if (!fpw) {
		perror("fopen");
		return -1;
	}

	while (!feof(fpr)) {
		char *p = NULL;
		if ((p = fgets(buf, BUF_MAX, fpr)) == NULL)
			perror("fgets");
		else {
			if (fputs(p, fpw) < 0) perror("fputs");
			p = NULL;
		}
		memset(buf, 0, BUF_MAX);
	}

	fclose(fpr);
	fclose(fpw);
#endif
	printf("%s done\n", argv[0]);

	return 0;
}
#endif
