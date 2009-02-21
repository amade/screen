/* Copyright (c) 2008 Sadrul Habib Chowdhury (sadrul@users.sf.net)
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
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 ****************************************************************
 * $Id$ FAU
 */

struct win;

struct ScriptFuncs
{
  int (*sf_Init) __P((void));
  int (*sf_Finit) __P((void));
  int (*sf_ForeWindowChanged) __P((void));
  int (*sf_Source) __P((const char *));
  int (*sf_ProcessCaption) __P((const char *, struct win *, int len));
  int (*sf_CommandExecuted) __P((const char *, const char **, int));
};

void LoadScripts(void);

