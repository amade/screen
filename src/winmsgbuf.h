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

#include <stdint.h>
#include "screen.h"

#define MAX_WINMSG_REND 256 /* rendition changes */

/* Represents a working buffer for window messages */
typedef struct {
	char      buf[MAXSTR];
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
void wmb_free(WinMsgBuf *);

WinMsgBufContext *wmbc_create(WinMsgBuf *);
inline void wmbc_fastfw(WinMsgBufContext *);
inline void wmbc_fastfw0(WinMsgBufContext *);
inline void wmbc_putchar(WinMsgBufContext *, char);
void wmbc_free(WinMsgBufContext *);

#endif
