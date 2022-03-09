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

#include <stdio.h>

static int nds_getentropy(void *, size_t);
static void printent(const void *, size_t);

int
main(void)
{
	int keys;
	static unsigned char entropy[8];

	consoleDemoInit();

	nds_getentropy(entropy, sizeof(entropy));
	printent(entropy, sizeof(entropy));
	for(;;) {
		swiWaitForVBlank();
		scanKeys();
		keys = keysDown();
		if (keys & KEY_START)
			break;
		if (keys & ~(KEY_TOUCH | KEY_LID)) {
			nds_getentropy(entropy, sizeof(entropy));
			printent(entropy, sizeof(entropy));
		}
	}

	return 0;
}

/* nds_getentropy: serialize arguments and call arm7_getentropy */
static int
nds_getentropy(void *buf, size_t buflen)
{
	struct pnl {
		void *buf;
		size_t len;
	} pnl, *p;
	union val {
		uint32_t u;
		int32_t s;
	} val;


	p = memUncached(&pnl);

	p->buf = buf;
	p->len = buflen;
	/*
	 * Don't want the cache overwritten while ARM7 handles it.
	 * This seems to have happened during testing, I got a sequence of 8
	 * all-0 bytes. What are the odds?
	 */
	DC_InvalidateRange(buf, buflen);
	fifoSendAddress(FIFO_USER_01, &pnl);
	while (!fifoCheckValue32(FIFO_USER_01))
		;
	/*
	 * Buf no longer contains what ARM9 thinks it does, invalidate the cache
	 * again.
	 */
	DC_InvalidateRange(buf, buflen);
	val.u = fifoGetValue32(FIFO_USER_01);
	return (val.s);
}

/* printent: print a line with all the entropy in `_buf` in hexadecimal */
static void
printent(const void *_buf, size_t len)
{
	const unsigned char *buf = _buf;
	size_t i;
	static char hex[16] = "0123456789ABCDEF";

	if (len == 0)
		return;

	fputs("entropy: ", stdout);
	for (i = 0; i < len;) {
		putchar(hex[buf[i] & 0x0F]);
		putchar(hex[buf[i] >> 4]);

		i++;
		if (i < len)
			putchar(' ');
	}
	putchar('\n');
}
