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

#include <sys/types.h>

#ifndef NOSYSLOG
#include <syslog.h>
#endif

#include "screen.h"		/* includes acls.h */
#include "extern.h"
#include "mark.h"

/************************************************************************
 * user managing code, this does not really belong into the acl stuff   *
 ************************************************************************/

struct acluser *users;

/*
 * =====================================================================
 * FindUserPtr-
 *       Searches for user
 *
 * Returns:
 *       an nonzero Address. Its contents is either a User-ptr,
 *       or NULL which may be replaced by a User-ptr to create the entry.
 * =====================================================================
 */
struct acluser **FindUserPtr(char *name)
{
	struct acluser **u;

	for (u = &users; *u; u = &(*u)->u_next)
		if (!strcmp((*u)->u_name, name))
			break;
	return u;
}

int DefaultEsc = -1;		/* initialised by screen.c:main() */
int DefaultMetaEsc = -1;

/*
 * =====================================================================
 * UserAdd-
 *       Adds a new user. His name must not be "none", as this represents
 *       the NULL-pointer when dealing with groups.
 *       He has default rights, determined by umask.
 * Returns:
 *       0 - on success
 *       1 - he is already there
 *      -1 - he still does not exist (didn't get memory)
 * =====================================================================
 */
int UserAdd(char *name, struct acluser **up)
{
	if (!up)
		up = FindUserPtr(name);
	if (*up) {
		return 1;
	}
	if (strcmp("none", name))	/* "none" is a reserved word */
		*up = calloc(1, sizeof(struct acluser));
	if (!*up)
		return -1;
	(*up)->u_plop.buf = NULL;
	(*up)->u_plop.len = 0;
	(*up)->u_plop.enc = 0;
	(*up)->u_Esc = DefaultEsc;
	(*up)->u_MetaEsc = DefaultMetaEsc;
	strncpy((*up)->u_name, name, MAXLOGINLEN);
	(*up)->u_detachwin = -1;
	(*up)->u_detachotherwin = -1;

	return 0;
}

/*
 * =====================================================================
 * UserDel-
 *       Remove a user from the list.
 *       Destroy all his permissions and completely detach him from the session.
 * Returns
 *       0 - success
 *      -1 - he who does not exist cannot be removed
 * =====================================================================
 */
int UserDel(char *name, struct acluser **up)
{
	struct acluser *u;
	struct display *old, *next;

	if (!up)
		up = FindUserPtr(name);
	if (!(u = *up))
		return -1;
	old = display;
	for (display = displays; display; display = next) {
		next = display->d_next;	/* read the next ptr now, Detach may zap it. */
		if (D_user != u)
			continue;
		if (display == old)
			old = NULL;
		Detach(D_REMOTE);
	}
	display = old;
	*up = u->u_next;

	UserFreeCopyBuffer(u);
	free((char *)u);
	if (!users) {
		Finit(0);	/* Destroying whole session. Noone could ever attach again. */
	}
	return 0;
}

/*
 * =====================================================================
 * UserFreeCopyBuffer-
 *       frees user buffer
 *       Also removes any references into the users copybuffer
 * Returns:
 *       0 - if the copy buffer was really deleted.
 *      -1 - cannot remove something that does not exist
 * =====================================================================
 */
int UserFreeCopyBuffer(struct acluser *u)
{
	Window *w;
	struct paster *pa;

	if (!u->u_plop.buf)
		return -1;
	for (w = windows; w; w = w->w_next) {
		pa = &w->w_paster;
		if (pa->pa_pasteptr >= u->u_plop.buf && pa->pa_pasteptr - u->u_plop.buf < u->u_plop.len)
			FreePaster(pa);
	}
	free((char *)u->u_plop.buf);
	u->u_plop.len = 0;
	u->u_plop.buf = 0;
	return 0;
}
