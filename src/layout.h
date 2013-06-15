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

#ifndef SCREEN_LAYOUT_H
#define SCREEN_LAYOUT_H

#include "canvas.h"

#define MAXLAY 10

struct layout {
	struct layout   *lay_next;
	char            *lay_title;
	int              lay_number;
	struct canvas    lay_canvas;
	struct canvas   *lay_forecv;
	struct canvas   *lay_cvlist;
	int              lay_autosave;
};

void  FreeLayoutCv(struct canvas *c);
struct layout *CreateLayout(char *, int);
void  AutosaveLayout (struct layout *);
void  LoadLayout (struct layout *);
void  NewLayout (char *, int);
void  SaveLayout (char *, struct canvas *);
void  ShowLayouts (int);
struct layout *FindLayout (char *);
void  UpdateLayoutCanvas (struct canvas *, struct win *);

void RenameLayout (struct layout *, const char *);
int RenumberLayout (struct layout *, int);

void  RemoveLayout (struct layout *);
int   LayoutDumpCanvas (struct canvas *, char *);

#endif /* SCREEN_LAYOUT_H */
