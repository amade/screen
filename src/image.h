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
 * $Id$ GNU
 */

struct mchar {
	unsigned char image;
	unsigned char attr;
	unsigned char font;
	unsigned char colorbg;
	unsigned char colorfg;
	unsigned char mbcs;
};

struct mline {
	unsigned char *image;
	unsigned char *attr;
	unsigned char *font;
	unsigned char *colorbg;
	unsigned char *colorfg;
};



#define save_mline(ml, n) {						\
	memmove((char *)mline_old.image,   (char *)(ml)->image,   (n));	\
	memmove((char *)mline_old.attr,    (char *)(ml)->attr,    (n));	\
	memmove((char *)mline_old.font,    (char *)(ml)->font,    (n));	\
	memmove((char *)mline_old.colorbg, (char *)(ml)->colorbg, (n));	\
	memmove((char *)mline_old.colorfg, (char *)(ml)->colorfg, (n));	\
}

#define copy_mline(ml, xf, xt, n) {							\
	memmove((char *)(ml)->image   + (xt), (char *)(ml)->image   + (xf), (n));	\
	memmove((char *)(ml)->attr    + (xt), (char *)(ml)->attr    + (xf), (n));	\
	memmove((char *)(ml)->font    + (xt), (char *)(ml)->font    + (xf), (n));	\
	memmove((char *)(ml)->colorbg + (xt), (char *)(ml)->colorbg + (xf), (n));	\
	memmove((char *)(ml)->colorfg + (xt), (char *)(ml)->colorfg + (xf), (n));	\
}

#define clear_mline(ml, x, n) {							\
	bclear((char *)(ml)->image + (x), (n));					\
	if ((ml)->attr    != null) memset((char *)(ml)->attr    + (x), 0, (n));	\
	if ((ml)->font    != null) memset((char *)(ml)->font    + (x), 0, (n));	\
	if ((ml)->colorbg != null) memset((char *)(ml)->colorbg + (x), 0, (n));	\
	if ((ml)->colorfg != null) memset((char *)(ml)->colorfg + (x), 0, (n));	\
}

#define cmp_mline(ml1, ml2, x) (			\
	   (ml1)->image[x]   == (ml2)->image[x]		\
	&& (ml1)->attr[x]    == (ml2)->attr[x]		\
	&& (ml1)->font[x]    == (ml2)->font[x]		\
	&& (ml1)->colorbg[x] == (ml2)->colorbg[x]	\
	&& (ml1)->colorfg[x] == (ml2)->colorfg[x]	\
)

#define cmp_mchar(mc1, mc2) (				\
	    (mc1)->image  == (mc2)->image		\
	&& (mc1)->attr    == (mc2)->attr			\
	&& (mc1)->font    == (mc2)->font			\
	&& (mc1)->colorbg == (mc2)->colorbg		\
	&& (mc1)->colorfg == (mc2)->colorfg		\
)

#define cmp_mchar_mline(mc, ml, x) (			\
	   (mc)->image   == (ml)->image[x]		\
	&& (mc)->attr    == (ml)->attr[x]		\
	&& (mc)->font    == (ml)->font[x]		\
	&& (mc)->colorbg == (ml)->colorbg[x]		\
	&& (mc)->colorfg == (ml)->colorfg[x]		\
)

#define copy_mchar2mline(mc, ml, x) {			\
	(ml)->image[x]   = (mc)->image;			\
	(ml)->attr[x]    = (mc)->attr;			\
	(ml)->font[x]    = (mc)->font;			\
	(ml)->colorbg[x] = (mc)->colorbg;		\
	(ml)->colorfg[x] = (mc)->colorfg;		\
}

#define copy_mline2mchar(mc, ml, x) {			\
	(mc)->image   = (ml)->image[x];			\
	(mc)->attr    = (ml)->attr[x];			\
	(mc)->font    = (ml)->font[x];			\
	(mc)->colorbg = (ml)->colorbg[x];		\
	(mc)->colorfg = (ml)->colorfg[x];		\
	(mc)->mbcs    = 0;				\
}

/*
#define rend_getbg(mc) (((mc)->color & 0xf0) >> 4 | ((mc)->attr & A_BBG ? 0x100 : 0) | ((mc)->colorx & 0xf0))
#define rend_setbg(mc, c) ((mc)->color = ((mc)->color & 0x0f) | (c << 4 & 0xf0), (mc)->colorx = ((mc)->colorx & 0x0f) | (c & 0xf0), (mc)->attr = ((mc)->attr | A_BBG) ^ (c & 0x100 ? 0 : A_BBG))
#define rend_getfg(mc) (((mc)->color & 0x0f) | ((mc)->attr & A_BFG ? 0x100 : 0) | (((mc)->colorx & 0x0f) << 4))
#define rend_setfg(mc, c) ((mc)->color = ((mc)->color & 0xf0) | (c & 0x0f), (mc)->colorx = ((mc)->colorx & 0xf0) | ((c & 0xf0) >> 4), (mc)->attr = ((mc)->attr | A_BFG) ^ (c & 0x100 ? 0 : A_BFG))
#define rend_setdefault(mc) ((mc)->color = (mc)->colorx = 0, (mc)->attr &= ~(A_BBG|A_BFG))
*/

#define rend_getbg(mc)		((mc)->colorbg)
#define rend_setbg(mc, c)	((mc)->colorbg = c)
#define rend_getfg(mc)		((mc)->colorfg)
#define rend_setfg(mc, c)	((mc)->colorfg = c)
#define rend_setdefault(mc)	((mc)->colorbg = (mc)->colorfg = 0, (mc)->attr &= ~(A_BBG|A_BFG))

#define coli2e(c) ((((c) & 0x1f8) == 0x108 ? (c) ^ 0x108 : (c & 0xff)) ^ 9)
#define cole2i(c) ((c) >= 8 && (c) < 16 ? (c) ^ 0x109 : (c) ^ 9)
enum
{
  REND_BELL = 0,
  REND_MONITOR,
  REND_SILENCE,
  NUM_RENDS
};

