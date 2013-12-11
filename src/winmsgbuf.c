/* Copyright (c) 2013
 *      Mike Gerwitz (mike@mikegerwitz.com)
 *
 * This file is part of GNU screen.
 *
 * GNU screen is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, see
 * <http://www.gnu.org/licenses>.
 *
 ****************************************************************
 */

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include "winmsgbuf.h"

/* TODO: why is this necessary?! (gcc 4.6.3) */
#define xvsnprintf vsnprintf


/* Allocate and initialize to the empty string a new window message buffer. The
 * return value must be freed using wmbc_free. */
WinMsgBuf *wmb_create()
{
	WinMsgBuf *w = malloc(sizeof(WinMsgBuf));
	if (w == NULL)
		Panic(0, "%s", strnomem);

	w->buf = malloc(WINMSGBUF_SIZE);
	if (w->buf == NULL)
		Panic(0, "%s", strnomem);

	w->size = WINMSGBUF_SIZE;
	wmb_reset(w);
	return w;
}

/* Attempts to expand the buffer to hold at least MIN bytes. The new size of the
 * buffer is returned, which may be unchanged from the original size if
 * additional memory could not be allocated. */
size_t wmb_expand(WinMsgBuf *wmb, size_t min)
{
	size_t size = wmb->size;

	if (size >= min)
		return size;

	/* keep doubling the buffer until we reach at least the requested size; this
	 * ensures that we'll always be a power of two (so long as the original
	 * buffer size was) and doubling will help cut down on excessive allocation
	 * requests on large buffers */
	while (size < min) {
		size *= 2;
	}

	void *p = realloc(wmb->buf, size);
	if (p == NULL) {
		/* reallocation failed; maybe the caller can do without? */
		return wmb->size;
	}

	/* realloc already handled the free for us */
	wmb->buf = p;
	wmb->size = size;
	return size;
}

/* Initializes window buffer to the empty string; useful for re-using an
 * existing buffer without allocating a new one. */
inline void wmb_reset(WinMsgBuf *w)
{
	*w->buf = '\0';
}

/* Deinitialize and free memory allocated to the given window buffer */
void wmb_free(WinMsgBuf *w)
{
	free(w->buf);
	free(w);
}


/* Allocate and initialize a buffer context for the given buffer. The return
 * value must be freed using wmbc_free. */
WinMsgBufContext *wmbc_create(WinMsgBuf *w)
{
	WinMsgBufContext *c = malloc(sizeof(WinMsgBufContext));
	if (c == NULL) {
		Panic(0, "%s", strnomem);
	}

	c->buf = w;
	c->p = w->buf;
	return c;
}

/* Place pointer at character immediately preceding the terminating null
 * character. */
inline void wmbc_fastfw(WinMsgBufContext *wmbc)
{
	wmbc->p += strlen(wmbc->p) - 1;
}

/* Place pointer at terminating null character. */
inline void wmbc_fastfw0(WinMsgBufContext *wmbc)
{
	wmbc->p += strlen(wmbc->p);
}

/* Place pointer at the last byte in the buffer, ignoring terminating null
 * characters */
inline void wmbc_fastfw_end(WinMsgBufContext *wmbc)
{
	wmbc->p = wmbc->buf->buf + wmbc->buf->size - 1;
}

/* Sets a character at the current buffer position and increments the pointer.
 * The terminating null character is not retained. The buffer will be
 * dynamically resized as needed. */
inline void wmbc_putchar(WinMsgBufContext *wmbc, char c)
{
	/* attempt to accomodate this character, but bail out silenty if it cannot
	 * fit */
	if (!wmbc_bytesleft(wmbc)) {
		size_t size = wmbc->buf->size + 1;
		if (wmb_expand(wmbc->buf, size) < size) {
			return;
		}
	}

	*wmbc->p++ = c;
}

/* Copies a string into the buffer, dynamically resizing the buffer as needed to
 * accomodate length N. If S is shorter than N characters in length, the
 * remaining bytes are filled will nulls. The context pointer is adjusted to the
 * terminating null byte. */
inline char *wmbc_strncpy(WinMsgBufContext *wmbc, const char *s, size_t n)
{
	size_t l = wmbc_bytesleft(wmbc);

	/* silently fail in the event that we cannot accomodate */
	if (l < n) {
		size_t size = wmbc->buf->size + (n - l);
		if (wmb_expand(wmbc->buf, size) < size) {
			return wmbc->p;
		}
	}

	char *p = wmbc->p;
	strncpy(wmbc->p, s, n);
	wmbc_fastfw0(wmbc);
	return p;
}

/* Copies a string into the buffer, dynamically resizing the buffer as needed to
 * accomodate the length of the string plus its terminating null byte. The
 * context pointer is adjusted to the the terminiating null byte. */
inline char *wmbc_strcpy(WinMsgBufContext *wmbc, const char *s)
{
	return wmbc_strncpy(wmbc, s, strlen(s) + 1);
}

/* Write data to the buffer using a printf-style format string. If needed, the
 * buffer will be automatically expanded to accomodate the resulting string and
 * is therefore protected against overflows. */
int wmbc_printf(WinMsgBufContext *wmbc, const char *fmt, ...)
{
	va_list ap;
	size_t  n, max;

	/* to prevent buffer overflows, cap the number of bytes to the remaining
	 * buffer size */
	va_start(ap, fmt);
	max = wmbc_bytesleft(wmbc);
	n = vsnprintf(wmbc->p, max, fmt, ap);

	/* more space is needed if vsnprintf returns a larger number than our max,
	 * in which case we should accomodate by dynamically resizing the buffer and
	 * trying again */
	if (n > max) {
		if (wmb_expand(wmbc->buf, n) < n) {
			/* failed to allocate additional memory; this will simply have to do */
			wmbc_fastfw_end(wmbc);
			return max;
		}

		size_t m = vsnprintf(wmbc->p, n, fmt, ap);
		assert(m == n); /* this should never fail */
	}

	wmbc_fastfw0(wmbc);
	return n;
}

/* Retrieve the 0-indexed offset of the context pointer into the buffer */
inline size_t wmbc_offset(WinMsgBufContext *wmbc)
{
	ptrdiff_t offset = wmbc->p - wmbc->buf->buf;

	/* when using wmbc_* functions (as one always should), the offset should
	 * always be within the bounds of the buffer */
	assert(offset > -1);
	assert((size_t)offset < wmbc->buf->size);

	return (size_t)offset;
}

/* Calculate the number of bytes remaining in the buffer relative to the current
 * position within the buffer */
inline size_t wmbc_bytesleft(WinMsgBufContext *wmbc)
{
	return wmbc->buf->size - (wmbc_offset(wmbc) + 1);
}

/* Deinitializes and frees previously allocated context. The contained buffer
 * must be freed separately; this function will not do so for you. */
void wmbc_free(WinMsgBufContext *c)
{
	free(c);
}
