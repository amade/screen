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

#include <stdlib.h>
#include <string.h>
#include "winmsgtok.h"


static WinMsgTok *_wmtok_tok()
{
	/* TODO: this is temporary code for POC and is obviously bad ;) */
	static WinMsgTok toks[512] = {{0}};
	static int i = 0;
	WinMsgTok *p = &toks[i++];

	return p;
}


/* Initialize tokenizer state. If ST is the null pointer, then a state will
 * be allocated. In either case, ST must be freed using wmtok_free to ensure
 * that any resources allocated by this function (both now and in the
 * future) are properly freed. If ST was allocated outside of this function,
 * it must also be freed by the caller following a call to wmtok_free. */
WinMsgTokState *wmtok_init(WinMsgTokState *st)
{
	if (st == NULL) {
		st = malloc(sizeof(WinMsgTokState));
		st->_dofree = true;
	}

	return st;
}

/* Tokenize a source window message string, producing a token string into
 * DEST. The number of tokens will not exceed LEN. A return value of
 * (size_t)-1 indicates that additional tokens may be available, in which
 * case this function may simply be called again with the same ST to resume
 * lexing. */
size_t wmtok_tokenize(WinMsgTok **dest, char *src,
                      size_t len, WinMsgTokState *st)
{
	WinMsgTok **p = dest;

	/* do not exceed LEN tokens; otherwise, we risk overflowing the
	 * destination buffer */
	for (size_t n = 0; n < len; n++) {
		/* upon reaching the end of the src string, lexing is complete */
		if (*src++ == '\0') {
			/* TODO: temporary code for POC */
			*p = _wmtok_tok();
			(*p)->lexeme.ptr = src - 1;
			(*p)->lexeme.len = 1;
			p++;
			break;
		}
	}

	/* actual length may differ from N */
	return p - dest;
}

/* Free tozenkizer state and any resources allocated to it; should always be
 * called after wmtok_init, even if ST was allocated independently. */
void wmtok_free(WinMsgTokState *st)
{
	if (st->_dofree) {
		free(st);
	}
}

/* Retrieves the type of the given token as a WMTOK_* constant. */
WinMsgTokType wmtok_type(WinMsgTok *tok)
{
	return tok->type;
}

/* Retrieve a copy of the source string (lexeme) from which the token was
 * generated. The last byte written will be the null byte and will overwrite
 * the last character in the lexeme if LEN is too small. */
char *wmtok_lexeme(char *dest, WinMsgTok *tok, size_t len)
{
	size_t n = (tok->lexeme.len > len) ? len - 1 : tok->lexeme.len;

	/* the lexeme points to the original string from which we derived the
	 * token; make a copy to avoid hard-to-find bugs */
	strncpy(dest, tok->lexeme.ptr, n);
	dest[n] = '\0';

	return dest;
}

/* Retrieve additional data needed to propely parse the token, if
 * applicable. The data may be of any type, so a transparent union is
 * returned to permit multiple types; the caller is expected to know which
 * type the token uses. */
union WinMsgTokData wmtok_data(WinMsgTok *tok)
{
	return tok->data;
}
