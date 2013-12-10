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

#include <assert.h>
#include "winmsgbuf.h"


/* Allocate and initialize to the empty string a new window message buffer. The
 * return value must be freed using wmbc_free. */
WinMsgBuf *wmb_create()
{
	WinMsgBuf *w = malloc(sizeof(WinMsgBuf));
	if (w == NULL) {
		Panic(0, "%s", strnomem);
	}

	wmb_reset(w);
	return w;
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

/* Sets a character at the current buffer position and increments the pointer.
 * The terminating null byte is not retained. */
inline void wmbc_putchar(WinMsgBufContext *wmbc, char c)
{
	assert(wmbc);
	*wmbc->p++ = c;
}

/* Deinitializes and frees previously allocated context. The contained buffer
 * must be freed separately; this function will not do so for you. */
void wmbc_free(WinMsgBufContext *c)
{
	free(c);
}
