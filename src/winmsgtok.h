/* Copyright (c) 2013
 *      Mike Gerwitz (mtg@gnu.org)
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

/* EBNF Grammar
 * ------------
 * The language understood by the window message parser is represented here
 * in Extended Backus-Naur Form; the lexer is structured accordingly.
 *
 * winmsg = expr ;
 * expr   = { term } , end ;
 *
 * term = literal
 *      | escseq ;
 *
 * (* a sequence of characters to be echoed *)
 * literal = { ctrlesc | character - escchar } ;
 *
 * (* a control escape sequence, e.g. ^[ *)
 * ctrlesc = '^' , ascii printable character ;
 *
 *
 * (* escape sequence; e.g. %n *)
 * escseq  = escchar , ( delimited escseq | standalone escseq ) ;
 * standalone escseq = [ escflag ] , [ number ] , [ esclong ] ,
 *                     ascii printable character , [ escfmt ] ;
 * escflag = '+' | '-' | '.' | '0' ;
 * esclong = 'L' ;
 *
 * (* used for certain escapes to override default format *)
 * escfmt      = '{' , escfmt expr ;
 * escfmt expr = { escfmt term } ;
 * escfmt term = '}'
 *             | term ;
 *
 * (* the escape character can be dynamically set; default is '%' *)
 * escchar = ? current escape character ? ;
 *
 * (* delimited escape sequences are of arbitrary length *)
 * delimited escseq = rendition
 *                  | conditional
 *                  | group ;
 *
 *
 * (* renditions set character output settings such as color or weight *)
 * rendition = '{' , rendexpr , '}' ;
 * rendexpr  = rendpop
 *           | [ rend attrmod ] , rend colordesc ;
 * rendpop   = '-' ;
 *
 * (* attribute modifier *)
 * rend attrmod    = rend attrchtype , ' ' , rend attrvalue ;
 * rend attrchtype = '+' | '-' | '!' | '=' ;
 * rend attrvalue  = 'd' | 'u' | 'b' | 'r' | 's' | 'B' ;
 *
 * (* color descriptor; fg and optional bg *)
 * rend colordesc = rend colorval , [ ';' ] , [ rend colorval ] ;
 * rend colorval  = rend colorcode
 *                | number
 *                | 'x' , hexnumber ;
 * rend colorcode = 'k' | 'r' | 'g' | 'y' | 'b' | 'm' | 'c' | 'w' | 'd'
 *                | 'K' | 'R' | 'G' | 'Y' | 'B' | 'M' | 'C' | 'W' | 'D'
 *                | 'i' | 'I' | '.' ;
 *
 *
 * (* conditional output *)
 * conditional  = condchar , condterm ;
 * condexpr     = { condterm } ;
 * condelseexpr = { condelseterm } ;
 * condterm     = condelse
 *              | condelseterm ;
 * condelseterm = condend
 *              | term ;
 * condelse     = escchar , ':' , condelseexpr ;
 * condend      = escchar , condchar ;
 * condchar     = '?' ;
 *
 *
 * (* acts as a sub-expression with own size restrictions, alignment, etc *)
 * group     = '[' , groupexpr , groupend ;
 * groupexpr = { groupterm } ;
 * groupterm = groupend
 *           | ? TODO: planned feature ?
 *           | term ;
 * groupend  = escchar , ']' ;
 *
 *
 * (* TODO: UTF-8 support is planned *)
 * character       = { ascii character } ;
 * ascii character = ascii control character
 *                 | ascii printable character ;
 *
 * ascii control character   = ? 0x00 <= byte value <= 0x1f ?
 * ascii printable character = ? byte value >= 0x20 ?
 *
 * digit  = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' ;
 * number = { digit } ;
 *
 * hexdigit  = 'a' | 'b' | 'c' | 'd' | 'e' | 'f'
 *           | 'A' | 'B' | 'C' | 'D' | 'E' | 'F'
 *           | digit ;
 * hexnumber = { hexdigit } ;
 *
 * end = '\0' ;
 */

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>


/* Token types */
typedef enum {
	WMTOK_END,        /* terminating token; end of string */
	WMTOK_ECHO,       /* lexeme should be echoed */
} WinMsgTokType;

/* Transparent type exposing token values */
union WinMsgTokData {
	/* integers of various sorts */
	uint_fast8_t  uint8;
	uint_fast16_t uint16;
	uint_fast32_t uint32;
	uintmax_t     uintmax;
	intmax_t      intmax;
	size_t        size;

	void *ptr;	/* pointer to separately allocated data */
};

/* Window message token */
typedef struct {
	WinMsgTokType type;  /* token type */

	/* token src string; to reduce overhead, it will point into the original
	 * src string, and therefore includes a length (since it may not be
	 * null-terminated) */
	struct {
		char   *ptr;  /* pointer to lexeme in src string */
		size_t	len;  /* length of lexeme */
	} lexeme;

	/* general-purpose, pre-allocated area for a basic value; use the void
	 * ptr if additional allocation is necessary */
	union WinMsgTokData data;
} WinMsgTok;

/* Represents state and configuration options for tokenizer */
typedef struct {
	char esc;  /* escape character */

	bool _dofree;  /* whether we allocated ourself */
} WinMsgTokState;


WinMsgTokState *wmtok_init(WinMsgTokState *);
size_t wmtok_tokenize(WinMsgTok **, char *, size_t, WinMsgTokState *);
void wmtok_free(WinMsgTokState *);

/** These functions expose data from the opaque WinMsgTok type in a way that
 * allows altering the structure in the future with no ill effects. **/
WinMsgTokType wmtok_type(WinMsgTok *);
char *wmtok_lexeme(char *, WinMsgTok *, size_t);
union WinMsgTokData wmtok_data(WinMsgTok *);
