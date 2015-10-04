/* Copyright (c) 2013
 *      Mike Gerwitz (mtg@gnu.org)
 * Copyright (c) 2010
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
#include <assert.h>

#include "window.h"
#include "winmsgbuf.h"
#include "winmsgcond.h"
#include "backtick.h"

#define RENDBUF_SIZE 128 /* max rendition byte count */

/* escape characters */
typedef enum {
	WINESC_WFLAGS          = 'f',
	WINESC_ESC_SEEN        = 'E',
	WINESC_FOCUS           = 'F',
	WINESC_HSTATUS         = 'h',
	WINESC_HOST            = 'H',
	WINESC_WIN_NUM         = 'n',
	WINESC_WIN_LOGNAME     = 'N',
	WINESC_PID             = 'p',
	WINESC_COPY_MODE       = 'P',  /* copy/_P_aste mode */
	WINESC_WIN_SIZE        = 's',
	WINESC_SESS_NAME       = 'S',
	WINESC_WIN_TITLE       = 't',
	WINESC_WIN_NAMES       = 'w',
	WINESC_WIN_NAMES_NOCUR = 'W',
	WINESC_CMD_ARGS        = 'x',
	WINESC_CMD             = 'X',
	WINESC_REND_START      = '{',
	WINESC_REND_END        = '}',
	WINESC_REND_POP        = '-',
	WINESC_COND            = '?',  /* start and end delimiter */
	WINESC_COND_ELSE       = ':',
	WINESC_BACKTICK        = '`',
	WINESC_PAD             = '=',
	WINESC_TRUNC           = '<',
	WINESC_TRUNC_POS       = '>',
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
char *MakeWinMsgEv(WinMsgBuf *, char *, Window *, int, int, Event *, int);
int   AddWinMsgRend(WinMsgBuf *, const char *, uint64_t);

extern WinMsgBuf *g_winmsg;

#endif
