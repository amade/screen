/* Copyright (c) 2008, 2009
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "config.h"
#include "screen.h"

#include <sys/ioctl.h>

/* for solaris 2.1, Unixware (SVR4.2) and possibly others */
#ifdef HAVE_STROPTS_H
fdasfa
# include <sys/stropts.h>
#endif

#include <pty.h>

#include "extern.h"

/*
 * if no PTYRANGE[01] is in the config file, we pick a default
 */
#ifndef PTYRANGE0
# define PTYRANGE0 "qpr"
#endif
#ifndef PTYRANGE1
# define PTYRANGE1 "0123456789abcdef"
#endif

static char TtyName[32];

static void initmaster (int);

int pty_preopen = 0;

/*
 *  Open all ptys with O_NOCTTY, just to be on the safe side
 *  (RISCos mips breaks otherwise)
 */
#ifndef O_NOCTTY
# define O_NOCTTY 0
#endif

/***************************************************************/

static void
initmaster(int f)
{
  tcflush(f, TCIOFLUSH);
#ifdef LOCKPTY
  (void) ioctl(f, TIOCEXCL, (char *) 0);
#endif
}

void
InitPTY(int f)
{
  if (f < 0)
    return;
}

int
OpenPTY(char **ttyn)
{
  int f, s;
  if (openpty(&f, &s, TtyName, NULL, NULL) != 0)
    return -1;
  close(s);
  initmaster(f);
  pty_preopen = 1;
  *ttyn = TtyName;
  return f;    
}

