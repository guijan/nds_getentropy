/*
 * Copyright (c) 2022 Guilherme Janczak <guilherme.janczak@yandex.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <nds.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sha1.h"

#define HD(x)	 (SHA1Update(&ctx, (void *)&(x), sizeof (x)))

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static unsigned char results[SHA1_DIGEST_LENGTH];
static int runs;

static void clear_results(void);

int
arm7_getentropy(void *_buf, size_t buflen)
{
	unsigned char *buf = _buf;
	int nw, i;
	SHA1_CTX ctx;
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

	/*
	 * XXX: The great majority of entropy sources are extremely likely to
	 * return the exact same result each loop iteration.
	 */
	for (nw = 0; nw < buflen; nw += MIN(buflen-nw, sizeof(results))) {
		SHA1Init(&ctx);
		for (i = 0; i < (sizeof(tscs) / sizeof(tscs[0])); i++) {
			tmp16 = touchRead(tscs[i]);
			HD(tmp16);
		}
		rtcGetTimeAndDate((void *)&time);
		HD(time);

		tmp16 = REG_SOUNDCNT;
		HD(tmp16);
		tmp16 = REG_KEYXY;
		HD(tmp16);

		HD(results);

		/*
		 * Everything that doesn't ever vary between loop iterations
		 * goes here.
		 */
		if (nw == 0) {
			/*
			 * Whatever garbage is in the stack. This is UB, but
			 * this program only runs in 1 implementation and the
			 * trick works there.
			 */
			HD(p);

			/* Stack address. */
			p = &p;
			HD(p);

			HD(buflen);
		}
		/*
		 * All the invariants go here. They don't vary during the
		 * program's lifetime, or even ever within the same program.
		 */
		if (runs == 0) {
			/*
			 * We don't have ASLR, but the address of this function
			 * might still be different between projects.
			 */
			HD(arm7_getentropy);

			HD(*PersonalData);
		}

		SHA1Final(results, &ctx);
		memcpy(buf+nw, results, MIN(buflen-nw, sizeof(results)));
		runs++;
	}

	explicit_bzero(&ctx, sizeof(ctx));

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
