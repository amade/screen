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

#include "../winmsgtok.h"
#include "signature.h"
#include "macros.h"

SIGNATURE_CHECK(wmtok_init, WinMsgTokState *, (WinMsgTokState *));
SIGNATURE_CHECK(wmtok_tokenize, size_t,
	(WinMsgTok **, char *, size_t, WinMsgTokState *));
SIGNATURE_CHECK(wmtok_free, void, (WinMsgTokState *));

SIGNATURE_CHECK(wmtok_type, WinMsgTokType, (WinMsgTok *));
SIGNATURE_CHECK(wmtok_lexeme, char *, (char *, WinMsgTok *, size_t));
SIGNATURE_CHECK(wmtok_data, union WinMsgTokData, (WinMsgTok *));

char lexbuf[256];

#define ASSERT_TOKSTR(s, ...) { \
		WinMsgTokType etok[] = {__VA_ARGS__, WMTOK_END}; \
		_assert_tokstr(s, etok); \
	}

#define ASSERT_TOK_NODATA(tok, type, lexeme) \
	ASSERT(wmtok_type(tok) == type); \
	ASSERT(STREQ(wmtok_lexeme(lexbuf, tok, sizeof(lexbuf)), lexeme));

#define ASSERT_TOK_P(tok, type, lexeme, data) \
	ASSERT_TOK_NODATA(tok, type, lexeme); \
	ASSERT(wmtok_data(tok).ptr == data);

#define ASSERT_TOK(tok, type, lexeme, data) \
	ASSERT_TOK_NODATA(tok, type, lexeme); \
	ASSERT(wmtok_data(tok).ptr == data);

/* expected values when asserting against tokens */
struct TokAssert {
	WinMsgTokType  type;
	char          *lexeme;
};


void _assert_tokstr(char *src, WinMsgTokType *expected)
{
	WinMsgTokState *st = wmtok_init(NULL);
	WinMsgTok dest[1024];
	WinMsgTok *tok = dest;

	size_t n = wmtok_tokenize(&tok, src, sizeof(dest), st);
	size_t i = 0;

	do {
		ASSERT(wmtok_type(tok++) == *expected);
	} while ((i++ < n) && (*expected++ != WMTOK_END));

	ASSERT(n == i);

	wmtok_free(st);
}

int main(void)
{
	/* allocate state if null pointer given */
	{
		WinMsgTokState *st;

		ASSERT_MALLOC(>0, st = wmtok_init(NULL));
		ASSERT(st != NULL);

		wmtok_free(st);
	}

	/* previously allocated state given */
	{
		WinMsgTokState *st = malloc(sizeof(WinMsgTokState));
		ASSERT(wmtok_init(st) == st);
		wmtok_free(st);
	}

	/* tokenizing the empty string should yield a single END token */
	{
		WinMsgTokState *st = wmtok_init(NULL);
		WinMsgTok dest[16];  /* extra in case something goes terribly wrong */
		WinMsgTok *tok = dest;

		ASSERT(wmtok_tokenize(&tok, "", sizeof(dest), st) == 1);
		ASSERT_TOK(tok, WMTOK_END, "", NULL);

		wmtok_free(st);
	}
}
