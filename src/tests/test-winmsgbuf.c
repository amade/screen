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

#include "../winmsgbuf.h"
#include "signature.h"
#include "macros.h"

SIGNATURE_CHECK(wmb_create, WinMsgBuf *, ());
SIGNATURE_CHECK(wmb_expand, size_t, (WinMsgBuf *, size_t));
SIGNATURE_CHECK(wmb_rendadd, void, (WinMsgBuf *, uint64_t, int));
SIGNATURE_CHECK(wmb_size, size_t, (const WinMsgBuf *));
SIGNATURE_CHECK(wmb_contents, const char *, (const WinMsgBuf *));
SIGNATURE_CHECK(wmb_reset, void, (WinMsgBuf *));
SIGNATURE_CHECK(wmb_free, void, (WinMsgBuf *));

SIGNATURE_CHECK(wmbc_create, WinMsgBufContext *, (WinMsgBuf *));
SIGNATURE_CHECK(wmbc_rewind, void, (WinMsgBufContext *));
SIGNATURE_CHECK(wmbc_fastfw0, void, (WinMsgBufContext *));
SIGNATURE_CHECK(wmbc_fastfw_end, void, (WinMsgBufContext *));
SIGNATURE_CHECK(wmbc_putchar, void, (WinMsgBufContext *, char));
SIGNATURE_CHECK(wmbc_strncpy, char *, (WinMsgBufContext *, const char *, size_t));
SIGNATURE_CHECK(wmbc_strcpy, char *, (WinMsgBufContext *, const char *));
SIGNATURE_CHECK(wmbc_printf, int, (WinMsgBufContext *, const char *, ...));
SIGNATURE_CHECK(wmbc_offset, size_t, (WinMsgBufContext *));
SIGNATURE_CHECK(wmbc_bytesleft, size_t, (WinMsgBufContext *));
SIGNATURE_CHECK(wmbc_mergewmb, char *, (WinMsgBufContext *, WinMsgBuf *));
SIGNATURE_CHECK(wmbc_finish, const char *, (WinMsgBufContext *));
SIGNATURE_CHECK(wmbc_free, void, (WinMsgBufContext *));

