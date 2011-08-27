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

#include "config.h"
#include "screen.h"
#include "extern.h"
#include "viewport.h"

int
RethinkDisplayViewports()
{
  struct canvas *cv;
  struct viewport *vp, *vpn;

  /* free old viewports */
  for (cv = display->d_cvlist; cv; cv = cv->c_next)
    {
      for (vp = cv->c_vplist; vp; vp = vpn)
	{
	  vp->v_canvas = 0;
	  vpn = vp->v_next;
          memset((char *)vp, 0, sizeof(*vp));
          free(vp);
	}
      cv->c_vplist = 0;
    }
  display->d_vpxmin = -1;
  display->d_vpxmax = -1;

  for (cv = display->d_cvlist; cv; cv = cv->c_next)
    {
      if ((vp = (struct viewport *)malloc(sizeof *vp)) == 0)
	return -1;
      vp->v_canvas = cv;
      vp->v_xs = cv->c_xs;
      vp->v_ys = cv->c_ys;
      vp->v_xe = cv->c_xe;
      vp->v_ye = cv->c_ye;
      vp->v_xoff = cv->c_xoff;
      vp->v_yoff = cv->c_yoff;
      vp->v_next = cv->c_vplist;
      cv->c_vplist = vp;

      if (cv->c_xs < display->d_vpxmin || display->d_vpxmin == -1)
        display->d_vpxmin = cv->c_xs;
      if (cv->c_xe > display->d_vpxmax || display->d_vpxmax == -1)
        display->d_vpxmax = cv->c_xe;
    }
  return 0;
}

void
RethinkViewportOffsets(struct canvas *cv)
{
  struct viewport *vp;

  for (vp = cv->c_vplist; vp; vp = vp->v_next)
    {
      vp->v_xoff = cv->c_xoff;
      vp->v_yoff = cv->c_yoff;
    }
}


