/* Copyright (c) 2013
 *      Mike Gerwitz (mtg@gnu.org)
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

#include <stdbool.h>


/* represents a window message condition (e.g. %?)*/
typedef struct {
	char *pos;     /* starting position in dest string */
	bool  state;   /* conditional truth value */
	bool  locked;  /* when set, prevents state from changing */
} WinMsgCond;

/* WinMsgCond is intended to be used as an opaque type */
void  wmc_init(WinMsgCond *, char *);
void  wmc_set(WinMsgCond *);
void  wmc_clear(WinMsgCond *);
bool  wmc_is_active(const WinMsgCond *);
bool  wmc_is_set(const WinMsgCond *);
char *wmc_else(WinMsgCond *, char *, bool *);
char *wmc_end(const WinMsgCond *, char *, bool *);
void  wmc_deinit(WinMsgCond *);

#endif