int main(void)
{
	{
		WinMsgBuf *wmb = wmb_create();

		/* we should start off with a null-terminated buffer */
		ASSERT(wmb_size(wmb) > 0);
		ASSERT(*wmb_contents(wmb) == '\0');
		/* TODO: rendition state */

		/* buffer shall be expandable to accomodate a minimum number of bytes */
		size_t old = wmb_size(wmb);
		size_t want = old + 3;
		ASSERT_REALLOC(>= want, ASSERT(wmb_expand(wmb, want) >= want));

		/* buffer will not expand if unneeded */
		size_t new = wmb_size(wmb);
		ASSERT_NOALLOC(ASSERT(wmb_expand(wmb, want) == new));
		ASSERT_NOALLOC(ASSERT(wmb_expand(wmb, want - 1) == new));
		ASSERT_NOALLOC(ASSERT(wmb_expand(wmb, 0) == new););

		/* if buffer expansion fails, the original size shall be retained */
		ASSERT_GCC(FAILLOC(size_t, wmb_expand(wmb, new + 5)) == new);

		/* resetting should put us back to our starting state, but should do
		 * nothing with the buffer size */
		wmb_reset(wmb);
		ASSERT(*wmb_contents(wmb) == '\0');
		ASSERT(wmb_size(wmb) == new);
		/* TODO: rendition state */

		wmb_free(wmb);
	}

	/* wmb_create should return NULL on allocation failure */
	{
		ASSERT_GCC(FAILLOC_PTR(wmb_create()) == NULL);
	}

	/* scenerio: writing to single buffer via separate contexts---while
	 * maintaining separate pointers between them---and retrieving a final
	 * result */
	{
		WinMsgBuf *wmb = wmb_create();
		WinMsgBufContext *wmbc = wmbc_create(wmb);
		WinMsgBufContext *wmbc2 = wmbc_create(wmb);

		/* the offset at this point should be 0 (beginning of buffer) */
		ASSERT(wmbc_offset(wmbc) == 0);
		ASSERT(wmbc_offset(wmbc2) == 0);
		ASSERT(wmbc_bytesleft(wmbc) == wmb_size(wmb));
		ASSERT(wmbc_bytesleft(wmbc2) == wmb_size(wmb));

		/* putting a character should increase the offset and decrease the
		 * number of bytes remaining */
		char c = 'c';
		wmbc_putchar(wmbc, c);
		ASSERT(wmbc_offset(wmbc) == 1);
		ASSERT(wmbc_bytesleft(wmbc) == wmb_size(wmb) - 1);

		/* multiple contexts should move independently of one-another */
		ASSERT(wmbc_offset(wmbc2) == 0);
		ASSERT(wmbc_bytesleft(wmbc2) == wmb_size(wmb));

		/* the contents of the buffer should reflect the change */
		ASSERT(*wmb_contents(wmb) == c);
		ASSERT(*wmbc_finish(wmbc) == c);

		/* the second context is still at the first position, so it should
		 * overwrite the first character */
		char c2 = 'd';
		wmbc_putchar(wmbc2, c2);
		ASSERT(wmbc_offset(wmbc2) == 1);
		ASSERT(*wmb_contents(wmb) == c2);
		ASSERT(*wmbc_finish(wmbc) == c2);
		ASSERT(*wmbc_finish(wmbc2) == c2);

		/* wmbc_finish should terminate the string; we will add a character at
		 * the second index to ensure that it is added */
		char cx = 'x';
		wmbc_putchar(wmbc2, cx);
		ASSERT(wmbc_offset(wmbc2) == 2);
		ASSERT(wmb_contents(wmb)[1] == cx);
		ASSERT(wmbc_finish(wmbc)[1] == '\0');
		ASSERT(wmb_contents(wmb)[1] == '\0');

		/* furthermore, finishing should not adjust the offset, so that we can
		 * continue where we left off */
		ASSERT(wmbc_offset(wmbc) == 1);
		wmbc_putchar(wmbc, cx);
		ASSERT(wmb_contents(wmb)[1] == cx);

		wmbc_free(wmbc2);
		wmbc_free(wmbc);
		wmb_free(wmb);
	}

	/* scenerio: write bytes and move around the buffer */
	{
		WinMsgBuf *wmb = wmb_create();
		WinMsgBufContext *wmbc = wmbc_create(wmb);

		/* initialize buffer to contain the string "foo" */
		wmbc_putchar(wmbc, 'f');
		wmbc_putchar(wmbc, 'o');
		wmbc_putchar(wmbc, 'o');
		wmbc_putchar(wmbc, '\0');

		/* before continuing, just make sure we are where we expect to be (test
		 * sanity check) */
		ASSERT(wmbc_offset(wmbc) == 4);

		/* rewind to the beginning */
		wmbc_rewind(wmbc);
		ASSERT(wmbc_offset(wmbc) == 0);

		/* we should be able to now affect the first character */
		char c = 'm';
		ASSERT(*wmb_contents(wmb) == 'f');  /* sanity check */
		wmbc_putchar(wmbc, c);
		ASSERT(*wmb_contents(wmb) == c);

		/* fast-forward to the null byte (which should bring our pointer to a
		 * single byte before our position after having written "foo") */
		ASSERT(wmbc_offset(wmbc) == 1);  /* sanity check */
		wmbc_fastfw0(wmbc);
		ASSERT(wmbc_offset(wmbc) == 3);

		/* that should allow us to continue where we left off, similar to how
		 * wmbc_finish allows us to continue */
		ASSERT(wmb_contents(wmb)[3] != c);  /* test case sanity check */
		wmbc_putchar(wmbc, c);
		ASSERT(wmb_contents(wmb)[3] == c);

		/* we should also be able to fast-forward to the very end of the buffer
		 * (that is, the next write would trigger an expansion); this is of
		 * limited use externally */
		wmbc_fastfw_end(wmbc);
		ASSERT(wmbc_offset(wmbc) == wmb_size(wmb));
		ASSERT(wmbc_bytesleft(wmbc) == 0);

		wmbc_free(wmbc);
		wmb_free(wmb);
	}

	/* scenerio: write past end of buffer to trigger buffer expansion rather
	 * than overflow */
	{
		WinMsgBuf *wmb = wmb_create();
		WinMsgBufContext *wmbc = wmbc_create(wmb);
		size_t szstart = wmb_size(wmb);

		/* fast-forward to end of buffer and write a character, which shall
		 * trigger expansion */
		char c = 'c';
		wmbc_fastfw_end(wmbc);
		wmbc_putchar(wmbc, c);
		ASSERT(wmb_size(wmb) > szstart);
		ASSERT(wmb_contents(wmb)[wmbc_offset(wmbc) - 1] == c);

		/* if expansion fails, then the character will not be written and shall
		 * be silently ignored; this has the effect of truncating the string
		 * (and should only ever happen in terrible conditions) */
		size_t sznew = wmb_size(wmb);
		char cfail = wmb_contents(wmb)[sznew - 1] + 1;
		wmbc_fastfw_end(wmbc);
		ASSERT(wmbc_offset(wmbc) > szstart);      /* sanity check */
		FAILLOC_VOID(wmbc_putchar(wmbc, cfail));  /* fail expansion */
		ASSERT_GCC(wmb_size(wmb) == sznew);
		/* should not have written char at last position */
		ASSERT_GCC(wmb_contents(wmb)[sznew - 1] != cfail);
		ASSERT_GCC(wmbc_offset(wmbc) == sznew);

		wmbc_free(wmbc);
		wmb_free(wmb);
	}

	/* context creation issues */
	{
		WinMsgBuf *wmb = wmb_create();

		/* wmbc_create() should return NULL on allocation failure */
		ASSERT_GCC(FAILLOC_PTR(wmbc_create(wmb)) == NULL);

		/* it should also return NULL if no buffer is provided (this could
		 * happen on an unchecked (*gasp*) wmb_create() failure */
		ASSERT(wmbc_create(NULL) == NULL);

		wmb_free(wmb);
	}

	return 0;
}
