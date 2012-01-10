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
#include <fcntl.h>

#include "config.h"
#include "screen.h"

#include "extern.h"

static int GetLoadav (void);

static LOADAV_TYPE loadav[LOADAV_NUM];
static int loadok;

/*
 * This is the easy way. It relies in /proc being mounted.
 * For the big and ugly way refer to previous screen version.
 */
void
InitLoadav()
{
  loadok = 1;
}

static int
GetLoadav()
{
  FILE *fp;
  char buf[128], *s;
  int i;
  double d, e;

  if ((fp = secfopen("/proc/loadavg", "r")) == NULL)
    return 0;
  *buf = 0;
  fgets(buf, sizeof(buf), fp);
  fclose(fp);
  /* can't use fscanf because the decimal point symbol depends on
   * the locale but the kernel uses always '.'.
   */
  s = buf;
  for (i = 0; i < (LOADAV_NUM > 3 ? 3 : LOADAV_NUM); i++)
    {
      d = e = 0;
      while(*s == ' ')
	s++;
      if (*s == 0)
	break;
      for(;;)
	{
	  if (*s == '.') 
	    e = 1;
	  else if (*s >= '0' && *s <= '9') 
	    {
	      d = d * 10 + (*s - '0'); 
	      if (e)
		e *= 10;
	    }
	  else    
	    break;
	  s++;    
	}
      loadav[i] = e ? d / e : d;
    }
  return i;
}

#ifndef FIX_TO_DBL
#define FIX_TO_DBL(l) ((double)(l) /  LOADAV_SCALE)
#endif

void
AddLoadav(char *p)
{
  int i, j;
  if (loadok == 0)
    return;
  j = GetLoadav();
  for (i = 0; i < j; i++)
    {
      sprintf(p, " %2.2f" + !i, FIX_TO_DBL(loadav[i]));
      p += strlen(p);
    }
}

