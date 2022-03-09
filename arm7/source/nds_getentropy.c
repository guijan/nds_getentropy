#include <nds.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HD(x)	 (swiSHA1Update(&ctx, (char *)&(x), sizeof (x)))

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static unsigned char results[20];
static int runs;

static void clear_results(void);

int
arm7_getentropy(void *_buf, size_t buflen)
{
	unsigned char *buf = _buf;
	int i, j;
	swiSHA1context_t ctx;
	uint16_t tmp16;
	RTCtime time;
	void *p;
	unsigned char tscs[] = {
		TSC_MEASURE_AUX, TSC_MEASURE_BATTERY, TSC_MEASURE_TEMP1,
		TSC_MEASURE_TEMP2, TSC_MEASURE_X, TSC_MEASURE_Y, TSC_MEASURE_Z1,
		TSC_MEASURE_Z2,
	};

	if (buflen > 256) {
		errno = EIO;
		return (-1);
	}

	if (runs == 0)
		atexit(clear_results);
	runs++;

	/*
	 * XXX: The great majority of entropy sources are extremely likely to
	 * return the exact same result each loop iteration.
	 */
	for (i = 0; i < buflen; i += MIN(buflen-i, sizeof(results))) {
		swiSHA1Init(&ctx);
		for (j = 0; j < (sizeof(tscs) / sizeof(tscs[0])); j++) {
			tmp16 = touchRead(tscs[j]);
			HD(tmp16);
		}
		rtcGetTimeAndDate(&time);
		HD(time);

		tmp16 = REG_SOUNDCNT;
		HD(tmp16);
		tmp16 = REG_KEYXY;
		HD(tmp16);

		HD(*PersonalData);

		/*
		 * We don't have ASLR, but the address of this function might
		 * still be different between projects.
		 */
		HD(arm7_getentropy);

		/* Stack address. */
		HD(p);

		/* Deliberately uninitialized when i == 0. */
		HD(results);

		swiSHA1Final(results, &ctx);
		memcpy(buf+i, results, MIN(buflen-i, sizeof(results)));
	}

	explicit_bzero(&ctx, sizeof(ctx));
#if 0
	/*
	 * XXX: I'm leaving `results` statically allocated and uncleared in a
	 * desperate attempt to get different results between every call.
	 */
	explicit_bzero(results, sizeof(results));
#endif

	return (0);
}

static void
clear_results(void)
{
	/* Results is not cleared in case of abnormal termination. */
	explicit_bzero(results, sizeof(results));

	/*
	 * Set `runs` to 0 so arm7_getentropy() installs us again in case
	 * another atexit() handler calls us.
	 */
	runs = 0;
}

/* Scribble for persistent seed. */
#if 0
static void
read_seed(void)
{
	int errnum;
	int fd;
	struct stat st;
	size_t filesize;
	size_t n, i;
	unsigned char zeroes[sizeof(results)] = {0};
	errnum = errno;

	while ((fd = open("/nds", O_RDONLY|O_CREAT|O_DIRECTORY)) == -1) {
		if (errno != EINTR)
			break;
	}
	close(fd);

	/* O_SYNC because we need to guarantee the seed is cleared later.
	 * There's no openat on devkitARM.
	 */
	while ((fd = open("/nds/random.seed", O_RDWR|O_CREAT|O_SYNC)) == -1) {
		if (errno != EINTR)
			goto end;
	}

	i = 0;
	while ((n = read(fd, results+i, sizeof(results)-i))
	    != sizeof(results)-i) {
		if (n == (size_t)-1) {
			if (errno == EINTR)
				continue;
			/* If we haven't read the seed, return. */
			if (i == 0)
				goto end;
			/* If we've partially read the seed, carry on. */
			break;
		}
		/* The file is empty. It could be that the length is 0 or we've
		 * reached EOF prematurely. Both cases are harmless, just carry
		 * on.
		 */
		if (n == 0)
			break;
		i += n;
	}

	lseek(fd, 0, SEEK_SET);
	if (fstat(fd, &st) == -1)
		filesize = sizeof(results); /* Better than nothing. */
	else
		filesize = st.st_size;

	for (i = 0; i < filesize; i += n) {
		/*
		 * This is a matter of robustness, the library might be
		 * upgraded to a new hash algorithm with a bigger hash, so we
		 * always clear the whole file. But in most cases this whole
		 * loop will only run once.
		 */
		if ((n = write(fd, zeroes, sizeof(zeroes))) == (size_t)-1) {
			if (errno == EINTR) {
				n = 0;
				continue;
			}
			/*
			 * No way to recover from the error now, just keep going
			 * and hope for the best.
			 */
			break;
		}
	}
	ftruncate(fd, sizeof(zeroes));

end:
	close(fd);
	errno = errnum;
}
#endif
