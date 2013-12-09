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

#ifndef SCREEN_WINMSGCOND_H
#define SCREEN_WINMSGCOND_H

#include <stdlib.h>
#include <stdbool.h>


/* represents a window message condition (e.g. %?)*/
typedef struct {
	char *pos;     /* starting position in dest string */
	bool  state;   /* conditional truth value */
	bool  locked;  /* when set, prevents state from changing */
} WinMsgCond;

/* WinMsgCond is intended to be used as an opaque type */
inline void  wmc_init(WinMsgCond *, char *);
inline void  wmc_set(WinMsgCond *);
inline void  wmc_clear(WinMsgCond *);
inline bool  wmc_is_active(const WinMsgCond *);
inline bool  wmc_is_set(const WinMsgCond *);
inline char *wmc_else(WinMsgCond *, char *);
inline char *wmc_end(const WinMsgCond *, char *);
inline void  wmc_deinit(WinMsgCond *);

#endif
