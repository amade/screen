/* Copyright (c) 2013
 *      Mike Gerwitz (mike@mikegerwitz.com)
 *
 * This file is part of GNU screen.
 *
 * GNU screen is free software; you can redistribute it and/or modify
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
 * <http://www.gnu.org/licenses>.
 *
 ****************************************************************
 */

#include "../winmsgcond.h"
#include "signature.h"
#include "macros.h"

SIGNATURE_CHECK(wmc_init, void, (WinMsgCond *, char *));
SIGNATURE_CHECK(wmc_set, void, (WinMsgCond *));
SIGNATURE_CHECK(wmc_clear, void, (WinMsgCond *));
SIGNATURE_CHECK(wmc_is_active, bool, (const WinMsgCond *));
SIGNATURE_CHECK(wmc_is_set, bool, (const WinMsgCond *));
SIGNATURE_CHECK(wmc_else, char *, (WinMsgCond *, char *));
SIGNATURE_CHECK(wmc_end, char *, (const WinMsgCond *, char *));
SIGNATURE_CHECK(wmc_deinit, void, (WinMsgCond *));

int main(void)
{
	/* simple test with no "else" */
	{
		WinMsgCond wmc;
		char *pos = "test";

		wmc_init(&wmc, pos);
		ASSERT(wmc_is_active(&wmc) == true);

		/* from the above init, we should be false by default and therefore the
		 * original pointer should be returned (non-match), intended to wipe out
		 * changes to the buffer*/
		ASSERT(wmc_is_set(&wmc) == false);
		ASSERT(wmc_end(&wmc, pos + 1) == pos);

		/* but if we set the flag, then the given position should be returned,
		 * indicating that it should be kept */
		wmc_set(&wmc);
		ASSERT(wmc_is_set(&wmc) == true);
		ASSERT(wmc_end(&wmc, pos + 1) == pos + 1);

		/* we should be able to subsequently clear the flag and once again have
		 * the original position returned */
		wmc_clear(&wmc);
		ASSERT(wmc_is_set(&wmc) == false);
		ASSERT(wmc_end(&wmc, pos + 1) == pos);

		/* after we deinitialize, we should not be considered active and sets
		 * should be ignored */
		wmc_deinit(&wmc);
		ASSERT(wmc_is_active(&wmc) == false);
		ASSERT(wmc_is_set(&wmc) == false);
		ASSERT(wmc_end(&wmc, pos + 1) == NULL);
		wmc_set(&wmc);
		ASSERT(wmc_is_set(&wmc) == false);
		ASSERT(wmc_end(&wmc, pos + 1) == NULL);
		wmc_clear(&wmc);
		ASSERT(wmc_end(&wmc, pos + 1) == NULL);

		/* after deinitializing when active, ending should not return given
		 * pointer (that is, ensure we're cleared) */
		wmc_init(&wmc, pos);
		wmc_set(&wmc);
		ASSERT(wmc_end(&wmc, pos + 1) == pos + 1);
		wmc_deinit(&wmc);
		ASSERT(wmc_is_set(&wmc) == false);
		ASSERT(wmc_end(&wmc, pos + 1) == NULL);
	}

	/* "else" condition */
	{
		WinMsgCond wmc;
		char *pos = "test";
		char *epos = pos + 2;

		/* if the first condition is never set, then the else condition shall
		 * always be true */
		wmc_init(&wmc, pos);
		ASSERT(wmc_else(&wmc, epos) == pos);
		ASSERT(wmc_end(&wmc, epos + 1) == epos + 1);

		/* furthermore, it cannot be set to false (that doesn't make sense---we
		 * are an "else" block */
		wmc_clear(&wmc);
		ASSERT(wmc_end(&wmc, epos + 1) == epos + 1);

		/* deinit should still clear us */
		wmc_deinit(&wmc);
		ASSERT(wmc_is_active(&wmc) == false);
		ASSERT(wmc_is_set(&wmc) == false);
		ASSERT(wmc_end(&wmc, epos + 1) == NULL);

		/* in the case of a truthful first condition, "else" shall never
		 * match */
		wmc_init(&wmc, pos);
		wmc_set(&wmc);
		wmc_else(&wmc, epos);
		ASSERT(wmc_is_set(&wmc) == false);
		ASSERT(wmc_end(&wmc, epos + 1) == epos);

		/* and deinit shall still work the same */
		wmc_deinit(&wmc);
		ASSERT(wmc_end(&wmc, epos + 1) == NULL);
	}

	return 0;
}
