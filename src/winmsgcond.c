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

#include "winmsgcond.h"


/* Initialize new condition and set to false; can be used to re-initialize a
 * condition for re-use */
inline void wmc_init(WinMsgCond *cond, char *pos)
{
	cond->pos = pos;
	wmc_clear(cond);
}

/* Mark condition as true */
inline void wmc_set(WinMsgCond *cond)
{
	cond->state = true;
}

/* Clear condition (equivalent to non-match) */
inline void wmc_clear(WinMsgCond *cond)
{
	cond->state = false;
}

/* Determine if condition is active (has been initialized and can be used) */
inline bool wmc_is_active(const WinMsgCond *cond)
{
	return (cond->pos != NULL);
}

/* Determine if a condition is true; the result is undefined if
 * !wmc_active(cond) */
inline bool wmc_is_set(const WinMsgCond *cond)
{
	return cond->state;
}

/* Retrieve condition beginning position */
inline char *wmc_get_pos(const WinMsgCond *cond)
{
	return cond->pos;
}

/* Deactivate a condition */
inline void wmc_deinit(WinMsgCond *cond)
{
	cond->pos = NULL;
}
