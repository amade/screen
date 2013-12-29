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
 * literal = { ctrlesc | character - esc char } ;
 *
 * (* a control escape sequence, e.g. ^[ *)
 * ctrlesc = '^' , ascii printable character ;
 *
 *
 * (* escape sequence; e.g. %n *)
 * escseq  = escchar , ( delimited escseq | standalone escseq ) ;
 * standalone escseq = [ escflag ] , [ number ] , [ esclong ] ,
 *                     ascii printable character ;
 * escflag = '+' | '-' | '0' ;
 * esclong = 'L' ;
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
 *           | rend attrmod , rend colordesc ;
 * rendpop   = '-' ;
 *
 * (* attribute modifier *)
 * rend attrmod    = [ rend attrchtype , ' ' ] , rend attrvalue ;
 * rend attrchtype = '+' | '-' | '!' | '=' ;
 * rend attrvalue  = 'd' | 'u' | 'b' | 'r' | 's' | 'B' | 'l' ;
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
