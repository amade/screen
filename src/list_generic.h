/* Copyright (c) 2010
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Sadrul Habib Chowdhury (sadrul@users.sourceforge.net)
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

struct ListData;

struct ListRow
{
  void *data;		/* Some data relevant to this row */
  struct ListRow *next, *prev;	/* doubly linked list */
  int y;	/* -1 if not on display */
};

struct GenericList
{
  int (*gl_printheader) (struct ListData *);			/* Print the header */
  int (*gl_printfooter) (struct ListData *);			/* Print the footer */
  int (*gl_printrow) (struct ListData *, struct ListRow *);	/* Print one row */
  int (*gl_pinput) (struct ListData *, char **inp, int *len);	/* Process input */
  int (*gl_freerow) (struct ListData *, struct ListRow *);	/* Free data for a row */
  int (*gl_free) (struct ListData *);				/* Free data for the list */
  int (*gl_matchrow) (struct ListData *, struct ListRow *, const char *);
};

struct ListData
{
  const char *name;		/* An identifier for the list */
  struct ListRow *root;		/* The first item in the list */
  struct ListRow *selected;	/* The selected row */
  struct ListRow *top;		/* The topmost visible row */

  struct GenericList *list_fn;	/* The functions that deal with the list */

  char *search;			/* The search term, if any */

  void *data;			/* List specific data */
};

extern struct LayFuncs ListLf;


struct ListRow * glist_add_row (struct ListData *ldata, void *data, struct ListRow *after);

void glist_remove_rows (struct ListData *ldata);

void glist_display_all (struct ListData *list);

struct ListData * glist_display (struct GenericList *list, const char *name);

void glist_abort (void);

void display_displays (void);

void display_windows (int onblank, int order, struct win *group);

