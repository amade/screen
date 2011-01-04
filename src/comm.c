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
#include "os.h"
#include "acls.h"
#include "comm.h"

#define bcopy :-(		/* or include screen.h here */

/* Must be in alpha order ! */

struct comm comms[RC_LAST + 1] =
{
  { "acladd",		ARGS_1234 },
  { "aclchg",		ARGS_23 },
  { "acldel",		ARGS_1 },
  { "aclgrp",		ARGS_12 },
  { "aclumask",		ARGS_1|ARGS_ORMORE },
  { "activity",		ARGS_1 },
  { "addacl",		ARGS_1234 },
  { "allpartial",	NEED_DISPLAY|ARGS_1 },
  { "altscreen",	ARGS_01 },
  { "at",		ARGS_2|ARGS_ORMORE },
  { "attrcolor",	ARGS_12 },
  { "autodetach",	ARGS_1 },
  { "autonuke",		NEED_DISPLAY|ARGS_1 },
  { "backtick",		ARGS_1|ARGS_ORMORE },
  { "bce",		NEED_FORE|ARGS_01 },
  { "bell",		ARGS_01 },
  { "bell_msg",		ARGS_01 },
  { "bind",		ARGS_1|ARGS_ORMORE },
  { "bindkey",		ARGS_0|ARGS_ORMORE },
  { "blanker",		NEED_DISPLAY|ARGS_0},
  { "blankerprg",	ARGS_0|ARGS_ORMORE },
  { "break",		NEED_FORE|ARGS_01 },
  { "breaktype",	NEED_FORE|ARGS_01 },
  { "bufferfile",	ARGS_01 },
  { "bumpleft",		ARGS_0 },
  { "bumpright",	ARGS_0 },
  { "c1",		NEED_FORE|ARGS_01 },
  { "caption",		ARGS_12 },
  { "chacl",		ARGS_23 },
  { "charset",          NEED_FORE|ARGS_1 },
  { "chdir",		ARGS_01 },
  { "cjkwidth",		ARGS_01 },
  { "clear",		NEED_FORE|ARGS_0 },
  { "collapse",		ARGS_0 },
  { "colon",		NEED_LAYER|ARGS_01 },
  { "command",		NEED_DISPLAY|ARGS_02 },
  { "compacthist",	ARGS_01 },
  { "console",		NEED_FORE|ARGS_01 },
  { "copy",		NEED_FORE|NEED_DISPLAY|ARGS_0 },
  { "crlf",		ARGS_01 },
  { "debug",		ARGS_01 },
  { "defautonuke",	ARGS_1 },
  { "defbce",		ARGS_1 },
  { "defbreaktype",	ARGS_01 },
  { "defc1",		ARGS_1 },
  { "defcharset",       ARGS_01 },
  { "defencoding",	ARGS_1 },
  { "defescape",	ARGS_1 },
  { "defflow",		ARGS_12 },
  { "defgr",		ARGS_1 },
  { "defhstatus",	ARGS_01 },
  { "defkanji",		ARGS_1 },
  { "deflog",		ARGS_1 },
#if defined(UTMPOK) && defined(LOGOUTOK)
  { "deflogin",		ARGS_1 },
#endif
  { "defmode",		ARGS_1 },
  { "defmonitor",	ARGS_1 },
  { "defmousetrack",	ARGS_1 },
  { "defnonblock",	ARGS_1 },
  { "defobuflimit",	ARGS_1 },
  { "defscrollback",	ARGS_1 },
  { "defshell",		ARGS_1 },
  { "defsilence",	ARGS_1 },
  { "defslowpaste",	ARGS_1 },
  { "defutf8",		ARGS_1 },
  { "defwrap",		ARGS_1 },
  { "defwritelock",	ARGS_1 },
  { "detach",		NEED_DISPLAY|ARGS_01 },
  { "digraph",		NEED_LAYER|ARGS_012 },
  { "dinfo",		NEED_DISPLAY|ARGS_0 },
  { "displays",		NEED_LAYER|ARGS_0 },
  { "dumptermcap",	NEED_FORE|ARGS_0 },
  { "echo",		CAN_QUERY|ARGS_12 },
  { "encoding",		ARGS_12 },
  { "escape",		ARGS_1 },
  { "eval",		ARGS_1|ARGS_ORMORE },
  { "exec",		ARGS_0|ARGS_ORMORE },
  { "fit",		NEED_DISPLAY|ARGS_0 },
  { "flow",		NEED_FORE|ARGS_01 },
  { "focus",		NEED_DISPLAY|ARGS_01 },
  { "focusminsize",	ARGS_02 },
  { "gr",		NEED_FORE|ARGS_01 },
  { "group",            NEED_FORE|ARGS_01 },
  { "hardcopy",		NEED_FORE|ARGS_012 },
  { "hardcopy_append",	ARGS_1 },
  { "hardcopydir",	ARGS_01 },
  { "hardstatus",	ARGS_012 },
  { "height",		ARGS_0123 },
  { "help",		NEED_LAYER|ARGS_02 },
  { "history",		NEED_DISPLAY|NEED_FORE|ARGS_0 },
  { "hstatus",		NEED_FORE|ARGS_1 },
  { "idle",		ARGS_0|ARGS_ORMORE },
  { "ignorecase",	ARGS_01 },
  { "info",		CAN_QUERY|NEED_LAYER|ARGS_0 },
  { "kanji",		NEED_FORE|ARGS_12 },
  { "kill",		NEED_FORE|ARGS_0 },
  { "lastmsg",		CAN_QUERY|NEED_DISPLAY|ARGS_0 },
  { "layout",           ARGS_1|ARGS_ORMORE},
  { "license",		NEED_LAYER|ARGS_0 },
  { "lockscreen",	NEED_DISPLAY|ARGS_0 },
  { "log",		NEED_FORE|ARGS_01 },
  { "logfile",		ARGS_012 },
#if defined(UTMPOK) && defined(LOGOUTOK)
  { "login",		NEED_FORE|ARGS_01 },
#endif
  { "logtstamp",	ARGS_012 },
  { "mapdefault",	NEED_DISPLAY|ARGS_0 },
  { "mapnotnext",	NEED_DISPLAY|ARGS_0 },
  { "maptimeout",	ARGS_01 },
  { "markkeys",		ARGS_1 },
  { "maxwin",		ARGS_01 },
  { "meta",		NEED_LAYER|ARGS_0 },
  { "monitor",		NEED_FORE|ARGS_01 },
  { "mousetrack",	NEED_DISPLAY | ARGS_01 },
  { "msgminwait",	ARGS_1 },
  { "msgwait",		ARGS_1 },
  { "multiuser",	ARGS_1 },
  { "nethack",		ARGS_1 },
  { "next",		ARGS_0 },
  { "nonblock",		NEED_DISPLAY|ARGS_01 },
  { "number",		CAN_QUERY|NEED_FORE|ARGS_01 },
  { "obuflimit",	NEED_DISPLAY|ARGS_01 },
  { "only",		NEED_DISPLAY|ARGS_0 },
  { "other",		ARGS_0 },
  { "partial",		NEED_FORE|ARGS_01 },
  { "password",		ARGS_01 },
  { "paste",		NEED_LAYER|ARGS_012 },
  { "pastefont",	ARGS_01 },
  { "pow_break",	NEED_FORE|ARGS_01 },
  { "pow_detach",	NEED_DISPLAY|ARGS_0 },
  { "pow_detach_msg",	ARGS_01 },
  { "prev",		ARGS_0 },
  { "printcmd",		ARGS_01 },
  { "process",		NEED_DISPLAY|ARGS_01 },
  { "quit",		ARGS_0 },
  { "readbuf",		ARGS_0123 },
  { "readreg",          ARGS_0|ARGS_ORMORE },
  { "redisplay",	NEED_DISPLAY|ARGS_0 },
  { "register",		ARGS_24 },
  { "remove",		NEED_DISPLAY|ARGS_0 },
  { "removebuf",	ARGS_0 },
  { "rendition",	ARGS_23 },
  { "reset",		NEED_FORE|ARGS_0 },
  { "resize",		NEED_DISPLAY|ARGS_0|ARGS_ORMORE },
  { "screen",		ARGS_0|ARGS_ORMORE },
  { "scrollback",	NEED_FORE|ARGS_1 },
  { "select",		CAN_QUERY|ARGS_01 },
  { "sessionname",	ARGS_01 },
  { "setenv",		ARGS_012 },
  { "setsid",		ARGS_1 },
  { "shell",		ARGS_1 },
  { "shelltitle",	ARGS_1 },
  { "silence",		NEED_FORE|ARGS_01 },
  { "silencewait",	ARGS_1 },
  { "sleep",		ARGS_1 },
  { "slowpaste",	NEED_FORE|ARGS_01 },
  { "sorendition",      ARGS_012 },
  { "source",		ARGS_1 },
  { "split",		NEED_DISPLAY|ARGS_01 },
  { "startup_message",	ARGS_1 },
  { "stuff",		NEED_LAYER|ARGS_012 },
  { "su",		NEED_DISPLAY|ARGS_012 },
#ifdef BSDJOBS
  { "suspend",		NEED_DISPLAY|ARGS_0 },
#endif
  { "term",		ARGS_1 },
  { "termcap",		ARGS_23 },
  { "termcapinfo",	ARGS_23 },
  { "terminfo",		ARGS_23 },
  { "time",		CAN_QUERY|ARGS_01 },
  { "title",		CAN_QUERY|NEED_FORE|ARGS_01 },
  { "umask",		ARGS_1|ARGS_ORMORE },
  { "unbindall",	ARGS_0 },
  { "unsetenv",		ARGS_1 },
  { "utf8",		NEED_FORE|ARGS_012 },
  { "vbell",		ARGS_01 },
  { "vbell_msg",	ARGS_01 },
  { "vbellwait",	ARGS_1 },
  { "verbose",		ARGS_01 },
  { "version",		ARGS_0 },
  { "wall",		NEED_DISPLAY|ARGS_1},
  { "width",		ARGS_0123 },
  { "windowlist",	ARGS_012 },
  { "windows",		CAN_QUERY|ARGS_0 },
  { "wrap",		NEED_FORE|ARGS_01 },
  { "writebuf",		ARGS_0123 },
  { "writelock",	NEED_FORE|ARGS_01 },
  { "xoff",		NEED_LAYER|ARGS_0 },
  { "xon",		NEED_LAYER|ARGS_0 },
  { "zmodem",		ARGS_012 },
  { "zombie",		ARGS_012 }
};
