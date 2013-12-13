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

#ifndef SCREEN_WINMSGBUF_H
#define SCREEN_WINMSGBUF_H

#include <stddef.h>
#include <stdint.h>
#include "screen.h"

/* Default window message buffer size */
#define WINMSGBUF_SIZE MAXSTR

#define MAX_WINMSG_REND 256 /* rendition changes */

/* Represents a working buffer for window messages */
typedef struct {
	char     *buf;
	size_t    size;
	uint64_t  rend[MAX_WINMSG_REND];
	int       rendpos[MAX_WINMSG_REND];
	int       numrend;
} WinMsgBuf;

typedef struct {
	WinMsgBuf *buf;
	char      *p;    /* pointer within buffer */
} WinMsgBufContext;


WinMsgBuf *wmb_create();
inline void wmb_reset(WinMsgBuf *);
size_t wmb_expand(WinMsgBuf *, size_t);
void wmb_rendadd(WinMsgBuf *, uint64_t, int);
void wmb_free(WinMsgBuf *);

WinMsgBufContext *wmbc_create(WinMsgBuf *);
inline void wmbc_fastfw(WinMsgBufContext *);
inline void wmbc_fastfw0(WinMsgBufContext *);
inline void wmbc_putchar(WinMsgBufContext *, char);
inline char *wmbc_strncpy(WinMsgBufContext *, const char *, size_t);
inline char *wmbc_strcpy(WinMsgBufContext *, const char *);
int wmbc_printf(WinMsgBufContext *, const char *, ...)
                __attribute__((format(printf,2,3)));
inline size_t wmbc_offset(WinMsgBufContext *);
inline size_t wmbc_bytesleft(WinMsgBufContext *);
char *wmbc_mergewmb(WinMsgBufContext *, WinMsgBuf *);
void wmbc_free(WinMsgBufContext *);

#endif
