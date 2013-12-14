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
SIGNATURE_CHECK(wmbc_fastfw, void, (WinMsgBufContext *));
SIGNATURE_CHECK(wmbc_fastfw0, void, (WinMsgBufContext *));
SIGNATURE_CHECK(wmbc_putchar, void, (WinMsgBufContext *, char));
SIGNATURE_CHECK(wmbc_strncpy, char *, (WinMsgBufContext *, const char *, size_t));
SIGNATURE_CHECK(wmbc_strcpy, char *, (WinMsgBufContext *, const char *));
SIGNATURE_CHECK(wmbc_printf, int, (WinMsgBufContext *, const char *, ...));
SIGNATURE_CHECK(wmbc_offset, size_t, (WinMsgBufContext *));
SIGNATURE_CHECK(wmbc_bytesleft, size_t, (WinMsgBufContext *));
SIGNATURE_CHECK(wmbc_mergewmb, char *, (WinMsgBufContext *, WinMsgBuf *));
SIGNATURE_CHECK(wmbc_free, void, (WinMsgBufContext *));

int main(void)
{
	{
		WinMsgBuf *wmb = wmb_create();

		/* we should start off with a null-terminated buffer */
		ASSERT(wmb_size(wmb) > 0);
		ASSERT(*wmb_contents(wmb) == '\0');


		wmb_free(wmb);
	}

	return 0;
}
