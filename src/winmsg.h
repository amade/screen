/* Copyright (c) 2010
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Sadrul Habib Chowdhury (sadrul@users.sourceforge.net)
 * Copyright (c) 2008, 2009
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 *      Micah Cowan (micah@cowan.name)
 *      Sadrul Habib Chowdhury (sadrul@users.sourceforge.net)
 * Copyright (c) 1993-2002, 2003, 2005, 2006, 2007
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
 * This program is free software; you can redistribute it and/or modify
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
 * http://www.gnu.org/licenses/, or contact Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 ****************************************************************
 */

#ifndef SCREEN_WINMSG_H
#define SCREEN_WINMSG_H

#include <stdint.h>
#include <stdbool.h>

#include "window.h"

#define MAX_WINMSG_REND 256	/* rendition changes */
#define RENDBUF_SIZE 128 /* max rendition byte count */

/* escape characters */
typedef enum {
	WINMSG_PID        = 'p',
	WINMSG_COPY_MODE  = 'P', /* copy/_P_aste mode */
	WINMSG_REND_START = '{',
	WINMSG_REND_END   = '}',
	WINMSG_REND_POP   = '-',
} WinMsgEscapeChar;

/* escape sequence */
typedef struct {
	int num;
	struct {
		bool zero  : 1;
		bool lng   : 1;
		bool minus : 1;
		bool plus  : 1;
	} flags;
} WinMsgEsc;

char *MakeWinMsg(char *, Window *, int);
char *MakeWinMsgEv(char *, Window *, int, int, Event *, int);
int   AddWinMsgRend(const char *, uint64_t);

#endif
