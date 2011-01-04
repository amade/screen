/* Copyright (c) 2010
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Sadrul Habib Chowdhury (sadrul@users.sourceforge.net)
 * Copyright (c) 2008, 2009
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
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#if !defined(sun) && !defined(B43) && !defined(ISC) && !defined(pyr) && !defined(_CX_UX)
# include <time.h>
#endif
#include <sys/time.h>
#ifndef sun
#include <sys/ioctl.h>
#endif


/* for solaris 2.1, Unixware (SVR4.2) and possibly others: */
#ifdef HAVE_STROPTS_H
# include <sys/stropts.h>
#endif

#include "screen.h"
#include "extern.h"
#include "logfile.h"
#include "layout.h"
#include "viewport.h"
#include "list_generic.h"

extern struct comm comms[];
extern char *rc_name;
extern char *RcFileName, *home;
extern char *BellString, *ActivityString, *ShellProg, *ShellArgs[];
extern char *hstatusstring, *captionstring, *timestring;
extern char *wliststr, *wlisttit;
extern int captionalways;
extern int queryflag;
extern char *hardcopydir, *screenlogfile, *logtstamp_string;
extern int log_flush, logtstamp_on, logtstamp_after;
extern char *VisualBellString;
extern int VBellWait, MsgWait, MsgMinWait, SilenceWait;
extern char SockPath[], *SockName;
extern int TtyMode, auto_detach, use_altscreen;
extern int iflag, maxwin;
extern int focusminwidth, focusminheight;
extern int use_hardstatus, visual_bell;
#ifdef COLOR
extern int attr2color[][4];
extern int nattr2color;
#endif
extern int hardstatusemu;
extern char *printcmd;
extern int default_startup;
extern int defobuflimit;
extern int defnonblock;
extern int defmousetrack;
extern int ZombieKey_destroy;
extern int ZombieKey_resurrect;
extern int ZombieKey_onerror;
#ifdef AUTO_NUKE
extern int defautonuke;
#endif
extern int separate_sids;
extern struct NewWindow nwin_default, nwin_undef;
#ifdef COPY_PASTE
extern int join_with_cr;
extern int compacthist;
extern int search_ic;
# ifdef FONT
extern int pastefont;
# endif
extern unsigned char mark_key_tab[];
extern char *BufferFile;
#endif
#ifdef POW_DETACH
extern char *BufferFile, *PowDetachString;
#endif
#ifdef MULTIUSER
extern struct acluser *EffectiveAclUser;	/* acl.c */
#endif
extern struct term term[];      /* terminal capabilities */
#ifdef MAPKEYS
extern char *kmapdef[];
extern char *kmapadef[];
extern char *kmapmdef[];
#endif
extern struct mchar mchar_so, mchar_null;
extern int renditions[];
extern int VerboseCreate;
#ifdef UTF8
extern char *screenencodings;
#endif
#ifdef DW_CHARS
extern int cjkwidth;
#endif

static int  CheckArgNum (int, char **);
static void ClearAction (struct action *);
static void SaveAction (struct action *, int, char **, int *);
static int  NextWindow (void);
static int  PreviousWindow (void);
static int  MoreWindows (void);
static void CollapseWindowlist (void);
static void LogToggle (int);
static void ShowInfo (void);
static void ShowDInfo (void);
static struct win *WindowByName (char *);
static int  WindowByNumber (char *);
static int  ParseOnOff (struct action *, int *);
static int  ParseWinNum (struct action *, int *);
static int  ParseBase (struct action *, char *, int *, int, char *);
static int  ParseNum1000 (struct action *, int *);
static char **SaveArgs (char **);
static int  IsNum (char *, int);
static void Colonfin (char *, int, char *);
static void InputSelect (void);
static void InputSetenv (char *);
static void InputAKA (void);
#ifdef MULTIUSER
static int  InputSu (struct win *, struct acluser **, char *);
static void su_fin (char *, int, char *);
#endif
static void AKAfin (char *, int, char *);
#ifdef COPY_PASTE
static void copy_reg_fn (char *, int, char *);
static void ins_reg_fn (char *, int, char *);
#endif
static void process_fn (char *, int, char *);
#ifdef PASSWORD
static void pass1 (char *, int, char *);
static void pass2 (char *, int, char *);
#endif
#ifdef POW_DETACH
static void pow_detach_fn (char *, int, char *);
#endif
static void digraph_fn (char *, int, char *);
static int  digraph_find (const char *buf);
static void confirm_fn (char *, int, char *);
static int  IsOnDisplay (struct win *);
static void ResizeRegions (char *, int);
static void ResizeFin (char *, int, char *);
static struct action *FindKtab (char *, int);
static void SelectFin (char *, int, char *);
static void SelectLayoutFin (char *, int, char *);


extern struct layer *flayer;
extern struct display *display, *displays;
extern struct win *fore, *console_window, *windows;
extern struct acluser *users;
extern struct layout *layouts, *layout_attach, layout_last_marker;
extern struct layout *laytab[];

extern char screenterm[], HostName[], version[];
extern struct NewWindow nwin_undef, nwin_default;
extern struct LayFuncs WinLf, MarkLf;

extern int Z0width, Z1width;
extern int real_uid, real_gid;

#ifdef NETHACK
extern int nethackflag;
#endif


extern struct win **wtab;

#ifdef MULTIUSER
extern char *multi;
extern int maxusercount;
#endif
char NullStr[] = "";

struct plop plop_tab[MAX_PLOP_DEFS];

#ifndef PTYMODE
# define PTYMODE 0622
#endif

int TtyMode = PTYMODE;
int hardcopy_append = 0;
int all_norefresh = 0;
#ifdef ZMODEM
int zmodem_mode = 0;
char *zmodem_sendcmd;
char *zmodem_recvcmd;
static char *zmodes[4] = {"off", "auto", "catch", "pass"};
#endif

int idletimo;
struct action idleaction;
#ifdef BLANKER_PRG
char **blankerprg;
#endif

struct action ktab[256 + KMAP_KEYS];	/* command key translation table */
struct kclass {
  struct kclass *next;
  char *name;
  struct action ktab[256 + KMAP_KEYS];
};
struct kclass *kclasses;

#ifdef MAPKEYS
struct action umtab[KMAP_KEYS+KMAP_AKEYS];
struct action dmtab[KMAP_KEYS+KMAP_AKEYS];
struct action mmtab[KMAP_KEYS+KMAP_AKEYS];
struct kmap_ext *kmap_exts;
int kmap_extn;
static int maptimeout = 300;
#endif

struct digraph
{
  unsigned char d[2];
  int value;
};

/* digraph table taken from old vim and rfc1345 */
static struct digraph digraphs[] = {
    {{'N', 'S'}, 0x00a0},   /* NO-BREAK SPACE */
    {{'!', 'I'}, 0x00a1},   /* INVERTED EXCLAMATION MARK */
    {{'C', 't'}, 0x00a2},   /* CENT SIGN */
    {{'P', 'd'}, 0x00a3},   /* POUND SIGN */
    {{'C', 'u'}, 0x00a4},   /* CURRENCY SIGN */
    {{'Y', 'e'}, 0x00a5},   /* YEN SIGN */
    {{'B', 'B'}, 0x00a6},   /* BROKEN BAR */
    {{'S', 'E'}, 0x00a7},   /* SECTION SIGN */
    {{'\'', ':'}, 0x00a8},   /* DIAERESIS */
    {{'C', 'o'}, 0x00a9},   /* COPYRIGHT SIGN */
    {{'-', 'a'}, 0x00aa},   /* FEMININE ORDINAL INDICATOR */
    {{'<', '<'}, 0x00ab},   /* LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
    {{'N', 'O'}, 0x00ac},   /* NOT SIGN */
    {{'-', '-'}, 0x00ad},   /* SOFT HYPHEN */
    {{'R', 'g'}, 0x00ae},   /* REGISTERED SIGN */
    {{'\'', 'm'}, 0x00af},   /* MACRON */
    {{'D', 'G'}, 0x00b0},   /* DEGREE SIGN */
    {{'+', '-'}, 0x00b1},   /* PLUS-MINUS SIGN */
    {{'2', 'S'}, 0x00b2},   /* SUPERSCRIPT TWO */
    {{'3', 'S'}, 0x00b3},   /* SUPERSCRIPT THREE */
    {{'\'', '\''}, 0x00b4},   /* ACUTE ACCENT */
    {{'M', 'y'}, 0x00b5},   /* MICRO SIGN */
    {{'P', 'I'}, 0x00b6},   /* PILCROW SIGN */
    {{'.', 'M'}, 0x00b7},   /* MIDDLE DOT */
    {{'\'', ','}, 0x00b8},   /* CEDILLA */
    {{'1', 'S'}, 0x00b9},   /* SUPERSCRIPT ONE */
    {{'-', 'o'}, 0x00ba},   /* MASCULINE ORDINAL INDICATOR */
    {{'>', '>'}, 0x00bb},   /* RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
    {{'1', '4'}, 0x00bc},   /* VULGAR FRACTION ONE QUARTER */
    {{'1', '2'}, 0x00bd},   /* VULGAR FRACTION ONE HALF */
    {{'3', '4'}, 0x00be},   /* VULGAR FRACTION THREE QUARTERS */
    {{'?', 'I'}, 0x00bf},   /* INVERTED QUESTION MARK */
    {{'A', '!'}, 0x00c0},   /* LATIN CAPITAL LETTER A WITH GRAVE */
    {{'A', '\''}, 0x00c1},   /* LATIN CAPITAL LETTER A WITH ACUTE */
    {{'A', '>'}, 0x00c2},   /* LATIN CAPITAL LETTER A WITH CIRCUMFLEX */
    {{'A', '?'}, 0x00c3},   /* LATIN CAPITAL LETTER A WITH TILDE */
    {{'A', ':'}, 0x00c4},   /* LATIN CAPITAL LETTER A WITH DIAERESIS */
    {{'A', 'A'}, 0x00c5},   /* LATIN CAPITAL LETTER A WITH RING ABOVE */
    {{'A', 'E'}, 0x00c6},   /* LATIN CAPITAL LETTER AE */
    {{'C', ','}, 0x00c7},   /* LATIN CAPITAL LETTER C WITH CEDILLA */
    {{'E', '!'}, 0x00c8},   /* LATIN CAPITAL LETTER E WITH GRAVE */
    {{'E', '\''}, 0x00c9},   /* LATIN CAPITAL LETTER E WITH ACUTE */
    {{'E', '>'}, 0x00ca},   /* LATIN CAPITAL LETTER E WITH CIRCUMFLEX */
    {{'E', ':'}, 0x00cb},   /* LATIN CAPITAL LETTER E WITH DIAERESIS */
    {{'I', '!'}, 0x00cc},   /* LATIN CAPITAL LETTER I WITH GRAVE */
    {{'I', '\''}, 0x00cd},   /* LATIN CAPITAL LETTER I WITH ACUTE */
    {{'I', '>'}, 0x00ce},   /* LATIN CAPITAL LETTER I WITH CIRCUMFLEX */
    {{'I', ':'}, 0x00cf},   /* LATIN CAPITAL LETTER I WITH DIAERESIS */
    {{'D', '-'}, 0x00d0},   /* LATIN CAPITAL LETTER ETH (Icelandic) */
    {{'N', '?'}, 0x00d1},   /* LATIN CAPITAL LETTER N WITH TILDE */
    {{'O', '!'}, 0x00d2},   /* LATIN CAPITAL LETTER O WITH GRAVE */
    {{'O', '\''}, 0x00d3},   /* LATIN CAPITAL LETTER O WITH ACUTE */
    {{'O', '>'}, 0x00d4},   /* LATIN CAPITAL LETTER O WITH CIRCUMFLEX */
    {{'O', '?'}, 0x00d5},   /* LATIN CAPITAL LETTER O WITH TILDE */
    {{'O', ':'}, 0x00d6},   /* LATIN CAPITAL LETTER O WITH DIAERESIS */
    {{'*', 'X'}, 0x00d7},   /* MULTIPLICATION SIGN */
    {{'O', '/'}, 0x00d8},   /* LATIN CAPITAL LETTER O WITH STROKE */
    {{'U', '!'}, 0x00d9},   /* LATIN CAPITAL LETTER U WITH GRAVE */
    {{'U', '\''}, 0x00da},   /* LATIN CAPITAL LETTER U WITH ACUTE */
    {{'U', '>'}, 0x00db},   /* LATIN CAPITAL LETTER U WITH CIRCUMFLEX */
    {{'U', ':'}, 0x00dc},   /* LATIN CAPITAL LETTER U WITH DIAERESIS */
    {{'Y', '\''}, 0x00dd},   /* LATIN CAPITAL LETTER Y WITH ACUTE */
    {{'T', 'H'}, 0x00de},   /* LATIN CAPITAL LETTER THORN (Icelandic) */
    {{'s', 's'}, 0x00df},   /* LATIN SMALL LETTER SHARP S (German) */
    {{'a', '!'}, 0x00e0},   /* LATIN SMALL LETTER A WITH GRAVE */
    {{'a', '\''}, 0x00e1},   /* LATIN SMALL LETTER A WITH ACUTE */
    {{'a', '>'}, 0x00e2},   /* LATIN SMALL LETTER A WITH CIRCUMFLEX */
    {{'a', '?'}, 0x00e3},   /* LATIN SMALL LETTER A WITH TILDE */
    {{'a', ':'}, 0x00e4},   /* LATIN SMALL LETTER A WITH DIAERESIS */
    {{'a', 'a'}, 0x00e5},   /* LATIN SMALL LETTER A WITH RING ABOVE */
    {{'a', 'e'}, 0x00e6},   /* LATIN SMALL LETTER AE */
    {{'c', ','}, 0x00e7},   /* LATIN SMALL LETTER C WITH CEDILLA */
    {{'e', '!'}, 0x00e8},   /* LATIN SMALL LETTER E WITH GRAVE */
    {{'e', '\''}, 0x00e9},   /* LATIN SMALL LETTER E WITH ACUTE */
    {{'e', '>'}, 0x00ea},   /* LATIN SMALL LETTER E WITH CIRCUMFLEX */
    {{'e', ':'}, 0x00eb},   /* LATIN SMALL LETTER E WITH DIAERESIS */
    {{'i', '!'}, 0x00ec},   /* LATIN SMALL LETTER I WITH GRAVE */
    {{'i', '\''}, 0x00ed},   /* LATIN SMALL LETTER I WITH ACUTE */
    {{'i', '>'}, 0x00ee},   /* LATIN SMALL LETTER I WITH CIRCUMFLEX */
    {{'i', ':'}, 0x00ef},   /* LATIN SMALL LETTER I WITH DIAERESIS */
    {{'d', '-'}, 0x00f0},   /* LATIN SMALL LETTER ETH (Icelandic) */
    {{'n', '?'}, 0x00f1},   /* LATIN SMALL LETTER N WITH TILDE */
    {{'o', '!'}, 0x00f2},   /* LATIN SMALL LETTER O WITH GRAVE */
    {{'o', '\''}, 0x00f3},   /* LATIN SMALL LETTER O WITH ACUTE */
    {{'o', '>'}, 0x00f4},   /* LATIN SMALL LETTER O WITH CIRCUMFLEX */
    {{'o', '?'}, 0x00f5},   /* LATIN SMALL LETTER O WITH TILDE */
    {{'o', ':'}, 0x00f6},   /* LATIN SMALL LETTER O WITH DIAERESIS */
    {{'-', ':'}, 0x00f7},   /* DIVISION SIGN */
    {{'o', '/'}, 0x00f8},   /* LATIN SMALL LETTER O WITH STROKE */
    {{'u', '!'}, 0x00f9},   /* LATIN SMALL LETTER U WITH GRAVE */
    {{'u', '\''}, 0x00fa},   /* LATIN SMALL LETTER U WITH ACUTE */
    {{'u', '>'}, 0x00fb},   /* LATIN SMALL LETTER U WITH CIRCUMFLEX */
    {{'u', ':'}, 0x00fc},   /* LATIN SMALL LETTER U WITH DIAERESIS */
    {{'y', '\''}, 0x00fd},   /* LATIN SMALL LETTER Y WITH ACUTE */
    {{'t', 'h'}, 0x00fe},   /* LATIN SMALL LETTER THORN (Icelandic) */
    {{'y', ':'}, 0x00ff},   /* LATIN SMALL LETTER Y WITH DIAERESIS */
    {{'A', '-'}, 0x0100},   /* LATIN CAPITAL LETTER A WITH MACRON */
    {{'a', '-'}, 0x0101},   /* LATIN SMALL LETTER A WITH MACRON */
    {{'A', '('}, 0x0102},   /* LATIN CAPITAL LETTER A WITH BREVE */
    {{'a', '('}, 0x0103},   /* LATIN SMALL LETTER A WITH BREVE */
    {{'A', ';'}, 0x0104},   /* LATIN CAPITAL LETTER A WITH OGONEK */
    {{'a', ';'}, 0x0105},   /* LATIN SMALL LETTER A WITH OGONEK */
    {{'C', '\''}, 0x0106},   /* LATIN CAPITAL LETTER C WITH ACUTE */
    {{'c', '\''}, 0x0107},   /* LATIN SMALL LETTER C WITH ACUTE */
    {{'C', '>'}, 0x0108},   /* LATIN CAPITAL LETTER C WITH CIRCUMFLEX */
    {{'c', '>'}, 0x0109},   /* LATIN SMALL LETTER C WITH CIRCUMFLEX */
    {{'C', '.'}, 0x010a},   /* LATIN CAPITAL LETTER C WITH DOT ABOVE */
    {{'c', '.'}, 0x010b},   /* LATIN SMALL LETTER C WITH DOT ABOVE */
    {{'C', '<'}, 0x010c},   /* LATIN CAPITAL LETTER C WITH CARON */
    {{'c', '<'}, 0x010d},   /* LATIN SMALL LETTER C WITH CARON */
    {{'D', '<'}, 0x010e},   /* LATIN CAPITAL LETTER D WITH CARON */
    {{'d', '<'}, 0x010f},   /* LATIN SMALL LETTER D WITH CARON */
    {{'D', '/'}, 0x0110},   /* LATIN CAPITAL LETTER D WITH STROKE */
    {{'d', '/'}, 0x0111},   /* LATIN SMALL LETTER D WITH STROKE */
    {{'E', '-'}, 0x0112},   /* LATIN CAPITAL LETTER E WITH MACRON */
    {{'e', '-'}, 0x0113},   /* LATIN SMALL LETTER E WITH MACRON */
    {{'E', '('}, 0x0114},   /* LATIN CAPITAL LETTER E WITH BREVE */
    {{'e', '('}, 0x0115},   /* LATIN SMALL LETTER E WITH BREVE */
    {{'E', '.'}, 0x0116},   /* LATIN CAPITAL LETTER E WITH DOT ABOVE */
    {{'e', '.'}, 0x0117},   /* LATIN SMALL LETTER E WITH DOT ABOVE */
    {{'E', ';'}, 0x0118},   /* LATIN CAPITAL LETTER E WITH OGONEK */
    {{'e', ';'}, 0x0119},   /* LATIN SMALL LETTER E WITH OGONEK */
    {{'E', '<'}, 0x011a},   /* LATIN CAPITAL LETTER E WITH CARON */
    {{'e', '<'}, 0x011b},   /* LATIN SMALL LETTER E WITH CARON */
    {{'G', '>'}, 0x011c},   /* LATIN CAPITAL LETTER G WITH CIRCUMFLEX */
    {{'g', '>'}, 0x011d},   /* LATIN SMALL LETTER G WITH CIRCUMFLEX */
    {{'G', '('}, 0x011e},   /* LATIN CAPITAL LETTER G WITH BREVE */
    {{'g', '('}, 0x011f},   /* LATIN SMALL LETTER G WITH BREVE */
    {{'G', '.'}, 0x0120},   /* LATIN CAPITAL LETTER G WITH DOT ABOVE */
    {{'g', '.'}, 0x0121},   /* LATIN SMALL LETTER G WITH DOT ABOVE */
    {{'G', ','}, 0x0122},   /* LATIN CAPITAL LETTER G WITH CEDILLA */
    {{'g', ','}, 0x0123},   /* LATIN SMALL LETTER G WITH CEDILLA */
    {{'H', '>'}, 0x0124},   /* LATIN CAPITAL LETTER H WITH CIRCUMFLEX */
    {{'h', '>'}, 0x0125},   /* LATIN SMALL LETTER H WITH CIRCUMFLEX */
    {{'H', '/'}, 0x0126},   /* LATIN CAPITAL LETTER H WITH STROKE */
    {{'h', '/'}, 0x0127},   /* LATIN SMALL LETTER H WITH STROKE */
    {{'I', '?'}, 0x0128},   /* LATIN CAPITAL LETTER I WITH TILDE */
    {{'i', '?'}, 0x0129},   /* LATIN SMALL LETTER I WITH TILDE */
    {{'I', '-'}, 0x012a},   /* LATIN CAPITAL LETTER I WITH MACRON */
    {{'i', '-'}, 0x012b},   /* LATIN SMALL LETTER I WITH MACRON */
    {{'I', '('}, 0x012c},   /* LATIN CAPITAL LETTER I WITH BREVE */
    {{'i', '('}, 0x012d},   /* LATIN SMALL LETTER I WITH BREVE */
    {{'I', ';'}, 0x012e},   /* LATIN CAPITAL LETTER I WITH OGONEK */
    {{'i', ';'}, 0x012f},   /* LATIN SMALL LETTER I WITH OGONEK */
    {{'I', '.'}, 0x0130},   /* LATIN CAPITAL LETTER I WITH DOT ABOVE */
    {{'i', '.'}, 0x0131},   /* LATIN SMALL LETTER I DOTLESS */
    {{'I', 'J'}, 0x0132},   /* LATIN CAPITAL LIGATURE IJ */
    {{'i', 'j'}, 0x0133},   /* LATIN SMALL LIGATURE IJ */
    {{'J', '>'}, 0x0134},   /* LATIN CAPITAL LETTER J WITH CIRCUMFLEX */
    {{'j', '>'}, 0x0135},   /* LATIN SMALL LETTER J WITH CIRCUMFLEX */
    {{'K', ','}, 0x0136},   /* LATIN CAPITAL LETTER K WITH CEDILLA */
    {{'k', ','}, 0x0137},   /* LATIN SMALL LETTER K WITH CEDILLA */
    {{'k', 'k'}, 0x0138},   /* LATIN SMALL LETTER KRA (Greenlandic) */
    {{'L', '\''}, 0x0139},   /* LATIN CAPITAL LETTER L WITH ACUTE */
    {{'l', '\''}, 0x013a},   /* LATIN SMALL LETTER L WITH ACUTE */
    {{'L', ','}, 0x013b},   /* LATIN CAPITAL LETTER L WITH CEDILLA */
    {{'l', ','}, 0x013c},   /* LATIN SMALL LETTER L WITH CEDILLA */
    {{'L', '<'}, 0x013d},   /* LATIN CAPITAL LETTER L WITH CARON */
    {{'l', '<'}, 0x013e},   /* LATIN SMALL LETTER L WITH CARON */
    {{'L', '.'}, 0x013f},   /* LATIN CAPITAL LETTER L WITH MIDDLE DOT */
    {{'l', '.'}, 0x0140},   /* LATIN SMALL LETTER L WITH MIDDLE DOT */
    {{'L', '/'}, 0x0141},   /* LATIN CAPITAL LETTER L WITH STROKE */
    {{'l', '/'}, 0x0142},   /* LATIN SMALL LETTER L WITH STROKE */
    {{'N', '\''}, 0x0143},   /* LATIN CAPITAL LETTER N WITH ACUTE */
    {{'n', '\''}, 0x0144},   /* LATIN SMALL LETTER N WITH ACUTE */
    {{'N', ','}, 0x0145},   /* LATIN CAPITAL LETTER N WITH CEDILLA */
    {{'n', ','}, 0x0146},   /* LATIN SMALL LETTER N WITH CEDILLA */
    {{'N', '<'}, 0x0147},   /* LATIN CAPITAL LETTER N WITH CARON */
    {{'n', '<'}, 0x0148},   /* LATIN SMALL LETTER N WITH CARON */
    {{'\'', 'n'}, 0x0149},   /* LATIN SMALL LETTER N PRECEDED BY APOSTROPHE */
    {{'N', 'G'}, 0x014a},   /* LATIN CAPITAL LETTER ENG (Lappish) */
    {{'n', 'g'}, 0x014b},   /* LATIN SMALL LETTER ENG (Lappish) */
    {{'O', '-'}, 0x014c},   /* LATIN CAPITAL LETTER O WITH MACRON */
    {{'o', '-'}, 0x014d},   /* LATIN SMALL LETTER O WITH MACRON */
    {{'O', '('}, 0x014e},   /* LATIN CAPITAL LETTER O WITH BREVE */
    {{'o', '('}, 0x014f},   /* LATIN SMALL LETTER O WITH BREVE */
    {{'O', '"'}, 0x0150},   /* LATIN CAPITAL LETTER O WITH DOUBLE ACUTE */
    {{'o', '"'}, 0x0151},   /* LATIN SMALL LETTER O WITH DOUBLE ACUTE */
    {{'O', 'E'}, 0x0152},   /* LATIN CAPITAL LIGATURE OE */
    {{'o', 'e'}, 0x0153},   /* LATIN SMALL LIGATURE OE */
    {{'R', '\''}, 0x0154},   /* LATIN CAPITAL LETTER R WITH ACUTE */
    {{'r', '\''}, 0x0155},   /* LATIN SMALL LETTER R WITH ACUTE */
    {{'R', ','}, 0x0156},   /* LATIN CAPITAL LETTER R WITH CEDILLA */
    {{'r', ','}, 0x0157},   /* LATIN SMALL LETTER R WITH CEDILLA */
    {{'R', '<'}, 0x0158},   /* LATIN CAPITAL LETTER R WITH CARON */
    {{'r', '<'}, 0x0159},   /* LATIN SMALL LETTER R WITH CARON */
    {{'S', '\''}, 0x015a},   /* LATIN CAPITAL LETTER S WITH ACUTE */
    {{'s', '\''}, 0x015b},   /* LATIN SMALL LETTER S WITH ACUTE */
    {{'S', '>'}, 0x015c},   /* LATIN CAPITAL LETTER S WITH CIRCUMFLEX */
    {{'s', '>'}, 0x015d},   /* LATIN SMALL LETTER S WITH CIRCUMFLEX */
    {{'S', ','}, 0x015e},   /* LATIN CAPITAL LETTER S WITH CEDILLA */
    {{'s', ','}, 0x015f},   /* LATIN SMALL LETTER S WITH CEDILLA */
    {{'S', '<'}, 0x0160},   /* LATIN CAPITAL LETTER S WITH CARON */
    {{'s', '<'}, 0x0161},   /* LATIN SMALL LETTER S WITH CARON */
    {{'T', ','}, 0x0162},   /* LATIN CAPITAL LETTER T WITH CEDILLA */
    {{'t', ','}, 0x0163},   /* LATIN SMALL LETTER T WITH CEDILLA */
    {{'T', '<'}, 0x0164},   /* LATIN CAPITAL LETTER T WITH CARON */
    {{'t', '<'}, 0x0165},   /* LATIN SMALL LETTER T WITH CARON */
    {{'T', '/'}, 0x0166},   /* LATIN CAPITAL LETTER T WITH STROKE */
    {{'t', '/'}, 0x0167},   /* LATIN SMALL LETTER T WITH STROKE */
    {{'U', '?'}, 0x0168},   /* LATIN CAPITAL LETTER U WITH TILDE */
    {{'u', '?'}, 0x0169},   /* LATIN SMALL LETTER U WITH TILDE */
    {{'U', '-'}, 0x016a},   /* LATIN CAPITAL LETTER U WITH MACRON */
    {{'u', '-'}, 0x016b},   /* LATIN SMALL LETTER U WITH MACRON */
    {{'U', '('}, 0x016c},   /* LATIN CAPITAL LETTER U WITH BREVE */
    {{'u', '('}, 0x016d},   /* LATIN SMALL LETTER U WITH BREVE */
    {{'U', '0'}, 0x016e},   /* LATIN CAPITAL LETTER U WITH RING ABOVE */
    {{'u', '0'}, 0x016f},   /* LATIN SMALL LETTER U WITH RING ABOVE */
    {{'U', '"'}, 0x0170},   /* LATIN CAPITAL LETTER U WITH DOUBLE ACUTE */
    {{'u', '"'}, 0x0171},   /* LATIN SMALL LETTER U WITH DOUBLE ACUTE */
    {{'U', ';'}, 0x0172},   /* LATIN CAPITAL LETTER U WITH OGONEK */
    {{'u', ';'}, 0x0173},   /* LATIN SMALL LETTER U WITH OGONEK */
    {{'W', '>'}, 0x0174},   /* LATIN CAPITAL LETTER W WITH CIRCUMFLEX */
    {{'w', '>'}, 0x0175},   /* LATIN SMALL LETTER W WITH CIRCUMFLEX */
    {{'Y', '>'}, 0x0176},   /* LATIN CAPITAL LETTER Y WITH CIRCUMFLEX */
    {{'y', '>'}, 0x0177},   /* LATIN SMALL LETTER Y WITH CIRCUMFLEX */
    {{'Y', ':'}, 0x0178},   /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
    {{'Z', '\''}, 0x0179},   /* LATIN CAPITAL LETTER Z WITH ACUTE */
    {{'z', '\''}, 0x017a},   /* LATIN SMALL LETTER Z WITH ACUTE */
    {{'Z', '.'}, 0x017b},   /* LATIN CAPITAL LETTER Z WITH DOT ABOVE */
    {{'z', '.'}, 0x017c},   /* LATIN SMALL LETTER Z WITH DOT ABOVE */
    {{'Z', '<'}, 0x017d},   /* LATIN CAPITAL LETTER Z WITH CARON */
    {{'z', '<'}, 0x017e},   /* LATIN SMALL LETTER Z WITH CARON */
    {{'O', '9'}, 0x01a0},   /* LATIN CAPITAL LETTER O WITH HORN */
    {{'o', '9'}, 0x01a1},   /* LATIN SMALL LETTER O WITH HORN */
    {{'O', 'I'}, 0x01a2},   /* LATIN CAPITAL LETTER OI */
    {{'o', 'i'}, 0x01a3},   /* LATIN SMALL LETTER OI */
    {{'y', 'r'}, 0x01a6},   /* LATIN LETTER YR */
    {{'U', '9'}, 0x01af},   /* LATIN CAPITAL LETTER U WITH HORN */
    {{'u', '9'}, 0x01b0},   /* LATIN SMALL LETTER U WITH HORN */
    {{'Z', '/'}, 0x01b5},   /* LATIN CAPITAL LETTER Z WITH STROKE */
    {{'z', '/'}, 0x01b6},   /* LATIN SMALL LETTER Z WITH STROKE */
    {{'E', 'D'}, 0x01b7},   /* LATIN CAPITAL LETTER EZH */
    {{'A', '<'}, 0x01cd},   /* LATIN CAPITAL LETTER A WITH CARON */
    {{'a', '<'}, 0x01ce},   /* LATIN SMALL LETTER A WITH CARON */
    {{'I', '<'}, 0x01cf},   /* LATIN CAPITAL LETTER I WITH CARON */
    {{'i', '<'}, 0x01d0},   /* LATIN SMALL LETTER I WITH CARON */
    {{'O', '<'}, 0x01d1},   /* LATIN CAPITAL LETTER O WITH CARON */
    {{'o', '<'}, 0x01d2},   /* LATIN SMALL LETTER O WITH CARON */
    {{'U', '<'}, 0x01d3},   /* LATIN CAPITAL LETTER U WITH CARON */
    {{'u', '<'}, 0x01d4},   /* LATIN SMALL LETTER U WITH CARON */
    {{'A', '1'}, 0x01de},   /* LATIN CAPITAL LETTER A WITH DIAERESIS AND MACRON */
    {{'a', '1'}, 0x01df},   /* LATIN SMALL LETTER A WITH DIAERESIS AND MACRON */
    {{'A', '7'}, 0x01e0},   /* LATIN CAPITAL LETTER A WITH DOT ABOVE AND MACRON */
    {{'a', '7'}, 0x01e1},   /* LATIN SMALL LETTER A WITH DOT ABOVE AND MACRON */
    {{'A', '3'}, 0x01e2},   /* LATIN CAPITAL LETTER AE WITH MACRON */
    {{'a', '3'}, 0x01e3},   /* LATIN SMALL LETTER AE WITH MACRON */
    {{'G', '/'}, 0x01e4},   /* LATIN CAPITAL LETTER G WITH STROKE */
    {{'g', '/'}, 0x01e5},   /* LATIN SMALL LETTER G WITH STROKE */
    {{'G', '<'}, 0x01e6},   /* LATIN CAPITAL LETTER G WITH CARON */
    {{'g', '<'}, 0x01e7},   /* LATIN SMALL LETTER G WITH CARON */
    {{'K', '<'}, 0x01e8},   /* LATIN CAPITAL LETTER K WITH CARON */
    {{'k', '<'}, 0x01e9},   /* LATIN SMALL LETTER K WITH CARON */
    {{'O', ';'}, 0x01ea},   /* LATIN CAPITAL LETTER O WITH OGONEK */
    {{'o', ';'}, 0x01eb},   /* LATIN SMALL LETTER O WITH OGONEK */
    {{'O', '1'}, 0x01ec},   /* LATIN CAPITAL LETTER O WITH OGONEK AND MACRON */
    {{'o', '1'}, 0x01ed},   /* LATIN SMALL LETTER O WITH OGONEK AND MACRON */
    {{'E', 'Z'}, 0x01ee},   /* LATIN CAPITAL LETTER EZH WITH CARON */
    {{'e', 'z'}, 0x01ef},   /* LATIN SMALL LETTER EZH WITH CARON */
    {{'j', '<'}, 0x01f0},   /* LATIN SMALL LETTER J WITH CARON */
    {{'G', '\''}, 0x01f4},   /* LATIN CAPITAL LETTER G WITH ACUTE */
    {{'g', '\''}, 0x01f5},   /* LATIN SMALL LETTER G WITH ACUTE */
    {{';', 'S'}, 0x02bf},   /* MODIFIER LETTER LEFT HALF RING */
    {{'\'', '<'}, 0x02c7},   /* CARON */
    {{'\'', '('}, 0x02d8},   /* BREVE */
    {{'\'', '.'}, 0x02d9},   /* DOT ABOVE */
    {{'\'', '0'}, 0x02da},   /* RING ABOVE */
    {{'\'', ';'}, 0x02db},   /* OGONEK */
    {{'\'', '"'}, 0x02dd},   /* DOUBLE ACUTE ACCENT */
    {{'A', '%'}, 0x0386},   /* GREEK CAPITAL LETTER ALPHA WITH ACUTE */
    {{'E', '%'}, 0x0388},   /* GREEK CAPITAL LETTER EPSILON WITH ACUTE */
    {{'Y', '%'}, 0x0389},   /* GREEK CAPITAL LETTER ETA WITH ACUTE */
    {{'I', '%'}, 0x038a},   /* GREEK CAPITAL LETTER IOTA WITH ACUTE */
    {{'O', '%'}, 0x038c},   /* GREEK CAPITAL LETTER OMICRON WITH ACUTE */
    {{'U', '%'}, 0x038e},   /* GREEK CAPITAL LETTER UPSILON WITH ACUTE */
    {{'W', '%'}, 0x038f},   /* GREEK CAPITAL LETTER OMEGA WITH ACUTE */
    {{'i', '3'}, 0x0390},   /* GREEK SMALL LETTER IOTA WITH ACUTE AND DIAERESIS */
    {{'A', '*'}, 0x0391},   /* GREEK CAPITAL LETTER ALPHA */
    {{'B', '*'}, 0x0392},   /* GREEK CAPITAL LETTER BETA */
    {{'G', '*'}, 0x0393},   /* GREEK CAPITAL LETTER GAMMA */
    {{'D', '*'}, 0x0394},   /* GREEK CAPITAL LETTER DELTA */
    {{'E', '*'}, 0x0395},   /* GREEK CAPITAL LETTER EPSILON */
    {{'Z', '*'}, 0x0396},   /* GREEK CAPITAL LETTER ZETA */
    {{'Y', '*'}, 0x0397},   /* GREEK CAPITAL LETTER ETA */
    {{'H', '*'}, 0x0398},   /* GREEK CAPITAL LETTER THETA */
    {{'I', '*'}, 0x0399},   /* GREEK CAPITAL LETTER IOTA */
    {{'K', '*'}, 0x039a},   /* GREEK CAPITAL LETTER KAPPA */
    {{'L', '*'}, 0x039b},   /* GREEK CAPITAL LETTER LAMDA */
    {{'M', '*'}, 0x039c},   /* GREEK CAPITAL LETTER MU */
    {{'N', '*'}, 0x039d},   /* GREEK CAPITAL LETTER NU */
    {{'C', '*'}, 0x039e},   /* GREEK CAPITAL LETTER XI */
    {{'O', '*'}, 0x039f},   /* GREEK CAPITAL LETTER OMICRON */
    {{'P', '*'}, 0x03a0},   /* GREEK CAPITAL LETTER PI */
    {{'R', '*'}, 0x03a1},   /* GREEK CAPITAL LETTER RHO */
    {{'S', '*'}, 0x03a3},   /* GREEK CAPITAL LETTER SIGMA */
    {{'T', '*'}, 0x03a4},   /* GREEK CAPITAL LETTER TAU */
    {{'U', '*'}, 0x03a5},   /* GREEK CAPITAL LETTER UPSILON */
    {{'F', '*'}, 0x03a6},   /* GREEK CAPITAL LETTER PHI */
    {{'X', '*'}, 0x03a7},   /* GREEK CAPITAL LETTER CHI */
    {{'Q', '*'}, 0x03a8},   /* GREEK CAPITAL LETTER PSI */
    {{'W', '*'}, 0x03a9},   /* GREEK CAPITAL LETTER OMEGA */
    {{'J', '*'}, 0x03aa},   /* GREEK CAPITAL LETTER IOTA WITH DIAERESIS */
    {{'V', '*'}, 0x03ab},   /* GREEK CAPITAL LETTER UPSILON WITH DIAERESIS */
    {{'a', '%'}, 0x03ac},   /* GREEK SMALL LETTER ALPHA WITH ACUTE */
    {{'e', '%'}, 0x03ad},   /* GREEK SMALL LETTER EPSILON WITH ACUTE */
    {{'y', '%'}, 0x03ae},   /* GREEK SMALL LETTER ETA WITH ACUTE */
    {{'i', '%'}, 0x03af},   /* GREEK SMALL LETTER IOTA WITH ACUTE */
    {{'u', '3'}, 0x03b0},   /* GREEK SMALL LETTER UPSILON WITH ACUTE AND DIAERESIS */
    {{'a', '*'}, 0x03b1},   /* GREEK SMALL LETTER ALPHA */
    {{'b', '*'}, 0x03b2},   /* GREEK SMALL LETTER BETA */
    {{'g', '*'}, 0x03b3},   /* GREEK SMALL LETTER GAMMA */
    {{'d', '*'}, 0x03b4},   /* GREEK SMALL LETTER DELTA */
    {{'e', '*'}, 0x03b5},   /* GREEK SMALL LETTER EPSILON */
    {{'z', '*'}, 0x03b6},   /* GREEK SMALL LETTER ZETA */
    {{'y', '*'}, 0x03b7},   /* GREEK SMALL LETTER ETA */
    {{'h', '*'}, 0x03b8},   /* GREEK SMALL LETTER THETA */
    {{'i', '*'}, 0x03b9},   /* GREEK SMALL LETTER IOTA */
    {{'k', '*'}, 0x03ba},   /* GREEK SMALL LETTER KAPPA */
    {{'l', '*'}, 0x03bb},   /* GREEK SMALL LETTER LAMDA */
    {{'m', '*'}, 0x03bc},   /* GREEK SMALL LETTER MU */
    {{'n', '*'}, 0x03bd},   /* GREEK SMALL LETTER NU */
    {{'c', '*'}, 0x03be},   /* GREEK SMALL LETTER XI */
    {{'o', '*'}, 0x03bf},   /* GREEK SMALL LETTER OMICRON */
    {{'p', '*'}, 0x03c0},   /* GREEK SMALL LETTER PI */
    {{'r', '*'}, 0x03c1},   /* GREEK SMALL LETTER RHO */
    {{'*', 's'}, 0x03c2},   /* GREEK SMALL LETTER FINAL SIGMA */
    {{'s', '*'}, 0x03c3},   /* GREEK SMALL LETTER SIGMA */
    {{'t', '*'}, 0x03c4},   /* GREEK SMALL LETTER TAU */
    {{'u', '*'}, 0x03c5},   /* GREEK SMALL LETTER UPSILON */
    {{'f', '*'}, 0x03c6},   /* GREEK SMALL LETTER PHI */
    {{'x', '*'}, 0x03c7},   /* GREEK SMALL LETTER CHI */
    {{'q', '*'}, 0x03c8},   /* GREEK SMALL LETTER PSI */
    {{'w', '*'}, 0x03c9},   /* GREEK SMALL LETTER OMEGA */
    {{'j', '*'}, 0x03ca},   /* GREEK SMALL LETTER IOTA WITH DIAERESIS */
    {{'v', '*'}, 0x03cb},   /* GREEK SMALL LETTER UPSILON WITH DIAERESIS */
    {{'o', '%'}, 0x03cc},   /* GREEK SMALL LETTER OMICRON WITH ACUTE */
    {{'u', '%'}, 0x03cd},   /* GREEK SMALL LETTER UPSILON WITH ACUTE */
    {{'w', '%'}, 0x03ce},   /* GREEK SMALL LETTER OMEGA WITH ACUTE */
    {{'\'', 'G'}, 0x03d8},   /* GREEK NUMERAL SIGN */
    {{',', 'G'}, 0x03d9},   /* GREEK LOWER NUMERAL SIGN */
    {{'T', '3'}, 0x03da},   /* GREEK CAPITAL LETTER STIGMA */
    {{'t', '3'}, 0x03db},   /* GREEK SMALL LETTER STIGMA */
    {{'M', '3'}, 0x03dc},   /* GREEK CAPITAL LETTER DIGAMMA */
    {{'m', '3'}, 0x03dd},   /* GREEK SMALL LETTER DIGAMMA */
    {{'K', '3'}, 0x03de},   /* GREEK CAPITAL LETTER KOPPA */
    {{'k', '3'}, 0x03df},   /* GREEK SMALL LETTER KOPPA */
    {{'P', '3'}, 0x03e0},   /* GREEK CAPITAL LETTER SAMPI */
    {{'p', '3'}, 0x03e1},   /* GREEK SMALL LETTER SAMPI */
    {{'\'', '%'}, 0x03f4},   /* ACUTE ACCENT AND DIAERESIS (Tonos and Dialytika) */
    {{'j', '3'}, 0x03f5},   /* GREEK IOTA BELOW */
    {{'I', 'O'}, 0x0401},   /* CYRILLIC CAPITAL LETTER IO */
    {{'D', '%'}, 0x0402},   /* CYRILLIC CAPITAL LETTER DJE (Serbocroatian) */
    {{'G', '%'}, 0x0403},   /* CYRILLIC CAPITAL LETTER GJE (Macedonian) */
    {{'I', 'E'}, 0x0404},   /* CYRILLIC CAPITAL LETTER UKRAINIAN IE */
    {{'D', 'S'}, 0x0405},   /* CYRILLIC CAPITAL LETTER DZE (Macedonian) */
    {{'I', 'I'}, 0x0406},   /* CYRILLIC CAPITAL LETTER BYELORUSSIAN-UKRAINIAN I */
    {{'Y', 'I'}, 0x0407},   /* CYRILLIC CAPITAL LETTER YI (Ukrainian) */
    {{'J', '%'}, 0x0408},   /* CYRILLIC CAPITAL LETTER JE */
    {{'L', 'J'}, 0x0409},   /* CYRILLIC CAPITAL LETTER LJE */
    {{'N', 'J'}, 0x040a},   /* CYRILLIC CAPITAL LETTER NJE */
    {{'T', 's'}, 0x040b},   /* CYRILLIC CAPITAL LETTER TSHE (Serbocroatian) */
    {{'K', 'J'}, 0x040c},   /* CYRILLIC CAPITAL LETTER KJE (Macedonian) */
    {{'V', '%'}, 0x040e},   /* CYRILLIC CAPITAL LETTER SHORT U (Byelorussian) */
    {{'D', 'Z'}, 0x040f},   /* CYRILLIC CAPITAL LETTER DZHE */
    {{'A', '='}, 0x0410},   /* CYRILLIC CAPITAL LETTER A */
    {{'B', '='}, 0x0411},   /* CYRILLIC CAPITAL LETTER BE */
    {{'V', '='}, 0x0412},   /* CYRILLIC CAPITAL LETTER VE */
    {{'G', '='}, 0x0413},   /* CYRILLIC CAPITAL LETTER GHE */
    {{'D', '='}, 0x0414},   /* CYRILLIC CAPITAL LETTER DE */
    {{'E', '='}, 0x0415},   /* CYRILLIC CAPITAL LETTER IE */
    {{'Z', '%'}, 0x0416},   /* CYRILLIC CAPITAL LETTER ZHE */
    {{'Z', '='}, 0x0417},   /* CYRILLIC CAPITAL LETTER ZE */
    {{'I', '='}, 0x0418},   /* CYRILLIC CAPITAL LETTER I */
    {{'J', '='}, 0x0419},   /* CYRILLIC CAPITAL LETTER SHORT I */
    {{'K', '='}, 0x041a},   /* CYRILLIC CAPITAL LETTER KA */
    {{'L', '='}, 0x041b},   /* CYRILLIC CAPITAL LETTER EL */
    {{'M', '='}, 0x041c},   /* CYRILLIC CAPITAL LETTER EM */
    {{'N', '='}, 0x041d},   /* CYRILLIC CAPITAL LETTER EN */
    {{'O', '='}, 0x041e},   /* CYRILLIC CAPITAL LETTER O */
    {{'P', '='}, 0x041f},   /* CYRILLIC CAPITAL LETTER PE */
    {{'R', '='}, 0x0420},   /* CYRILLIC CAPITAL LETTER ER */
    {{'S', '='}, 0x0421},   /* CYRILLIC CAPITAL LETTER ES */
    {{'T', '='}, 0x0422},   /* CYRILLIC CAPITAL LETTER TE */
    {{'U', '='}, 0x0423},   /* CYRILLIC CAPITAL LETTER U */
    {{'F', '='}, 0x0424},   /* CYRILLIC CAPITAL LETTER EF */
    {{'H', '='}, 0x0425},   /* CYRILLIC CAPITAL LETTER HA */
    {{'C', '='}, 0x0426},   /* CYRILLIC CAPITAL LETTER TSE */
    {{'C', '%'}, 0x0427},   /* CYRILLIC CAPITAL LETTER CHE */
    {{'S', '%'}, 0x0428},   /* CYRILLIC CAPITAL LETTER SHA */
    {{'S', 'c'}, 0x0429},   /* CYRILLIC CAPITAL LETTER SHCHA */
    {{'=', '"'}, 0x042a},   /* CYRILLIC CAPITAL LETTER HARD SIGN */
    {{'Y', '='}, 0x042b},   /* CYRILLIC CAPITAL LETTER YERU */
    {{'%', '"'}, 0x042c},   /* CYRILLIC CAPITAL LETTER SOFT SIGN */
    {{'J', 'E'}, 0x042d},   /* CYRILLIC CAPITAL LETTER E */
    {{'J', 'U'}, 0x042e},   /* CYRILLIC CAPITAL LETTER YU */
    {{'J', 'A'}, 0x042f},   /* CYRILLIC CAPITAL LETTER YA */
    {{'a', '='}, 0x0430},   /* CYRILLIC SMALL LETTER A */
    {{'b', '='}, 0x0431},   /* CYRILLIC SMALL LETTER BE */
    {{'v', '='}, 0x0432},   /* CYRILLIC SMALL LETTER VE */
    {{'g', '='}, 0x0433},   /* CYRILLIC SMALL LETTER GHE */
    {{'d', '='}, 0x0434},   /* CYRILLIC SMALL LETTER DE */
    {{'e', '='}, 0x0435},   /* CYRILLIC SMALL LETTER IE */
    {{'z', '%'}, 0x0436},   /* CYRILLIC SMALL LETTER ZHE */
    {{'z', '='}, 0x0437},   /* CYRILLIC SMALL LETTER ZE */
    {{'i', '='}, 0x0438},   /* CYRILLIC SMALL LETTER I */
    {{'j', '='}, 0x0439},   /* CYRILLIC SMALL LETTER SHORT I */
    {{'k', '='}, 0x043a},   /* CYRILLIC SMALL LETTER KA */
    {{'l', '='}, 0x043b},   /* CYRILLIC SMALL LETTER EL */
    {{'m', '='}, 0x043c},   /* CYRILLIC SMALL LETTER EM */
    {{'n', '='}, 0x043d},   /* CYRILLIC SMALL LETTER EN */
    {{'o', '='}, 0x043e},   /* CYRILLIC SMALL LETTER O */
    {{'p', '='}, 0x043f},   /* CYRILLIC SMALL LETTER PE */
    {{'r', '='}, 0x0440},   /* CYRILLIC SMALL LETTER ER */
    {{'s', '='}, 0x0441},   /* CYRILLIC SMALL LETTER ES */
    {{'t', '='}, 0x0442},   /* CYRILLIC SMALL LETTER TE */
    {{'u', '='}, 0x0443},   /* CYRILLIC SMALL LETTER U */
    {{'f', '='}, 0x0444},   /* CYRILLIC SMALL LETTER EF */
    {{'h', '='}, 0x0445},   /* CYRILLIC SMALL LETTER HA */
    {{'c', '='}, 0x0446},   /* CYRILLIC SMALL LETTER TSE */
    {{'c', '%'}, 0x0447},   /* CYRILLIC SMALL LETTER CHE */
    {{'s', '%'}, 0x0448},   /* CYRILLIC SMALL LETTER SHA */
    {{'s', 'c'}, 0x0449},   /* CYRILLIC SMALL LETTER SHCHA */
    {{'=', '\''}, 0x044a},   /* CYRILLIC SMALL LETTER HARD SIGN */
    {{'y', '='}, 0x044b},   /* CYRILLIC SMALL LETTER YERU */
    {{'%', '\''}, 0x044c},   /* CYRILLIC SMALL LETTER SOFT SIGN */
    {{'j', 'e'}, 0x044d},   /* CYRILLIC SMALL LETTER E */
    {{'j', 'u'}, 0x044e},   /* CYRILLIC SMALL LETTER YU */
    {{'j', 'a'}, 0x044f},   /* CYRILLIC SMALL LETTER YA */
    {{'i', 'o'}, 0x0451},   /* CYRILLIC SMALL LETTER IO */
    {{'d', '%'}, 0x0452},   /* CYRILLIC SMALL LETTER DJE (Serbocroatian) */
    {{'g', '%'}, 0x0453},   /* CYRILLIC SMALL LETTER GJE (Macedonian) */
    {{'i', 'e'}, 0x0454},   /* CYRILLIC SMALL LETTER UKRAINIAN IE */
    {{'d', 's'}, 0x0455},   /* CYRILLIC SMALL LETTER DZE (Macedonian) */
    {{'i', 'i'}, 0x0456},   /* CYRILLIC SMALL LETTER BYELORUSSIAN-UKRAINIAN I */
    {{'y', 'i'}, 0x0457},   /* CYRILLIC SMALL LETTER YI (Ukrainian) */
    {{'j', '%'}, 0x0458},   /* CYRILLIC SMALL LETTER JE */
    {{'l', 'j'}, 0x0459},   /* CYRILLIC SMALL LETTER LJE */
    {{'n', 'j'}, 0x045a},   /* CYRILLIC SMALL LETTER NJE */
    {{'t', 's'}, 0x045b},   /* CYRILLIC SMALL LETTER TSHE (Serbocroatian) */
    {{'k', 'j'}, 0x045c},   /* CYRILLIC SMALL LETTER KJE (Macedonian) */
    {{'v', '%'}, 0x045e},   /* CYRILLIC SMALL LETTER SHORT U (Byelorussian) */
    {{'d', 'z'}, 0x045f},   /* CYRILLIC SMALL LETTER DZHE */
    {{'Y', '3'}, 0x0462},   /* CYRILLIC CAPITAL LETTER YAT */
    {{'y', '3'}, 0x0463},   /* CYRILLIC SMALL LETTER YAT */
    {{'O', '3'}, 0x046a},   /* CYRILLIC CAPITAL LETTER BIG YUS */
    {{'o', '3'}, 0x046b},   /* CYRILLIC SMALL LETTER BIG YUS */
    {{'F', '3'}, 0x0472},   /* CYRILLIC CAPITAL LETTER FITA */
    {{'f', '3'}, 0x0473},   /* CYRILLIC SMALL LETTER FITA */
    {{'V', '3'}, 0x0474},   /* CYRILLIC CAPITAL LETTER IZHITSA */
    {{'v', '3'}, 0x0475},   /* CYRILLIC SMALL LETTER IZHITSA */
    {{'C', '3'}, 0x0480},   /* CYRILLIC CAPITAL LETTER KOPPA */
    {{'c', '3'}, 0x0481},   /* CYRILLIC SMALL LETTER KOPPA */
    {{'G', '3'}, 0x0490},   /* CYRILLIC CAPITAL LETTER GHE WITH UPTURN */
    {{'g', '3'}, 0x0491},   /* CYRILLIC SMALL LETTER GHE WITH UPTURN */
    {{'A', '+'}, 0x05d0},   /* HEBREW LETTER ALEF */
    {{'B', '+'}, 0x05d1},   /* HEBREW LETTER BET */
    {{'G', '+'}, 0x05d2},   /* HEBREW LETTER GIMEL */
    {{'D', '+'}, 0x05d3},   /* HEBREW LETTER DALET */
    {{'H', '+'}, 0x05d4},   /* HEBREW LETTER HE */
    {{'W', '+'}, 0x05d5},   /* HEBREW LETTER VAV */
    {{'Z', '+'}, 0x05d6},   /* HEBREW LETTER ZAYIN */
    {{'X', '+'}, 0x05d7},   /* HEBREW LETTER HET */
    {{'T', 'j'}, 0x05d8},   /* HEBREW LETTER TET */
    {{'J', '+'}, 0x05d9},   /* HEBREW LETTER YOD */
    {{'K', '%'}, 0x05da},   /* HEBREW LETTER FINAL KAF */
    {{'K', '+'}, 0x05db},   /* HEBREW LETTER KAF */
    {{'L', '+'}, 0x05dc},   /* HEBREW LETTER LAMED */
    {{'M', '%'}, 0x05dd},   /* HEBREW LETTER FINAL MEM */
    {{'M', '+'}, 0x05de},   /* HEBREW LETTER MEM */
    {{'N', '%'}, 0x05df},   /* HEBREW LETTER FINAL NUN */
    {{'N', '+'}, 0x05e0},   /* HEBREW LETTER NUN */
    {{'S', '+'}, 0x05e1},   /* HEBREW LETTER SAMEKH */
    {{'E', '+'}, 0x05e2},   /* HEBREW LETTER AYIN */
    {{'P', '%'}, 0x05e3},   /* HEBREW LETTER FINAL PE */
    {{'P', '+'}, 0x05e4},   /* HEBREW LETTER PE */
    {{'Z', 'j'}, 0x05e5},   /* HEBREW LETTER FINAL TSADI */
    {{'Z', 'J'}, 0x05e6},   /* HEBREW LETTER TSADI */
    {{'Q', '+'}, 0x05e7},   /* HEBREW LETTER QOF */
    {{'R', '+'}, 0x05e8},   /* HEBREW LETTER RESH */
    {{'S', 'h'}, 0x05e9},   /* HEBREW LETTER SHIN */
    {{'T', '+'}, 0x05ea},   /* HEBREW LETTER TAV */
    {{',', '+'}, 0x060c},   /* ARABIC COMMA */
    {{';', '+'}, 0x061b},   /* ARABIC SEMICOLON */
    {{'?', '+'}, 0x061f},   /* ARABIC QUESTION MARK */
    {{'H', '\''}, 0x0621},   /* ARABIC LETTER HAMZA */
    {{'a', 'M'}, 0x0622},   /* ARABIC LETTER ALEF WITH MADDA ABOVE */
    {{'a', 'H'}, 0x0623},   /* ARABIC LETTER ALEF WITH HAMZA ABOVE */
    {{'w', 'H'}, 0x0624},   /* ARABIC LETTER WAW WITH HAMZA ABOVE */
    {{'a', 'h'}, 0x0625},   /* ARABIC LETTER ALEF WITH HAMZA BELOW */
    {{'y', 'H'}, 0x0626},   /* ARABIC LETTER YEH WITH HAMZA ABOVE */
    {{'a', '+'}, 0x0627},   /* ARABIC LETTER ALEF */
    {{'b', '+'}, 0x0628},   /* ARABIC LETTER BEH */
    {{'t', 'm'}, 0x0629},   /* ARABIC LETTER TEH MARBUTA */
    {{'t', '+'}, 0x062a},   /* ARABIC LETTER TEH */
    {{'t', 'k'}, 0x062b},   /* ARABIC LETTER THEH */
    {{'g', '+'}, 0x062c},   /* ARABIC LETTER JEEM */
    {{'h', 'k'}, 0x062d},   /* ARABIC LETTER HAH */
    {{'x', '+'}, 0x062e},   /* ARABIC LETTER KHAH */
    {{'d', '+'}, 0x062f},   /* ARABIC LETTER DAL */
    {{'d', 'k'}, 0x0630},   /* ARABIC LETTER THAL */
    {{'r', '+'}, 0x0631},   /* ARABIC LETTER REH */
    {{'z', '+'}, 0x0632},   /* ARABIC LETTER ZAIN */
    {{'s', '+'}, 0x0633},   /* ARABIC LETTER SEEN */
    {{'s', 'n'}, 0x0634},   /* ARABIC LETTER SHEEN */
    {{'c', '+'}, 0x0635},   /* ARABIC LETTER SAD */
    {{'d', 'd'}, 0x0636},   /* ARABIC LETTER DAD */
    {{'t', 'j'}, 0x0637},   /* ARABIC LETTER TAH */
    {{'z', 'H'}, 0x0638},   /* ARABIC LETTER ZAH */
    {{'e', '+'}, 0x0639},   /* ARABIC LETTER AIN */
    {{'i', '+'}, 0x063a},   /* ARABIC LETTER GHAIN */
    {{'+', '+'}, 0x0640},   /* ARABIC TATWEEL */
    {{'f', '+'}, 0x0641},   /* ARABIC LETTER FEH */
    {{'q', '+'}, 0x0642},   /* ARABIC LETTER QAF */
    {{'k', '+'}, 0x0643},   /* ARABIC LETTER KAF */
    {{'l', '+'}, 0x0644},   /* ARABIC LETTER LAM */
    {{'m', '+'}, 0x0645},   /* ARABIC LETTER MEEM */
    {{'n', '+'}, 0x0646},   /* ARABIC LETTER NOON */
    {{'h', '+'}, 0x0647},   /* ARABIC LETTER HEH */
    {{'w', '+'}, 0x0648},   /* ARABIC LETTER WAW */
    {{'j', '+'}, 0x0649},   /* ARABIC LETTER ALEF MAKSURA */
    {{'y', '+'}, 0x064a},   /* ARABIC LETTER YEH */
    {{':', '+'}, 0x064b},   /* ARABIC FATHATAN */
    {{'"', '+'}, 0x064c},   /* ARABIC DAMMATAN */
    {{'=', '+'}, 0x064d},   /* ARABIC KASRATAN */
    {{'/', '+'}, 0x064e},   /* ARABIC FATHA */
    {{'\'', '+'}, 0x064f},   /* ARABIC DAMMA */
    {{'1', '+'}, 0x0650},   /* ARABIC KASRA */
    {{'3', '+'}, 0x0651},   /* ARABIC SHADDA */
    {{'0', '+'}, 0x0652},   /* ARABIC SUKUN */
    {{'a', 'S'}, 0x0670},   /* SUPERSCRIPT ARABIC LETTER ALEF */
    {{'p', '+'}, 0x067e},   /* ARABIC LETTER PEH */
    {{'v', '+'}, 0x06a4},   /* ARABIC LETTER VEH */
    {{'g', 'f'}, 0x06af},   /* ARABIC LETTER GAF */
    {{'0', 'a'}, 0x06f0},   /* EASTERN ARABIC-INDIC DIGIT ZERO */
    {{'1', 'a'}, 0x06f1},   /* EASTERN ARABIC-INDIC DIGIT ONE */
    {{'2', 'a'}, 0x06f2},   /* EASTERN ARABIC-INDIC DIGIT TWO */
    {{'3', 'a'}, 0x06f3},   /* EASTERN ARABIC-INDIC DIGIT THREE */
    {{'4', 'a'}, 0x06f4},   /* EASTERN ARABIC-INDIC DIGIT FOUR */
    {{'5', 'a'}, 0x06f5},   /* EASTERN ARABIC-INDIC DIGIT FIVE */
    {{'6', 'a'}, 0x06f6},   /* EASTERN ARABIC-INDIC DIGIT SIX */
    {{'7', 'a'}, 0x06f7},   /* EASTERN ARABIC-INDIC DIGIT SEVEN */
    {{'8', 'a'}, 0x06f8},   /* EASTERN ARABIC-INDIC DIGIT EIGHT */
    {{'9', 'a'}, 0x06f9},   /* EASTERN ARABIC-INDIC DIGIT NINE */
    {{'B', '.'}, 0x1e02},   /* LATIN CAPITAL LETTER B WITH DOT ABOVE */
    {{'b', '.'}, 0x1e03},   /* LATIN SMALL LETTER B WITH DOT ABOVE */
    {{'B', '_'}, 0x1e06},   /* LATIN CAPITAL LETTER B WITH LINE BELOW */
    {{'b', '_'}, 0x1e07},   /* LATIN SMALL LETTER B WITH LINE BELOW */
    {{'D', '.'}, 0x1e0a},   /* LATIN CAPITAL LETTER D WITH DOT ABOVE */
    {{'d', '.'}, 0x1e0b},   /* LATIN SMALL LETTER D WITH DOT ABOVE */
    {{'D', '_'}, 0x1e0e},   /* LATIN CAPITAL LETTER D WITH LINE BELOW */
    {{'d', '_'}, 0x1e0f},   /* LATIN SMALL LETTER D WITH LINE BELOW */
    {{'D', ','}, 0x1e10},   /* LATIN CAPITAL LETTER D WITH CEDILLA */
    {{'d', ','}, 0x1e11},   /* LATIN SMALL LETTER D WITH CEDILLA */
    {{'F', '.'}, 0x1e1e},   /* LATIN CAPITAL LETTER F WITH DOT ABOVE */
    {{'f', '.'}, 0x1e1f},   /* LATIN SMALL LETTER F WITH DOT ABOVE */
    {{'G', '-'}, 0x1e20},   /* LATIN CAPITAL LETTER G WITH MACRON */
    {{'g', '-'}, 0x1e21},   /* LATIN SMALL LETTER G WITH MACRON */
    {{'H', '.'}, 0x1e22},   /* LATIN CAPITAL LETTER H WITH DOT ABOVE */
    {{'h', '.'}, 0x1e23},   /* LATIN SMALL LETTER H WITH DOT ABOVE */
    {{'H', ':'}, 0x1e26},   /* LATIN CAPITAL LETTER H WITH DIAERESIS */
    {{'h', ':'}, 0x1e27},   /* LATIN SMALL LETTER H WITH DIAERESIS */
    {{'H', ','}, 0x1e28},   /* LATIN CAPITAL LETTER H WITH CEDILLA */
    {{'h', ','}, 0x1e29},   /* LATIN SMALL LETTER H WITH CEDILLA */
    {{'K', '\''}, 0x1e30},   /* LATIN CAPITAL LETTER K WITH ACUTE */
    {{'k', '\''}, 0x1e31},   /* LATIN SMALL LETTER K WITH ACUTE */
    {{'K', '_'}, 0x1e34},   /* LATIN CAPITAL LETTER K WITH LINE BELOW */
    {{'k', '_'}, 0x1e35},   /* LATIN SMALL LETTER K WITH LINE BELOW */
    {{'L', '_'}, 0x1e3a},   /* LATIN CAPITAL LETTER L WITH LINE BELOW */
    {{'l', '_'}, 0x1e3b},   /* LATIN SMALL LETTER L WITH LINE BELOW */
    {{'M', '\''}, 0x1e3e},   /* LATIN CAPITAL LETTER M WITH ACUTE */
    {{'m', '\''}, 0x1e3f},   /* LATIN SMALL LETTER M WITH ACUTE */
    {{'M', '.'}, 0x1e40},   /* LATIN CAPITAL LETTER M WITH DOT ABOVE */
    {{'m', '.'}, 0x1e41},   /* LATIN SMALL LETTER M WITH DOT ABOVE */
    {{'N', '.'}, 0x1e44},   /* LATIN CAPITAL LETTER N WITH DOT ABOVE */
    {{'n', '.'}, 0x1e45},   /* LATIN SMALL LETTER N WITH DOT ABOVE */
    {{'N', '_'}, 0x1e48},   /* LATIN CAPITAL LETTER N WITH LINE BELOW */
    {{'n', '_'}, 0x1e49},   /* LATIN SMALL LETTER N WITH LINE BELOW */
    {{'P', '\''}, 0x1e54},   /* LATIN CAPITAL LETTER P WITH ACUTE */
    {{'p', '\''}, 0x1e55},   /* LATIN SMALL LETTER P WITH ACUTE */
    {{'P', '.'}, 0x1e56},   /* LATIN CAPITAL LETTER P WITH DOT ABOVE */
    {{'p', '.'}, 0x1e57},   /* LATIN SMALL LETTER P WITH DOT ABOVE */
    {{'R', '.'}, 0x1e58},   /* LATIN CAPITAL LETTER R WITH DOT ABOVE */
    {{'r', '.'}, 0x1e59},   /* LATIN SMALL LETTER R WITH DOT ABOVE */
    {{'R', '_'}, 0x1e5e},   /* LATIN CAPITAL LETTER R WITH LINE BELOW */
    {{'r', '_'}, 0x1e5f},   /* LATIN SMALL LETTER R WITH LINE BELOW */
    {{'S', '.'}, 0x1e60},   /* LATIN CAPITAL LETTER S WITH DOT ABOVE */
    {{'s', '.'}, 0x1e61},   /* LATIN SMALL LETTER S WITH DOT ABOVE */
    {{'T', '.'}, 0x1e6a},   /* LATIN CAPITAL LETTER T WITH DOT ABOVE */
    {{'t', '.'}, 0x1e6b},   /* LATIN SMALL LETTER T WITH DOT ABOVE */
    {{'T', '_'}, 0x1e6e},   /* LATIN CAPITAL LETTER T WITH LINE BELOW */
    {{'t', '_'}, 0x1e6f},   /* LATIN SMALL LETTER T WITH LINE BELOW */
    {{'V', '?'}, 0x1e7c},   /* LATIN CAPITAL LETTER V WITH TILDE */
    {{'v', '?'}, 0x1e7d},   /* LATIN SMALL LETTER V WITH TILDE */
    {{'W', '!'}, 0x1e80},   /* LATIN CAPITAL LETTER W WITH GRAVE */
    {{'w', '!'}, 0x1e81},   /* LATIN SMALL LETTER W WITH GRAVE */
    {{'W', '\''}, 0x1e82},   /* LATIN CAPITAL LETTER W WITH ACUTE */
    {{'w', '\''}, 0x1e83},   /* LATIN SMALL LETTER W WITH ACUTE */
    {{'W', ':'}, 0x1e84},   /* LATIN CAPITAL LETTER W WITH DIAERESIS */
    {{'w', ':'}, 0x1e85},   /* LATIN SMALL LETTER W WITH DIAERESIS */
    {{'W', '.'}, 0x1e86},   /* LATIN CAPITAL LETTER W WITH DOT ABOVE */
    {{'w', '.'}, 0x1e87},   /* LATIN SMALL LETTER W WITH DOT ABOVE */
    {{'X', '.'}, 0x1e8a},   /* LATIN CAPITAL LETTER X WITH DOT ABOVE */
    {{'x', '.'}, 0x1e8b},   /* LATIN SMALL LETTER X WITH DOT ABOVE */
    {{'X', ':'}, 0x1e8c},   /* LATIN CAPITAL LETTER X WITH DIAERESIS */
    {{'x', ':'}, 0x1e8d},   /* LATIN SMALL LETTER X WITH DIAERESIS */
    {{'Y', '.'}, 0x1e8e},   /* LATIN CAPITAL LETTER Y WITH DOT ABOVE */
    {{'y', '.'}, 0x1e8f},   /* LATIN SMALL LETTER Y WITH DOT ABOVE */
    {{'Z', '>'}, 0x1e90},   /* LATIN CAPITAL LETTER Z WITH CIRCUMFLEX */
    {{'z', '>'}, 0x1e91},   /* LATIN SMALL LETTER Z WITH CIRCUMFLEX */
    {{'Z', '_'}, 0x1e94},   /* LATIN CAPITAL LETTER Z WITH LINE BELOW */
    {{'z', '_'}, 0x1e95},   /* LATIN SMALL LETTER Z WITH LINE BELOW */
    {{'h', '_'}, 0x1e96},   /* LATIN SMALL LETTER H WITH LINE BELOW */
    {{'t', ':'}, 0x1e97},   /* LATIN SMALL LETTER T WITH DIAERESIS */
    {{'w', '0'}, 0x1e98},   /* LATIN SMALL LETTER W WITH RING ABOVE */
    {{'y', '0'}, 0x1e99},   /* LATIN SMALL LETTER Y WITH RING ABOVE */
    {{'A', '2'}, 0x1ea2},   /* LATIN CAPITAL LETTER A WITH HOOK ABOVE */
    {{'a', '2'}, 0x1ea3},   /* LATIN SMALL LETTER A WITH HOOK ABOVE */
    {{'E', '2'}, 0x1eba},   /* LATIN CAPITAL LETTER E WITH HOOK ABOVE */
    {{'e', '2'}, 0x1ebb},   /* LATIN SMALL LETTER E WITH HOOK ABOVE */
    {{'E', '?'}, 0x1ebc},   /* LATIN CAPITAL LETTER E WITH TILDE */
    {{'e', '?'}, 0x1ebd},   /* LATIN SMALL LETTER E WITH TILDE */
    {{'I', '2'}, 0x1ec8},   /* LATIN CAPITAL LETTER I WITH HOOK ABOVE */
    {{'i', '2'}, 0x1ec9},   /* LATIN SMALL LETTER I WITH HOOK ABOVE */
    {{'O', '2'}, 0x1ece},   /* LATIN CAPITAL LETTER O WITH HOOK ABOVE */
    {{'o', '2'}, 0x1ecf},   /* LATIN SMALL LETTER O WITH HOOK ABOVE */
    {{'U', '2'}, 0x1ee6},   /* LATIN CAPITAL LETTER U WITH HOOK ABOVE */
    {{'u', '2'}, 0x1ee7},   /* LATIN SMALL LETTER U WITH HOOK ABOVE */
    {{'Y', '!'}, 0x1ef2},   /* LATIN CAPITAL LETTER Y WITH GRAVE */
    {{'y', '!'}, 0x1ef3},   /* LATIN SMALL LETTER Y WITH GRAVE */
    {{'Y', '2'}, 0x1ef6},   /* LATIN CAPITAL LETTER Y WITH HOOK ABOVE */
    {{'y', '2'}, 0x1ef7},   /* LATIN SMALL LETTER Y WITH HOOK ABOVE */
    {{'Y', '?'}, 0x1ef8},   /* LATIN CAPITAL LETTER Y WITH TILDE */
    {{'y', '?'}, 0x1ef9},   /* LATIN SMALL LETTER Y WITH TILDE */
    {{';', '\''}, 0x1f00},   /* GREEK DASIA AND ACUTE ACCENT */
    {{',', '\''}, 0x1f01},   /* GREEK PSILI AND ACUTE ACCENT */
    {{';', '!'}, 0x1f02},   /* GREEK DASIA AND VARIA */
    {{',', '!'}, 0x1f03},   /* GREEK PSILI AND VARIA */
    {{'?', ';'}, 0x1f04},   /* GREEK DASIA AND PERISPOMENI */
    {{'?', ','}, 0x1f05},   /* GREEK PSILI AND PERISPOMENI */
    {{'!', ':'}, 0x1f06},   /* GREEK DIAERESIS AND VARIA */
    {{'?', ':'}, 0x1f07},   /* GREEK DIAERESIS AND PERISPOMENI */
    {{'1', 'N'}, 0x2002},   /* EN SPACE */
    {{'1', 'M'}, 0x2003},   /* EM SPACE */
    {{'3', 'M'}, 0x2004},   /* THREE-PER-EM SPACE */
    {{'4', 'M'}, 0x2005},   /* FOUR-PER-EM SPACE */
    {{'6', 'M'}, 0x2006},   /* SIX-PER-EM SPACE */
    {{'1', 'T'}, 0x2009},   /* THIN SPACE */
    {{'1', 'H'}, 0x200a},   /* HAIR SPACE */
    {{'-', '1'}, 0x2010},   /* HYPHEN */
    {{'-', 'N'}, 0x2013},   /* EN DASH */
    {{'-', 'M'}, 0x2014},   /* EM DASH */
    {{'-', '3'}, 0x2015},   /* HORIZONTAL BAR */
    {{'!', '2'}, 0x2016},   /* DOUBLE VERTICAL LINE */
    {{'=', '2'}, 0x2017},   /* DOUBLE LOW LINE */
    {{'\'', '6'}, 0x2018},   /* LEFT SINGLE QUOTATION MARK */
    {{'\'', '9'}, 0x2019},   /* RIGHT SINGLE QUOTATION MARK */
    {{'.', '9'}, 0x201a},   /* SINGLE LOW-9 QUOTATION MARK */
    {{'9', '\''}, 0x201b},   /* SINGLE HIGH-REVERSED-9 QUOTATION MARK */
    {{'"', '6'}, 0x201c},   /* LEFT DOUBLE QUOTATION MARK */
    {{'"', '9'}, 0x201d},   /* RIGHT DOUBLE QUOTATION MARK */
    {{':', '9'}, 0x201e},   /* DOUBLE LOW-9 QUOTATION MARK */
    {{'9', '"'}, 0x201f},   /* DOUBLE HIGH-REVERSED-9 QUOTATION MARK */
    {{'/', '-'}, 0x2020},   /* DAGGER */
    {{'/', '='}, 0x2021},   /* DOUBLE DAGGER */
    {{'.', '.'}, 0x2025},   /* TWO DOT LEADER */
    {{'%', '0'}, 0x2030},   /* PER MILLE SIGN */
    {{'1', '\''}, 0x2032},   /* PRIME */
    {{'2', '\''}, 0x2033},   /* DOUBLE PRIME */
    {{'3', '\''}, 0x2034},   /* TRIPLE PRIME */
    {{'1', '"'}, 0x2035},   /* REVERSED PRIME */
    {{'2', '"'}, 0x2036},   /* REVERSED DOUBLE PRIME */
    {{'3', '"'}, 0x2037},   /* REVERSED TRIPLE PRIME */
    {{'C', 'a'}, 0x2038},   /* CARET */
    {{'<', '1'}, 0x2039},   /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
    {{'>', '1'}, 0x203a},   /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
    {{':', 'X'}, 0x203b},   /* REFERENCE MARK */
    {{'\'', '-'}, 0x203e},   /* OVERLINE */
    {{'/', 'f'}, 0x2044},   /* FRACTION SLASH */
    {{'0', 'S'}, 0x2070},   /* SUPERSCRIPT DIGIT ZERO */
    {{'4', 'S'}, 0x2074},   /* SUPERSCRIPT DIGIT FOUR */
    {{'5', 'S'}, 0x2075},   /* SUPERSCRIPT DIGIT FIVE */
    {{'6', 'S'}, 0x2076},   /* SUPERSCRIPT DIGIT SIX */
    {{'7', 'S'}, 0x2077},   /* SUPERSCRIPT DIGIT SEVEN */
    {{'8', 'S'}, 0x2078},   /* SUPERSCRIPT DIGIT EIGHT */
    {{'9', 'S'}, 0x2079},   /* SUPERSCRIPT DIGIT NINE */
    {{'+', 'S'}, 0x207a},   /* SUPERSCRIPT PLUS SIGN */
    {{'-', 'S'}, 0x207b},   /* SUPERSCRIPT MINUS */
    {{'=', 'S'}, 0x207c},   /* SUPERSCRIPT EQUALS SIGN */
    {{'(', 'S'}, 0x207d},   /* SUPERSCRIPT LEFT PARENTHESIS */
    {{')', 'S'}, 0x207e},   /* SUPERSCRIPT RIGHT PARENTHESIS */
    {{'n', 'S'}, 0x207f},   /* SUPERSCRIPT LATIN SMALL LETTER N */
    {{'0', 's'}, 0x2080},   /* SUBSCRIPT DIGIT ZERO */
    {{'1', 's'}, 0x2081},   /* SUBSCRIPT DIGIT ONE */
    {{'2', 's'}, 0x2082},   /* SUBSCRIPT DIGIT TWO */
    {{'3', 's'}, 0x2083},   /* SUBSCRIPT DIGIT THREE */
    {{'4', 's'}, 0x2084},   /* SUBSCRIPT DIGIT FOUR */
    {{'5', 's'}, 0x2085},   /* SUBSCRIPT DIGIT FIVE */
    {{'6', 's'}, 0x2086},   /* SUBSCRIPT DIGIT SIX */
    {{'7', 's'}, 0x2087},   /* SUBSCRIPT DIGIT SEVEN */
    {{'8', 's'}, 0x2088},   /* SUBSCRIPT DIGIT EIGHT */
    {{'9', 's'}, 0x2089},   /* SUBSCRIPT DIGIT NINE */
    {{'+', 's'}, 0x208a},   /* SUBSCRIPT PLUS SIGN */
    {{'-', 's'}, 0x208b},   /* SUBSCRIPT MINUS */
    {{'=', 's'}, 0x208c},   /* SUBSCRIPT EQUALS SIGN */
    {{'(', 's'}, 0x208d},   /* SUBSCRIPT LEFT PARENTHESIS */
    {{')', 's'}, 0x208e},   /* SUBSCRIPT RIGHT PARENTHESIS */
    {{'L', 'i'}, 0x20a4},   /* LIRA SIGN */
    {{'P', 't'}, 0x20a7},   /* PESETA SIGN */
    {{'W', '='}, 0x20a9},   /* WON SIGN */
    {{'o', 'C'}, 0x2103},   /* DEGREE CENTIGRADE */
    {{'c', 'o'}, 0x2105},   /* CARE OF */
    {{'o', 'F'}, 0x2109},   /* DEGREE FAHRENHEIT */
    {{'N', '0'}, 0x2116},   /* NUMERO SIGN */
    {{'P', 'O'}, 0x2117},   /* SOUND RECORDING COPYRIGHT */
    {{'R', 'x'}, 0x211e},   /* PRESCRIPTION TAKE */
    {{'S', 'M'}, 0x2120},   /* SERVICE MARK */
    {{'T', 'M'}, 0x2122},   /* TRADE MARK SIGN */
    {{'O', 'm'}, 0x2126},   /* OHM SIGN */
    {{'A', 'O'}, 0x212b},   /* ANGSTROEM SIGN */
    {{'1', '3'}, 0x2153},   /* VULGAR FRACTION ONE THIRD */
    {{'2', '3'}, 0x2154},   /* VULGAR FRACTION TWO THIRDS */
    {{'1', '5'}, 0x2155},   /* VULGAR FRACTION ONE FIFTH */
    {{'2', '5'}, 0x2156},   /* VULGAR FRACTION TWO FIFTHS */
    {{'3', '5'}, 0x2157},   /* VULGAR FRACTION THREE FIFTHS */
    {{'4', '5'}, 0x2158},   /* VULGAR FRACTION FOUR FIFTHS */
    {{'1', '6'}, 0x2159},   /* VULGAR FRACTION ONE SIXTH */
    {{'5', '6'}, 0x215a},   /* VULGAR FRACTION FIVE SIXTHS */
    {{'1', '8'}, 0x215b},   /* VULGAR FRACTION ONE EIGHTH */
    {{'3', '8'}, 0x215c},   /* VULGAR FRACTION THREE EIGHTHS */
    {{'5', '8'}, 0x215d},   /* VULGAR FRACTION FIVE EIGHTHS */
    {{'7', '8'}, 0x215e},   /* VULGAR FRACTION SEVEN EIGHTHS */
    {{'1', 'R'}, 0x2160},   /* ROMAN NUMERAL ONE */
    {{'2', 'R'}, 0x2161},   /* ROMAN NUMERAL TWO */
    {{'3', 'R'}, 0x2162},   /* ROMAN NUMERAL THREE */
    {{'4', 'R'}, 0x2163},   /* ROMAN NUMERAL FOUR */
    {{'5', 'R'}, 0x2164},   /* ROMAN NUMERAL FIVE */
    {{'6', 'R'}, 0x2165},   /* ROMAN NUMERAL SIX */
    {{'7', 'R'}, 0x2166},   /* ROMAN NUMERAL SEVEN */
    {{'8', 'R'}, 0x2167},   /* ROMAN NUMERAL EIGHT */
    {{'9', 'R'}, 0x2168},   /* ROMAN NUMERAL NINE */
    {{'a', 'R'}, 0x2169},   /* ROMAN NUMERAL TEN */
    {{'b', 'R'}, 0x216a},   /* ROMAN NUMERAL ELEVEN */
    {{'c', 'R'}, 0x216b},   /* ROMAN NUMERAL TWELVE */
    {{'1', 'r'}, 0x2170},   /* SMALL ROMAN NUMERAL ONE */
    {{'2', 'r'}, 0x2171},   /* SMALL ROMAN NUMERAL TWO */
    {{'3', 'r'}, 0x2172},   /* SMALL ROMAN NUMERAL THREE */
    {{'4', 'r'}, 0x2173},   /* SMALL ROMAN NUMERAL FOUR */
    {{'5', 'r'}, 0x2174},   /* SMALL ROMAN NUMERAL FIVE */
    {{'6', 'r'}, 0x2175},   /* SMALL ROMAN NUMERAL SIX */
    {{'7', 'r'}, 0x2176},   /* SMALL ROMAN NUMERAL SEVEN */
    {{'8', 'r'}, 0x2177},   /* SMALL ROMAN NUMERAL EIGHT */
    {{'9', 'r'}, 0x2178},   /* SMALL ROMAN NUMERAL NINE */
    {{'a', 'r'}, 0x2179},   /* SMALL ROMAN NUMERAL TEN */
    {{'b', 'r'}, 0x217a},   /* SMALL ROMAN NUMERAL ELEVEN */
    {{'c', 'r'}, 0x217b},   /* SMALL ROMAN NUMERAL TWELVE */
    {{'<', '-'}, 0x2190},   /* LEFTWARDS ARROW */
    {{'-', '!'}, 0x2191},   /* UPWARDS ARROW */
    {{'-', '>'}, 0x2192},   /* RIGHTWARDS ARROW */
    {{'-', 'v'}, 0x2193},   /* DOWNWARDS ARROW */
    {{'<', '>'}, 0x2194},   /* LEFT RIGHT ARROW */
    {{'U', 'D'}, 0x2195},   /* UP DOWN ARROW */
    {{'<', '='}, 0x21d0},   /* LEFTWARDS DOUBLE ARROW */
    {{'=', '>'}, 0x21d2},   /* RIGHTWARDS DOUBLE ARROW */
    {{'=', '='}, 0x21d4},   /* LEFT RIGHT DOUBLE ARROW */
    {{'F', 'A'}, 0x2200},   /* FOR ALL */
    {{'d', 'P'}, 0x2202},   /* PARTIAL DIFFERENTIAL */
    {{'T', 'E'}, 0x2203},   /* THERE EXISTS */
    {{'/', '0'}, 0x2205},   /* EMPTY SET */
    {{'D', 'E'}, 0x2206},   /* INCREMENT */
    {{'N', 'B'}, 0x2207},   /* NABLA */
    {{'(', '-'}, 0x2208},   /* ELEMENT OF */
    {{'-', ')'}, 0x220b},   /* CONTAINS AS MEMBER */
    {{'*', 'P'}, 0x220f},   /* N-ARY PRODUCT */
    {{'+', 'Z'}, 0x2211},   /* N-ARY SUMMATION */
    {{'-', '2'}, 0x2212},   /* MINUS SIGN */
    {{'-', '+'}, 0x2213},   /* MINUS-OR-PLUS SIGN */
    {{'*', '-'}, 0x2217},   /* ASTERISK OPERATOR */
    {{'O', 'b'}, 0x2218},   /* RING OPERATOR */
    {{'S', 'b'}, 0x2219},   /* BULLET OPERATOR */
    {{'R', 'T'}, 0x221a},   /* SQUARE ROOT */
    {{'0', '('}, 0x221d},   /* PROPORTIONAL TO */
    {{'0', '0'}, 0x221e},   /* INFINITY */
    {{'-', 'L'}, 0x221f},   /* RIGHT ANGLE */
    {{'-', 'V'}, 0x2220},   /* ANGLE */
    {{'P', 'P'}, 0x2225},   /* PARALLEL TO */
    {{'A', 'N'}, 0x2227},   /* LOGICAL AND */
    {{'O', 'R'}, 0x2228},   /* LOGICAL OR */
    {{'(', 'U'}, 0x2229},   /* INTERSECTION */
    {{')', 'U'}, 0x222a},   /* UNION */
    {{'I', 'n'}, 0x222b},   /* INTEGRAL */
    {{'D', 'I'}, 0x222c},   /* DOUBLE INTEGRAL */
    {{'I', 'o'}, 0x222e},   /* CONTOUR INTEGRAL */
    {{'.', ':'}, 0x2234},   /* THEREFORE */
    {{':', '.'}, 0x2235},   /* BECAUSE */
    {{':', 'R'}, 0x2236},   /* RATIO */
    {{':', ':'}, 0x2237},   /* PROPORTION */
    {{'?', '1'}, 0x223c},   /* TILDE OPERATOR */
    {{'C', 'G'}, 0x223e},   /* INVERTED LAZY S */
    {{'?', '-'}, 0x2243},   /* ASYMPTOTICALLY EQUAL TO */
    {{'?', '='}, 0x2245},   /* APPROXIMATELY EQUAL TO */
    {{'?', '2'}, 0x2248},   /* ALMOST EQUAL TO */
    {{'=', '?'}, 0x224c},   /* ALL EQUAL TO */
    {{'H', 'I'}, 0x2253},   /* IMAGE OF OR APPROXIMATELY EQUAL TO */
    {{'!', '='}, 0x2260},   /* NOT EQUAL TO */
    {{'=', '3'}, 0x2261},   /* IDENTICAL TO */
    {{'=', '<'}, 0x2264},   /* LESS-THAN OR EQUAL TO */
    {{'>', '='}, 0x2265},   /* GREATER-THAN OR EQUAL TO */
    {{'<', '*'}, 0x226a},   /* MUCH LESS-THAN */
    {{'*', '>'}, 0x226b},   /* MUCH GREATER-THAN */
    {{'!', '<'}, 0x226e},   /* NOT LESS-THAN */
    {{'!', '>'}, 0x226f},   /* NOT GREATER-THAN */
    {{'(', 'C'}, 0x2282},   /* SUBSET OF */
    {{')', 'C'}, 0x2283},   /* SUPERSET OF */
    {{'(', '_'}, 0x2286},   /* SUBSET OF OR EQUAL TO */
    {{')', '_'}, 0x2287},   /* SUPERSET OF OR EQUAL TO */
    {{'0', '.'}, 0x2299},   /* CIRCLED DOT OPERATOR */
    {{'0', '2'}, 0x229a},   /* CIRCLED RING OPERATOR */
    {{'-', 'T'}, 0x22a5},   /* UP TACK */
    {{'.', 'P'}, 0x22c5},   /* DOT OPERATOR */
    {{':', '3'}, 0x22ee},   /* VERTICAL ELLIPSIS */
    {{'.', '3'}, 0x22ef},   /* MIDLINE HORIZONTAL ELLIPSIS */
    {{'E', 'h'}, 0x2302},   /* HOUSE */
    {{'<', '7'}, 0x2308},   /* LEFT CEILING */
    {{'>', '7'}, 0x2309},   /* RIGHT CEILING */
    {{'7', '<'}, 0x230a},   /* LEFT FLOOR */
    {{'7', '>'}, 0x230b},   /* RIGHT FLOOR */
    {{'N', 'I'}, 0x2310},   /* REVERSED NOT SIGN */
    {{'(', 'A'}, 0x2312},   /* ARC */
    {{'T', 'R'}, 0x2315},   /* TELEPHONE RECORDER */
    {{'I', 'u'}, 0x2320},   /* TOP HALF INTEGRAL */
    {{'I', 'l'}, 0x2321},   /* BOTTOM HALF INTEGRAL */
    {{'<', '/'}, 0x2329},   /* LEFT-POINTING ANGLE BRACKET */
    {{'/', '>'}, 0x232a},   /* RIGHT-POINTING ANGLE BRACKET */
    {{'V', 's'}, 0x2423},   /* OPEN BOX */
    {{'1', 'h'}, 0x2440},   /* OCR HOOK */
    {{'3', 'h'}, 0x2441},   /* OCR CHAIR */
    {{'2', 'h'}, 0x2442},   /* OCR FORK */
    {{'4', 'h'}, 0x2443},   /* OCR INVERTED FORK */
    {{'1', 'j'}, 0x2446},   /* OCR BRANCH BANK IDENTIFICATION */
    {{'2', 'j'}, 0x2447},   /* OCR AMOUNT OF CHECK */
    {{'3', 'j'}, 0x2448},   /* OCR DASH */
    {{'4', 'j'}, 0x2449},   /* OCR CUSTOMER ACCOUNT NUMBER */
    {{'1', '.'}, 0x2488},   /* DIGIT ONE FULL STOP */
    {{'2', '.'}, 0x2489},   /* DIGIT TWO FULL STOP */
    {{'3', '.'}, 0x248a},   /* DIGIT THREE FULL STOP */
    {{'4', '.'}, 0x248b},   /* DIGIT FOUR FULL STOP */
    {{'5', '.'}, 0x248c},   /* DIGIT FIVE FULL STOP */
    {{'6', '.'}, 0x248d},   /* DIGIT SIX FULL STOP */
    {{'7', '.'}, 0x248e},   /* DIGIT SEVEN FULL STOP */
    {{'8', '.'}, 0x248f},   /* DIGIT EIGHT FULL STOP */
    {{'9', '.'}, 0x2490},   /* DIGIT NINE FULL STOP */
    {{'h', 'h'}, 0x2500},   /* BOX DRAWINGS LIGHT HORIZONTAL */
    {{'H', 'H'}, 0x2501},   /* BOX DRAWINGS HEAVY HORIZONTAL */
    {{'v', 'v'}, 0x2502},   /* BOX DRAWINGS LIGHT VERTICAL */
    {{'V', 'V'}, 0x2503},   /* BOX DRAWINGS HEAVY VERTICAL */
    {{'3', '-'}, 0x2504},   /* BOX DRAWINGS LIGHT TRIPLE DASH HORIZONTAL */
    {{'3', '_'}, 0x2505},   /* BOX DRAWINGS HEAVY TRIPLE DASH HORIZONTAL */
    {{'3', '!'}, 0x2506},   /* BOX DRAWINGS LIGHT TRIPLE DASH VERTICAL */
    {{'3', '/'}, 0x2507},   /* BOX DRAWINGS HEAVY TRIPLE DASH VERTICAL */
    {{'4', '-'}, 0x2508},   /* BOX DRAWINGS LIGHT QUADRUPLE DASH HORIZONTAL */
    {{'4', '_'}, 0x2509},   /* BOX DRAWINGS HEAVY QUADRUPLE DASH HORIZONTAL */
    {{'4', '!'}, 0x250a},   /* BOX DRAWINGS LIGHT QUADRUPLE DASH VERTICAL */
    {{'4', '/'}, 0x250b},   /* BOX DRAWINGS HEAVY QUADRUPLE DASH VERTICAL */
    {{'d', 'r'}, 0x250c},   /* BOX DRAWINGS LIGHT DOWN AND RIGHT */
    {{'d', 'R'}, 0x250d},   /* BOX DRAWINGS DOWN LIGHT AND RIGHT HEAVY */
    {{'D', 'r'}, 0x250e},   /* BOX DRAWINGS DOWN HEAVY AND RIGHT LIGHT */
    {{'D', 'R'}, 0x250f},   /* BOX DRAWINGS HEAVY DOWN AND RIGHT */
    {{'d', 'l'}, 0x2510},   /* BOX DRAWINGS LIGHT DOWN AND LEFT */
    {{'d', 'L'}, 0x2511},   /* BOX DRAWINGS DOWN LIGHT AND LEFT HEAVY */
    {{'D', 'l'}, 0x2512},   /* BOX DRAWINGS DOWN HEAVY AND LEFT LIGHT */
    {{'L', 'D'}, 0x2513},   /* BOX DRAWINGS HEAVY DOWN AND LEFT */
    {{'u', 'r'}, 0x2514},   /* BOX DRAWINGS LIGHT UP AND RIGHT */
    {{'u', 'R'}, 0x2515},   /* BOX DRAWINGS UP LIGHT AND RIGHT HEAVY */
    {{'U', 'r'}, 0x2516},   /* BOX DRAWINGS UP HEAVY AND RIGHT LIGHT */
    {{'U', 'R'}, 0x2517},   /* BOX DRAWINGS HEAVY UP AND RIGHT */
    {{'u', 'l'}, 0x2518},   /* BOX DRAWINGS LIGHT UP AND LEFT */
    {{'u', 'L'}, 0x2519},   /* BOX DRAWINGS UP LIGHT AND LEFT HEAVY */
    {{'U', 'l'}, 0x251a},   /* BOX DRAWINGS UP HEAVY AND LEFT LIGHT */
    {{'U', 'L'}, 0x251b},   /* BOX DRAWINGS HEAVY UP AND LEFT */
    {{'v', 'r'}, 0x251c},   /* BOX DRAWINGS LIGHT VERTICAL AND RIGHT */
    {{'v', 'R'}, 0x251d},   /* BOX DRAWINGS VERTICAL LIGHT AND RIGHT HEAVY */
    {{'V', 'r'}, 0x2520},   /* BOX DRAWINGS VERTICAL HEAVY AND RIGHT LIGHT */
    {{'V', 'R'}, 0x2523},   /* BOX DRAWINGS HEAVY VERTICAL AND RIGHT */
    {{'v', 'l'}, 0x2524},   /* BOX DRAWINGS LIGHT VERTICAL AND LEFT */
    {{'v', 'L'}, 0x2525},   /* BOX DRAWINGS VERTICAL LIGHT AND LEFT HEAVY */
    {{'V', 'l'}, 0x2528},   /* BOX DRAWINGS VERTICAL HEAVY AND LEFT LIGHT */
    {{'V', 'L'}, 0x252b},   /* BOX DRAWINGS HEAVY VERTICAL AND LEFT */
    {{'d', 'h'}, 0x252c},   /* BOX DRAWINGS LIGHT DOWN AND HORIZONTAL */
    {{'d', 'H'}, 0x252f},   /* BOX DRAWINGS DOWN LIGHT AND HORIZONTAL HEAVY */
    {{'D', 'h'}, 0x2530},   /* BOX DRAWINGS DOWN HEAVY AND HORIZONTAL LIGHT */
    {{'D', 'H'}, 0x2533},   /* BOX DRAWINGS HEAVY DOWN AND HORIZONTAL */
    {{'u', 'h'}, 0x2534},   /* BOX DRAWINGS LIGHT UP AND HORIZONTAL */
    {{'u', 'H'}, 0x2537},   /* BOX DRAWINGS UP LIGHT AND HORIZONTAL HEAVY */
    {{'U', 'h'}, 0x2538},   /* BOX DRAWINGS UP HEAVY AND HORIZONTAL LIGHT */
    {{'U', 'H'}, 0x253b},   /* BOX DRAWINGS HEAVY UP AND HORIZONTAL */
    {{'v', 'h'}, 0x253c},   /* BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL */
    {{'v', 'H'}, 0x253f},   /* BOX DRAWINGS VERTICAL LIGHT AND HORIZONTAL HEAVY */
    {{'V', 'h'}, 0x2542},   /* BOX DRAWINGS VERTICAL HEAVY AND HORIZONTAL LIGHT */
    {{'V', 'H'}, 0x254b},   /* BOX DRAWINGS HEAVY VERTICAL AND HORIZONTAL */
    {{'F', 'D'}, 0x2571},   /* BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO LOWER LEFT */
    {{'B', 'D'}, 0x2572},   /* BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO LOWER RIGHT */
    {{'T', 'B'}, 0x2580},   /* UPPER HALF BLOCK */
    {{'L', 'B'}, 0x2584},   /* LOWER HALF BLOCK */
    {{'F', 'B'}, 0x2588},   /* FULL BLOCK */
    {{'l', 'B'}, 0x258c},   /* LEFT HALF BLOCK */
    {{'R', 'B'}, 0x2590},   /* RIGHT HALF BLOCK */
    {{'.', 'S'}, 0x2591},   /* LIGHT SHADE */
    {{':', 'S'}, 0x2592},   /* MEDIUM SHADE */
    {{'?', 'S'}, 0x2593},   /* DARK SHADE */
    {{'f', 'S'}, 0x25a0},   /* BLACK SQUARE */
    {{'O', 'S'}, 0x25a1},   /* WHITE SQUARE */
    {{'R', 'O'}, 0x25a2},   /* WHITE SQUARE WITH ROUNDED CORNERS */
    {{'R', 'r'}, 0x25a3},   /* WHITE SQUARE CONTAINING BLACK SMALL SQUARE */
    {{'R', 'F'}, 0x25a4},   /* SQUARE WITH HORIZONTAL FILL */
    {{'R', 'Y'}, 0x25a5},   /* SQUARE WITH VERTICAL FILL */
    {{'R', 'H'}, 0x25a6},   /* SQUARE WITH ORTHOGONAL CROSSHATCH FILL */
    {{'R', 'Z'}, 0x25a7},   /* SQUARE WITH UPPER LEFT TO LOWER RIGHT FILL */
    {{'R', 'K'}, 0x25a8},   /* SQUARE WITH UPPER RIGHT TO LOWER LEFT FILL */
    {{'R', 'X'}, 0x25a9},   /* SQUARE WITH DIAGONAL CROSSHATCH FILL */
    {{'s', 'B'}, 0x25aa},   /* BLACK SMALL SQUARE */
    {{'S', 'R'}, 0x25ac},   /* BLACK RECTANGLE */
    {{'O', 'r'}, 0x25ad},   /* WHITE RECTANGLE */
    {{'U', 'T'}, 0x25b2},   /* BLACK UP-POINTING TRIANGLE */
    {{'u', 'T'}, 0x25b3},   /* WHITE UP-POINTING TRIANGLE */
    {{'P', 'R'}, 0x25b6},   /* BLACK RIGHT-POINTING TRIANGLE */
    {{'T', 'r'}, 0x25b7},   /* WHITE RIGHT-POINTING TRIANGLE */
    {{'D', 't'}, 0x25bc},   /* BLACK DOWN-POINTING TRIANGLE */
    {{'d', 'T'}, 0x25bd},   /* WHITE DOWN-POINTING TRIANGLE */
    {{'P', 'L'}, 0x25c0},   /* BLACK LEFT-POINTING TRIANGLE */
    {{'T', 'l'}, 0x25c1},   /* WHITE LEFT-POINTING TRIANGLE */
    {{'D', 'b'}, 0x25c6},   /* BLACK DIAMOND */
    {{'D', 'w'}, 0x25c7},   /* WHITE DIAMOND */
    {{'L', 'Z'}, 0x25ca},   /* LOZENGE */
    {{'0', 'm'}, 0x25cb},   /* WHITE CIRCLE */
    {{'0', 'o'}, 0x25ce},   /* BULLSEYE */
    {{'0', 'M'}, 0x25cf},   /* BLACK CIRCLE */
    {{'0', 'L'}, 0x25d0},   /* CIRCLE WITH LEFT HALF BLACK */
    {{'0', 'R'}, 0x25d1},   /* CIRCLE WITH RIGHT HALF BLACK */
    {{'S', 'n'}, 0x25d8},   /* INVERSE BULLET */
    {{'I', 'c'}, 0x25d9},   /* INVERSE WHITE CIRCLE */
    {{'F', 'd'}, 0x25e2},   /* BLACK LOWER RIGHT TRIANGLE */
    {{'B', 'd'}, 0x25e3},   /* BLACK LOWER LEFT TRIANGLE */
    {{'*', '2'}, 0x2605},   /* BLACK STAR */
    {{'*', '1'}, 0x2606},   /* WHITE STAR */
    {{'<', 'H'}, 0x261c},   /* WHITE LEFT POINTING INDEX */
    {{'>', 'H'}, 0x261e},   /* WHITE RIGHT POINTING INDEX */
    {{'0', 'u'}, 0x263a},   /* WHITE SMILING FACE */
    {{'0', 'U'}, 0x263b},   /* BLACK SMILING FACE */
    {{'S', 'U'}, 0x263c},   /* WHITE SUN WITH RAYS */
    {{'F', 'm'}, 0x2640},   /* FEMALE SIGN */
    {{'M', 'l'}, 0x2642},   /* MALE SIGN */
    {{'c', 'S'}, 0x2660},   /* BLACK SPADE SUIT */
    {{'c', 'H'}, 0x2661},   /* WHITE HEART SUIT */
    {{'c', 'D'}, 0x2662},   /* WHITE DIAMOND SUIT */
    {{'c', 'C'}, 0x2663},   /* BLACK CLUB SUIT */
    {{'M', 'd'}, 0x2669},   /* QUARTER NOTE */
    {{'M', '8'}, 0x266a},   /* EIGHTH NOTE */
    {{'M', '2'}, 0x266b},   /* BARRED EIGHTH NOTES */
    {{'M', 'b'}, 0x266d},   /* MUSIC FLAT SIGN */
    {{'M', 'x'}, 0x266e},   /* MUSIC NATURAL SIGN */
    {{'M', 'X'}, 0x266f},   /* MUSIC SHARP SIGN */
    {{'O', 'K'}, 0x2713},   /* CHECK MARK */
    {{'X', 'X'}, 0x2717},   /* BALLOT X */
    {{'-', 'X'}, 0x2720},   /* MALTESE CROSS */
    {{'I', 'S'}, 0x3000},   /* IDEOGRAPHIC SPACE */
    {{',', '_'}, 0x3001},   /* IDEOGRAPHIC COMMA */
    {{'.', '_'}, 0x3002},   /* IDEOGRAPHIC PERIOD */
    {{'+', '"'}, 0x3003},   /* DITTO MARK */
    {{'+', '_'}, 0x3004},   /* IDEOGRAPHIC DITTO MARK */
    {{'*', '_'}, 0x3005},   /* IDEOGRAPHIC ITERATION MARK */
    {{';', '_'}, 0x3006},   /* IDEOGRAPHIC CLOSING MARK */
    {{'0', '_'}, 0x3007},   /* IDEOGRAPHIC NUMBER ZERO */
    {{'<', '+'}, 0x300a},   /* LEFT DOUBLE ANGLE BRACKET */
    {{'>', '+'}, 0x300b},   /* RIGHT DOUBLE ANGLE BRACKET */
    {{'<', '\''}, 0x300c},   /* LEFT CORNER BRACKET */
    {{'>', '\''}, 0x300d},   /* RIGHT CORNER BRACKET */
    {{'<', '"'}, 0x300e},   /* LEFT WHITE CORNER BRACKET */
    {{'>', '"'}, 0x300f},   /* RIGHT WHITE CORNER BRACKET */
    {{'(', '"'}, 0x3010},   /* LEFT BLACK LENTICULAR BRACKET */
    {{')', '"'}, 0x3011},   /* RIGHT BLACK LENTICULAR BRACKET */
    {{'=', 'T'}, 0x3012},   /* POSTAL MARK */
    {{'=', '_'}, 0x3013},   /* GETA MARK */
    {{'(', '\''}, 0x3014},   /* LEFT TORTOISE SHELL BRACKET */
    {{')', '\''}, 0x3015},   /* RIGHT TORTOISE SHELL BRACKET */
    {{'(', 'I'}, 0x3016},   /* LEFT WHITE LENTICULAR BRACKET */
    {{')', 'I'}, 0x3017},   /* RIGHT WHITE LENTICULAR BRACKET */
    {{'-', '?'}, 0x301c},   /* WAVE DASH */
    {{'A', '5'}, 0x3041},   /* HIRAGANA LETTER SMALL A */
    {{'a', '5'}, 0x3042},   /* HIRAGANA LETTER A */
    {{'I', '5'}, 0x3043},   /* HIRAGANA LETTER SMALL I */
    {{'i', '5'}, 0x3044},   /* HIRAGANA LETTER I */
    {{'U', '5'}, 0x3045},   /* HIRAGANA LETTER SMALL U */
    {{'u', '5'}, 0x3046},   /* HIRAGANA LETTER U */
    {{'E', '5'}, 0x3047},   /* HIRAGANA LETTER SMALL E */
    {{'e', '5'}, 0x3048},   /* HIRAGANA LETTER E */
    {{'O', '5'}, 0x3049},   /* HIRAGANA LETTER SMALL O */
    {{'o', '5'}, 0x304a},   /* HIRAGANA LETTER O */
    {{'k', 'a'}, 0x304b},   /* HIRAGANA LETTER KA */
    {{'g', 'a'}, 0x304c},   /* HIRAGANA LETTER GA */
    {{'k', 'i'}, 0x304d},   /* HIRAGANA LETTER KI */
    {{'g', 'i'}, 0x304e},   /* HIRAGANA LETTER GI */
    {{'k', 'u'}, 0x304f},   /* HIRAGANA LETTER KU */
    {{'g', 'u'}, 0x3050},   /* HIRAGANA LETTER GU */
    {{'k', 'e'}, 0x3051},   /* HIRAGANA LETTER KE */
    {{'g', 'e'}, 0x3052},   /* HIRAGANA LETTER GE */
    {{'k', 'o'}, 0x3053},   /* HIRAGANA LETTER KO */
    {{'g', 'o'}, 0x3054},   /* HIRAGANA LETTER GO */
    {{'s', 'a'}, 0x3055},   /* HIRAGANA LETTER SA */
    {{'z', 'a'}, 0x3056},   /* HIRAGANA LETTER ZA */
    {{'s', 'i'}, 0x3057},   /* HIRAGANA LETTER SI */
    {{'z', 'i'}, 0x3058},   /* HIRAGANA LETTER ZI */
    {{'s', 'u'}, 0x3059},   /* HIRAGANA LETTER SU */
    {{'z', 'u'}, 0x305a},   /* HIRAGANA LETTER ZU */
    {{'s', 'e'}, 0x305b},   /* HIRAGANA LETTER SE */
    {{'z', 'e'}, 0x305c},   /* HIRAGANA LETTER ZE */
    {{'s', 'o'}, 0x305d},   /* HIRAGANA LETTER SO */
    {{'z', 'o'}, 0x305e},   /* HIRAGANA LETTER ZO */
    {{'t', 'a'}, 0x305f},   /* HIRAGANA LETTER TA */
    {{'d', 'a'}, 0x3060},   /* HIRAGANA LETTER DA */
    {{'t', 'i'}, 0x3061},   /* HIRAGANA LETTER TI */
    {{'d', 'i'}, 0x3062},   /* HIRAGANA LETTER DI */
    {{'t', 'U'}, 0x3063},   /* HIRAGANA LETTER SMALL TU */
    {{'t', 'u'}, 0x3064},   /* HIRAGANA LETTER TU */
    {{'d', 'u'}, 0x3065},   /* HIRAGANA LETTER DU */
    {{'t', 'e'}, 0x3066},   /* HIRAGANA LETTER TE */
    {{'d', 'e'}, 0x3067},   /* HIRAGANA LETTER DE */
    {{'t', 'o'}, 0x3068},   /* HIRAGANA LETTER TO */
    {{'d', 'o'}, 0x3069},   /* HIRAGANA LETTER DO */
    {{'n', 'a'}, 0x306a},   /* HIRAGANA LETTER NA */
    {{'n', 'i'}, 0x306b},   /* HIRAGANA LETTER NI */
    {{'n', 'u'}, 0x306c},   /* HIRAGANA LETTER NU */
    {{'n', 'e'}, 0x306d},   /* HIRAGANA LETTER NE */
    {{'n', 'o'}, 0x306e},   /* HIRAGANA LETTER NO */
    {{'h', 'a'}, 0x306f},   /* HIRAGANA LETTER HA */
    {{'b', 'a'}, 0x3070},   /* HIRAGANA LETTER BA */
    {{'p', 'a'}, 0x3071},   /* HIRAGANA LETTER PA */
    {{'h', 'i'}, 0x3072},   /* HIRAGANA LETTER HI */
    {{'b', 'i'}, 0x3073},   /* HIRAGANA LETTER BI */
    {{'p', 'i'}, 0x3074},   /* HIRAGANA LETTER PI */
    {{'h', 'u'}, 0x3075},   /* HIRAGANA LETTER HU */
    {{'b', 'u'}, 0x3076},   /* HIRAGANA LETTER BU */
    {{'p', 'u'}, 0x3077},   /* HIRAGANA LETTER PU */
    {{'h', 'e'}, 0x3078},   /* HIRAGANA LETTER HE */
    {{'b', 'e'}, 0x3079},   /* HIRAGANA LETTER BE */
    {{'p', 'e'}, 0x307a},   /* HIRAGANA LETTER PE */
    {{'h', 'o'}, 0x307b},   /* HIRAGANA LETTER HO */
    {{'b', 'o'}, 0x307c},   /* HIRAGANA LETTER BO */
    {{'p', 'o'}, 0x307d},   /* HIRAGANA LETTER PO */
    {{'m', 'a'}, 0x307e},   /* HIRAGANA LETTER MA */
    {{'m', 'i'}, 0x307f},   /* HIRAGANA LETTER MI */
    {{'m', 'u'}, 0x3080},   /* HIRAGANA LETTER MU */
    {{'m', 'e'}, 0x3081},   /* HIRAGANA LETTER ME */
    {{'m', 'o'}, 0x3082},   /* HIRAGANA LETTER MO */
    {{'y', 'A'}, 0x3083},   /* HIRAGANA LETTER SMALL YA */
    {{'y', 'a'}, 0x3084},   /* HIRAGANA LETTER YA */
    {{'y', 'U'}, 0x3085},   /* HIRAGANA LETTER SMALL YU */
    {{'y', 'u'}, 0x3086},   /* HIRAGANA LETTER YU */
    {{'y', 'O'}, 0x3087},   /* HIRAGANA LETTER SMALL YO */
    {{'y', 'o'}, 0x3088},   /* HIRAGANA LETTER YO */
    {{'r', 'a'}, 0x3089},   /* HIRAGANA LETTER RA */
    {{'r', 'i'}, 0x308a},   /* HIRAGANA LETTER RI */
    {{'r', 'u'}, 0x308b},   /* HIRAGANA LETTER RU */
    {{'r', 'e'}, 0x308c},   /* HIRAGANA LETTER RE */
    {{'r', 'o'}, 0x308d},   /* HIRAGANA LETTER RO */
    {{'w', 'A'}, 0x308e},   /* HIRAGANA LETTER SMALL WA */
    {{'w', 'a'}, 0x308f},   /* HIRAGANA LETTER WA */
    {{'w', 'i'}, 0x3090},   /* HIRAGANA LETTER WI */
    {{'w', 'e'}, 0x3091},   /* HIRAGANA LETTER WE */
    {{'w', 'o'}, 0x3092},   /* HIRAGANA LETTER WO */
    {{'n', '5'}, 0x3093},   /* HIRAGANA LETTER N */
    {{'v', 'u'}, 0x3094},   /* HIRAGANA LETTER VU */
    {{'"', '5'}, 0x309b},   /* KATAKANA-HIRAGANA VOICED SOUND MARK */
    {{'0', '5'}, 0x309c},   /* KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK */
    {{'*', '5'}, 0x309d},   /* HIRAGANA ITERATION MARK */
    {{'+', '5'}, 0x309e},   /* HIRAGANA VOICED ITERATION MARK */
    {{'a', '6'}, 0x30a1},   /* KATAKANA LETTER SMALL A */
    {{'A', '6'}, 0x30a2},   /* KATAKANA LETTER A */
    {{'i', '6'}, 0x30a3},   /* KATAKANA LETTER SMALL I */
    {{'I', '6'}, 0x30a4},   /* KATAKANA LETTER I */
    {{'u', '6'}, 0x30a5},   /* KATAKANA LETTER SMALL U */
    {{'U', '6'}, 0x30a6},   /* KATAKANA LETTER U */
    {{'e', '6'}, 0x30a7},   /* KATAKANA LETTER SMALL E */
    {{'E', '6'}, 0x30a8},   /* KATAKANA LETTER E */
    {{'o', '6'}, 0x30a9},   /* KATAKANA LETTER SMALL O */
    {{'O', '6'}, 0x30aa},   /* KATAKANA LETTER O */
    {{'K', 'a'}, 0x30ab},   /* KATAKANA LETTER KA */
    {{'G', 'a'}, 0x30ac},   /* KATAKANA LETTER GA */
    {{'K', 'i'}, 0x30ad},   /* KATAKANA LETTER KI */
    {{'G', 'i'}, 0x30ae},   /* KATAKANA LETTER GI */
    {{'K', 'u'}, 0x30af},   /* KATAKANA LETTER KU */
    {{'G', 'u'}, 0x30b0},   /* KATAKANA LETTER GU */
    {{'K', 'e'}, 0x30b1},   /* KATAKANA LETTER KE */
    {{'G', 'e'}, 0x30b2},   /* KATAKANA LETTER GE */
    {{'K', 'o'}, 0x30b3},   /* KATAKANA LETTER KO */
    {{'G', 'o'}, 0x30b4},   /* KATAKANA LETTER GO */
    {{'S', 'a'}, 0x30b5},   /* KATAKANA LETTER SA */
    {{'Z', 'a'}, 0x30b6},   /* KATAKANA LETTER ZA */
    {{'S', 'i'}, 0x30b7},   /* KATAKANA LETTER SI */
    {{'Z', 'i'}, 0x30b8},   /* KATAKANA LETTER ZI */
    {{'S', 'u'}, 0x30b9},   /* KATAKANA LETTER SU */
    {{'Z', 'u'}, 0x30ba},   /* KATAKANA LETTER ZU */
    {{'S', 'e'}, 0x30bb},   /* KATAKANA LETTER SE */
    {{'Z', 'e'}, 0x30bc},   /* KATAKANA LETTER ZE */
    {{'S', 'o'}, 0x30bd},   /* KATAKANA LETTER SO */
    {{'Z', 'o'}, 0x30be},   /* KATAKANA LETTER ZO */
    {{'T', 'a'}, 0x30bf},   /* KATAKANA LETTER TA */
    {{'D', 'a'}, 0x30c0},   /* KATAKANA LETTER DA */
    {{'T', 'i'}, 0x30c1},   /* KATAKANA LETTER TI */
    {{'D', 'i'}, 0x30c2},   /* KATAKANA LETTER DI */
    {{'T', 'U'}, 0x30c3},   /* KATAKANA LETTER SMALL TU */
    {{'T', 'u'}, 0x30c4},   /* KATAKANA LETTER TU */
    {{'D', 'u'}, 0x30c5},   /* KATAKANA LETTER DU */
    {{'T', 'e'}, 0x30c6},   /* KATAKANA LETTER TE */
    {{'D', 'e'}, 0x30c7},   /* KATAKANA LETTER DE */
    {{'T', 'o'}, 0x30c8},   /* KATAKANA LETTER TO */
    {{'D', 'o'}, 0x30c9},   /* KATAKANA LETTER DO */
    {{'N', 'a'}, 0x30ca},   /* KATAKANA LETTER NA */
    {{'N', 'i'}, 0x30cb},   /* KATAKANA LETTER NI */
    {{'N', 'u'}, 0x30cc},   /* KATAKANA LETTER NU */
    {{'N', 'e'}, 0x30cd},   /* KATAKANA LETTER NE */
    {{'N', 'o'}, 0x30ce},   /* KATAKANA LETTER NO */
    {{'H', 'a'}, 0x30cf},   /* KATAKANA LETTER HA */
    {{'B', 'a'}, 0x30d0},   /* KATAKANA LETTER BA */
    {{'P', 'a'}, 0x30d1},   /* KATAKANA LETTER PA */
    {{'H', 'i'}, 0x30d2},   /* KATAKANA LETTER HI */
    {{'B', 'i'}, 0x30d3},   /* KATAKANA LETTER BI */
    {{'P', 'i'}, 0x30d4},   /* KATAKANA LETTER PI */
    {{'H', 'u'}, 0x30d5},   /* KATAKANA LETTER HU */
    {{'B', 'u'}, 0x30d6},   /* KATAKANA LETTER BU */
    {{'P', 'u'}, 0x30d7},   /* KATAKANA LETTER PU */
    {{'H', 'e'}, 0x30d8},   /* KATAKANA LETTER HE */
    {{'B', 'e'}, 0x30d9},   /* KATAKANA LETTER BE */
    {{'P', 'e'}, 0x30da},   /* KATAKANA LETTER PE */
    {{'H', 'o'}, 0x30db},   /* KATAKANA LETTER HO */
    {{'B', 'o'}, 0x30dc},   /* KATAKANA LETTER BO */
    {{'P', 'o'}, 0x30dd},   /* KATAKANA LETTER PO */
    {{'M', 'a'}, 0x30de},   /* KATAKANA LETTER MA */
    {{'M', 'i'}, 0x30df},   /* KATAKANA LETTER MI */
    {{'M', 'u'}, 0x30e0},   /* KATAKANA LETTER MU */
    {{'M', 'e'}, 0x30e1},   /* KATAKANA LETTER ME */
    {{'M', 'o'}, 0x30e2},   /* KATAKANA LETTER MO */
    {{'Y', 'A'}, 0x30e3},   /* KATAKANA LETTER SMALL YA */
    {{'Y', 'a'}, 0x30e4},   /* KATAKANA LETTER YA */
    {{'Y', 'U'}, 0x30e5},   /* KATAKANA LETTER SMALL YU */
    {{'Y', 'u'}, 0x30e6},   /* KATAKANA LETTER YU */
    {{'Y', 'O'}, 0x30e7},   /* KATAKANA LETTER SMALL YO */
    {{'Y', 'o'}, 0x30e8},   /* KATAKANA LETTER YO */
    {{'R', 'a'}, 0x30e9},   /* KATAKANA LETTER RA */
    {{'R', 'i'}, 0x30ea},   /* KATAKANA LETTER RI */
    {{'R', 'u'}, 0x30eb},   /* KATAKANA LETTER RU */
    {{'R', 'e'}, 0x30ec},   /* KATAKANA LETTER RE */
    {{'R', 'o'}, 0x30ed},   /* KATAKANA LETTER RO */
    {{'W', 'A'}, 0x30ee},   /* KATAKANA LETTER SMALL WA */
    {{'W', 'a'}, 0x30ef},   /* KATAKANA LETTER WA */
    {{'W', 'i'}, 0x30f0},   /* KATAKANA LETTER WI */
    {{'W', 'e'}, 0x30f1},   /* KATAKANA LETTER WE */
    {{'W', 'o'}, 0x30f2},   /* KATAKANA LETTER WO */
    {{'N', '6'}, 0x30f3},   /* KATAKANA LETTER N */
    {{'V', 'u'}, 0x30f4},   /* KATAKANA LETTER VU */
    {{'K', 'A'}, 0x30f5},   /* KATAKANA LETTER SMALL KA */
    {{'K', 'E'}, 0x30f6},   /* KATAKANA LETTER SMALL KE */
    {{'V', 'a'}, 0x30f7},   /* KATAKANA LETTER VA */
    {{'V', 'i'}, 0x30f8},   /* KATAKANA LETTER VI */
    {{'V', 'e'}, 0x30f9},   /* KATAKANA LETTER VE */
    {{'V', 'o'}, 0x30fa},   /* KATAKANA LETTER VO */
    {{'.', '6'}, 0x30fb},   /* KATAKANA MIDDLE DOT */
    {{'-', '6'}, 0x30fc},   /* KATAKANA-HIRAGANA PROLONGED SOUND MARK */
    {{'*', '6'}, 0x30fd},   /* KATAKANA ITERATION MARK */
    {{'+', '6'}, 0x30fe},   /* KATAKANA VOICED ITERATION MARK */
    {{'b', '4'}, 0x3105},   /* BOPOMOFO LETTER B */
    {{'p', '4'}, 0x3106},   /* BOPOMOFO LETTER P */
    {{'m', '4'}, 0x3107},   /* BOPOMOFO LETTER M */
    {{'f', '4'}, 0x3108},   /* BOPOMOFO LETTER F */
    {{'d', '4'}, 0x3109},   /* BOPOMOFO LETTER D */
    {{'t', '4'}, 0x310a},   /* BOPOMOFO LETTER T */
    {{'n', '4'}, 0x310b},   /* BOPOMOFO LETTER N */
    {{'l', '4'}, 0x310c},   /* BOPOMOFO LETTER L */
    {{'g', '4'}, 0x310d},   /* BOPOMOFO LETTER G */
    {{'k', '4'}, 0x310e},   /* BOPOMOFO LETTER K */
    {{'h', '4'}, 0x310f},   /* BOPOMOFO LETTER H */
    {{'j', '4'}, 0x3110},   /* BOPOMOFO LETTER J */
    {{'q', '4'}, 0x3111},   /* BOPOMOFO LETTER Q */
    {{'x', '4'}, 0x3112},   /* BOPOMOFO LETTER X */
    {{'z', 'h'}, 0x3113},   /* BOPOMOFO LETTER ZH */
    {{'c', 'h'}, 0x3114},   /* BOPOMOFO LETTER CH */
    {{'s', 'h'}, 0x3115},   /* BOPOMOFO LETTER SH */
    {{'r', '4'}, 0x3116},   /* BOPOMOFO LETTER R */
    {{'z', '4'}, 0x3117},   /* BOPOMOFO LETTER Z */
    {{'c', '4'}, 0x3118},   /* BOPOMOFO LETTER C */
    {{'s', '4'}, 0x3119},   /* BOPOMOFO LETTER S */
    {{'a', '4'}, 0x311a},   /* BOPOMOFO LETTER A */
    {{'o', '4'}, 0x311b},   /* BOPOMOFO LETTER O */
    {{'e', '4'}, 0x311c},   /* BOPOMOFO LETTER E */
    {{'a', 'i'}, 0x311e},   /* BOPOMOFO LETTER AI */
    {{'e', 'i'}, 0x311f},   /* BOPOMOFO LETTER EI */
    {{'a', 'u'}, 0x3120},   /* BOPOMOFO LETTER AU */
    {{'o', 'u'}, 0x3121},   /* BOPOMOFO LETTER OU */
    {{'a', 'n'}, 0x3122},   /* BOPOMOFO LETTER AN */
    {{'e', 'n'}, 0x3123},   /* BOPOMOFO LETTER EN */
    {{'a', 'N'}, 0x3124},   /* BOPOMOFO LETTER ANG */
    {{'e', 'N'}, 0x3125},   /* BOPOMOFO LETTER ENG */
    {{'e', 'r'}, 0x3126},   /* BOPOMOFO LETTER ER */
    {{'i', '4'}, 0x3127},   /* BOPOMOFO LETTER I */
    {{'u', '4'}, 0x3128},   /* BOPOMOFO LETTER U */
    {{'i', 'u'}, 0x3129},   /* BOPOMOFO LETTER IU */
    {{'v', '4'}, 0x312a},   /* BOPOMOFO LETTER V */
    {{'n', 'G'}, 0x312b},   /* BOPOMOFO LETTER NG */
    {{'g', 'n'}, 0x312c},   /* BOPOMOFO LETTER GN */
    {{'1', 'c'}, 0x3220},   /* PARENTHESIZED IDEOGRAPH ONE */
    {{'2', 'c'}, 0x3221},   /* PARENTHESIZED IDEOGRAPH TWO */
    {{'3', 'c'}, 0x3222},   /* PARENTHESIZED IDEOGRAPH THREE */
    {{'4', 'c'}, 0x3223},   /* PARENTHESIZED IDEOGRAPH FOUR */
    {{'5', 'c'}, 0x3224},   /* PARENTHESIZED IDEOGRAPH FIVE */
    {{'6', 'c'}, 0x3225},   /* PARENTHESIZED IDEOGRAPH SIX */
    {{'7', 'c'}, 0x3226},   /* PARENTHESIZED IDEOGRAPH SEVEN */
    {{'8', 'c'}, 0x3227},   /* PARENTHESIZED IDEOGRAPH EIGHT */
    {{'9', 'c'}, 0x3228},   /* PARENTHESIZED IDEOGRAPH NINE */
    {{'f', 'f'}, 0xfb00},   /* LATIN SMALL LIGATURE FF */
    {{'f', 'i'}, 0xfb01},   /* LATIN SMALL LIGATURE FI */
    {{'f', 'l'}, 0xfb02},   /* LATIN SMALL LIGATURE FL */
    {{'f', 't'}, 0xfb05},   /* LATIN SMALL LIGATURE FT */
    {{'s', 't'}, 0xfb06},   /* LATIN SMALL LIGATURE ST */
    {{'N', 'U'}, 0x0000},   /* NULL (NUL) */
    {{'S', 'H'}, 0x0001},   /* START OF HEADING (SOH) */
    {{'S', 'X'}, 0x0002},   /* START OF TEXT (STX) */
    {{'E', 'X'}, 0x0003},   /* END OF TEXT (ETX) */
    {{'E', 'T'}, 0x0004},   /* END OF TRANSMISSION (EOT) */
    {{'E', 'Q'}, 0x0005},   /* ENQUIRY (ENQ) */
    {{'A', 'K'}, 0x0006},   /* ACKNOWLEDGE (ACK) */
    {{'B', 'L'}, 0x0007},   /* BELL (BEL) */
    {{'B', 'S'}, 0x0008},   /* BACKSPACE (BS) */
    {{'H', 'T'}, 0x0009},   /* CHARACTER TABULATION (HT) */
    {{'L', 'F'}, 0x000a},   /* LINE FEED (LF) */
    {{'V', 'T'}, 0x000b},   /* LINE TABULATION (VT) */
    {{'F', 'F'}, 0x000c},   /* FORM FEED (FF) */
    {{'C', 'R'}, 0x000d},   /* CARRIAGE RETURN (CR) */
    {{'S', 'O'}, 0x000e},   /* SHIFT OUT (SO) */
    {{'S', 'I'}, 0x000f},   /* SHIFT IN (SI) */
    {{'D', 'L'}, 0x0010},   /* DATALINK ESCAPE (DLE) */
    {{'D', '1'}, 0x0011},   /* DEVICE CONTROL ONE (DC1) */
    {{'D', '2'}, 0x0012},   /* DEVICE CONTROL TWO (DC2) */
    {{'D', '3'}, 0x0013},   /* DEVICE CONTROL THREE (DC3) */
    {{'D', '4'}, 0x0014},   /* DEVICE CONTROL FOUR (DC4) */
    {{'N', 'K'}, 0x0015},   /* NEGATIVE ACKNOWLEDGE (NAK) */
    {{'S', 'Y'}, 0x0016},   /* SYNCRONOUS IDLE (SYN) */
    {{'E', 'B'}, 0x0017},   /* END OF TRANSMISSION BLOCK (ETB) */
    {{'C', 'N'}, 0x0018},   /* CANCEL (CAN) */
    {{'E', 'M'}, 0x0019},   /* END OF MEDIUM (EM) */
    {{'S', 'B'}, 0x001a},   /* SUBSTITUTE (SUB) */
    {{'E', 'C'}, 0x001b},   /* ESCAPE (ESC) */
    {{'F', 'S'}, 0x001c},   /* FILE SEPARATOR (IS4) */
    {{'G', 'S'}, 0x001d},   /* GROUP SEPARATOR (IS3) */
    {{'R', 'S'}, 0x001e},   /* RECORD SEPARATOR (IS2) */
    {{'U', 'S'}, 0x001f},   /* UNIT SEPARATOR (IS1) */
    {{'D', 'T'}, 0x007f},   /* DELETE (DEL) */
    {{'P', 'A'}, 0x0080},   /* PADDING CHARACTER (PAD) */
    {{'H', 'O'}, 0x0081},   /* HIGH OCTET PRESET (HOP) */
    {{'B', 'H'}, 0x0082},   /* BREAK PERMITTED HERE (BPH) */
    {{'N', 'H'}, 0x0083},   /* NO BREAK HERE (NBH) */
    {{'I', 'N'}, 0x0084},   /* INDEX (IND) */
    {{'N', 'L'}, 0x0085},   /* NEXT LINE (NEL) */
    {{'S', 'A'}, 0x0086},   /* START OF SELECTED AREA (SSA) */
    {{'E', 'S'}, 0x0087},   /* END OF SELECTED AREA (ESA) */
    {{'H', 'S'}, 0x0088},   /* CHARACTER TABULATION SET (HTS) */
    {{'H', 'J'}, 0x0089},   /* CHARACTER TABULATION WITH JUSTIFICATION (HTJ) */
    {{'V', 'S'}, 0x008a},   /* LINE TABULATION SET (VTS) */
    {{'P', 'D'}, 0x008b},   /* PARTIAL LINE FORWARD (PLD) */
    {{'P', 'U'}, 0x008c},   /* PARTIAL LINE BACKWARD (PLU) */
    {{'R', 'I'}, 0x008d},   /* REVERSE LINE FEED (RI) */
    {{'S', '2'}, 0x008e},   /* SINGLE-SHIFT TWO (SS2) */
    {{'S', '3'}, 0x008f},   /* SINGLE-SHIFT THREE (SS3) */
    {{'D', 'C'}, 0x0090},   /* DEVICE CONTROL STRING (DCS) */
    {{'P', '1'}, 0x0091},   /* PRIVATE USE ONE (PU1) */
    {{'P', '2'}, 0x0092},   /* PRIVATE USE TWO (PU2) */
    {{'T', 'S'}, 0x0093},   /* SET TRANSMIT STATE (STS) */
    {{'C', 'C'}, 0x0094},   /* CANCEL CHARACTER (CCH) */
    {{'M', 'W'}, 0x0095},   /* MESSAGE WAITING (MW) */
    {{'S', 'G'}, 0x0096},   /* START OF GUARDED AREA (SPA) */
    {{'E', 'G'}, 0x0097},   /* END OF GUARDED AREA (EPA) */
    {{'S', 'S'}, 0x0098},   /* START OF STRING (SOS) */
    {{'G', 'C'}, 0x0099},   /* SINGLE GRAPHIC CHARACTER INTRODUCER (SGCI) */
    {{'S', 'C'}, 0x009a},   /* SINGLE CHARACTER INTRODUCER (SCI) */
    {{'C', 'I'}, 0x009b},   /* CONTROL SEQUENCE INTRODUCER (CSI) */
    {{'S', 'T'}, 0x009c},   /* STRING TERMINATOR (ST) */
    {{'O', 'C'}, 0x009d},   /* OPERATING SYSTEM COMMAND (OSC) */
    {{'P', 'M'}, 0x009e},   /* PRIVACY MESSAGE (PM) */
    {{'A', 'C'}, 0x009f},   /* APPLICATION PROGRAM COMMAND (APC) */
    {{' ', ' '}, 0xe000},   /* indicates unfinished (Mnemonic) */
    {{'/', 'c'}, 0xe001},   /* JOIN THIS LINE WITH NEXT LINE (Mnemonic) */
    {{'U', 'A'}, 0xe002},   /* Unit space A (ISO-IR-8-1 064) */
    {{'U', 'B'}, 0xe003},   /* Unit space B (ISO-IR-8-1 096) */
    {{'"', '3'}, 0xe004},   /* NON-SPACING UMLAUT (ISO-IR-38 201) (character part) */
    {{'"', '1'}, 0xe005},   /* NON-SPACING DIAERESIS WITH ACCENT (ISO-IR-70 192) (character part) */
    {{'"', '!'}, 0xe006},   /* NON-SPACING GRAVE ACCENT (ISO-IR-103 193) (character part) */
    {{'"', '\''}, 0xe007},   /* NON-SPACING ACUTE ACCENT (ISO-IR-103 194) (character part) */
    {{'"', '>'}, 0xe008},   /* NON-SPACING CIRCUMFLEX ACCENT (ISO-IR-103 195) (character part) */
    {{'"', '?'}, 0xe009},   /* NON-SPACING TILDE (ISO-IR-103 196) (character part) */
    {{'"', '-'}, 0xe00a},   /* NON-SPACING MACRON (ISO-IR-103 197) (character part) */
    {{'"', '('}, 0xe00b},   /* NON-SPACING BREVE (ISO-IR-103 198) (character part) */
    {{'"', '.'}, 0xe00c},   /* NON-SPACING DOT ABOVE (ISO-IR-103 199) (character part) */
    {{'"', ':'}, 0xe00d},   /* NON-SPACING DIAERESIS (ISO-IR-103 200) (character part) */
    {{'"', '0'}, 0xe00e},   /* NON-SPACING RING ABOVE (ISO-IR-103 202) (character part) */
    {{'"', '"'}, 0xe00f},   /* NON-SPACING DOUBLE ACCUTE (ISO-IR-103 204) (character part) */
    {{'"', '<'}, 0xe010},   /* NON-SPACING CARON (ISO-IR-103 206) (character part) */
    {{'"', ','}, 0xe011},   /* NON-SPACING CEDILLA (ISO-IR-103 203) (character part) */
    {{'"', ';'}, 0xe012},   /* NON-SPACING OGONEK (ISO-IR-103 206) (character part) */
    {{'"', '_'}, 0xe013},   /* NON-SPACING LOW LINE (ISO-IR-103 204) (character part) */
    {{'"', '='}, 0xe014},   /* NON-SPACING DOUBLE LOW LINE (ISO-IR-38 217) (character part) */
    {{'"', '/'}, 0xe015},   /* NON-SPACING LONG SOLIDUS (ISO-IR-128 201) (character part) */
    {{'"', 'i'}, 0xe016},   /* GREEK NON-SPACING IOTA BELOW (ISO-IR-55 39) (character part) */
    {{'"', 'd'}, 0xe017},   /* GREEK NON-SPACING DASIA PNEUMATA (ISO-IR-55 38) (character part) */
    {{'"', 'p'}, 0xe018},   /* GREEK NON-SPACING PSILI PNEUMATA (ISO-IR-55 37) (character part) */
    {{';', ';'}, 0xe019},   /* GREEK DASIA PNEUMATA (ISO-IR-18 92) */
    {{',', ','}, 0xe01a},   /* GREEK PSILI PNEUMATA (ISO-IR-18 124) */
    {{'b', '3'}, 0xe01b},   /* GREEK SMALL LETTER MIDDLE BETA (ISO-IR-18 99) */
    {{'C', 'i'}, 0xe01c},   /* CIRCLE (ISO-IR-83 0294) */
    {{'f', '('}, 0xe01d},   /* FUNCTION SIGN (ISO-IR-143 221) */
    {{'e', 'd'}, 0xe01e},   /* LATIN SMALL LETTER EZH (ISO-IR-158 142) */
    {{'a', 'm'}, 0xe01f},   /* ANTE MERIDIAM SIGN (ISO-IR-149 0267) */
    {{'p', 'm'}, 0xe020},   /* POST MERIDIAM SIGN (ISO-IR-149 0268) */
    {{'F', 'l'}, 0xe023},   /* DUTCH GUILDER SIGN (IBM437 159) */
    {{'G', 'F'}, 0xe024},   /* GAMMA FUNCTION SIGN (ISO-10646-1DIS 032/032/037/122) */
    {{'>', 'V'}, 0xe025},   /* RIGHTWARDS VECTOR ABOVE (ISO-10646-1DIS 032/032/038/046) */
    {{'!', '*'}, 0xe026},   /* GREEK VARIA (ISO-10646-1DIS 032/032/042/164) */
    {{'?', '*'}, 0xe027},   /* GREEK PERISPOMENI (ISO-10646-1DIS 032/032/042/165) */
    {{'J', '<'}, 0xe028}    /* LATIN CAPITAL LETTER J WITH CARON (lowercase: 000/000/001/240) */
};

#define RESIZE_FLAG_H 1
#define RESIZE_FLAG_V 2
#define RESIZE_FLAG_L 4

static char *resizeprompts[] = {
  "resize # lines: ",
  "resize -h # lines: ",
  "resize -v # lines: ",
  "resize -b # lines: ",
  "resize -l # lines: ",
  "resize -l -h # lines: ",
  "resize -l -v # lines: ",
  "resize -l -b # lines: ",
};

static int
parse_input_int(const char *buf, int len, int *val)
{
  int x = 0, i;
  if (len >= 1 && ((*buf == 'U' && buf[1] == '+') || (*buf == '0' && (buf[1] == 'x' || buf[1] == 'X'))))
    {
      x = 0;
      for (i = 2; i < len; i++)
	{
	  if (buf[i] >= '0' && buf[i] <= '9')
	    x = x * 16 | (buf[i] - '0');
	  else if (buf[i] >= 'a' && buf[i] <= 'f')
	    x = x * 16 | (buf[i] - ('a' - 10));
	  else if (buf[i] >= 'A' && buf[i] <= 'F')
	    x = x * 16 | (buf[i] - ('A' - 10));
	  else
	    return 0;
	}
    }
  else if (buf[0] == '0')
    {
      x = 0;
      for (i = 1; i < len; i++)
	{
	  if (buf[i] < '0' || buf[i] > '7')
	    return 0;
	  x = x * 8 | (buf[i] - '0');
	}
    }
  else
    return 0;
  *val = x;
  return 1;
}

char *noargs[1];

int enter_window_name_mode = 0;

void
InitKeytab()
{
  register unsigned int i;
#ifdef MAPKEYS
  char *argarr[2];
#endif

  for (i = 0; i < sizeof(ktab)/sizeof(*ktab); i++)
    {
      ktab[i].nr = RC_ILLEGAL;
      ktab[i].args = noargs;
      ktab[i].argl = 0;
    }
#ifdef MAPKEYS
  for (i = 0; i < KMAP_KEYS+KMAP_AKEYS; i++)
    {
      umtab[i].nr = RC_ILLEGAL;
      umtab[i].args = noargs;
      umtab[i].argl = 0;
      dmtab[i].nr = RC_ILLEGAL;
      dmtab[i].args = noargs;
      dmtab[i].argl = 0;
      mmtab[i].nr = RC_ILLEGAL;
      mmtab[i].args = noargs;
      mmtab[i].argl = 0;
    }
  argarr[1] = 0;
  for (i = 0; i < NKMAPDEF; i++)
    {
      if (i + KMAPDEFSTART < T_CAPS)
	continue;
      if (i + KMAPDEFSTART >= T_CAPS + KMAP_KEYS)
	continue;
      if (kmapdef[i] == 0)
	continue;
      argarr[0] = kmapdef[i];
      SaveAction(dmtab + i + (KMAPDEFSTART - T_CAPS), RC_STUFF, argarr, 0);
    }
  for (i = 0; i < NKMAPADEF; i++)
    {
      if (i + KMAPADEFSTART < T_CURSOR)
	continue;
      if (i + KMAPADEFSTART >= T_CURSOR + KMAP_AKEYS)
	continue;
      if (kmapadef[i] == 0)
	continue;
      argarr[0] = kmapadef[i];
      SaveAction(dmtab + i + (KMAPADEFSTART - T_CURSOR + KMAP_KEYS), RC_STUFF, argarr, 0);
    }
  for (i = 0; i < NKMAPMDEF; i++)
    {
      if (i + KMAPMDEFSTART < T_CAPS)
	continue;
      if (i + KMAPMDEFSTART >= T_CAPS + KMAP_KEYS)
	continue;
      if (kmapmdef[i] == 0)
	continue;
      argarr[0] = kmapmdef[i];
      argarr[1] = 0;
      SaveAction(mmtab + i + (KMAPMDEFSTART - T_CAPS), RC_STUFF, argarr, 0);
    }
#endif

  ktab['h'].nr = RC_HARDCOPY;
#ifdef BSDJOBS
  ktab['z'].nr = ktab[Ctrl('z')].nr = RC_SUSPEND;
#endif
  ktab['c'].nr = ktab[Ctrl('c')].nr = RC_SCREEN;
  ktab[' '].nr = ktab[Ctrl(' ')].nr =
    ktab['n'].nr = ktab[Ctrl('n')].nr = RC_NEXT;
  ktab['N'].nr = RC_NUMBER;
  ktab[Ctrl('h')].nr = ktab[0177].nr = ktab['p'].nr = ktab[Ctrl('p')].nr = RC_PREV;
  ktab['k'].nr = ktab[Ctrl('k')].nr = RC_KILL;
  ktab['l'].nr = ktab[Ctrl('l')].nr = RC_REDISPLAY;
  ktab['w'].nr = ktab[Ctrl('w')].nr = RC_WINDOWS;
  ktab['v'].nr = RC_VERSION;
  ktab[Ctrl('v')].nr = RC_DIGRAPH;
  ktab['q'].nr = ktab[Ctrl('q')].nr = RC_XON;
  ktab['s'].nr = ktab[Ctrl('s')].nr = RC_XOFF;
  ktab['t'].nr = ktab[Ctrl('t')].nr = RC_TIME;
  ktab['i'].nr = ktab[Ctrl('i')].nr = RC_INFO;
  ktab['m'].nr = ktab[Ctrl('m')].nr = RC_LASTMSG;
  ktab['A'].nr = RC_TITLE;
#if defined(UTMPOK) && defined(LOGOUTOK)
  ktab['L'].nr = RC_LOGIN;
#endif
  ktab[','].nr = RC_LICENSE;
  ktab['W'].nr = RC_WIDTH;
  ktab['.'].nr = RC_DUMPTERMCAP;
  ktab[Ctrl('\\')].nr = RC_QUIT;
#ifdef DETACH
  ktab['d'].nr = ktab[Ctrl('d')].nr = RC_DETACH;
# ifdef POW_DETACH
  ktab['D'].nr = RC_POW_DETACH;
# endif
#endif
  ktab['r'].nr = ktab[Ctrl('r')].nr = RC_WRAP;
  ktab['f'].nr = ktab[Ctrl('f')].nr = RC_FLOW;
  ktab['C'].nr = RC_CLEAR;
  ktab['Z'].nr = RC_RESET;
  ktab['H'].nr = RC_LOG;
  ktab['M'].nr = RC_MONITOR;
  ktab['?'].nr = RC_HELP;
#ifdef MULTI
  ktab['*'].nr = RC_DISPLAYS;
#endif
  {
    char *args[2];
    args[0] = "-";
    args[1] = NULL;
    SaveAction(ktab + '-', RC_SELECT, args, 0);
  }
  for (i = 0; i < ((maxwin && maxwin < 10) ? maxwin : 10); i++)
    {
      char *args[2], arg1[10];
      args[0] = arg1;
      args[1] = 0;
      sprintf(arg1, "%d", i);
      SaveAction(ktab + '0' + i, RC_SELECT, args, 0);
    }
  ktab['\''].nr = RC_SELECT; /* calling a window by name */
  {
    char *args[2];
    args[0] = "-b";
    args[1] = 0;
    SaveAction(ktab + '"', RC_WINDOWLIST, args, 0);
  }
  ktab[Ctrl('G')].nr = RC_VBELL;
  ktab[':'].nr = RC_COLON;
#ifdef COPY_PASTE
  ktab['['].nr = ktab[Ctrl('[')].nr = RC_COPY;
  {
    char *args[2];
    args[0] = ".";
    args[1] = 0;
    SaveAction(ktab + ']', RC_PASTE, args, 0);
    SaveAction(ktab + Ctrl(']'), RC_PASTE, args, 0);
  }
  ktab['{'].nr = RC_HISTORY;
  ktab['}'].nr = RC_HISTORY;
  ktab['>'].nr = RC_WRITEBUF;
  ktab['<'].nr = RC_READBUF;
  ktab['='].nr = RC_REMOVEBUF;
#endif
/* co-opting "^A [<>=]", looking for alternatives */
  ktab['>'].nr = ktab[Ctrl('.')].nr = RC_BUMPRIGHT;
  ktab['<'].nr = ktab[Ctrl(',')].nr = RC_BUMPLEFT;
  ktab['='].nr = RC_COLLAPSE;
#ifdef POW_DETACH
  ktab['D'].nr = RC_POW_DETACH;
#endif
#ifdef LOCK
  ktab['x'].nr = ktab[Ctrl('x')].nr = RC_LOCKSCREEN;
#endif
  ktab['b'].nr = ktab[Ctrl('b')].nr = RC_BREAK;
  ktab['B'].nr = RC_POW_BREAK;
  ktab['_'].nr = RC_SILENCE;
  ktab['S'].nr = RC_SPLIT;
  ktab['Q'].nr = RC_ONLY;
  ktab['X'].nr = RC_REMOVE;
  ktab['F'].nr = RC_FIT;
  ktab['\t'].nr = RC_FOCUS;
  {
    char *args[2];
    args[0] = "prev";
    args[1] = 0;
    SaveAction(ktab + T_BACKTAB - T_CAPS + 256, RC_FOCUS, args, 0);
  }
  {
    char *args[2];
    args[0] = "-v";
    args[1] = 0;
    SaveAction(ktab + '|', RC_SPLIT, args, 0);
  }
  /* These come last; they may want overwrite others: */
  if (DefaultEsc >= 0)
    {
      ClearAction(&ktab[DefaultEsc]);
      ktab[DefaultEsc].nr = RC_OTHER;
    }
  if (DefaultMetaEsc >= 0)
    {
      ClearAction(&ktab[DefaultMetaEsc]);
      ktab[DefaultMetaEsc].nr = RC_META;
    }

  idleaction.nr = RC_BLANKER;
  idleaction.args = noargs;
  idleaction.argl = 0;
}

static struct action *
FindKtab(char *class, int create)
{
  struct kclass *kp, **kpp;
  int i;

  if (class == 0)
    return ktab;
  for (kpp = &kclasses; (kp = *kpp) != 0; kpp = &kp->next)
    if (!strcmp(kp->name, class))
      break;
  if (kp == 0)
    {
      if (!create)
	return 0;
      if (strlen(class) > 80)
	{
	  Msg(0, "Command class name too long.");
	  return 0;
	}
      kp = malloc(sizeof(*kp));
      if (kp == 0)
	{
	  Msg(0, "%s", strnomem);
	  return 0;
	}
      kp->name = SaveStr(class);
      for (i = 0; i < (int)(sizeof(kp->ktab)/sizeof(*kp->ktab)); i++)
	{
	  kp->ktab[i].nr = RC_ILLEGAL;
	  kp->ktab[i].args = noargs;
	  kp->ktab[i].argl = 0;
	  kp->ktab[i].quiet = 0;
	}
      kp->next = 0;
      *kpp = kp;
    }
  return kp->ktab;
}

static void
ClearAction(struct action *act)
{
  char **p;

  if (act->nr == RC_ILLEGAL)
    return;
  act->nr = RC_ILLEGAL;
  if (act->args == noargs)
    return;
  for (p = act->args; *p; p++)
    free(*p);
  free((char *)act->args);
  act->args = noargs;
  act->argl = 0;
}

/*
 * ProcessInput: process input from display and feed it into
 * the layer on canvas D_forecv.
 */

#ifdef MAPKEYS

/*
 *  This ProcessInput just does the keybindings and passes
 *  everything else on to ProcessInput2.
 */

void
ProcessInput(char *ibuf, int ilen)
{
  int ch, slen;
  unsigned char *s, *q;
  int i, l;
  char *p;

  debug1("ProcessInput: %d bytes\n", ilen);
  if (display == 0 || ilen == 0)
    return;
  if (D_seql)
    evdeq(&D_mapev);
  slen = ilen;
  s = (unsigned char *)ibuf;
  while (ilen-- > 0)
    {
      ch = *s++;
      if (D_dontmap || !D_nseqs)
	{
          D_dontmap = 0;
	  continue;
	}
      for (;;)
	{
	  debug3("cmp %c %c[%d]\n", ch, *D_seqp, D_seqp - D_kmaps);
	  if (*D_seqp != ch)
	    {
	      l = D_seqp[D_seqp[-D_seql-1] + 1];
	      if (l)
		{
		  D_seqp += l * 2 + 4;
		  debug1("miss %d\n", D_seqp - D_kmaps);
		  continue;
		}
	      debug("complete miss\n");
	      D_mapdefault = 0;
	      l = D_seql;
	      p = (char *)D_seqp - l;
	      D_seql = 0;
	      D_seqp = D_kmaps + 3;
	      if (l == 0)
		break;
	      if ((q = D_seqh) != 0)
		{
		  D_seqh = 0;
		  i = q[0] << 8 | q[1];
		  i &= ~KMAP_NOTIMEOUT;
		  debug1("Mapping former hit #%d - ", i);
		  debug2("%d(%s) - ", q[2], q + 3);
		  if (StuffKey(i))
		    ProcessInput2((char *)q + 3, q[2]);
		  if (display == 0)
		    return;
		  l -= q[2];
		  p += q[2];
		}
	      else
	        D_dontmap = 1;
	      debug1("flush old %d\n", l);
	      ProcessInput(p, l);
	      if (display == 0)
		return;
	      evdeq(&D_mapev);
	      continue;
	    }
	  if (D_seql++ == 0)
	    {
	      /* Finish old stuff */
	      slen -= ilen + 1;
	      debug1("finish old %d\n", slen);
	      if (slen)
	        ProcessInput2(ibuf, slen);
	      if (display == 0)
		return;
	      D_seqh = 0;
	    }
	  ibuf = (char *)s;
	  slen = ilen;
	  D_seqp++;
	  l = D_seql;
	  debug2("length am %d, want %d\n", l, D_seqp[-l - 1]);
	  if (l == D_seqp[-l - 1])
	    {
	      if (D_seqp[l] != l)
		{
		  q = D_seqp + 1 + l;
		  if (D_kmaps + D_nseqs > q && q[2] > l && !bcmp(D_seqp - l, q + 3, l))
		    {
		      debug1("have another mapping (%s), delay execution\n", q + 3);
		      D_seqh = D_seqp - 3 - l;
		      D_seqp = q + 3 + l;
		      break;
		    }
		}
	      i = D_seqp[-l - 3] << 8 | D_seqp[-l - 2];
	      i &= ~KMAP_NOTIMEOUT;
	      debug1("Mapping #%d - ", i);
	      p = (char *)D_seqp - l;
	      debug2("%d(%s) - ", l, p);
	      D_seql = 0;
	      D_seqp = D_kmaps + 3;
	      D_seqh = 0;
	      if (StuffKey(i))
		ProcessInput2(p, l);
	      if (display == 0)
		return;
	    }
	  break;
	}
    }
  if (D_seql)
    {
      debug("am in sequence -> check for timeout\n");
      l = D_seql;
      for (s = D_seqp; ; s += i * 2 + 4)
	{
	  if (s[-l-3] & KMAP_NOTIMEOUT >> 8)
	    break;
	  if ((i = s[s[-l-1] + 1]) == 0)
	    {
	      SetTimeout(&D_mapev, maptimeout);
	      evenq(&D_mapev);
	      break;
	    }
	}
    }
  ProcessInput2(ibuf, slen);
}

#else
# define ProcessInput2 ProcessInput
#endif


/*
 *  Here only the screen escape commands are handled.
 */

void
ProcessInput2(char *ibuf, int ilen)
{
  char *s;
  int ch, slen;
  struct action *ktabp;

  debug1("ProcessInput2: %d bytes\n", ilen);
  while (ilen && display)
    {
      debug1(" - ilen now %d bytes\n", ilen);
      flayer = D_forecv->c_layer;
      fore = D_fore;
      slen = ilen;
      s = ibuf;
      if (!D_ESCseen)
	{
	  while (ilen > 0)
	    {
	      if ((unsigned char)*s++ == D_user->u_Esc)
		break;
	      ilen--;
	    }
	  slen -= ilen;
	  if (slen)
	    DoProcess(fore, &ibuf, &slen, 0);
	  if (--ilen == 0)
	    {
	      D_ESCseen = ktab;
	      WindowChanged(fore, 'E');
	    }
	}
      if (ilen <= 0)
        return;
      ktabp = D_ESCseen ? D_ESCseen : ktab;
      if (D_ESCseen)
        {
          D_ESCseen = 0;
          WindowChanged(fore, 'E');
        }
      ch = (unsigned char)*s;

      /* 
       * As users have different esc characters, but a common ktab[],
       * we fold back the users esc and meta-esc key to the Default keys
       * that can be looked up in the ktab[]. grmbl. jw.
       * XXX: make ktab[] a per user thing.
       */
      if (ch == D_user->u_Esc) 
        ch = DefaultEsc;
      else if (ch == D_user->u_MetaEsc) 
        ch = DefaultMetaEsc;

      if (ch >= 0)
        DoAction(&ktabp[ch], ch);
      ibuf = (char *)(s + 1);
      ilen--;
    }
}

void
DoProcess(struct win *p, char **bufp, int *lenp, struct paster *pa)
{
  int oldlen;
  struct display *d = display;

#ifdef COPY_PASTE
  /* XXX -> PasteStart */
  if (pa && *lenp > 1 && p && p->w_slowpaste)
    {
      /* schedule slowpaste event */
      SetTimeout(&p->w_paster.pa_slowev, p->w_slowpaste);
      evenq(&p->w_paster.pa_slowev);
      return;
    }
#endif
  while (flayer && *lenp)
    {
#ifdef COPY_PASTE
      if (!pa && p && p->w_paster.pa_pastelen && flayer == p->w_paster.pa_pastelayer)
	{
	  debug("layer is busy - beep!\n");
	  WBell(p, visual_bell);
	  *bufp += *lenp;
	  *lenp = 0;
	  display = d;
	  return;
	}
#endif
      oldlen = *lenp;
      LayProcess(bufp, lenp);
#ifdef COPY_PASTE
      if (pa && !pa->pa_pastelayer)
	break;		/* flush rest of paste */
#endif
      if (*lenp == oldlen)
	{
	  if (pa)
	    {
	      display = d;
	      return;
	    }
	  /* We're full, let's beep */
	  debug("layer is full - beep!\n");
	  WBell(p, visual_bell);
	  break;
	}
    }
  *bufp += *lenp;
  *lenp = 0;
  display = d;
#ifdef COPY_PASTE
  if (pa && pa->pa_pastelen == 0)
    FreePaster(pa);
#endif
}

int
FindCommnr(const char *str)
{
  int x, m, l = 0, r = RC_LAST;
  while (l <= r)
    {
      m = (l + r) / 2;
      x = strcmp(str, comms[m].name);
      if (x > 0)
	l = m + 1;
      else if (x < 0)
	r = m - 1;
      else
	return m;
    }
  return RC_ILLEGAL;
}

static int
CheckArgNum(int nr, char **args)
{
  int i, n;
  static char *argss[] = {"no", "one", "two", "three", "four", "OOPS"};
  static char *orformat[] = 
    {
      "%s: %s: %s argument%s required",
      "%s: %s: %s or %s argument%s required",
      "%s: %s: %s, %s or %s argument%s required",
      "%s: %s: %s, %s, %s or %s argument%s required"
    };

  n = comms[nr].flags & ARGS_MASK;
  for (i = 0; args[i]; i++)
    ;
  if (comms[nr].flags & ARGS_ORMORE)
    {
      if (i < n)
	{
	  Msg(0, "%s: %s: at least %s argument%s required", 
	      rc_name, comms[nr].name, argss[n], n != 1 ? "s" : "");
	  return -1;
	}
    }
  else if ((comms[nr].flags & ARGS_PLUS1) && 
           (comms[nr].flags & ARGS_PLUS2) &&
	   (comms[nr].flags & ARGS_PLUS3))
    {
      if (i != n && i != n + 1 && i != n + 2 && i != n + 3)
        {
	  Msg(0, orformat[3], rc_name, comms[nr].name, argss[n], 
	      argss[n + 1], argss[n + 2], argss[n + 3], "");
	  return -1;
	}
    }
  else if ((comms[nr].flags & ARGS_PLUS1) &&
           (comms[nr].flags & ARGS_PLUS2))
    {
      if (i != n && i != n + 1 && i != n + 2)
	{
	  Msg(0, orformat[2], rc_name, comms[nr].name, argss[n], 
	      argss[n + 1], argss[n + 2], "");
          return -1;
	}
    }
  else if ((comms[nr].flags & ARGS_PLUS1) &&
           (comms[nr].flags & ARGS_PLUS3))
    {
      if (i != n && i != n + 1 && i != n + 3)
        {
	  Msg(0, orformat[2], rc_name, comms[nr].name, argss[n], 
	      argss[n + 1], argss[n + 3], "");
	  return -1;
	}
    }
  else if ((comms[nr].flags & ARGS_PLUS2) &&
           (comms[nr].flags & ARGS_PLUS3))
    {
      if (i != n && i != n + 2 && i != n + 3)
        {
	  Msg(0, orformat[2], rc_name, comms[nr].name, argss[n], 
	      argss[n + 2], argss[n + 3], "");
	  return -1;
	}
    }
  else if (comms[nr].flags & ARGS_PLUS1)
    {
      if (i != n && i != n + 1)
        {
	  Msg(0, orformat[1], rc_name, comms[nr].name, argss[n], 
	      argss[n + 1], n != 0 ? "s" : "");
	  return -1;
	}
    }
  else if (comms[nr].flags & ARGS_PLUS2)
    {
      if (i != n && i != n + 2)
        {
	  Msg(0, orformat[1], rc_name, comms[nr].name, argss[n], 
	      argss[n + 2], "s");
	  return -1;
	}
    }
  else if (comms[nr].flags & ARGS_PLUS3)
    {
      if (i != n && i != n + 3)
        {
	  Msg(0, orformat[1], rc_name, comms[nr].name, argss[n], 
	      argss[n + 3], "");
	  return -1;
	}
    }
  else if (i != n)
    {
      Msg(0, orformat[0], rc_name, comms[nr].name, argss[n], n != 1 ? "s" : "");
      return -1;
    }
  return i;
}

static void
StuffFin(char *buf, int len, char *data)
{
  if (!flayer)
    return;
  while(len)
    LayProcess(&buf, &len);
}

/* If the command is not 'quieted', then use Msg to output the message. If it's a remote
 * query, then Msg takes care of also outputting the message to the querying client.
 *
 * If we want the command to be quiet, and it's a remote query, then use QueryMsg so that
 * the response does go back to the querying client.
 *
 * If the command is quieted, and it's not a remote query, then just don't print the message.
 */
#define OutputMsg	(!act->quiet ? Msg : queryflag >= 0 ? QueryMsg : Dummy)

/*ARGSUSED*/
void
DoAction(struct action *act, int key)
{
  int nr = act->nr;
  char **args = act->args;
  int *argl = act->argl;
  struct win *p;
  int argc, i, n, msgok;
  char *s;
  char ch;
  struct display *odisplay = display;
  struct acluser *user;

  user = display ? D_user : users;
  if (nr == RC_ILLEGAL)
    {
      debug1("key '%c': No action\n", key);
      return;
    }
  n = comms[nr].flags;
  /* Commands will have a CAN_QUERY flag, depending on whether they have
   * something to return on a query. For example, 'windows' can return a result,
   * but 'other' cannot.
   * If some command causes an error, then it should reset queryflag to -1, so that
   * the process requesting the query can be notified that an error happened.
   */
  if (!(n & CAN_QUERY) && queryflag >= 0)
    {
      /* Query flag is set, but this command cannot be queried. */
      OutputMsg(0, "%s command cannot be queried.", comms[nr].name);
      queryflag = -1;
      return;
    }
  if ((n & NEED_DISPLAY) && display == 0)
    {
      OutputMsg(0, "%s: %s: display required", rc_name, comms[nr].name);
      queryflag = -1;
      return;
    }
  if ((n & NEED_FORE) && fore == 0)
    {
      OutputMsg(0, "%s: %s: window required", rc_name, comms[nr].name);
      queryflag = -1;
      return;
    }
  if ((n & NEED_LAYER) && flayer == 0)
    {
      OutputMsg(0, "%s: %s: display or window required", rc_name, comms[nr].name);
      queryflag = -1;
      return;
    }
  if ((argc = CheckArgNum(nr, args)) < 0)
    return;
#ifdef MULTIUSER
  if (display)
    {
      if (AclCheckPermCmd(D_user, ACL_EXEC, &comms[nr]))
        {
	  OutputMsg(0, "%s: %s: permission denied (user %s)", 
	      rc_name, comms[nr].name, (EffectiveAclUser ? EffectiveAclUser : D_user)->u_name);
	  queryflag = -1;
	  return;
	}
    }
#endif /* MULTIUSER */

  msgok = display && !*rc_name;
  switch(nr)
    {
    case RC_SELECT:
      if (!*args)
        InputSelect();
      else if (args[0][0] == '-' && !args[0][1])
	{
	  SetForeWindow((struct win *)0);
	  Activate(0);
	}
      else if (args[0][0] == '.' && !args[0][1])
	{
	  if (!fore)
	    {
	      OutputMsg(0, "select . needs a window");
	      queryflag = -1;
	    }
	  else
	    {
	      SetForeWindow(fore);
	      Activate(0);
	    }
	}
      else if (ParseWinNum(act, &n) == 0)
        SwitchWindow(n);
      else if (queryflag >= 0)
	queryflag = -1;	/* ParseWinNum already prints out an appropriate error message. */
      break;
#ifdef AUTO_NUKE
    case RC_DEFAUTONUKE:
      if (ParseOnOff(act, &defautonuke) == 0 && msgok)
	OutputMsg(0, "Default autonuke turned %s", defautonuke ? "on" : "off");
      if (display && *rc_name)
	D_auto_nuke = defautonuke;
      break;
    case RC_AUTONUKE:
      if (ParseOnOff(act, &D_auto_nuke) == 0 && msgok)
	OutputMsg(0, "Autonuke turned %s", D_auto_nuke ? "on" : "off");
      break;
#endif
    case RC_DEFOBUFLIMIT:
      if (ParseNum(act, &defobuflimit) == 0 && msgok)
	OutputMsg(0, "Default limit set to %d", defobuflimit);
      if (display && *rc_name)
	{
	  D_obufmax = defobuflimit;
	  D_obuflenmax = D_obuflen - D_obufmax;
	}
      break;
    case RC_OBUFLIMIT:
      if (*args == 0)
	OutputMsg(0, "Limit is %d, current buffer size is %d", D_obufmax, D_obuflen);
      else if (ParseNum(act, &D_obufmax) == 0 && msgok)
	OutputMsg(0, "Limit set to %d", D_obufmax);
      D_obuflenmax = D_obuflen - D_obufmax;
      break;
    case RC_DUMPTERMCAP:
      WriteFile(user, (char *)0, DUMP_TERMCAP);
      break;
    case RC_HARDCOPY:
      {
	int mode = DUMP_HARDCOPY;
	char *file = NULL;

	if (args[0])
	  {
	    if (!strcmp(*args, "-h"))
	      {
		mode = DUMP_SCROLLBACK;
		file = args[1];
	      }
	    else if (!strcmp(*args, "--") && args[1])
	      file = args[1];
	    else
	      file = args[0];
	  }

	if (args[0] && file == args[0] && args[1])
	  {
	    OutputMsg(0, "%s: hardcopy: too many arguments", rc_name);
	    break;
	  }
	WriteFile(user, file, mode);
      }
      break;
    case RC_DEFLOG:
      (void)ParseOnOff(act, &nwin_default.Lflag);
      break;
    case RC_LOG:
      n = fore->w_log ? 1 : 0;
      ParseSwitch(act, &n);
      LogToggle(n);
      break;
#ifdef BSDJOBS
    case RC_SUSPEND:
      Detach(D_STOP);
      break;
#endif
    case RC_NEXT:
      if (MoreWindows())
	SwitchWindow(NextWindow());
      break;
    case RC_PREV:
      if (MoreWindows())
	SwitchWindow(PreviousWindow());
      break;
    case RC_KILL:
      {
	char *name;

	if (key >= 0)
	  {
#ifdef PSEUDOS
	    Input(fore->w_pwin ? "Really kill this filter [y/n]" : "Really kill this window [y/n]", 1, INP_RAW, confirm_fn, NULL, RC_KILL);
#else
	    Input("Really kill this window [y/n]", 1, INP_RAW, confirm_fn, NULL, RC_KILL);
#endif
	    break;
	  }
	n = fore->w_number;
#ifdef PSEUDOS
	if (fore->w_pwin)
	  {
	    FreePseudowin(fore);
	    OutputMsg(0, "Filter removed.");
	    break;
	  }
#endif
	name = SaveStr(fore->w_title);
	KillWindow(fore);
	OutputMsg(0, "Window %d (%s) killed.", n, name);
	if (name)
	  free(name);
	break;
      }
    case RC_QUIT:
      if (key >= 0)
	{
	  Input("Really quit and kill all your windows [y/n]", 1, INP_RAW, confirm_fn, NULL, RC_QUIT);
	  break;
	}
      Finit(0);
      /* NOTREACHED */
#ifdef DETACH
    case RC_DETACH:
      if (*args && !strcmp(*args, "-h"))
        Hangup();
      else
        Detach(D_DETACH);
      break;
# ifdef POW_DETACH
    case RC_POW_DETACH:
      if (key >= 0)
	{
	  static char buf[2];

	  buf[0] = key;
	  Input(buf, 1, INP_RAW, pow_detach_fn, NULL, 0);
	}
      else
        Detach(D_POWER); /* detach and kill Attacher's parent */
      break;
# endif
#endif
    case RC_DEBUG:
#ifdef DEBUG
      if (!*args)
        {
	  if (dfp)
	    OutputMsg(0, "debugging info is written to %s/", DEBUGDIR);
	  else
	    OutputMsg(0, "debugging is currently off. Use 'debug on' to enable.");
	  break;
	}
      if (dfp)
        {
	  debug("debug: closing debug file.\n");
	  fflush(dfp);
	  fclose(dfp);
	  dfp = NULL;
	}
      if (strcmp("off", *args))
        opendebug(0, 1);
# ifdef SIG_NODEBUG
      else if (display)
        kill(D_userpid, SIG_NODEBUG);	/* a one shot item, but hey... */
# endif /* SIG_NODEBUG */
#else
      if (*args == 0 || strcmp("off", *args))
        OutputMsg(0, "Sorry, screen was compiled without -DDEBUG option.");
#endif
      break;
#ifdef ZMODEM
    case RC_ZMODEM:
      if (*args && !strcmp(*args, "sendcmd"))
	{
	  if (args[1])
	    {
	      free(zmodem_sendcmd);
	      zmodem_sendcmd = SaveStr(args[1]);
	    }
	  if (msgok)
	    OutputMsg(0, "zmodem sendcmd: %s", zmodem_sendcmd);
	  break;
	}
      if (*args && !strcmp(*args, "recvcmd"))
	{
	  if (args[1])
	    {
	      free(zmodem_recvcmd);
	      zmodem_recvcmd = SaveStr(args[1]);
	    }
	  if (msgok)
	    OutputMsg(0, "zmodem recvcmd: %s", zmodem_recvcmd);
	  break;
	}
      if (*args)
	{
	  for (i = 0; i < 4; i++)
	    if (!strcmp(zmodes[i], *args))
	      break;
	  if (i == 4 && !strcmp(*args, "on"))
	    i = 1;
	  if (i == 4)
	    {
	      OutputMsg(0, "usage: zmodem off|auto|catch|pass");
	      break;
	    }
	  zmodem_mode = i;
	}
      if (msgok)
	OutputMsg(0, "zmodem mode is %s", zmodes[zmodem_mode]);
      break;
#endif
    case RC_UNBINDALL:
      {
        register unsigned int i;

        for (i = 0; i < sizeof(ktab)/sizeof(*ktab); i++)
	  ClearAction(&ktab[i]);
        OutputMsg(0, "Unbound all keys." );
        break;
      }
    case RC_ZOMBIE:
      {
        if (!(s = *args))
          {
            ZombieKey_destroy = 0;
            break;
          }
	if (*argl == 0 || *argl > 2)
	  {
	    OutputMsg(0, "%s:zombie: one or two characters expected.", rc_name);
	    break;
	  }
	if (args[1])
	  {
	    if (!strcmp(args[1], "onerror"))
	      {
		ZombieKey_onerror = 1;
	      } else {
		OutputMsg(0, "usage: zombie [keys [onerror]]");
	    	break;
	      }
	  } else
	    ZombieKey_onerror = 0;
        ZombieKey_destroy = args[0][0];
        ZombieKey_resurrect = *argl == 2 ? args[0][1] : 0;
      }
      break;
    case RC_WALL:
#ifdef MULTIUSER
      s = D_user->u_name;
#else
      s = D_usertty;
#endif
        {
	  struct display *olddisplay = display;
          display = 0;		/* no display will cause a broadcast */
          OutputMsg(0, "%s: %s", s, *args);
	  display = olddisplay;
        }
      break;
    case RC_AT:
      /* where this AT command comes from: */
      if (!user)
	break;
#ifdef MULTIUSER
      s = SaveStr(user->u_name);
      /* DO NOT RETURN FROM HERE WITHOUT RESETTING THIS: */
      EffectiveAclUser = user;
#else
      s = SaveStr(display ? D_usertty : user->u_name);
#endif
      n = strlen(args[0]);
      if (n) n--;
      /*
       * the windows/displays loops are quite dangerous here, take extra
       * care not to trigger landmines. Things may appear/disappear while
       * we are walking along.
       */
      switch (args[0][n])
        {
	case '*':		/* user */
	  {
	    struct display *nd;
	    struct acluser *u;

	    if (!n)
	      u = user;
	    else
	      {
		for (u = users; u; u = u->u_next)
		  {
		    debug3("strncmp('%s', '%s', %d)\n", *args, u->u_name, n);
		    if (!strncmp(*args, u->u_name, n))
		      break;
		  }
		if (!u)
		  {
		    args[0][n] = '\0';
		    OutputMsg(0, "Did not find any user matching '%s'", args[0]);
		    break;
		  }
	      }
	    debug1("at all displays of user %s\n", u->u_name);
	    for (display = displays; display; display = nd)
	      {
		nd = display->d_next;
		if (D_forecv == 0)
		  continue;
		flayer = D_forecv->c_layer;
		fore = D_fore;
	        if (D_user != u)
		  continue;
		debug1("AT display %s\n", D_usertty);
		DoCommand(args + 1, argl + 1);
		if (display)
		  OutputMsg(0, "command from %s: %s %s", 
		      s, args[1], args[2] ? args[2] : "");
		display = NULL;
		flayer = 0;
		fore = NULL;
	      }
	    break;
	  }
	case '%':		/* display */
	  {
	    struct display *nd;

	    debug1("at display matching '%s'\n", args[0]);
	    for (display = displays; display; display = nd)
	      {
	        nd = display->d_next;
		if (D_forecv == 0)
		  continue;
		fore = D_fore;
		flayer = D_forecv->c_layer;
	        if (strncmp(args[0], D_usertty, n) && 
		    (strncmp("/dev/", D_usertty, 5) || 
		     strncmp(args[0], D_usertty + 5, n)) &&
		    (strncmp("/dev/tty", D_usertty, 8) ||
		     strncmp(args[0], D_usertty + 8, n)))
		  continue;
		debug1("AT display %s\n", D_usertty);
		DoCommand(args + 1, argl + 1);
		if (display)
		  OutputMsg(0, "command from %s: %s %s", 
		      s, args[1], args[2] ? args[2] : "");
		display = NULL;
		fore = NULL;
		flayer = 0;
	      }
	    break;
	  }
	case '#':		/* window */
	  n--;
	  /* FALLTHROUGH */
	default:
	  {
	    struct win *nw;
	    int ch;

	    n++;
	    ch = args[0][n];
	    args[0][n] = '\0';
	    if (!*args[0] || (i = WindowByNumber(args[0])) < 0)
	      {
	        args[0][n] = ch;      /* must restore string in case of bind */
	        /* try looping over titles */
		for (fore = windows; fore; fore = nw)
		  {
		    nw = fore->w_next;
		    if (strncmp(args[0], fore->w_title, n))
		      continue;
		    debug2("AT window %d(%s)\n", fore->w_number, fore->w_title);
		    /*
		     * consider this a bug or a feature: 
		     * while looping through windows, we have fore AND
		     * display context. This will confuse users who try to 
		     * set up loops inside of loops, but often allows to do 
		     * what you mean, even when you adress your context wrong.
		     */
		    i = 0;
		    /* XXX: other displays? */
		    if (fore->w_layer.l_cvlist)
		      display = fore->w_layer.l_cvlist->c_display;
		    flayer = fore->w_savelayer ? fore->w_savelayer : &fore->w_layer;
		    DoCommand(args + 1, argl + 1);	/* may destroy our display */
		    if (fore && fore->w_layer.l_cvlist)
		      {
		        display = fore->w_layer.l_cvlist->c_display;
		        OutputMsg(0, "command from %s: %s %s", 
			    s, args[1], args[2] ? args[2] : "");
		      }
		  }
		display = NULL;
		fore = NULL;
		if (i < 0)
		  OutputMsg(0, "%s: at '%s': no such window.\n", rc_name, args[0]);
		break;
	      }
	    else if (i < maxwin && (fore = wtab[i]))
	      {
	        args[0][n] = ch;      /* must restore string in case of bind */
	        debug2("AT window %d (%s)\n", fore->w_number, fore->w_title);
		if (fore->w_layer.l_cvlist)
		  display = fore->w_layer.l_cvlist->c_display;
		flayer = fore->w_savelayer ? fore->w_savelayer : &fore->w_layer;
		DoCommand(args + 1, argl + 1);
		if (fore && fore->w_layer.l_cvlist)
		  {
		    display = fore->w_layer.l_cvlist->c_display;
		    OutputMsg(0, "command from %s: %s %s", 
		        s, args[1], args[2] ? args[2] : "");
		  }
		display = NULL;
		fore = NULL;
	      }
	    else
	      OutputMsg(0, "%s: at [identifier][%%|*|#] command [args]", rc_name);
	    break;
	  }
	}
      free(s);
#ifdef MULTIUSER
      EffectiveAclUser = NULL;
#endif
      break;

#ifdef COPY_PASTE
    case RC_READREG:
#ifdef ENCODINGS
      i = fore ? fore->w_encoding : display ? display->d_encoding : 0;
      if (args[0] && args[1] && !strcmp(args[0], "-e"))
	{
	  i = FindEncoding(args[1]);
	  if (i == -1)
	    {
	      OutputMsg(0, "%s: readreg: unknown encoding", rc_name);
	      break;
	    }
	  args += 2;
	}
#endif
      /* 
       * Without arguments we prompt for a destination register.
       * It will receive the copybuffer contents.
       * This is not done by RC_PASTE, as we prompt for source
       * (not dest) there.
       */
      if ((s = *args) == NULL)
	{
	  Input("Copy to register:", 1, INP_RAW, copy_reg_fn, NULL, 0);
	  break;
	}
      if (*argl != 1)
	{
	  OutputMsg(0, "%s: copyreg: character, ^x, or (octal) \\032 expected.", rc_name);
	  break;
	}
      ch = args[0][0];
      /* 
       * With two arguments we *really* read register contents from file
       */
      if (args[1])
        {
	  if (args[2])
	    {
	      OutputMsg(0, "%s: readreg: too many arguments", rc_name);
	      break;
	    }
	  if ((s = ReadFile(args[1], &n)))
	    {
	      struct plop *pp = plop_tab + (int)(unsigned char)ch;

	      if (pp->buf)
		free(pp->buf);
	      pp->buf = s;
	      pp->len = n;
#ifdef ENCODINGS
	      pp->enc = i;
#endif
	    }
	}
      else
        /*
	 * with one argument we copy the copybuffer into a specified register
	 * This could be done with RC_PASTE too, but is here to be consistent
	 * with the zero argument call.
	 */
        copy_reg_fn(&ch, 0, NULL);
      break;
#endif
    case RC_REGISTER:
#ifdef ENCODINGS
      i = fore ? fore->w_encoding : display ? display->d_encoding : 0;
      if (args[0] && args[1] && !strcmp(args[0], "-e"))
	{
	  i = FindEncoding(args[1]);
	  if (i == -1)
	    {
	      OutputMsg(0, "%s: register: unknown encoding", rc_name);
	      break;
	    }
	  args += 2;
	  argc -= 2;
	}
#endif
      if (argc != 2)
	{
	  OutputMsg(0, "%s: register: illegal number of arguments.", rc_name);
	  break;
	}
      if (*argl != 1)
	{
	  OutputMsg(0, "%s: register: character, ^x, or (octal) \\032 expected.", rc_name);
	  break;
	}
      ch = args[0][0];
#ifdef COPY_PASTE
      if (ch == '.')
	{
	  if (user->u_plop.buf != NULL)
	    UserFreeCopyBuffer(user);
	  if (args[1] && args[1][0])
	    {
	      user->u_plop.buf = SaveStrn(args[1], argl[1]);
	      user->u_plop.len = argl[1];
#ifdef ENCODINGS
	      user->u_plop.enc = i;
#endif
	    }
	}
      else
#endif
	{
	  struct plop *plp = plop_tab + (int)(unsigned char)ch;

	  if (plp->buf)
	    free(plp->buf);
	  plp->buf = SaveStrn(args[1], argl[1]);
	  plp->len = argl[1];
#ifdef ENCODINGS
	  plp->enc = i;
#endif
	}
      break;
    case RC_PROCESS:
      if ((s = *args) == NULL)
	{
	  Input("Process register:", 1, INP_RAW, process_fn, NULL, 0);
	  break;
	}
      if (*argl != 1)
	{
	  OutputMsg(0, "%s: process: character, ^x, or (octal) \\032 expected.", rc_name);
	  break;
	}
      ch = args[0][0];
      process_fn(&ch, 0, NULL);
      break;
    case RC_STUFF:
      s = *args;
      if (!args[0])
	{
	  Input("Stuff:", 100, INP_COOKED, StuffFin, NULL, 0);
	  break;
	}
      n = *argl;
      if (args[1])
	{
	  if (strcmp(s, "-k"))
	    {
	      OutputMsg(0, "%s: stuff: invalid option %s", rc_name, s);
	      break;
	    }
	  s = args[1];
	  for (i = T_CAPS; i < T_OCAPS; i++)
	    if (strcmp(term[i].tcname, s) == 0)
	      break;
	  if (i == T_OCAPS)
	    {
	      OutputMsg(0, "%s: stuff: unknown key '%s'", rc_name, s);
	      break;
	    }
#ifdef MAPKEYS
	  if (StuffKey(i - T_CAPS) == 0)
	    break;
#endif
	  s = display ? D_tcs[i].str : 0;
	  if (s == 0)
	    break;
	  n = strlen(s);
	}
      while(n)
        LayProcess(&s, &n);
      break;
    case RC_REDISPLAY:
      Activate(-1);
      break;
    case RC_WINDOWS:
      ShowWindows(-1);
      break;
    case RC_VERSION:
      OutputMsg(0, "screen %s", version);
      break;
    case RC_TIME:
      if (*args)
	{
	  timestring = SaveStr(*args);
	  break;
	}
      OutputMsg(0, "%s", MakeWinMsg(timestring, fore, '%'));
      break;
    case RC_INFO:
      ShowInfo();
      break;
    case RC_DINFO:
      ShowDInfo();
      break;
    case RC_COMMAND:
	{
	  struct action *ktabp = ktab;
	  if (argc == 2 && !strcmp(*args, "-c"))
	    {
	      if ((ktabp = FindKtab(args[1], 0)) == 0)
		{
		  OutputMsg(0, "Unknown command class '%s'", args[1]);
		  break;
		}
	    }
	  if (D_ESCseen != ktab || ktabp != ktab)
	    {
	      if (D_ESCseen != ktabp)
	        {
	          D_ESCseen = ktabp;
	          WindowChanged(fore, 'E');
	        }
	      break;
	    }
	  if (D_ESCseen)
	    {
	      D_ESCseen = 0;
	      WindowChanged(fore, 'E');
	    }
	}
      /* FALLTHROUGH */
    case RC_OTHER:
      if (MoreWindows())
	SwitchWindow(display && D_other ? D_other->w_number : NextWindow());
      break;
    case RC_META:
      if (user->u_Esc == -1)
        break;
      ch = user->u_Esc;
      s = &ch;
      n = 1;
      LayProcess(&s, &n);
      break;
    case RC_XON:
      ch = Ctrl('q');
      s = &ch;
      n = 1;
      LayProcess(&s, &n);
      break;
    case RC_XOFF:
      ch = Ctrl('s');
      s = &ch;
      n = 1;
      LayProcess(&s, &n);
      break;
    case RC_DEFBREAKTYPE:
    case RC_BREAKTYPE:
	{
	  static char *types[] = { "TIOCSBRK", "TCSBRK", "tcsendbreak", NULL };
	  extern int breaktype;

	  if (*args)
	    {
	      if (ParseNum(act, &n))
		for (n = 0; n < (int)(sizeof(types)/sizeof(*types)); n++)
		  {
		    for (i = 0; i < 4; i++)
		      {
			ch = args[0][i];
			if (ch >= 'a' && ch <= 'z')
			  ch -= 'a' - 'A';
			if (ch != types[n][i] && (ch + ('a' - 'A')) != types[n][i])
			  break;
		      }
		    if (i == 4)
		      break;
		  }
	      if (n < 0 || n >= (int)(sizeof(types)/sizeof(*types)))
	        OutputMsg(0, "%s invalid, chose one of %s, %s or %s", *args, types[0], types[1], types[2]);
	      else
	        {
		  breaktype = n;
	          OutputMsg(0, "breaktype set to (%d) %s", n, types[n]);
		}
	    }
	  else
	    OutputMsg(0, "breaktype is (%d) %s", breaktype, types[breaktype]);
	}
      break;
    case RC_POW_BREAK:
    case RC_BREAK:
      n = 0;
      if (*args && ParseNum(act, &n))
	break;
      SendBreak(fore, n, nr == RC_POW_BREAK);
      break;
#ifdef LOCK
    case RC_LOCKSCREEN:
      Detach(D_LOCK);
      break;
#endif
    case RC_WIDTH:
    case RC_HEIGHT:
      {
	int w, h;
	int what = 0;
	
        i = 1;
	if (*args && !strcmp(*args, "-w"))
	  what = 1;
	else if (*args && !strcmp(*args, "-d"))
	  what = 2;
	if (what)
	  args++;
	if (what == 0 && flayer && !display)
	  what = 1;
	if (what == 1)
	  {
	    if (!flayer)
	      {
		OutputMsg(0, "%s: %s: window required", rc_name, comms[nr].name);
		break;
	      }
	    w = flayer->l_width;
	    h = flayer->l_height;
	  }
	else
	  {
	    if (!display)
	      {
		OutputMsg(0, "%s: %s: display required", rc_name, comms[nr].name);
		break;
	      }
	    w = D_width;
	    h = D_height;
	  }
        if (*args && args[0][0] == '-')
	  {
	    OutputMsg(0, "%s: %s: unknown option %s", rc_name, comms[nr].name, *args);
	    break;
	  }
	if (nr == RC_HEIGHT)
	  {
	    if (!*args)
	      {
#define H0height 42
#define H1height 24
		if (h == H0height)
		  h = H1height;
		else if (h == H1height)
		  h = H0height;
		else if (h > (H0height + H1height) / 2)
		  h = H0height;
		else
		  h = H1height;
	      }
	    else
	      {
		h = atoi(*args);
		if (args[1])
		  w = atoi(args[1]);
	      }
	  }
	else
	  {
	    if (!*args)
	      {
		if (w == Z0width)
		  w = Z1width;
		else if (w == Z1width)
		  w = Z0width;
		else if (w > (Z0width + Z1width) / 2)
		  w = Z0width;
		else
		  w = Z1width;
	      }
	    else
	      {
		w = atoi(*args);
		if (args[1])
		  h = atoi(args[1]);
	      }
	  }
        if (*args && args[1] && args[2])
	  {
	    OutputMsg(0, "%s: %s: too many arguments", rc_name, comms[nr].name);
	    break;
	  }
	if (w <= 0)
	  {
	    OutputMsg(0, "Illegal width");
	    break;
	  }
	if (h <= 0)
	  {
	    OutputMsg(0, "Illegal height");
	    break;
	  }
	if (what == 1)
	  {
	    if (flayer->l_width == w && flayer->l_height == h)
	      break;
	    ResizeLayer(flayer, w, h, (struct display *)0);
	    break;
	  }
	if (D_width == w && D_height == h)
	  break;
	if (what == 2)
	  {
	    ChangeScreenSize(w, h, 1);
	  }
	else
	  {
	    if (ResizeDisplay(w, h) == 0)
	      {
		Activate(D_fore ? D_fore->w_norefresh : 0);
		/* autofit */
		ResizeLayer(D_forecv->c_layer, D_forecv->c_xe - D_forecv->c_xs + 1, D_forecv->c_ye - D_forecv->c_ys + 1, 0);
		break;
	      }
	    if (h == D_height)
	      OutputMsg(0, "Your termcap does not specify how to change the terminal's width to %d.", w);
	    else if (w == D_width)
	      OutputMsg(0, "Your termcap does not specify how to change the terminal's height to %d.", h);
	    else
	      OutputMsg(0, "Your termcap does not specify how to change the terminal's resolution to %dx%d.", w, h);
	  }
      }
      break;
    case RC_TITLE:
      if (queryflag >= 0)
	{
	  if (fore)
	    OutputMsg(0, "%s", fore->w_title);
	  else
	    queryflag = -1;
	  break;
	}
      if (*args == 0)
	InputAKA();
      else
	ChangeAKA(fore, *args, strlen(*args));
      break;
    case RC_COLON:
      Input(":", MAXSTR, INP_EVERY, Colonfin, NULL, 0);
      if (*args && **args)
	{
	  s = *args;
	  n = strlen(s);
	  LayProcess(&s, &n);
	}
      break;
    case RC_LASTMSG:
      if (D_status_lastmsg)
	OutputMsg(0, "%s", D_status_lastmsg);
      break;
    case RC_SCREEN:
      DoScreen("key", args);
      break;
    case RC_WRAP:
      if (ParseSwitch(act, &fore->w_wrap) == 0 && msgok)
        OutputMsg(0, "%cwrap", fore->w_wrap ? '+' : '-');
      break;
    case RC_FLOW:
      if (*args)
	{
	  if (args[0][0] == 'a')
	    {
	      fore->w_flow = (fore->w_flow & FLOW_AUTO) ? FLOW_AUTOFLAG |FLOW_AUTO|FLOW_NOW : FLOW_AUTOFLAG;
	    }
	  else
	    {
	      if (ParseOnOff(act, &n))
		break;
	      fore->w_flow = (fore->w_flow & FLOW_AUTO) | n;
	    }
	}
      else
	{
	  if (fore->w_flow & FLOW_AUTOFLAG)
	    fore->w_flow = (fore->w_flow & FLOW_AUTO) | FLOW_NOW;
	  else if (fore->w_flow & FLOW_NOW)
	    fore->w_flow &= ~FLOW_NOW;
	  else
	    fore->w_flow = fore->w_flow ? FLOW_AUTOFLAG|FLOW_AUTO|FLOW_NOW : FLOW_AUTOFLAG;
	}
      SetFlow(fore->w_flow & FLOW_NOW);
      if (msgok)
	OutputMsg(0, "%cflow%s", (fore->w_flow & FLOW_NOW) ? '+' : '-',
	    (fore->w_flow & FLOW_AUTOFLAG) ? "(auto)" : "");
      break;
#ifdef MULTIUSER
    case RC_DEFWRITELOCK:
      if (args[0][0] == 'a')
	nwin_default.wlock = WLOCK_AUTO;
      else
	{
	  if (ParseOnOff(act, &n))
	    break;
	  nwin_default.wlock = n ? WLOCK_ON : WLOCK_OFF;
	}
      break;
    case RC_WRITELOCK:
      if (*args)
	{
	  if (args[0][0] == 'a')
	    {
	      fore->w_wlock = WLOCK_AUTO;
	    }
	  else
	    {
	      if (ParseOnOff(act, &n))
		break;
	      fore->w_wlock = n ? WLOCK_ON : WLOCK_OFF;
	    }
	  /* 
	   * user may have permission to change the writelock setting, 
	   * but he may never aquire the lock himself without write permission
	   */
	  if (!AclCheckPermWin(D_user, ACL_WRITE, fore))
	    fore->w_wlockuser = D_user;
	}
      OutputMsg(0, "writelock %s", (fore->w_wlock == WLOCK_AUTO) ? "auto" :
	  ((fore->w_wlock == WLOCK_OFF) ? "off" : "on"));
      break;
#endif
    case RC_CLEAR:
      ResetAnsiState(fore);
      WriteString(fore, "\033[H\033[J", 6);
      break;
    case RC_RESET:
      ResetAnsiState(fore);
#ifdef ZMODEM
      if (fore->w_zdisplay)
        zmodem_abort(fore, fore->w_zdisplay);
#endif
      WriteString(fore, "\033c", 2);
      break;
    case RC_MONITOR:
      n = fore->w_monitor != MON_OFF;
#ifdef MULTIUSER
      if (display)
	n = n && (ACLBYTE(fore->w_mon_notify, D_user->u_id) & ACLBIT(D_user->u_id));
#endif
      if (ParseSwitch(act, &n))
	break;
      if (n)
	{
#ifdef MULTIUSER
	  if (display)	/* we tell only this user */
	    ACLBYTE(fore->w_mon_notify, D_user->u_id) |= ACLBIT(D_user->u_id);
	  else
	    for (i = 0; i < maxusercount; i++)
	      ACLBYTE(fore->w_mon_notify, i) |= ACLBIT(i);
#endif
	  if (fore->w_monitor == MON_OFF)
	    fore->w_monitor = MON_ON;
	  OutputMsg(0, "Window %d (%s) is now being monitored for all activity.", fore->w_number, fore->w_title);
	}
      else
	{
#ifdef MULTIUSER
	  if (display) /* we remove only this user */
	    ACLBYTE(fore->w_mon_notify, D_user->u_id) 
	      &= ~ACLBIT(D_user->u_id);
	  else
	    for (i = 0; i < maxusercount; i++)
	      ACLBYTE(fore->w_mon_notify, i) &= ~ACLBIT(i);
	  for (i = maxusercount - 1; i >= 0; i--)
	    if (ACLBYTE(fore->w_mon_notify, i))
	      break;
	  if (i < 0)
#endif
	    fore->w_monitor = MON_OFF;
	  OutputMsg(0, "Window %d (%s) is no longer being monitored for activity.", fore->w_number, fore->w_title);
	}
      break;
#ifdef MULTI
    case RC_DISPLAYS:
      display_displays();
      break;
#endif
    case RC_WINDOWLIST:
      if (!*args)
        display_windows(0, WLIST_NUM, (struct win *)0);
      else if (!strcmp(*args, "string"))
	{
	  if (args[1])
	    {
	      if (wliststr)
		free(wliststr);
	      wliststr = SaveStr(args[1]);
	    }
	  if (msgok)
	    OutputMsg(0, "windowlist string is '%s'", wliststr);
	}
      else if (!strcmp(*args, "title"))
	{
	  if (args[1])
	    {
	      if (wlisttit)
		free(wlisttit);
	      wlisttit = SaveStr(args[1]);
	    }
	  if (msgok)
	    OutputMsg(0, "windowlist title is '%s'", wlisttit);
	}
      else
	{
	  int flag = 0;
	  int blank = 0;
	  for (i = 0; i < argc; i++)
	    if (!args[i])
	      continue;
	    else if (!strcmp(args[i], "-m"))
	      flag |= WLIST_MRU;
	    else if (!strcmp(args[i], "-b"))
	      blank = 1;
	    else if (!strcmp(args[i], "-g"))
	      flag |= WLIST_NESTED;
	    else
	      {
		OutputMsg(0, "usage: windowlist [-b] [-g] [-m] [string [string] | title [title]]");
		break;
	      }
	  if (i == argc)
	    display_windows(blank, flag, (struct win *)0);
	}
      break;
    case RC_HELP:
      if (argc == 2 && !strcmp(*args, "-c"))
	{
	  struct action *ktabp;
	  if ((ktabp = FindKtab(args[1], 0)) == 0)
	    {
	      OutputMsg(0, "Unknown command class '%s'", args[1]);
	      break;
	    }
          display_help(args[1], ktabp);
	}
      else
        display_help((char *)0, ktab);
      break;
    case RC_LICENSE:
      display_copyright();
      break;
#ifdef COPY_PASTE
    case RC_COPY:
      if (flayer->l_layfn != &WinLf)
	{
	  OutputMsg(0, "Must be on a window layer");
	  break;
	}
      MarkRoutine();
      WindowChanged(fore, 'P');
      break;
    case RC_HISTORY:
      {
        static char *pasteargs[] = {".", 0};
	static int pasteargl[] = {1};

	if (flayer->l_layfn != &WinLf)
	  {
	    OutputMsg(0, "Must be on a window layer");
	    break;
	  }
	if (GetHistory() == 0)
	  break;
	if (user->u_plop.buf == NULL)
	  break;
	args = pasteargs;
	argl = pasteargl;
      }
      /*FALLTHROUGH*/
    case RC_PASTE:
      {
        char *ss, *dbuf, dch;
        int l = 0;
# ifdef ENCODINGS
	int enc = -1;
# endif

	/*
	 * without args we prompt for one(!) register to be pasted in the window
	 */
	if ((s = *args) == NULL)
	  {
	    Input("Paste from register:", 1, INP_RAW, ins_reg_fn, NULL, 0);
	    break;
	  }
	if (args[1] == 0 && !fore)	/* no window? */
	  break;
	/*	
	 * with two arguments we paste into a destination register
	 * (no window needed here).
	 */
	if (args[1] && argl[1] != 1)
	  {
	    OutputMsg(0, "%s: paste destination: character, ^x, or (octal) \\032 expected.",
		rc_name);
	    break;
	  }
# ifdef ENCODINGS
        else if (fore)
	  enc = fore->w_encoding;
# endif

	/*
	 * measure length of needed buffer 
	 */
        for (ss = s = *args; (ch = *ss); ss++)
          {
	    if (ch == '.')
	      {
# ifdef ENCODINGS
		if (enc == -1)
		  enc = user->u_plop.enc;
		if (enc != user->u_plop.enc)
		  l += RecodeBuf((unsigned char *)user->u_plop.buf, user->u_plop.len, user->u_plop.enc, enc, (unsigned char *)0);
		else
# endif
		  l += user->u_plop.len;
	      }
	    else
	      {
# ifdef ENCODINGS
		if (enc == -1)
		  enc = plop_tab[(int)(unsigned char)ch].enc;
		if (enc != plop_tab[(int)(unsigned char)ch].enc)
		  l += RecodeBuf((unsigned char *)plop_tab[(int)(unsigned char)ch].buf, plop_tab[(int)(unsigned char)ch].len, plop_tab[(int)(unsigned char)ch].enc, enc, (unsigned char *)0);
		else
# endif
                  l += plop_tab[(int)(unsigned char)ch].len;
	      }
          }
        if (l == 0)
	  {
	    OutputMsg(0, "empty buffer");
	    break;
	  }
	/*
	 * shortcut: 
	 * if there is only one source and the destination is a window, then
	 * pass a pointer rather than duplicating the buffer.
	 */
        if (s[1] == 0 && args[1] == 0)
# ifdef ENCODINGS
	  if (enc == (*s == '.' ? user->u_plop.enc : plop_tab[(int)(unsigned char)*s].enc))
# endif
            {
	      MakePaster(&fore->w_paster, *s == '.' ? user->u_plop.buf : plop_tab[(int)(unsigned char)*s].buf, l, 0);
	      break;
            }
	/*
	 * if no shortcut, we construct a buffer
	 */
        if ((dbuf = (char *)malloc(l)) == 0)
          {
	    OutputMsg(0, "%s", strnomem);
	    break;
          }
        l = 0;
	/*
	 * concatenate all sources into our own buffer, copy buffer is
	 * special and is skipped if no display exists.
	 */
        for (ss = s; (ch = *ss); ss++)
          {
	    struct plop *pp = (ch == '.' ? &user->u_plop : &plop_tab[(int)(unsigned char)ch]);
#ifdef ENCODINGS
	    if (pp->enc != enc)
	      {
		l += RecodeBuf((unsigned char *)pp->buf, pp->len, pp->enc, enc, (unsigned char *)dbuf + l);
		continue;
	      }
#endif
	    bcopy(pp->buf, dbuf + l, pp->len);
	    l += pp->len;
          }
	/*
	 * when called with one argument we paste our buffer into the window 
	 */
	if (args[1] == 0)
	  {
	    MakePaster(&fore->w_paster, dbuf, l, 1);
	  }
	else
	  {
	    /*
	     * we have two arguments, the second is already in dch.
	     * use this as destination rather than the window.
	     */
	    dch = args[1][0];
	    if (dch == '.')
	      {
	        if (user->u_plop.buf != NULL)
	          UserFreeCopyBuffer(user);
		user->u_plop.buf = dbuf;
		user->u_plop.len = l;
#ifdef ENCODINGS
		user->u_plop.enc = enc;
#endif
	      }
	    else
	      {
		struct plop *pp = plop_tab + (int)(unsigned char)dch;
		if (pp->buf)
		  free(pp->buf);
		pp->buf = dbuf;
		pp->len = l;
#ifdef ENCODINGS
		pp->enc = enc;
#endif
	      }
	  }
        break;
      }
    case RC_WRITEBUF:
      if (!user->u_plop.buf)
	{
	  OutputMsg(0, "empty buffer");
	  break;
	}
#ifdef ENCODINGS
	{
	  struct plop oldplop;

	  oldplop = user->u_plop;
	  if (args[0] && args[1] && !strcmp(args[0], "-e"))
	    {
	      int enc, l;
	      char *newbuf;

	      enc = FindEncoding(args[1]);
	      if (enc == -1)
		{
		  OutputMsg(0, "%s: writebuf: unknown encoding", rc_name);
		  break;
		}
	      if (enc != oldplop.enc)
		{
		  l = RecodeBuf((unsigned char *)oldplop.buf, oldplop.len, oldplop.enc, enc, (unsigned char *)0);
		  newbuf = malloc(l + 1);
		  if (!newbuf)
		    {
		      OutputMsg(0, "%s", strnomem);
		      break;
		    }
		  user->u_plop.len = RecodeBuf((unsigned char *)oldplop.buf, oldplop.len, oldplop.enc, enc, (unsigned char *)newbuf);
		  user->u_plop.buf = newbuf;
		  user->u_plop.enc = enc;
		}
	      args += 2;
	    }
#endif
	  if (args[0] && args[1])
	    OutputMsg(0, "%s: writebuf: too many arguments", rc_name);
	  else
	    WriteFile(user, args[0], DUMP_EXCHANGE);
#ifdef ENCODINGS
	  if (user->u_plop.buf != oldplop.buf)
	    free(user->u_plop.buf);
	  user->u_plop = oldplop;
	}
#endif
      break;
    case RC_READBUF:
#ifdef ENCODINGS
      i = fore ? fore->w_encoding : display ? display->d_encoding : 0;
      if (args[0] && args[1] && !strcmp(args[0], "-e"))
	{
	  i = FindEncoding(args[1]);
	  if (i == -1)
	    {
	      OutputMsg(0, "%s: readbuf: unknown encoding", rc_name);
	      break;
	    }
	  args += 2;
	}
#endif
      if (args[0] && args[1])
	{
	  OutputMsg(0, "%s: readbuf: too many arguments", rc_name);
	  break;
	}
      if ((s = ReadFile(args[0] ? args[0] : BufferFile, &n)))
	{
	  if (user->u_plop.buf)
	    UserFreeCopyBuffer(user);
	  user->u_plop.len = n;
	  user->u_plop.buf = s;
#ifdef ENCODINGS
	  user->u_plop.enc = i;
#endif
	}
      break;
    case RC_REMOVEBUF:
      KillBuffers();
      break;
    case RC_IGNORECASE:
      (void)ParseSwitch(act, &search_ic);
      if (msgok)
        OutputMsg(0, "Will %signore case in searches", search_ic ? "" : "not ");
      break;
#endif				/* COPY_PASTE */
    case RC_ESCAPE:
      if (*argl == 0)
	SetEscape(user, -1, -1);
      else if (*argl == 2)
	SetEscape(user, (int)(unsigned char)args[0][0], (int)(unsigned char)args[0][1]);
      else
	{
	  OutputMsg(0, "%s: two characters required after escape.", rc_name);
	  break;
	}
      /* Change defescape if master user. This is because we only
       * have one ktab.
       */
      if (display && user != users)
	break;
      /* FALLTHROUGH */
    case RC_DEFESCAPE:
      if (*argl == 0)
	SetEscape(NULL, -1, -1);
      else if (*argl == 2)
	SetEscape(NULL, (int)(unsigned char)args[0][0], (int)(unsigned char)args[0][1]);
      else
	{
	  OutputMsg(0, "%s: two characters required after defescape.", rc_name);
	  break;
	}
#ifdef MAPKEYS
      CheckEscape();
#endif
      break;
    case RC_CHDIR:
      s = *args ? *args : home;
      if (chdir(s) == -1)
	OutputMsg(errno, "%s", s);
      break;
    case RC_SHELL:
    case RC_DEFSHELL:
      if (ParseSaveStr(act, &ShellProg) == 0)
        ShellArgs[0] = ShellProg;
      break;
    case RC_HARDCOPYDIR:
      if (*args)
        (void)ParseSaveStr(act, &hardcopydir);
      if (msgok)
	OutputMsg(0, "hardcopydir is %s\n", hardcopydir && *hardcopydir ? hardcopydir : "<cwd>");
      break;
    case RC_LOGFILE:
      if (*args)
	{
	  if (args[1] && !(strcmp(*args, "flush")))
	    {
	      log_flush = atoi(args[1]);
	      if (msgok)
		OutputMsg(0, "log flush timeout set to %ds\n", log_flush);
	      break;
	    }
	  if (ParseSaveStr(act, &screenlogfile) || !msgok)
	    break;
	}
      OutputMsg(0, "logfile is '%s'", screenlogfile);
      break;
    case RC_LOGTSTAMP:
      if (!*args || !strcmp(*args, "on") || !strcmp(*args, "off"))
        {
	  if (ParseSwitch(act, &logtstamp_on) == 0 && msgok)
            OutputMsg(0, "timestamps turned %s", logtstamp_on ? "on" : "off");
        }
      else if (!strcmp(*args, "string"))
	{
	  if (args[1])
	    {
	      if (logtstamp_string)
		free(logtstamp_string);
	      logtstamp_string = SaveStr(args[1]);
	    }
	  if (msgok)
	    OutputMsg(0, "logfile timestamp is '%s'", logtstamp_string);
	}
      else if (!strcmp(*args, "after"))
	{
	  if (args[1])
	    {
	      logtstamp_after = atoi(args[1]);
	      if (!msgok)
		break;
	    }
	  OutputMsg(0, "timestamp printed after %ds\n", logtstamp_after);
	}
      else
        OutputMsg(0, "usage: logtstamp [after [n]|string [str]|on|off]");
      break;
    case RC_SHELLTITLE:
      (void)ParseSaveStr(act, &nwin_default.aka);
      break;
    case RC_TERMCAP:
    case RC_TERMCAPINFO:
    case RC_TERMINFO:
      if (!rc_name || !*rc_name)
        OutputMsg(0, "Sorry, too late now. Place that in your .screenrc file.");
      break;
    case RC_SLEEP:
      break;			/* Already handled */
    case RC_TERM:
      s = NULL;
      if (ParseSaveStr(act, &s))
	break;
      if (strlen(s) >= MAXTERMLEN)
	{
	  OutputMsg(0, "%s: term: argument too long ( < %d)", rc_name, MAXTERMLEN);
	  free(s);
	  break;
	}
      strcpy(screenterm, s);
      free(s);
      debug1("screenterm set to %s\n", screenterm);
      MakeTermcap((display == 0));
      debug("new termcap made\n");
      break;
    case RC_ECHO:
      if (!msgok && (!rc_name || strcmp(rc_name, "-X")))
	break;
      /*
       * user typed ^A:echo... well, echo isn't FinishRc's job,
       * but as he wanted to test us, we show good will
       */
      if (argc > 1 && !strcmp(*args, "-n"))
	{
	  args++;
	  argc--;
	}
      s = *args;
      if (argc > 1 && !strcmp(*args, "-p"))
	{
	  args++;
	  argc--;
	  s = *args;
	  if (s)
	    s = MakeWinMsg(s, fore, '%');
	}
      if (s)
	OutputMsg(0, "%s", s);
      else
	{
	  OutputMsg(0, "%s: 'echo [-n] [-p] \"string\"' expected.", rc_name);
	  queryflag = -1;
	}
      break;
    case RC_BELL:
    case RC_BELL_MSG:
      if (*args == 0)
	{
	  char buf[256];
	  AddXChars(buf, sizeof(buf), BellString);
	  OutputMsg(0, "bell_msg is '%s'", buf);
	  break;
	}
      (void)ParseSaveStr(act, &BellString);
      break;
#ifdef COPY_PASTE
    case RC_BUFFERFILE:
      if (*args == 0)
	BufferFile = SaveStr(DEFAULT_BUFFERFILE);
      else if (ParseSaveStr(act, &BufferFile))
        break;
      if (msgok)
        OutputMsg(0, "Bufferfile is now '%s'", BufferFile);
      break;
#endif
    case RC_ACTIVITY:
      (void)ParseSaveStr(act, &ActivityString);
      break;
#if defined(DETACH) && defined(POW_DETACH)
    case RC_POW_DETACH_MSG:
      if (*args == 0)
        {
	  char buf[256];
          AddXChars(buf, sizeof(buf), PowDetachString);
	  OutputMsg(0, "pow_detach_msg is '%s'", buf);
	  break;
	}
      (void)ParseSaveStr(act, &PowDetachString);
      break;
#endif
#if defined(UTMPOK) && defined(LOGOUTOK)
    case RC_LOGIN:
      n = fore->w_slot != (slot_t)-1;
      if (*args && !strcmp(*args, "always"))
	{
	  fore->w_lflag = 3;
	  if (!displays && n)
	    SlotToggle(n);
	  break;
	}
      if (*args && !strcmp(*args, "attached"))
	{
	  fore->w_lflag = 1;
	  if (!displays && n)
	    SlotToggle(0);
	  break;
	}
      if (ParseSwitch(act, &n) == 0)
        SlotToggle(n);
      break;
    case RC_DEFLOGIN:
      if (!strcmp(*args, "always"))
	nwin_default.lflag |= 2;
      else if (!strcmp(*args, "attached"))
	nwin_default.lflag &= ~2;
      else
        (void)ParseOnOff(act, &nwin_default.lflag);
      break;
#endif
    case RC_DEFFLOW:
      if (args[0] && args[1] && args[1][0] == 'i')
	{
	  iflag = 1;
	  for (display = displays; display; display = display->d_next)
	    {
	      if (!D_flow)
		continue;
#if defined(TERMIO) || defined(POSIX)
	      D_NewMode.tio.c_cc[VINTR] = D_OldMode.tio.c_cc[VINTR];
	      D_NewMode.tio.c_lflag |= ISIG;
#else /* TERMIO || POSIX */
	      D_NewMode.m_tchars.t_intrc = D_OldMode.m_tchars.t_intrc;
#endif /* TERMIO || POSIX */
	      SetTTY(D_userfd, &D_NewMode);
	    }
	}
      if (args[0] && args[0][0] == 'a')
	nwin_default.flowflag = FLOW_AUTOFLAG;
      else
	(void)ParseOnOff(act, &nwin_default.flowflag);
      break;
    case RC_DEFWRAP:
      (void)ParseOnOff(act, &nwin_default.wrap);
      break;
    case RC_DEFC1:
      (void)ParseOnOff(act, &nwin_default.c1);
      break;
#ifdef COLOR
    case RC_DEFBCE:
      (void)ParseOnOff(act, &nwin_default.bce);
      break;
#endif
    case RC_DEFGR:
      (void)ParseOnOff(act, &nwin_default.gr);
      break;
    case RC_DEFMONITOR:
      if (ParseOnOff(act, &n) == 0)
        nwin_default.monitor = (n == 0) ? MON_OFF : MON_ON;
      break;
    case RC_DEFMOUSETRACK:
      if (ParseOnOff(act, &n) == 0)
	defmousetrack = (n == 0) ? 0 : 1000;
      break;
    case RC_MOUSETRACK:
      if (!args[0])
	{
	  OutputMsg(0, "Mouse tracking for this display is turned %s", D_mousetrack ? "on" : "off");
	}
      else if (ParseOnOff(act, &n) == 0)
	{
	  D_mousetrack = n == 0 ? 0 : 1000;
	  if (D_fore)
	    MouseMode(D_fore->w_mouse);
	}
      break;
    case RC_DEFSILENCE:
      if (ParseOnOff(act, &n) == 0)
        nwin_default.silence = (n == 0) ? SILENCE_OFF : SILENCE_ON;
      break;
    case RC_VERBOSE:
      if (!*args)
	OutputMsg(0, "W%s echo command when creating windows.", 
	  VerboseCreate ? "ill" : "on't");
      else if (ParseOnOff(act, &n) == 0)
        VerboseCreate = n;
      break;
    case RC_HARDSTATUS:
      if (display)
	{
	  OutputMsg(0, "%s", "");	/* wait till mintime (keep gcc quiet) */
          RemoveStatus();
	}
      if (args[0] && strcmp(args[0], "on") && strcmp(args[0], "off"))
	{
          struct display *olddisplay = display;
	  int old_use, new_use = -1;

	  s = args[0];
	  if (!strncmp(s, "always", 6))
	    s += 6;
	  if (!strcmp(s, "firstline"))
	    new_use = HSTATUS_FIRSTLINE;
	  else if (!strcmp(s, "lastline"))
	    new_use = HSTATUS_LASTLINE;
	  else if (!strcmp(s, "ignore"))
	    new_use = HSTATUS_IGNORE;
	  else if (!strcmp(s, "message"))
	    new_use = HSTATUS_MESSAGE;
	  else if (!strcmp(args[0], "string"))
	    {
	      if (!args[1])
		{
		  char buf[256];
		  AddXChars(buf, sizeof(buf), hstatusstring);
		  OutputMsg(0, "hardstatus string is '%s'", buf);
		  break;
		}
	    }
	  else
	    {
	      OutputMsg(0, "%s: usage: hardstatus [always]lastline|ignore|message|string [string]", rc_name);
	      break;
	    }
	  if (new_use != -1)
	    {
	      hardstatusemu = new_use | (s == args[0] ? 0 : HSTATUS_ALWAYS);
	      for (display = displays; display; display = display->d_next)
		{
		  RemoveStatus();
		  new_use = hardstatusemu & ~HSTATUS_ALWAYS;
		  if (D_HS && s == args[0])
		    new_use = HSTATUS_HS;
		  ShowHStatus((char *)0);
		  old_use = D_has_hstatus;
		  D_has_hstatus = new_use;
		  if ((new_use == HSTATUS_LASTLINE && old_use != HSTATUS_LASTLINE) || (new_use != HSTATUS_LASTLINE && old_use == HSTATUS_LASTLINE))
		    ChangeScreenSize(D_width, D_height, 1);
		  if ((new_use == HSTATUS_FIRSTLINE && old_use != HSTATUS_FIRSTLINE) || (new_use != HSTATUS_FIRSTLINE && old_use == HSTATUS_FIRSTLINE))
		    ChangeScreenSize(D_width, D_height, 1);
		  RefreshHStatus();
		}
	    }
	  if (args[1])
	    {
	      if (hstatusstring)
		free(hstatusstring);
	      hstatusstring = SaveStr(args[1]);
	      for (display = displays; display; display = display->d_next)
	        RefreshHStatus();
	    }
	  display = olddisplay;
	  break;
	}
      (void)ParseSwitch(act, &use_hardstatus);
      if (msgok)
        OutputMsg(0, "messages displayed on %s", use_hardstatus ? "hardstatus line" : "window");
      break;
    case RC_CAPTION:
      if (strcmp(args[0], "always") == 0 || strcmp(args[0], "splitonly") == 0)
	{
	  struct display *olddisplay = display;

	  captionalways = args[0][0] == 'a';
	  for (display = displays; display; display = display->d_next)
	    ChangeScreenSize(D_width, D_height, 1);
	  display = olddisplay;
	}
      else if (strcmp(args[0], "string") == 0)
	{
	  if (!args[1])
	    {
	      char buf[256];
	      AddXChars(buf, sizeof(buf), captionstring);
	      OutputMsg(0, "caption string is '%s'", buf);
	      break;
	    }
	}
      else
	{
	  OutputMsg(0, "%s: usage: caption always|splitonly|string <string>", rc_name);
	  break;
	}
      if (!args[1])
	break;
      if (captionstring)
	free(captionstring);
      captionstring = SaveStr(args[1]);
      RedisplayDisplays(0);
      break;
    case RC_CONSOLE:
      n = (console_window != 0);
      if (ParseSwitch(act, &n))
        break;
      if (TtyGrabConsole(fore->w_ptyfd, n, rc_name))
	break;
      if (n == 0)
	  OutputMsg(0, "%s: releasing console %s", rc_name, HostName);
      else if (console_window)
	  OutputMsg(0, "%s: stealing console %s from window %d (%s)", rc_name, 
	      HostName, console_window->w_number, console_window->w_title);
      else
	  OutputMsg(0, "%s: grabbing console %s", rc_name, HostName);
      console_window = n ? fore : 0;
      break;
    case RC_ALLPARTIAL:
      if (ParseOnOff(act, &all_norefresh))
	break;
      if (!all_norefresh && fore)
	Activate(-1);
      if (msgok)
        OutputMsg(0, all_norefresh ? "No refresh on window change!\n" :
			       "Window specific refresh\n");
      break;
    case RC_PARTIAL:
      (void)ParseSwitch(act, &n);
      fore->w_norefresh = n;
      break;
    case RC_VBELL:
      if (ParseSwitch(act, &visual_bell) || !msgok)
        break;
      if (visual_bell == 0)
        OutputMsg(0, "switched to audible bell.");
      else
        OutputMsg(0, "switched to visual bell.");
      break;
    case RC_VBELLWAIT:
      if (ParseNum1000(act, &VBellWait) == 0 && msgok)
        OutputMsg(0, "vbellwait set to %.10g seconds", VBellWait/1000.);
      break;
    case RC_MSGWAIT:
      if (ParseNum1000(act, &MsgWait) == 0 && msgok)
        OutputMsg(0, "msgwait set to %.10g seconds", MsgWait/1000.);
      break;
    case RC_MSGMINWAIT:
      if (ParseNum1000(act, &MsgMinWait) == 0 && msgok)
        OutputMsg(0, "msgminwait set to %.10g seconds", MsgMinWait/1000.);
      break;
    case RC_SILENCEWAIT:
      if (ParseNum(act, &SilenceWait))
	break;
      if (SilenceWait < 1)
	SilenceWait = 1;
      for (p = windows; p; p = p->w_next)
	p->w_silencewait = SilenceWait;
      if (msgok)
	OutputMsg(0, "silencewait set to %d seconds", SilenceWait);
      break;
    case RC_BUMPRIGHT:
      if (fore->w_number < NextWindow())
        WindowChangeNumber(fore->w_number, NextWindow());
      break;
    case RC_BUMPLEFT:
      if (fore->w_number > PreviousWindow())
        WindowChangeNumber(fore->w_number, PreviousWindow());
      break;
    case RC_COLLAPSE:
      CollapseWindowlist();
      break;
    case RC_NUMBER:
      if (*args == 0)
        OutputMsg(0, queryflag >= 0 ? "%d (%s)" : "This is window %d (%s).", fore->w_number, fore->w_title);
      else
        {
	  int old = fore->w_number;
	  int rel = 0, parse;
	  if (args[0][0] == '+')
	    rel = 1;
	  else if (args[0][0] == '-')
	    rel = -1;
	  if (rel)
	    ++act->args[0];
	  parse = ParseNum(act, &n);
	  if (rel)
	    --act->args[0];
	  if (parse)
	    break;
	  if (rel > 0)
	    n += old;
	  else if (rel < 0)
	    n = old - n;
	  if (!WindowChangeNumber(old, n))
	    {
	      /* Window number could not be changed. */
	      queryflag = -1;
	      return;
	    }
	}
      break;
    case RC_SILENCE:
      n = fore->w_silence != 0;
      i = fore->w_silencewait;
      if (args[0] && (args[0][0] == '-' || (args[0][0] >= '0' && args[0][0] <= '9')))
        {
	  if (ParseNum(act, &i))
	    break;
	  n = i > 0;
	}
      else if (ParseSwitch(act, &n))
        break;
      if (n)
        {
#ifdef MULTIUSER
	  if (display)	/* we tell only this user */
	    ACLBYTE(fore->w_lio_notify, D_user->u_id) |= ACLBIT(D_user->u_id);
	  else
	    for (n = 0; n < maxusercount; n++)
	      ACLBYTE(fore->w_lio_notify, n) |= ACLBIT(n);
#endif
	  fore->w_silencewait = i;
	  fore->w_silence = SILENCE_ON;
	  SetTimeout(&fore->w_silenceev, fore->w_silencewait * 1000);
	  evenq(&fore->w_silenceev);

	  if (!msgok)
	    break;
	  OutputMsg(0, "The window is now being monitored for %d sec. silence.", fore->w_silencewait);
	}
      else
        {
#ifdef MULTIUSER
	  if (display) /* we remove only this user */
	    ACLBYTE(fore->w_lio_notify, D_user->u_id) 
	      &= ~ACLBIT(D_user->u_id);
	  else
	    for (n = 0; n < maxusercount; n++)
	      ACLBYTE(fore->w_lio_notify, n) &= ~ACLBIT(n);
	  for (i = maxusercount - 1; i >= 0; i--)
	    if (ACLBYTE(fore->w_lio_notify, i))
	      break;
	  if (i < 0)
#endif
	    {
	      fore->w_silence = SILENCE_OFF;
	      evdeq(&fore->w_silenceev);
	    }
	  if (!msgok)
	    break;
	  OutputMsg(0, "The window is no longer being monitored for silence.");
	}
      break;
#ifdef COPY_PASTE
    case RC_DEFSCROLLBACK:
      (void)ParseNum(act, &nwin_default.histheight);
      break;
    case RC_SCROLLBACK:
      if (flayer->l_layfn == &MarkLf)
	{
	  OutputMsg(0, "Cannot resize scrollback buffer in copy/scrollback mode.");
	  break;
	}
      (void)ParseNum(act, &n);
      ChangeWindowSize(fore, fore->w_width, fore->w_height, n);
      if (msgok)
	OutputMsg(0, "scrollback set to %d", fore->w_histheight);
      break;
#endif
    case RC_SESSIONNAME:
      if (*args == 0)
	OutputMsg(0, "This session is named '%s'\n", SockName);
      else
	{
	  char buf[MAXPATHLEN];

	  s = 0;
	  if (ParseSaveStr(act, &s))
	    break;
	  if (!*s || strlen(s) + (SockName - SockPath) > MAXPATHLEN - 13 || index(s, '/'))
	    {
	      OutputMsg(0, "%s: bad session name '%s'\n", rc_name, s);
	      free(s);
	      break;
	    }
	  strncpy(buf, SockPath, SockName - SockPath);
	  sprintf(buf + (SockName - SockPath), "%d.%s", (int)getpid(), s); 
	  free(s);
	  if ((access(buf, F_OK) == 0) || (errno != ENOENT))
	    {
	      OutputMsg(0, "%s: inappropriate path: '%s'.", rc_name, buf);
	      break;
	    }
	  if (rename(SockPath, buf))
	    {
	      OutputMsg(errno, "%s: failed to rename(%s, %s)", rc_name, SockPath, buf);
	      break;
	    }
	  debug2("rename(%s, %s) done\n", SockPath, buf);
	  strcpy(SockPath, buf);
	  MakeNewEnv();
	  WindowChanged((struct win *)0, 'S');
	}
      break;
    case RC_SETENV:
      if (!args[0] || !args[1])
        {
	  debug1("RC_SETENV arguments missing: %s\n", args[0] ? args[0] : "");
          InputSetenv(args[0]);
	}
      else
        {
          xsetenv(args[0], args[1]);
          MakeNewEnv();
	}
      break;
    case RC_UNSETENV:
      unsetenv(*args);
      MakeNewEnv();
      break;
#ifdef COPY_PASTE
    case RC_DEFSLOWPASTE:
      (void)ParseNum(act, &nwin_default.slow);
      break;
    case RC_SLOWPASTE:
      if (*args == 0)
	OutputMsg(0, fore->w_slowpaste ? 
               "Slowpaste in window %d is %d milliseconds." :
               "Slowpaste in window %d is unset.", 
	    fore->w_number, fore->w_slowpaste);
      else if (ParseNum(act, &fore->w_slowpaste) == 0 && msgok)
	OutputMsg(0, fore->w_slowpaste ?
               "Slowpaste in window %d set to %d milliseconds." :
               "Slowpaste in window %d now unset.", 
	    fore->w_number, fore->w_slowpaste);
      break;
    case RC_MARKKEYS:
      if (CompileKeys(*args, *argl, mark_key_tab))
	{
	  OutputMsg(0, "%s: markkeys: syntax error.", rc_name);
	  break;
	}
      debug1("markkeys %s\n", *args);
      break;
# ifdef FONT
    case RC_PASTEFONT:
      if (ParseSwitch(act, &pastefont) == 0 && msgok)
        OutputMsg(0, "Will %spaste font settings", pastefont ? "" : "not ");
      break;
# endif
    case RC_CRLF:
      (void)ParseSwitch(act, &join_with_cr);
      break;
    case RC_COMPACTHIST:
      if (ParseSwitch(act, &compacthist) == 0 && msgok)
	OutputMsg(0, "%scompacting history lines", compacthist ? "" : "not ");
      break;
#endif
#ifdef NETHACK
    case RC_NETHACK:
      (void)ParseOnOff(act, &nethackflag);
      break;
#else
    case RC_NETHACK:
      Msg(0, "nethack disabled at build time");
      break;
#endif
    case RC_HARDCOPY_APPEND:
      (void)ParseOnOff(act, &hardcopy_append);
      break;
    case RC_VBELL_MSG:
      if (*args == 0) 
        { 
	  char buf[256];
          AddXChars(buf, sizeof(buf), VisualBellString);
	  OutputMsg(0, "vbell_msg is '%s'", buf);
	  break; 
	}
      (void)ParseSaveStr(act, &VisualBellString);
      debug1(" new vbellstr '%s'\n", VisualBellString);
      break;
    case RC_DEFMODE:
      if (ParseBase(act, *args, &n, 8, "octal"))
        break;
      if (n < 0 || n > 0777)
	{
	  OutputMsg(0, "%s: mode: Invalid tty mode %o", rc_name, n);
          break;
	}
      TtyMode = n;
      if (msgok)
	OutputMsg(0, "Ttymode set to %03o", TtyMode);
      break;
    case RC_AUTODETACH:
      (void)ParseOnOff(act, &auto_detach);
      break;
    case RC_STARTUP_MESSAGE:
      (void)ParseOnOff(act, &default_startup);
      break;
#ifdef PASSWORD
    case RC_PASSWORD:
      if (*args)
	{
	  n = (*user->u_password) ? 1 : 0;
	  if (user->u_password != NullStr) free((char *)user->u_password);
	  user->u_password = SaveStr(*args);
	  if (!strcmp(user->u_password, "none"))
	    {
	      if (n)
	        OutputMsg(0, "Password checking disabled");
	      free(user->u_password);
	      user->u_password = NullStr;
	    }
	}
      else
	{
	  if (!fore)
	    {
	      OutputMsg(0, "%s: password: window required", rc_name);
	      break;
	    }
	  Input("New screen password:", 100, INP_NOECHO, pass1, display ? (char *)D_user : (char *)users, 0);
	}
      break;
#endif				/* PASSWORD */
    case RC_BIND:
	{
	  struct action *ktabp = ktab;
	  int kflag = 0;

	  for (;;)
	    {
	      if (argc > 2 && !strcmp(*args, "-c"))
		{
		  ktabp = FindKtab(args[1], 1);
		  if (ktabp == 0)
		    break;
		  args += 2;
		  argl += 2;
		  argc -= 2;
		}
	      else if (argc > 1 && !strcmp(*args, "-k"))
	        {
		  kflag = 1;
		  args++;
		  argl++;
		  argc--;
		}
	      else
	        break;
	    }
#ifdef MAPKEYS
          if (kflag)
	    {
	      for (n = 0; n < KMAP_KEYS; n++)
		if (strcmp(term[n + T_CAPS].tcname, *args) == 0)
		  break;
	      if (n == KMAP_KEYS)
		{
		  OutputMsg(0, "%s: bind: unknown key '%s'", rc_name, *args);
		  break;
		}
	      n += 256;
	    }
	  else
#endif
	  if (*argl != 1)
	    {
	      OutputMsg(0, "%s: bind: character, ^x, or (octal) \\032 expected.", rc_name);
	      break;
	    }
	  else
	    n = (unsigned char)args[0][0];

	  if (args[1])
	    {
	      if ((i = FindCommnr(args[1])) == RC_ILLEGAL)
		{
		  OutputMsg(0, "%s: bind: unknown command '%s'", rc_name, args[1]);
		  break;
		}
	      if (CheckArgNum(i, args + 2) < 0)
		break;
	      ClearAction(&ktabp[n]);
	      SaveAction(ktabp + n, i, args + 2, argl + 2);
	    }
	  else
	    ClearAction(&ktabp[n]);
	}
      break;
#ifdef MAPKEYS
    case RC_BINDKEY:
	{
	  struct action *newact;
          int newnr, fl = 0, kf = 0, af = 0, df = 0, mf = 0;
	  struct display *odisp = display;
	  int used = 0;
          struct kmap_ext *kme = NULL;

	  for (; *args && **args == '-'; args++, argl++)
	    {
	      if (strcmp(*args, "-t") == 0)
		fl = KMAP_NOTIMEOUT;
	      else if (strcmp(*args, "-k") == 0)
		kf = 1;
	      else if (strcmp(*args, "-a") == 0)
		af = 1;
	      else if (strcmp(*args, "-d") == 0)
		df = 1;
	      else if (strcmp(*args, "-m") == 0)
		mf = 1;
	      else if (strcmp(*args, "--") == 0)
		{
		  args++;
		  argl++;
		  break;
		}
	      else
		{
	          OutputMsg(0, "%s: bindkey: invalid option %s", rc_name, *args);
		  return;
		}
	    }
	  if (df && mf)
	    {
	      OutputMsg(0, "%s: bindkey: -d does not work with -m", rc_name);
	      break;
	    }
	  if (*args == 0)
	    {
	      if (mf)
		display_bindkey("Edit mode", mmtab);
	      else if (df)
		display_bindkey("Default", dmtab);
	      else
		display_bindkey("User", umtab);
	      break;
	    }
	  if (kf == 0)
	    {
	      if (af)
		{
		  OutputMsg(0, "%s: bindkey: -a only works with -k", rc_name);
		  break;
		}
	      if (*argl == 0)
		{
		  OutputMsg(0, "%s: bindkey: empty string makes no sense", rc_name);
		  break;
		}
	      for (i = 0, kme = kmap_exts; i < kmap_extn; i++, kme++)
		if (kme->str == 0)
		  {
		    if (args[1])
		      break;
		  }
		else
		  if (*argl == (kme->fl & ~KMAP_NOTIMEOUT) && bcmp(kme->str, *args, *argl) == 0)
		      break;
	      if (i == kmap_extn)
		{
		  if (!args[1])
		    {
		      OutputMsg(0, "%s: bindkey: keybinding not found", rc_name);
		      break;
		    }
		  kmap_extn += 8;
		  kmap_exts = (struct kmap_ext *)xrealloc((char *)kmap_exts, kmap_extn * sizeof(*kmap_exts));
		  kme = kmap_exts + i;
		  bzero((char *)kme, 8 * sizeof(*kmap_exts));
		  for (; i < kmap_extn; i++, kme++)
		    {
		      kme->str = 0;
		      kme->dm.nr = kme->mm.nr = kme->um.nr = RC_ILLEGAL;
		      kme->dm.args = kme->mm.args = kme->um.args = noargs;
		      kme->dm.argl = kme->mm.argl = kme->um.argl = 0;
		    }
		  i -= 8;
		  kme -= 8;
		}
	      if (df == 0 && kme->dm.nr != RC_ILLEGAL)
		used = 1;
	      if (mf == 0 && kme->mm.nr != RC_ILLEGAL)
		used = 1;
	      if ((df || mf) && kme->um.nr != RC_ILLEGAL)
		used = 1;
	      i += KMAP_KEYS + KMAP_AKEYS;
	      newact = df ? &kme->dm : mf ? &kme->mm : &kme->um;
	    }
	  else
	    {
	      for (i = T_CAPS; i < T_OCAPS; i++)
		if (strcmp(term[i].tcname, *args) == 0)
		  break;
	      if (i == T_OCAPS)
		{
		  OutputMsg(0, "%s: bindkey: unknown key '%s'", rc_name, *args);
		  break;
		}
	      if (af && i >= T_CURSOR && i < T_OCAPS)
	        i -=  T_CURSOR - KMAP_KEYS;
	      else
	        i -=  T_CAPS;
	      newact = df ? &dmtab[i] : mf ? &mmtab[i] : &umtab[i];
	    }
	  if (args[1])
	    {
	      if ((newnr = FindCommnr(args[1])) == RC_ILLEGAL)
		{
		  OutputMsg(0, "%s: bindkey: unknown command '%s'", rc_name, args[1]);
		  break;
		}
	      if (CheckArgNum(newnr, args + 2) < 0)
		break;
	      ClearAction(newact);
	      SaveAction(newact, newnr, args + 2, argl + 2);
	      if (kf == 0 && args[1])
		{
		  if (kme->str)
		    free(kme->str);
		  kme->str = SaveStrn(*args, *argl);
		  kme->fl = fl | *argl;
		}
	    }
	  else
	    ClearAction(newact);
	  for (display = displays; display; display = display->d_next)
	    remap(i, args[1] ? 1 : 0);
	  if (kf == 0 && !args[1])
	    {
	      if (!used && kme->str)
		{
		  free(kme->str);
		  kme->str = 0;
		  kme->fl = 0;
		}
	    }
	  display = odisp;
	}
      break;
    case RC_MAPTIMEOUT:
      if (*args)
	{
          if (ParseNum(act, &n))
	    break;
	  if (n < 0)
	    {
	      OutputMsg(0, "%s: maptimeout: illegal time %d", rc_name, n);
	      break;
	    }
	  maptimeout = n;
	}
      if (*args == 0 || msgok)
        OutputMsg(0, "maptimeout is %dms", maptimeout);
      break;
    case RC_MAPNOTNEXT:
      D_dontmap = 1;
      break;
    case RC_MAPDEFAULT:
      D_mapdefault = 1;
      break;
#endif
#ifdef MULTIUSER
    case RC_ACLCHG:
    case RC_ACLADD:
    case RC_ADDACL:
    case RC_CHACL:
      UsersAcl(NULL, argc, args);
      break;
    case RC_ACLDEL:
      if (UserDel(args[0], NULL))
	break;
      if (msgok)
	OutputMsg(0, "%s removed from acl database", args[0]);
      break;
    case RC_ACLGRP:
      /*
       * modify a user to gain or lose rights granted to a group.
       * This group is actually a normal user whose rights were defined
       * with chacl in the usual way.
       */
      if (args[1])
        {
	  if (strcmp(args[1], "none"))	/* link a user to another user */
	    {
	      if (AclLinkUser(args[0], args[1]))
		break;
	      if (msgok)
		OutputMsg(0, "User %s joined acl-group %s", args[0], args[1]);
	    }
	  else				/* remove all groups from user */
	    {
	      struct acluser *u;
	      struct aclusergroup *g;

	      if (!(u = *FindUserPtr(args[0])))
	        break;
	      while ((g = u->u_group))
	        {
		  u->u_group = g->next;
	  	  free((char *)g);
	        }
	    }
	}
      else				/* show all groups of user */
	{
	  char buf[256], *p = buf;
	  int ngroups = 0;
	  struct acluser *u;
	  struct aclusergroup *g;

	  if (!(u = *FindUserPtr(args[0])))
	    {
	      if (msgok)
		OutputMsg(0, "User %s does not exist.", args[0]);
	      break;
	    }
	  g = u->u_group;
	  while (g)
	    {
	      ngroups++;
	      sprintf(p, "%s ", g->u->u_name);
	      p += strlen(p);
	      if (p > buf+200)
		break;
	      g = g->next;
	    }
	  if (ngroups)
	    *(--p) = '\0';
	  OutputMsg(0, "%s's group%s: %s.", args[0], (ngroups == 1) ? "" : "s",
	      (ngroups == 0) ? "none" : buf);
	}
      break;
    case RC_ACLUMASK:
    case RC_UMASK:
      while ((s = *args++))
        {
	  char *err = 0;

	  if (AclUmask(display ? D_user : users, s, &err))
	    OutputMsg(0, "umask: %s\n", err);
	}
      break;
    case RC_MULTIUSER:
      if (ParseOnOff(act, &n))
	break;
      multi = n ? "" : 0;
      chsock();
      if (msgok)
	OutputMsg(0, "Multiuser mode %s", multi ? "enabled" : "disabled");
      break;
#endif /* MULTIUSER */
#ifdef PSEUDOS
    case RC_EXEC:
      winexec(args);
      break;
#endif
#ifdef MULTI
    case RC_NONBLOCK:
      i = D_nonblock >= 0;
      if (*args && ((args[0][0] >= '0' && args[0][0] <= '9') || args[0][0] == '.'))
	{
          if (ParseNum1000(act, &i))
	    break;
	}
      else if (!ParseSwitch(act, &i))
	i = i == 0 ? -1 : 1000;
      else
	break;
      if (msgok && i == -1)
        OutputMsg(0, "display set to blocking mode");
      else if (msgok && i == 0)
        OutputMsg(0, "display set to nonblocking mode, no timeout");
      else if (msgok)
        OutputMsg(0, "display set to nonblocking mode, %.10gs timeout", i/1000.);
      D_nonblock = i;
      if (D_nonblock <= 0)
	evdeq(&D_blockedev);
      break;
    case RC_DEFNONBLOCK:
      if (*args && ((args[0][0] >= '0' && args[0][0] <= '9') || args[0][0] == '.'))
	{
          if (ParseNum1000(act, &defnonblock))
	    break;
	}
      else if (!ParseOnOff(act, &defnonblock))
        defnonblock = defnonblock == 0 ? -1 : 1000;
      else
	break;
      if (display && *rc_name)
	{
	  D_nonblock = defnonblock;
          if (D_nonblock <= 0)
	    evdeq(&D_blockedev);
	}
      break;
#endif
    case RC_GR:
#ifdef ENCODINGS
      if (fore->w_gr == 2)
	fore->w_gr = 0;
#endif
      if (ParseSwitch(act, &fore->w_gr) == 0 && msgok)
        OutputMsg(0, "Will %suse GR", fore->w_gr ? "" : "not ");
#ifdef ENCODINGS
      if (fore->w_gr == 0 && fore->w_FontE)
	fore->w_gr = 2;
#endif
      break;
    case RC_C1:
      if (ParseSwitch(act, &fore->w_c1) == 0 && msgok)
        OutputMsg(0, "Will %suse C1", fore->w_c1 ? "" : "not ");
      break;
#ifdef COLOR
    case RC_BCE:
      if (ParseSwitch(act, &fore->w_bce) == 0 && msgok)
        OutputMsg(0, "Will %serase with background color", fore->w_bce ? "" : "not ");
      break;
#endif
#ifdef ENCODINGS
    case RC_KANJI:
    case RC_ENCODING:
#ifdef UTF8
      if (*args && !strcmp(args[0], "-d"))
	{
	  if (!args[1])
	    OutputMsg(0, "encodings directory is %s", screenencodings ? screenencodings : "<unset>");
	  else
	    {
	      free(screenencodings);
	      screenencodings = SaveStr(args[1]);
	    }
	  break;
	}
      if (*args && !strcmp(args[0], "-l"))
	{
	  if (!args[1])
	    OutputMsg(0, "encoding: -l: argument required");
	  else if (LoadFontTranslation(-1, args[1]))
	    OutputMsg(0, "encoding: could not load utf8 encoding file");
	  else if (msgok)
	    OutputMsg(0, "encoding: utf8 encoding file loaded");
	  break;
	}
#else
      if (*args && (!strcmp(args[0], "-l") || !strcmp(args[0], "-d")))
	{
	  if (msgok)
	    OutputMsg(0, "encoding: screen is not compiled for UTF-8.");
	  break;
	}
#endif
      for (i = 0; i < 2; i++)
	{
	  if (args[i] == 0)
	    break;
	  if (!strcmp(args[i], "."))
	    continue;
	  n = FindEncoding(args[i]);
	  if (n == -1)
	    {
	      OutputMsg(0, "encoding: unknown encoding '%s'", args[i]);
	      break;
	    }
	  if (i == 0 && fore)
	    {
	      WinSwitchEncoding(fore, n);
	      ResetCharsets(fore);
	    }
	  else if (i && display)
	    D_encoding  = n;
	}
      break;
    case RC_DEFKANJI:
    case RC_DEFENCODING:
      n = FindEncoding(*args);
      if (n == -1)
	{
	  OutputMsg(0, "defencoding: unknown encoding '%s'", *args);
	  break;
	}
      nwin_default.encoding = n;
      break;
#endif

#ifdef UTF8
    case RC_DEFUTF8:
      n = nwin_default.encoding == UTF8;
      if (ParseSwitch(act, &n) == 0)
	{
	  nwin_default.encoding = n ? UTF8 : 0;
	  if (msgok)
            OutputMsg(0, "Will %suse UTF-8 encoding for new windows", n ? "" : "not ");
	}
      break;
    case RC_UTF8:
      for (i = 0; i < 2; i++)
	{
	  if (i && args[i] == 0)
	    break;
	  if (args[i] == 0)
	    n = fore->w_encoding != UTF8;
	  else if (strcmp(args[i], "off") == 0)
	    n = 0;
	  else if (strcmp(args[i], "on") == 0)
	    n = 1;
	  else
	    {
	      OutputMsg(0, "utf8: illegal argument (%s)", args[i]);
	      break;
	    }
	  if (i == 0)
	    {
	      WinSwitchEncoding(fore, n ? UTF8 : 0);
	      if (msgok)
		OutputMsg(0, "Will %suse UTF-8 encoding", n ? "" : "not ");
	    }
	  else if (display)
	    D_encoding = n ? UTF8 : 0;
	  if (args[i] == 0)
	    break;
	}
      break;
#endif

    case RC_PRINTCMD:
      if (*args)
	{
	  if (printcmd)
	    free(printcmd);
	  printcmd = 0;
	  if (**args)
	    printcmd = SaveStr(*args);
	}
      if (*args == 0 || msgok)
	{
	  if (printcmd)
	    OutputMsg(0, "using '%s' as print command", printcmd);
	  else
	    OutputMsg(0, "using termcap entries for printing");
	    break;
	}
      break;

    case RC_DIGRAPH:
      if (argl && argl[0] > 0 && argl[1] > 0)
	{
	  if (argl[0] != 2)
	    {
	      OutputMsg(0, "Two characters expected to define a digraph");
	      break;
	    }
	  i = digraph_find(args[0]);
	  digraphs[i].d[0] = args[0][0];
	  digraphs[i].d[1] = args[0][1];
	  if (!parse_input_int(args[1], argl[1], &digraphs[i].value))
	    {
	      if (!(digraphs[i].value = atoi(args[1])))
		{
		  if (!args[1][1])
		    digraphs[i].value = (int)args[1][0];
#ifdef UTF8
		  else
		    {
		      int t;
		      unsigned char *s = (unsigned char *)args[1];
		      digraphs[i].value = 0;
		      while (*s)
			{
			  t = FromUtf8(*s++, &digraphs[i].value);
			  if (t == -1)
			    continue;
			  if (t == -2)
			    digraphs[i].value = 0;
			  else
			    digraphs[i].value = t;
			  break;
			}
		    }
#endif
		}
	    }
	  break;
	}
      Input("Enter digraph: ", 10, INP_EVERY, digraph_fn, NULL, 0);
      if (*args && **args)
	{
	  s = *args;
	  n = strlen(s);
	  LayProcess(&s, &n);
	}
      break;

    case RC_DEFHSTATUS:
      if (*args == 0)
	{
	  char buf[256];
          *buf = 0;
	  if (nwin_default.hstatus)
            AddXChars(buf, sizeof(buf), nwin_default.hstatus);
	  OutputMsg(0, "default hstatus is '%s'", buf);
	  break;
        }
      (void)ParseSaveStr(act, &nwin_default.hstatus);
      if (*nwin_default.hstatus == 0)
	{
	  free(nwin_default.hstatus);
	  nwin_default.hstatus = 0;
	}
      break;
    case RC_HSTATUS:
      (void)ParseSaveStr(act, &fore->w_hstatus);
      if (*fore->w_hstatus == 0)
	{
	  free(fore->w_hstatus);
	  fore->w_hstatus = 0;
	}
      WindowChanged(fore, 'h');
      break;

#ifdef FONT
    case RC_DEFCHARSET:
    case RC_CHARSET:
      if (*args == 0)
        {
	  char buf[256];
          *buf = 0;
	  if (nwin_default.charset)
            AddXChars(buf, sizeof(buf), nwin_default.charset);
	  OutputMsg(0, "default charset is '%s'", buf);
	  break;
        }
      n = strlen(*args);
      if (n == 0 || n > 6)
	{
	  OutputMsg(0, "%s: %s: string has illegal size.", rc_name, comms[nr].name);
	  break;
	}
      if (n > 4 && (
        ((args[0][4] < '0' || args[0][4] > '3') && args[0][4] != '.') ||
        ((args[0][5] < '0' || args[0][5] > '3') && args[0][5] && args[0][5] != '.')))
	{
	  OutputMsg(0, "%s: %s: illegal mapping number.", rc_name, comms[nr].name);
	  break;
	}
      if (nr == RC_CHARSET)
	{
	  SetCharsets(fore, *args);
	  break;
	}
      if (nwin_default.charset)
	free(nwin_default.charset);
      nwin_default.charset = SaveStr(*args);
      break;
#endif
#ifdef COLOR
    case RC_ATTRCOLOR:
      s = args[0];
      if (*s >= '0' && *s <= '9')
        i = *s - '0';
      else
	for (i = 0; i < 8; i++)
	  if (*s == "dubrsBiI"[i])
	    break;
      s++;
      nr = 0;
      if (*s && s[1] && !s[2])
	{
	  if (*s == 'd' && s[1] == 'd')
	    nr = 3;
	  else if (*s == '.' && s[1] == 'd')
	    nr = 2;
	  else if (*s == 'd' && s[1] == '.')
	    nr = 1;
	  else if (*s != '.' || s[1] != '.')
	    s--;
	  s += 2;
	}
      if (*s || i < 0 || i >= 8)
	{
	  OutputMsg(0, "%s: attrcolor: unknown attribute '%s'.", rc_name, args[0]);
	  break;
	}
      n = 0;
      if (args[1])
        n = ParseAttrColor(args[1], args[2], 1);
      if (n == -1)
	break;
      attr2color[i][nr] = n;
      n = 0;
      for (i = 0; i < 8; i++)
	if (attr2color[i][0] || attr2color[i][1] || attr2color[i][2] || attr2color[i][3])
	  n |= 1 << i;
      nattr2color = n;
      break;
#endif
    case RC_RENDITION:
      i = -1;
      if (strcmp(args[0], "bell") == 0)
	{
	  i = REND_BELL;
	}
      else if (strcmp(args[0], "monitor") == 0)
	{
	  i = REND_MONITOR;
	}
      else if (strcmp(args[0], "silence") == 0)
	{
	  i = REND_SILENCE;
	}
      else if (strcmp(args[0], "so") != 0)
	{
	  OutputMsg(0, "Invalid option '%s' for rendition", args[0]);
	  break;
	}

      ++args;
      ++argl;

      if (i != -1)
	{
	  renditions[i] = ParseAttrColor(args[0], args[1], 1);
	  WindowChanged((struct win *)0, 'w');
	  WindowChanged((struct win *)0, 'W');
	  WindowChanged((struct win *)0, 0);
	  break;
	}

      /* We are here, means we want to set the sorendition. */
      /* FALLTHROUGH*/
    case RC_SORENDITION:
      i = 0;
      if (*args)
	{
          i = ParseAttrColor(*args, args[1], 1);
	  if (i == -1)
	    break;
	  ApplyAttrColor(i, &mchar_so);
	  WindowChanged((struct win *)0, 0);
	  debug2("--> %x %x\n", mchar_so.attr, mchar_so.color);
	}
      if (msgok)
#ifdef COLOR
        OutputMsg(0, "Standout attributes 0x%02x  color 0x%02x", (unsigned char)mchar_so.attr, 0x99 ^ (unsigned char)mchar_so.color);
#else
        OutputMsg(0, "Standout attributes 0x%02x ", (unsigned char)mchar_so.attr);
#endif
      break;

      case RC_SOURCE:
	do_source(*args);
	break;

#ifdef MULTIUSER
    case RC_SU:
      s = NULL;
      if (!*args)
        {
	  OutputMsg(0, "%s:%s screen login", HostName, SockPath);
          InputSu(D_fore, &D_user, NULL);
	}
      else if (!args[1])
        InputSu(D_fore, &D_user, args[0]);
      else if (!args[2])
        s = DoSu(&D_user, args[0], args[1], "\377");
      else
        s = DoSu(&D_user, args[0], args[1], args[2]);
      if (s)
        OutputMsg(0, "%s", s);
      break;
#endif /* MULTIUSER */
    case RC_SPLIT:
      s = args[0];
      if (s && !strcmp(s, "-v"))
        AddCanvas(SLICE_HORI);
      else
        AddCanvas(SLICE_VERT);
      Activate(-1);
      break;
    case RC_REMOVE:
      RemCanvas();
      Activate(-1);
      break;
    case RC_ONLY:
      OneCanvas();
      Activate(-1);
      break;
    case RC_FIT:
      D_forecv->c_xoff = D_forecv->c_xs;
      D_forecv->c_yoff = D_forecv->c_ys;
      RethinkViewportOffsets(D_forecv);
      ResizeLayer(D_forecv->c_layer, D_forecv->c_xe - D_forecv->c_xs + 1, D_forecv->c_ye - D_forecv->c_ys + 1, 0);
      flayer = D_forecv->c_layer;
      LaySetCursor();
      break;
    case RC_FOCUS:
      {
	struct canvas *cv = 0;
	if (!*args || !strcmp(*args, "next"))
	  cv = D_forecv->c_next ? D_forecv->c_next : D_cvlist;
	else if (!strcmp(*args, "prev"))
	  {
	    for (cv = D_cvlist; cv->c_next && cv->c_next != D_forecv; cv = cv->c_next)
	      ;
	  }
	else if (!strcmp(*args, "top"))
	  cv = D_cvlist;
	else if (!strcmp(*args, "bottom"))
	  {
	    for (cv = D_cvlist; cv->c_next; cv = cv->c_next)
	      ;
	  }
	else if (!strcmp(*args, "up"))
	  cv = FindCanvas(D_forecv->c_xs, D_forecv->c_ys - 1);
	else if (!strcmp(*args, "down"))
	  cv = FindCanvas(D_forecv->c_xs, D_forecv->c_ye + 2);
	else if (!strcmp(*args, "left"))
	  cv = FindCanvas(D_forecv->c_xs - 1, D_forecv->c_ys);
	else if (!strcmp(*args, "right"))
	  cv = FindCanvas(D_forecv->c_xe + 1, D_forecv->c_ys);
	else
	  {
	    OutputMsg(0, "%s: usage: focus [next|prev|up|down|left|right|top|bottom]", rc_name);
	    break;
	  }
	SetForeCanvas(display, cv);
      }
      break;
    case RC_RESIZE:
      i = 0;
      if (D_forecv->c_slorient == SLICE_UNKN)
	{
	  OutputMsg(0, "resize: need more than one region");
	  break;
	}
      for (; *args; args++)
	{
	  if (!strcmp(*args, "-h"))
	    i |= RESIZE_FLAG_H;
	  else if (!strcmp(*args, "-v"))
	    i |= RESIZE_FLAG_V;
	  else if (!strcmp(*args, "-b"))
	    i |= RESIZE_FLAG_H | RESIZE_FLAG_V;
	  else if (!strcmp(*args, "-p"))
	    i |= D_forecv->c_slorient == SLICE_VERT ? RESIZE_FLAG_H : RESIZE_FLAG_V;
	  else if (!strcmp(*args, "-l"))
	    i |= RESIZE_FLAG_L;
	  else
	    break;
	}
      if (*args && args[1])
	{
	  OutputMsg(0, "%s: usage: resize [-h] [-v] [-l] [num]\n", rc_name);
	  break;
	}
      if (*args)
	ResizeRegions(*args, i);
      else
	Input(resizeprompts[i], 20, INP_EVERY, ResizeFin, (char*)0, i);
      break;
    case RC_SETSID:
      (void)ParseSwitch(act, &separate_sids);
      break;
    case RC_EVAL:
      args = SaveArgs(args);
      for (i = 0; args[i]; i++)
	{
	  if (args[i][0])
	    Colonfin(args[i], strlen(args[i]), (char *)0);
	  free(args[i]);
	}
      free(args);
      break;
    case RC_ALTSCREEN:
      (void)ParseSwitch(act, &use_altscreen);
      if (msgok)
        OutputMsg(0, "Will %sdo alternate screen switching", use_altscreen ? "" : "not ");
      break;
    case RC_MAXWIN:
      if (!args[0])
	{
	  OutputMsg(0, "maximum windows allowed: %d", maxwin);
	  break;
	}
      if (ParseNum(act, &n))
	break;
      if (n < 1)
        OutputMsg(0, "illegal maxwin number specified");
      else if (n > 2048)
	OutputMsg(0, "maximum 2048 windows allowed");
      else if (n > maxwin && windows)
	OutputMsg(0, "may increase maxwin only when there's no window");
      else
	{
	  if (!windows)
	    wtab = realloc(wtab, n * sizeof(struct win *));
	  maxwin = n;
	}
      break;
    case RC_BACKTICK:
      if (ParseBase(act, *args, &n, 10, "decimal"))
	break;
      if (!args[1])
        setbacktick(n, 0, 0, (char **)0);
      else
	{
	  int lifespan, tick;
	  if (argc < 4)
	    {
	      OutputMsg(0, "%s: usage: backtick num [lifespan tick cmd args...]", rc_name);
	      break;
	    }
	  if (ParseBase(act, args[1], &lifespan, 10, "decimal"))
	    break;
	  if (ParseBase(act, args[2], &tick, 10, "decimal"))
	    break;
	  setbacktick(n, lifespan, tick, SaveArgs(args + 3));
	}
      WindowChanged(0, '`');
      break;
    case RC_BLANKER:
#ifdef BLANKER_PRG
      if (blankerprg)
	{
          RunBlanker(blankerprg);
	  break;
	}
#endif
      ClearAll();
      CursorVisibility(-1);
      D_blocked = 4;
      break;
#ifdef BLANKER_PRG
    case RC_BLANKERPRG:
      if (!args[0])
	{
	  if (blankerprg)
	    {
	      char path[MAXPATHLEN];
	      char *p = path, **pp;
	      for (pp = blankerprg; *pp; pp++)
		p += snprintf(p, sizeof(path) - (p - path) - 1, "%s ", *pp);
	      *(p - 1) = '\0';
	      OutputMsg(0, "blankerprg: %s", path);
	    }
	  else
	    OutputMsg(0, "No blankerprg set.");
	  break;
	}
      if (blankerprg)
	{
	  char **pp;
	  for (pp = blankerprg; *pp; pp++)
	    free(*pp);
	  free(blankerprg);
	  blankerprg = 0;
	}
      if (args[0][0])
	blankerprg = SaveArgs(args);
      break;
#endif
    case RC_IDLE:
      if (*args)
	{
	  struct display *olddisplay = display;
	  if (!strcmp(*args, "off"))
	    idletimo = 0;
	  else if (args[0][0])
	    idletimo = atoi(*args) * 1000;
	  if (argc > 1)
	    {
	      if ((i = FindCommnr(args[1])) == RC_ILLEGAL)
		{
		  OutputMsg(0, "%s: idle: unknown command '%s'", rc_name, args[1]);
		  break;
		}
	      if (CheckArgNum(i, args + 2) < 0)
		break;
	      ClearAction(&idleaction);
	      SaveAction(&idleaction, i, args + 2, argl + 2);
	    }
	  for (display = displays; display; display = display->d_next)
	    ResetIdle();
	  display = olddisplay;
	}
      if (msgok)
	{
	  if (idletimo)
	    OutputMsg(0, "idle timeout %ds, %s", idletimo / 1000, comms[idleaction.nr].name);
	  else
	    OutputMsg(0, "idle off");
	}
      break;
    case RC_FOCUSMINSIZE:
      for (i = 0; i < 2 && args[i]; i++)
	{
	  if (!strcmp(args[i], "max") || !strcmp(args[i], "_"))
	    n = -1;
	  else
	    n = atoi(args[i]);
	  if (i == 0)
	    focusminwidth = n;
	  else
            focusminheight = n;
	}
      if (msgok)
	{
	  char b[2][20];
	  for (i = 0; i < 2; i++)
	    {
	      n = i == 0 ? focusminwidth : focusminheight;
	      if (n == -1)
		strcpy(b[i], "max");
	      else
		sprintf(b[i], "%d", n);
	    }
          OutputMsg(0, "focus min size is %s %s\n", b[0], b[1]);
	}
      break;
    case RC_GROUP:
      if (*args)
	{
	  fore->w_group = 0;
	  if (args[0][0])
	    {
	      fore->w_group = WindowByName(*args);
	      if (fore->w_group == fore || (fore->w_group && fore->w_group->w_type != W_TYPE_GROUP))
		fore->w_group = 0;
	    }
	  WindowChanged((struct win *)0, 'w');
	  WindowChanged((struct win *)0, 'W');
	  WindowChanged((struct win *)0, 0);
	}
      if (msgok)
	{
	  if (fore->w_group)
	    OutputMsg(0, "window group is %d (%s)\n", fore->w_group->w_number, fore->w_group->w_title);
	  else
	    OutputMsg(0, "window belongs to no group");
	}
      break;
    case RC_LAYOUT:
      // A number of the subcommands for "layout" are ignored, or not processed correctly when there
      // is no attached display.

      if (!strcmp(args[0], "title"))
	{
          if (!display)
            {
	      if (!args[1])  // There is no display, and there is no new title. Ignore.
		break;
	      if (!layout_attach || layout_attach == &layout_last_marker)
		layout_attach = CreateLayout(args[1], 0);
	      else
		RenameLayout(layout_attach, args[1]);
	      break;
	    }

	  if (!D_layout)
	    {
	      OutputMsg(0, "not on a layout");
	      break;
	    }
	  if (!args[1])
	    {
	      OutputMsg(0, "current layout is %d (%s)", D_layout->lay_number, D_layout->lay_title);
	      break;
	    }
	  RenameLayout(D_layout, args[1]);
	}
      else if (!strcmp(args[0], "number"))
	{
	  if (!display)
	    {
	      if (args[1] && layout_attach && layout_attach != &layout_last_marker)
		RenumberLayout(layout_attach, atoi(args[1]));
	      break;
	    }

	  if (!D_layout)
	    {
	      OutputMsg(0, "not on a layout");
	      break;
	    }
	  if (!args[1])
	    {
	      OutputMsg(0, "This is layout %d (%s).\n", D_layout->lay_number, D_layout->lay_title);
	      break;
	    }
	   RenumberLayout(D_layout, atoi(args[1]));
	   break;
	}
      else if (!strcmp(args[0], "autosave"))
	{
	  if (!display)
	    {
	      if (args[1] && layout_attach && layout_attach != &layout_last_marker)
		{
		  if (!strcmp(args[1], "on"))
		    layout_attach->lay_autosave = 1;
		  else if (!strcmp(args[1], "off"))
		    layout_attach->lay_autosave = 0;
		}
	      break;
	    }

	  if (!D_layout)
	    {
	      OutputMsg(0, "not on a layout");
	      break;
	    }
	  if (args[1])
	    {
	      if (!strcmp(args[1], "on"))
		D_layout->lay_autosave = 1;
	      else if (!strcmp(args[1], "off"))
		D_layout->lay_autosave = 0;
	      else
		{
		  OutputMsg(0, "invalid argument. Give 'on' or 'off");
		  break;
		}
	    }
	  if (msgok)
	    OutputMsg(0, "autosave is %s", D_layout->lay_autosave ? "on" : "off");
	}
      else if (!strcmp(args[0], "new"))
	{
	  char *t = args[1];
	  n = 0;
	  if (t)
	    {
	      while (*t >= '0' && *t <= '9')
		t++;
	      if (t != args[1] && (!*t || *t == ':'))
		{
		  n = atoi(args[1]);
		  if (*t)
		    t++;
		}
	      else
		t = args[1];
	    }
	  if (!t || !*t)
	    t = "layout";
          NewLayout(t, n);
	  Activate(-1);
	}
      else if (!strcmp(args[0], "save"))
	{
	  if (!args[1])
	    {
	      OutputMsg(0, "usage: layout save <name>");
	      break;
	    }
	  if (display)
	    SaveLayout(args[1], &D_canvas);
	}
      else if (!strcmp(args[0], "select"))
	{
	  if (!display)
	    {
	      if (args[1])
		layout_attach = FindLayout(args[1]);
	      break;
	    }
          if (!args[1])
	    {
	      Input("Switch to layout: ", 20, INP_COOKED, SelectLayoutFin, NULL, 0);
	      break;
	    }
	  SelectLayoutFin(args[1], strlen(args[1]), (char *)0);
	}
      else if (!strcmp(args[0], "next"))
	{
	  if (!display)
	    {
	      if (layout_attach && layout_attach != &layout_last_marker)
		layout_attach = layout_attach->lay_next ? layout_attach->lay_next : layouts;;
	      break;
	    }
	  struct layout *lay = D_layout;
	  if (lay)
	    lay = lay->lay_next ? lay->lay_next : layouts;
	  else
	    lay = layouts;
	  if (!lay)
	    {
	      OutputMsg(0, "no layout defined");
	      break;
	    }
	  if (lay == D_layout)
	    break;
	  LoadLayout(lay, &D_canvas);
	  Activate(-1);
	}
      else if (!strcmp(args[0], "prev"))
	{
	  struct layout *lay = display ? D_layout : layout_attach;
	  struct layout *target = lay;
	  if (lay)
	    {
	      for (lay = layouts; lay->lay_next && lay->lay_next != target; lay = lay->lay_next)
		;
	    }
	  else
	    lay = layouts;

	  if (!display)
	    {
	      layout_attach = lay;
	      break;
	    }

	  if (!lay)
	    {
	      OutputMsg(0, "no layout defined");
	      break;
	    }
	  if (lay == D_layout)
	    break;
	  LoadLayout(lay, &D_canvas);
	  Activate(-1);
	}
      else if (!strcmp(args[0], "attach"))
	{
	  if (!args[1])
	    {
	      if (!layout_attach)
	        OutputMsg(0, "no attach layout set");
	      else if (layout_attach == &layout_last_marker)
	        OutputMsg(0, "will attach to last layout");
	      else
	        OutputMsg(0, "will attach to layout %d (%s)", layout_attach->lay_number, layout_attach->lay_title);
	      break;
	    }
	  if (!strcmp(args[1], ":last"))
	    layout_attach = &layout_last_marker;
	  else if (!args[1][0])
	    layout_attach = 0;
	  else
	    {
	      struct layout *lay;
	      lay = FindLayout(args[1]);
	      if (!lay)
		{
		  OutputMsg(0, "unknown layout '%s'", args[1]);
		  break;
		}
	      layout_attach = lay;
	    }
	}
      else if (!strcmp(args[0], "show"))
	{
	  ShowLayouts(-1);
	}
      else if (!strcmp(args[0], "remove"))
	{
	  struct layout *lay = display ? D_layout : layouts;
	  if (args[1])
	    {
	      lay = layouts ? FindLayout(args[1]) : (struct layout *)0;
	      if (!lay)
		{
		  OutputMsg(0, "unknown layout '%s'", args[1]);
		  break;
		}
	    }
	  if (lay)
	    RemoveLayout(lay);
	}
      else if (!strcmp(args[0], "dump"))
	{
	  if (!display)
	    OutputMsg(0, "Must have a display for 'layout dump'.");
	  else if (!LayoutDumpCanvas(&D_canvas, args[1] ? args[1] : "layout-dump"))
	    OutputMsg(errno, "Error dumping layout.");
	  else
	    OutputMsg(0, "Layout dumped to \"%s\"", args[1] ? args[1] : "layout-dump");
	}
      else
	OutputMsg(0, "unknown layout subcommand");
      break;
#ifdef DW_CHARS
    case RC_CJKWIDTH:
      if(ParseSwitch(act, &cjkwidth) == 0)
      {
        if(msgok)
          OutputMsg(0, "Treat ambiguous width characters as %s width", cjkwidth ? "full" : "half");
      }
      break;
#endif
    default:
      break;
    }
  if (display != odisplay)
    {
      for (display = displays; display; display = display->d_next)
        if (display == odisplay)
	  break;
    }
}
#undef OutputMsg

void
CollapseWindowlist()
/* renumber windows from 0, leaving no gaps */
{
  int pos, moveto=0;

  for (pos = 1; pos < MAXWIN; pos++)
    if (wtab[pos])
      for (; moveto < pos; moveto++)
        if (!wtab[moveto])
          {
          WindowChangeNumber(pos, moveto);
          break;
          }
}

void
DoCommand(char **argv, int *argl) 
{
  struct action act;
  const char *cmd = *argv;

  act.quiet = 0;
  /* For now, we actually treat both 'supress error' and 'suppress normal message' as the
   * same, and ignore all messages on either flag. If we wanted to do otherwise, we would
   * need to change the definition of 'OutputMsg' slightly. */
  if (*cmd == '@')	/* Suppress error */
    {
      act.quiet |= 0x01;
      cmd++;
    }
  if (*cmd == '-')	/* Suppress normal message */
    {
      act.quiet |= 0x02;
      cmd++;
    }

  if ((act.nr = FindCommnr(cmd)) == RC_ILLEGAL)
    {
      Msg(0, "%s: unknown command '%s'", rc_name, cmd);
      return;
    }
  act.args = argv + 1;
  act.argl = argl + 1;
  DoAction(&act, -1);
}

static void
SaveAction(struct action *act, int nr, char **args, int *argl)
{
  register int argc = 0;
  char **pp;
  int *lp;

  if (args)
    while (args[argc])
      argc++;
  if (argc == 0)
    {
      act->nr = nr;
      act->args = noargs;
      act->argl = 0;
      return;
    }
  if ((pp = (char **)malloc((unsigned)(argc + 1) * sizeof(char **))) == 0)
    Panic(0, "%s", strnomem);
  if ((lp = (int *)malloc((unsigned)(argc) * sizeof(int *))) == 0)
    Panic(0, "%s", strnomem);
  act->nr = nr;
  act->args = pp;
  act->argl = lp;
  while (argc--)
    {
      *lp = argl ? *argl++ : (int)strlen(*args);
      *pp++ = SaveStrn(*args++, *lp++);
    }
  *pp = 0;
}

static char **
SaveArgs(char **args)
{
  register char **ap, **pp;
  register int argc = 0;

  while (args[argc])
    argc++;
  if ((pp = ap = (char **)malloc((unsigned)(argc + 1) * sizeof(char **))) == 0)
    Panic(0, "%s", strnomem);
  while (argc--)
    *pp++ = SaveStr(*args++);
  *pp = 0;
  return ap;
}


/*
 * buf is split into argument vector args.
 * leading whitespace is removed.
 * @!| abbreviations are expanded.
 * the end of buffer is recognized by '\0' or an un-escaped '#'.
 * " and ' are interpreted.
 *
 * argc is returned.
 */
int 
Parse(char *buf, int bufl, char **args, int *argl)
{
  register char *p = buf, **ap = args, *pp;
  register int delim, argc;
  int *lp = argl;

  debug2("Parse %d %s\n", bufl, buf);
  argc = 0;
  pp = buf;
  delim = 0;
  for (;;)
    {
      *lp = 0;
      while (*p && (*p == ' ' || *p == '\t'))
	++p;
#ifdef PSEUDOS
      if (argc == 0 && *p == '!')
	{
	  *ap++ = "exec";
	  *lp++ = 4;
	  p++;
	  argc++;
	  continue;
        }
#endif
      if (*p == '\0' || *p == '#' || *p == '\n')
	{
	  *p = '\0';
	  for (delim = 0; delim < argc; delim++)
	    debug1("-- %s\n", args[delim]);
	  args[argc] = 0;
	  return argc;
	}
      if (++argc >= MAXARGS)
	{
	  Msg(0, "%s: too many tokens.", rc_name);
	  return 0;
	}
      *ap++ = pp;

      debug1("- new arg %s\n", p);
      while (*p)
	{
	  if (*p == delim)
	    delim = 0;
	  else if (delim != '\'' && *p == '\\' && (p[1] == 'n' || p[1] == 'r' || p[1] == 't' || p[1] == '\'' || p[1] == '"' || p[1] == '\\' || p[1] == '$' || p[1] == '#' || p[1] == '^' || (p[1] >= '0' && p[1] <= '7')))
	    {
	      p++;
	      if (*p >= '0' && *p <= '7')
		{
		  *pp = *p - '0';
		  if (p[1] >= '0' && p[1] <= '7')
		    {
		      p++;
		      *pp = (*pp << 3) | (*p - '0');
		      if (p[1] >= '0' && p[1] <= '7')
			{
			  p++;
			  *pp = (*pp << 3) | (*p - '0');
			}
		    }
		  pp++;
		}
	      else
		{
		  switch (*p)
		    {
		      case 'n': *pp = '\n'; break;
		      case 'r': *pp = '\r'; break;
		      case 't': *pp = '\t'; break;
		      default: *pp = *p; break;
		    }
		  pp++;
		}
	    }
	  else if (delim != '\'' && *p == '$' && (p[1] == '{' || p[1] == ':' || (p[1] >= 'a' && p[1] <= 'z') || (p[1] >= 'A' && p[1] <= 'Z') || (p[1] >= '0' && p[1] <= '9') || p[1] == '_'))

	    {
	      char *ps, *pe, op, *v, xbuf[11], path[MAXPATHLEN];
	      int vl;

	      ps = ++p;
	      debug1("- var %s\n", ps);
	      p++;
	      while (*p)
		{
		  if (*ps == '{' && *p == '}')
		    break;
		  if (*ps == ':' && *p == ':')
		    break;
		  if (*ps != '{' && *ps != ':' && (*p < 'a' || *p > 'z') && (*p < 'A' || *p > 'Z') && (*p < '0' || *p > '9') && *p != '_')
		    break;
		  p++;
		}
	      pe = p;
	      if (*ps == '{' || *ps == ':')
		{
		  if (!*p)
		    {
		      Msg(0, "%s: bad variable name.", rc_name);
		      return 0;
		    }
		  p++;
		}
	      op = *pe;
	      *pe = 0;
	      debug1("- var is '%s'\n", ps);
	      if (*ps == ':')
		v = gettermcapstring(ps + 1);
	      else
		{
		  if (*ps == '{')
		    ps++;
		  v = xbuf;
		  if (!strcmp(ps, "TERM"))
		    v = display ? D_termname : "unknown";
		  else if (!strcmp(ps, "COLUMNS"))
		    sprintf(xbuf, "%d", display ? D_width : -1);
		  else if (!strcmp(ps, "LINES"))
		    sprintf(xbuf, "%d", display ? D_height : -1);
		  else if (!strcmp(ps, "PID"))
		    sprintf(xbuf, "%d", getpid());
		  else if (!strcmp(ps, "PWD"))
		    {
		      if (getcwd(path, sizeof(path) - 1) == 0)
			v = "?";
		      else
			v = path;
		    }
		  else if (!strcmp(ps, "STY"))
		    {
		      if ((v = strchr(SockName, '.')))	/* Skip the PID */
			v++;
		      else
			v = SockName;
		    }
		  else
		    v = getenv(ps);
		}
	      *pe = op;
	      vl = v ? strlen(v) : 0;
	      if (vl)
		{
		  debug1("- sub is '%s'\n", v);
		  if (p - pp < vl)
		    {
		      int right = buf + bufl - (p + strlen(p) + 1);
		      if (right > 0)
			{
			  bcopy(p, p + right, strlen(p) + 1);
			  p += right;
			}
		    }
		  if (p - pp < vl)
		    {
		      Msg(0, "%s: no space left for variable expansion.", rc_name);
		      return 0;
		    }
		  bcopy(v, pp, vl);
		  pp += vl;
		}
	      continue;
	    }
	  else if (delim != '\'' && *p == '^' && p[1])
	    {
	      p++;
	      *pp++ = *p == '?' ? '\177' : *p & 0x1f;
	    }
	  else if (delim == 0 && (*p == '\'' || *p == '"'))
	    delim = *p;
	  else if (delim == 0 && (*p == ' ' || *p == '\t' || *p == '\n'))
	    break;
	  else
	    *pp++ = *p;
	  p++;
	}
      if (delim)
	{
	  Msg(0, "%s: Missing %c quote.", rc_name, delim);
	  return 0;
	}
      if (*p)
	p++;
      *pp = 0;
      debug2("- arg done, '%s' rest %s\n", ap[-1], p);
      *lp++ = pp - ap[-1];
      pp++;
    }
}

void
SetEscape(struct acluser *u, int e, int me)
{
  if (u)
    {
      u->u_Esc = e;
      u->u_MetaEsc = me;
    }
  else
    {
      if (users)
	{
	  if (DefaultEsc >= 0)
	    ClearAction(&ktab[DefaultEsc]);
	  if (DefaultMetaEsc >= 0)
	    ClearAction(&ktab[DefaultMetaEsc]);
	}
      DefaultEsc = e;
      DefaultMetaEsc = me;
      if (users)
	{
	  if (DefaultEsc >= 0)
	    {
	      ClearAction(&ktab[DefaultEsc]);
	      ktab[DefaultEsc].nr = RC_OTHER;
	    }
	  if (DefaultMetaEsc >= 0)
	    {
	      ClearAction(&ktab[DefaultMetaEsc]);
	      ktab[DefaultMetaEsc].nr = RC_META;
	    }
	}
    }
}

int
ParseSwitch(struct action *act, int *var)
{
  if (*act->args == 0)
    {
      *var ^= 1;
      return 0;
    }
  return ParseOnOff(act, var);
}

static int
ParseOnOff(struct action *act, int *var)
{
  register int num = -1;
  char **args = act->args;

  if (args[1] == 0)
    {
      if (strcmp(args[0], "on") == 0)
	num = 1;
      else if (strcmp(args[0], "off") == 0)
	num = 0;
    }
  if (num < 0)
    {
      Msg(0, "%s: %s: invalid argument. Give 'on' or 'off'", rc_name, comms[act->nr].name);
      return -1;
    }
  *var = num;
  return 0;
}

int
ParseSaveStr(struct action *act, char **var)
{
  char **args = act->args;
  if (*args == 0 || args[1])
    {
      Msg(0, "%s: %s: one argument required.", rc_name, comms[act->nr].name);
      return -1;
    }
  if (*var)
    free(*var);
  *var = SaveStr(*args);
  return 0;
}

int
ParseNum(struct action *act, int *var)
{
  int i;
  char *p, **args = act->args;

  p = *args;
  if (p == 0 || *p == 0 || args[1])
    {
      Msg(0, "%s: %s: invalid argument. Give one argument.",
          rc_name, comms[act->nr].name);
      return -1;
    }
  i = 0; 
  while (*p)
    {
      if (*p >= '0' && *p <= '9')
	i = 10 * i + (*p - '0');
      else
	{
	  Msg(0, "%s: %s: invalid argument. Give numeric argument.",
	      rc_name, comms[act->nr].name);
	  return -1;
	}    
      p++;
    }
  debug1("ParseNum got %d\n", i);
  *var = i;
  return 0;
}

static int
ParseNum1000(struct action *act, int *var)
{
  int i;
  char *p, **args = act->args;
  int dig = 0;

  p = *args;
  if (p == 0 || *p == 0 || args[1])
    {
      Msg(0, "%s: %s: invalid argument. Give one argument.",
          rc_name, comms[act->nr].name);
      return -1;
    }
  i = 0; 
  while (*p)
    {
      if (*p >= '0' && *p <= '9')
	{
	  if (dig < 4)
	    i = 10 * i + (*p - '0');
          else if (dig == 4 && *p >= '5')
	    i++;
	  if (dig)
	    dig++;
	}
      else if (*p == '.' && !dig)
        dig++;
      else
	{
	  Msg(0, "%s: %s: invalid argument. Give floating point argument.",
	      rc_name, comms[act->nr].name);
	  return -1;
	}    
      p++;
    }
  if (dig == 0)
    i *= 1000;
  else
    while (dig++ < 4)
      i *= 10;
  if (i < 0)
    i = (int)((unsigned int)~0 >> 1);
  debug1("ParseNum1000 got %d\n", i);
  *var = i;
  return 0;
}

static struct win *
WindowByName(char *s)
{
  struct win *p;

  for (p = windows; p; p = p->w_next)
    if (!strcmp(p->w_title, s))
      return p;
  for (p = windows; p; p = p->w_next)
    if (!strncmp(p->w_title, s, strlen(s)))
      return p;
  return 0;
}

static int
WindowByNumber(char *str)
{
  int i;
  char *s;

  for (i = 0, s = str; *s; s++)
    {
      if (*s < '0' || *s > '9')
        break;
      i = i * 10 + (*s - '0');
    }
  return *s ? -1 : i;
}

/* 
 * Get window number from Name or Number string.
 * Numbers are tried first, then names, a prefix match suffices.
 * Be careful when assigning numeric strings as WindowTitles.
 */
int
WindowByNoN(char *str)
{
  int i;
  struct win *p;
  
  if ((i = WindowByNumber(str)) < 0 || i >= maxwin)
    {
      if ((p = WindowByName(str)))
	return p->w_number;
      return -1;
    }
  return i;
}

static int
ParseWinNum(struct action *act, int *var)
{
  char **args = act->args;
  int i = 0;

  if (*args == 0 || args[1])
    {
      Msg(0, "%s: %s: one argument required.", rc_name, comms[act->nr].name);
      return -1;
    }
  
  i = WindowByNoN(*args);
  if (i < 0)
    {
      Msg(0, "%s: %s: invalid argument. Give window number or name.",
          rc_name, comms[act->nr].name);
      return -1;
    }
  debug1("ParseWinNum got %d\n", i);
  *var = i;
  return 0;
}

static int
ParseBase(struct action *act, char *p, int *var, int base, char *bname)
{
  int i = 0;
  int c;

  if (*p == 0)
    {
      Msg(0, "%s: %s: empty argument.", rc_name, comms[act->nr].name);
      return -1;
    }
  while ((c = *p++))
    {
      if (c >= 'a' && c <= 'z')
	c -= 'a' - 'A';
      if (c >= 'A' && c <= 'Z')
	c -= 'A' - ('0' + 10);
      c -= '0';
      if (c < 0 || c >= base)
	{
	  Msg(0, "%s: %s: argument is not %s.", rc_name, comms[act->nr].name, bname);
	  return -1;
	}    
      i = base * i + c;
    }
  debug1("ParseBase got %d\n", i);
  *var = i;
  return 0;
}

static int
IsNum(register char *s, register int base)
{
  for (base += '0'; *s; ++s)
    if (*s < '0' || *s > base)
      return 0;
  return 1;
}

int
IsNumColon(char *s, int base, char *p, int psize)
{
  char *q;
  if ((q = rindex(s, ':')) != 0)
    {
      strncpy(p, q + 1, psize - 1);
      p[psize - 1] = '\0';
      *q = '\0';
    }
  else
    *p = '\0';
  return IsNum(s, base);
}

void
SwitchWindow(int n)
{
  struct win *p;

  debug1("SwitchWindow %d\n", n);
  if (n < 0 || n >= maxwin)
    {
      ShowWindows(-1);
      return;
    }
  if ((p = wtab[n]) == 0)
    {
      ShowWindows(n);
      return;
    }
  if (display == 0)
    {
      fore = p;
      return;
    }
  if (p == D_fore)
    {
      Msg(0, "This IS window %d (%s).", n, p->w_title);
      return;
    }
#ifdef MULTIUSER
  if (AclCheckPermWin(D_user, ACL_READ, p))
    {
      Msg(0, "Access to window %d denied.", p->w_number);
      return;
    }
#endif
  SetForeWindow(p);
  Activate(fore->w_norefresh);  
}

/*
 * SetForeWindow changes the window in the input focus of the display.
 * Puts window wi in canvas display->d_forecv.
 */
void
SetForeWindow(struct win *win)
{
  struct win *p;
  if (display == 0)
    {
      fore = win;
      return;
    }
  p = Layer2Window(D_forecv->c_layer);
  SetCanvasWindow(D_forecv, win);
  if (p)
    WindowChanged(p, 'u');
  if (win)
    WindowChanged(win, 'u');
  flayer = D_forecv->c_layer;
  /* Activate called afterwards, so no RefreshHStatus needed */
}


/*****************************************************************/

/* 
 *  Activate - make fore window active
 *  norefresh = -1 forces a refresh, disregard all_norefresh then.
 */
void
Activate(int norefresh)
{
  debug1("Activate(%d)\n", norefresh);
  if (display == 0)
    return;
  if (D_status)
    {
      Msg(0, "%s", "");	/* wait till mintime (keep gcc quiet) */
      RemoveStatus();
    }

  if (MayResizeLayer(D_forecv->c_layer))
    ResizeLayer(D_forecv->c_layer, D_forecv->c_xe - D_forecv->c_xs + 1, D_forecv->c_ye - D_forecv->c_ys + 1, display);

  fore = D_fore;
  if (fore)
    {
      /* XXX ? */
      if (fore->w_monitor != MON_OFF)
	fore->w_monitor = MON_ON;
      fore->w_bell = BELL_ON;
      WindowChanged(fore, 'f');
    }
  Redisplay(norefresh + all_norefresh);
}


static int
NextWindow()
{
  register struct win **pp;
  int n = fore ? fore->w_number : maxwin;
  struct win *group = fore ? fore->w_group : 0;

  for (pp = fore ? wtab + n + 1 : wtab; pp != wtab + n; pp++)
    {
      if (pp == wtab + maxwin)
	pp = wtab;
      if (*pp)
	{
	  if (!fore || group == (*pp)->w_group)
	    break;
	}
    }
  if (pp == wtab + n)
    return -1;
  return pp - wtab;
}

static int
PreviousWindow()
{
  register struct win **pp;
  int n = fore ? fore->w_number : -1;
  struct win *group = fore ? fore->w_group : 0;

  for (pp = wtab + n - 1; pp != wtab + n; pp--)
    {
      if (pp == wtab - 1)
	pp = wtab + maxwin - 1;
      if (*pp)
	{
	  if (!fore || group == (*pp)->w_group)
	    break;
	}
    }
  if (pp == wtab + n)
    return -1;
  return pp - wtab;
}

static int
MoreWindows()
{
  char *m = "No other window.";
  if (windows && (fore == 0 || windows->w_next))
    return 1;
  if (fore == 0)
    {
      Msg(0, "No window available");
      return 0;
    }
  Msg(0, m, fore->w_number);	/* other arg for nethack */
  return 0;
}

void
KillWindow(struct win *win)
{
  struct win **pp, *p;
  struct canvas *cv;
  int gotone;
  struct layout *lay;

  /*
   * Remove window from linked list.
   */
  for (pp = &windows; (p = *pp); pp = &p->w_next)
    if (p == win)
      break;
  ASSERT(p);
  *pp = p->w_next;
  win->w_inlen = 0;
  wtab[win->w_number] = 0;

  if (windows == 0)
    {
      FreeWindow(win);
      Finit(0);
    }

  /*
   * switch to different window on all canvases
   */
  for (display = displays; display; display = display->d_next)
    {
      gotone = 0;
      for (cv = D_cvlist; cv; cv = cv->c_next)
	{
	  if (Layer2Window(cv->c_layer) != win)
	    continue;
	  /* switch to other window */
	  SetCanvasWindow(cv, FindNiceWindow(D_other, 0));
	  gotone = 1;
	}
      if (gotone)
	{
#ifdef ZMODEM
	  if (win->w_zdisplay == display)
	    {
	      D_blocked = 0;
	      D_readev.condpos = D_readev.condneg = 0;
	    }
#endif
	  Activate(-1);
	}
    }

  /* do the same for the layouts */
  for (lay = layouts; lay; lay = lay->lay_next)
    UpdateLayoutCanvas(&lay->lay_canvas, win);

  FreeWindow(win);
  WindowChanged((struct win *)0, 'w');
  WindowChanged((struct win *)0, 'W');
  WindowChanged((struct win *)0, 0);
}

static void
LogToggle(int on)
{
  char buf[1024];

  if ((fore->w_log != 0) == on)
    {
      if (display && !*rc_name)
	Msg(0, "You are %s logging.", on ? "already" : "not");
      return;
    }
  if (fore->w_log != 0)
    {
      Msg(0, "Logfile \"%s\" closed.", fore->w_log->name);
      logfclose(fore->w_log);
      fore->w_log = 0;
      WindowChanged(fore, 'f');
      return;
    }
  if (DoStartLog(fore, buf, sizeof(buf)))
    {
      Msg(errno, "Error opening logfile \"%s\"", buf);
      return;
    }
  if (ftell(fore->w_log->fp) == 0)
    Msg(0, "Creating logfile \"%s\".", fore->w_log->name);
  else
    Msg(0, "Appending to logfile \"%s\".", fore->w_log->name);
  WindowChanged(fore, 'f');
}

char *
AddWindows(char *buf, int len, int flags, int where)
{
  register char *s, *ss;
  register struct win **pp, *p;
  register char *cmd;
  int l;

  s = ss = buf;
  if ((flags & 8) && where < 0)
    {
      *s = 0;
      return ss;
    }
  for (pp = ((flags & 4) && where >= 0) ? wtab + where + 1: wtab; pp < wtab + maxwin; pp++)
    {
      int rend = -1;
      if (pp - wtab == where && ss == buf)
        ss = s;
      if ((p = *pp) == 0)
	continue;
      if ((flags & 1) && display && p == D_fore)
	continue;
      if (display && D_fore && D_fore->w_group != p->w_group)
	continue;

      cmd = p->w_title;
      l = strlen(cmd);
      if (l > 20)
        l = 20;
      if (s - buf + l > len - 24)
	break;
      if (s > buf || (flags & 4))
	{
	  *s++ = ' ';
	  *s++ = ' ';
	}
      if (p->w_number == where)
        {
          ss = s;
          if (flags & 8)
            break;
        }
      if (!(flags & 4) || where < 0 || ((flags & 4) && where < p->w_number))
	{
	  if (p->w_monitor == MON_DONE && renditions[REND_MONITOR] != -1)
	    rend = renditions[REND_MONITOR];
	  else if ((p->w_bell == BELL_DONE || p->w_bell == BELL_FOUND) && renditions[REND_BELL] != -1)
	    rend = renditions[REND_BELL];
	  else if ((p->w_silence == SILENCE_FOUND || p->w_silence == SILENCE_DONE) && renditions[REND_SILENCE] != -1)
	    rend = renditions[REND_SILENCE];
	}
      if (rend != -1)
	AddWinMsgRend(s, rend);
      sprintf(s, "%d", p->w_number);
      s += strlen(s);
      if (display && p == D_fore)
	*s++ = '*';
      if (!(flags & 2))
	{
          if (display && p == D_other)
	    *s++ = '-';
          s = AddWindowFlags(s, len, p);
	}
      *s++ = ' ';
      strncpy(s, cmd, l);
      s += l;
      if (rend != -1)
	AddWinMsgRend(s, -1);
    }
  *s = 0;
  return ss;
}

char *
AddWindowFlags(char *buf, int len, struct win *p)
{
  char *s = buf;
  if (p == 0 || len < 12)
    {
      *s = 0;
      return s;
    }
  if (p->w_layer.l_cvlist && p->w_layer.l_cvlist->c_lnext)
    *s++ = '&';
  if (p->w_monitor == MON_DONE
#ifdef MULTIUSER
      && (ACLBYTE(p->w_mon_notify, D_user->u_id) & ACLBIT(D_user->u_id))
#endif
     )
    *s++ = '@';
  if (p->w_bell == BELL_DONE)
    *s++ = '!';
#ifdef UTMPOK
  if (p->w_slot != (slot_t) 0 && p->w_slot != (slot_t) -1)
    *s++ = '$';
#endif
  if (p->w_log != 0)
    {
      strcpy(s, "(L)");
      s += 3;
    }
  if (p->w_ptyfd < 0 && p->w_type != W_TYPE_GROUP)
    *s++ = 'Z';
  *s = 0;
  return s;
}

char *
AddOtherUsers(char *buf, int len, struct win *p)
{
  struct display *d, *olddisplay = display;
  struct canvas *cv;
  char *s;
  int l;

  s = buf;
  for (display = displays; display; display = display->d_next)
    {
      if (olddisplay && D_user == olddisplay->d_user)
	continue;
      for (cv = D_cvlist; cv; cv = cv->c_next)
	if (Layer2Window(cv->c_layer) == p)
	  break;
      if (!cv)
	continue;
      for (d = displays; d && d != display; d = d->d_next)
	if (D_user == d->d_user)
	  break;
      if (d && d != display)
	continue;
      if (len > 1 && s != buf)
	{
	  *s++ = ',';
	  len--;
	}
      l = strlen(D_user->u_name);
      if (l + 1 > len)
	break;
      strcpy(s, D_user->u_name);
      s += l;
      len -= l;
    }
  *s = 0;
  display = olddisplay;
  return s;
}

void
ShowWindows(int where)
{
  char buf[1024];
  char *s, *ss;

  if (display && where == -1 && D_fore)
    where = D_fore->w_number;
  ss = AddWindows(buf, sizeof(buf), 0, where);
  s = buf + strlen(buf);
  if (display && ss - buf > D_width / 2)
    {
      ss -= D_width / 2;
      if (s - ss < D_width)
	{
	  ss = s - D_width;
	  if (ss < buf)
	    ss = buf;
	}
    }
  else
    ss = buf;
  Msg(0, "%s", ss);
}

static void
ShowInfo()
{
  char buf[512], *p;
  register struct win *wp = fore;
  register int i;

  if (wp == 0)
    {
      Msg(0, "(%d,%d)/(%d,%d) no window", D_x + 1, D_y + 1, D_width, D_height);
      return;
    }
  p = buf;
  if (buf < (p += GetAnsiStatus(wp, p)))
    *p++ = ' ';
  sprintf(p, "(%d,%d)/(%d,%d)",
    wp->w_x + 1, wp->w_y + 1, wp->w_width, wp->w_height);
#ifdef COPY_PASTE
  sprintf(p += strlen(p), "+%d", wp->w_histheight);
#endif
  sprintf(p += strlen(p), " %c%sflow",
  	  (wp->w_flow & FLOW_NOW) ? '+' : '-',
	  (wp->w_flow & FLOW_AUTOFLAG) ? "" : 
	   ((wp->w_flow & FLOW_AUTO) ? "(+)" : "(-)"));
  if (!wp->w_wrap) sprintf(p += strlen(p), " -wrap");
  if (wp->w_insert) sprintf(p += strlen(p), " ins");
  if (wp->w_origin) sprintf(p += strlen(p), " org");
  if (wp->w_keypad) sprintf(p += strlen(p), " app");
  if (wp->w_log)    sprintf(p += strlen(p), " log");
  if (wp->w_monitor != MON_OFF
#ifdef MULTIUSER
      && (ACLBYTE(wp->w_mon_notify, D_user->u_id) & ACLBIT(D_user->u_id))
#endif
     )
    sprintf(p += strlen(p), " mon");
  if (wp->w_mouse) sprintf(p += strlen(p), " mouse");
#ifdef COLOR
  if (wp->w_bce) sprintf(p += strlen(p), " bce");
#endif
  if (!wp->w_c1) sprintf(p += strlen(p), " -c1");
  if (wp->w_norefresh) sprintf(p += strlen(p), " nored");

  p += strlen(p);
#ifdef FONT
# ifdef ENCODINGS
  if (wp->w_encoding && (display == 0 || D_encoding != wp->w_encoding || EncodingDefFont(wp->w_encoding) <= 0))
    {
      *p++ = ' ';
      strcpy(p, EncodingName(wp->w_encoding));
      p += strlen(p);
    }
#  ifdef UTF8
  if (wp->w_encoding != UTF8)
#  endif
# endif
    if (D_CC0 || (D_CS0 && *D_CS0))
      {
	if (wp->w_gr == 2)
	  {
	    sprintf(p, " G%c", wp->w_Charset + '0');
	    if (wp->w_FontE >= ' ')
	      p[3] = wp->w_FontE;
	    else
	      {
	        p[3] = '^';
	        p[4] = wp->w_FontE ^ 0x40;
		p++;
	      }
	    p[4] = '[';
	    p++;
	  }
	else if (wp->w_gr)
	  sprintf(p++, " G%c%c[", wp->w_Charset + '0', wp->w_CharsetR + '0');
	else
	  sprintf(p, " G%c[", wp->w_Charset + '0');
	p += 4;
	for (i = 0; i < 4; i++)
	  {
	    if (wp->w_charsets[i] == ASCII)
	      *p++ = 'B';
	    else if (wp->w_charsets[i] >= ' ')
	      *p++ = wp->w_charsets[i];
	    else
	      {
		*p++ = '^';
		*p++ = wp->w_charsets[i] ^ 0x40;
	      }
	  }
	*p++ = ']';
	*p = 0;
      }
#endif

  if (wp->w_type == W_TYPE_PLAIN)
    {
      /* add info about modem control lines */
      *p++ = ' ';
      TtyGetModemStatus(wp->w_ptyfd, p);
    }
#ifdef BUILTIN_TELNET
  else if (wp->w_type == W_TYPE_TELNET)
    {
      *p++ = ' ';
      TelStatus(wp, p, sizeof(buf) - 1 - (p - buf));
    }
#endif
  Msg(0, "%s %d(%s)", buf, wp->w_number, wp->w_title);
}

static void
ShowDInfo()
{
  char buf[512], *p;
  if (display == 0)
    return;
  p = buf;
  sprintf(p, "(%d,%d)", D_width, D_height),
  p += strlen(p);
#ifdef ENCODINGS
  if (D_encoding)
    {
      *p++ = ' ';
      strcpy(p, EncodingName(D_encoding));
      p += strlen(p);
    }
#endif
  if (D_CXT)
    {
      strcpy(p, " xterm");
      p += strlen(p);
    }
#ifdef COLOR
  if (D_hascolor)
    {
      strcpy(p, " color");
      p += strlen(p);
    }
#endif
#ifdef FONT
  if (D_CG0)
    {
      strcpy(p, " iso2022");
      p += strlen(p);
    }
  else if (D_CS0 && *D_CS0)
    {
      strcpy(p, " altchar");
      p += strlen(p);
    }
#endif
  Msg(0, "%s", buf);
}

static void
AKAfin(char *buf, int len, char *data)
{
  ASSERT(display);
  if (len && fore)
    ChangeAKA(fore, buf, strlen(buf));

  enter_window_name_mode = 0;
}

static void
InputAKA()
{
  char *s, *ss;
  int n;

  if (enter_window_name_mode == 1) return;

  enter_window_name_mode = 1;

  Input("Set window's title to: ", sizeof(fore->w_akabuf) - 1, INP_COOKED, AKAfin, NULL, 0);
  s = fore->w_title;
  if (!s)
    return;
  for (; *s; s++)
    {
      if ((*(unsigned char *)s & 0x7f) < 0x20 || *s == 0x7f)
	continue;
      ss = s;
      n = 1;
      LayProcess(&ss, &n);
    }
}

static void
Colonfin(char *buf, int len, char *data)
{
  char mbuf[256];

  RemoveStatus();
  if (buf[len] == '\t')
    {
      int m, x;
      int l = 0, r = RC_LAST;
      int showmessage = 0;
      char *s = buf;

      while (*s && s - buf < len)
	if (*s++ == ' ')
	  return;

      /* Showing a message when there's no hardstatus or caption cancels the input */
      if (display &&
	  (captionalways || D_has_hstatus == HSTATUS_LASTLINE || (D_canvas.c_slperp && D_canvas.c_slperp->c_slnext)))
	showmessage = 1;

      while (l <= r)
	{
	  m = (l + r) / 2;
	  x = strncmp(buf, comms[m].name, len);
	  if (x > 0)
	    l = m + 1;
	  else if (x < 0)
	    r = m - 1;
	  else
	    {
	      s = mbuf;
	      for (l = m - 1; l >= 0 && strncmp(buf, comms[l].name, len) == 0; l--)
		;
	      for (m = ++l; m <= r && strncmp(buf, comms[m].name, len) == 0 && s - mbuf < sizeof(mbuf); m++)
		s += snprintf(s, sizeof(mbuf) - (s - mbuf), " %s", comms[m].name);
	      if (l < m - 1)
		{
		  if (showmessage)
		    Msg(0, "Possible commands:%s", mbuf);
		}
	      else
		{
		  s = mbuf;
		  len = snprintf(mbuf, sizeof(mbuf), "%s \t", comms[l].name + len);
		  if (len > 0 && len < sizeof(mbuf))
		    LayProcess(&s, &len);
		}
	      break;
	    }
	}
      if (l > r && showmessage)
	Msg(0, "No commands matching '%*s'", len, buf);
      return;
    }

  if (!len || buf[len])
    return;

  len = strlen(buf) + 1;
  if (len > (int)sizeof(mbuf))
    RcLine(buf, len);
  else
    {
      bcopy(buf, mbuf, len);
      RcLine(mbuf, sizeof mbuf);
    }
}

static void
SelectFin(char *buf, int len, char *data)
{
  int n;

  if (!len || !display)
    return;
  if (len == 1 && *buf == '-')
    {
      SetForeWindow((struct win *)0);
      Activate(0);
      return;
    }
  if ((n = WindowByNoN(buf)) < 0)
    return;
  SwitchWindow(n);
}

static void
SelectLayoutFin(char *buf, int len, char *data)
{
  struct layout *lay;

  if (!len || !display)
    return;
  if (len == 1 && *buf == '-')
    {
      LoadLayout((struct layout *)0, (struct canvas *)0);
      Activate(0);
      return;
    }
  lay = FindLayout(buf);
  if (!lay)
    Msg(0, "No such layout\n");
  else if (lay == D_layout)
    Msg(0, "This IS layout %d (%s).\n", lay->lay_number, lay->lay_title);
  else
    {
      LoadLayout(lay, &D_canvas);
      Activate(0);
    }
}

    
static void
InputSelect()
{
  Input("Switch to window: ", 20, INP_COOKED, SelectFin, NULL, 0);
}

static char setenv_var[31];


static void
SetenvFin1(char *buf, int len, char *data)
{
  if (!len || !display)
    return;
  InputSetenv(buf);
}
  
static void
SetenvFin2(char *buf, int len, char *data)
{
  if (!len || !display)
    return;
  debug2("SetenvFin2: setenv '%s' '%s'\n", setenv_var, buf);
  xsetenv(setenv_var, buf);
  MakeNewEnv();
}

static void
InputSetenv(char *arg)
{
  static char setenv_buf[50 + sizeof(setenv_var)];	/* need to be static here, cannot be freed */

  if (arg)
    {
      strncpy(setenv_var, arg, sizeof(setenv_var) - 1);
      sprintf(setenv_buf, "Enter value for %s: ", setenv_var);
      Input(setenv_buf, 30, INP_COOKED, SetenvFin2, NULL, 0);
    }
  else
    Input("Setenv: Enter variable name: ", 30, INP_COOKED, SetenvFin1, NULL, 0);
}

/*
 * the following options are understood by this parser:
 * -f, -f0, -f1, -fy, -fa
 * -t title, -T terminal-type, -h height-of-scrollback, 
 * -ln, -l0, -ly, -l1, -l
 * -a, -M, -L
 */
void
DoScreen(char *fn, char **av)
{
  struct NewWindow nwin;
  register int num;
  char buf[20];

  nwin = nwin_undef;
  while (av && *av && av[0][0] == '-')
    {
      if (av[0][1] == '-')
	{
	  av++;
	  break;
	}
      switch (av[0][1])
	{
	case 'f':
	  switch (av[0][2])
	    {
	    case 'n':
	    case '0':
	      nwin.flowflag = FLOW_NOW * 0;
	      break;
	    case 'y':
	    case '1':
	    case '\0':
	      nwin.flowflag = FLOW_NOW * 1;
	      break;
	    case 'a':
	      nwin.flowflag = FLOW_AUTOFLAG;
	      break;
	    default:
	      break;
	    }
	  break;
	case 't':	/* no more -k */
	  if (av[0][2])
	    nwin.aka = &av[0][2];
	  else if (*++av)
	    nwin.aka = *av;
	  else
	    --av;
	  break;
	case 'T':
	  if (av[0][2])
	    nwin.term = &av[0][2];
	  else if (*++av)
	    nwin.term = *av;
	  else
	    --av;
	  break;
	case 'h':
	  if (av[0][2])
	    nwin.histheight = atoi(av[0] + 2);
	  else if (*++av)
	    nwin.histheight = atoi(*av);
	  else 
	    --av;
	  break;
#ifdef LOGOUTOK
	case 'l':
	  switch (av[0][2])
	    {
	    case 'n':
	    case '0':
	      nwin.lflag = 0;
	      break;
	    case 'y':
	    case '1':
	    case '\0':
	      nwin.lflag = 1;
	      break;
	    case 'a':
	      nwin.lflag = 3;
	      break;
	    default:
	      break;
	    }
	  break;
#endif
	case 'a':
	  nwin.aflag = 1;
	  break;
	case 'M':
	  nwin.monitor = MON_ON;
	  break;
	case 'L':
	  nwin.Lflag = 1;
	  break;
	default:
	  Msg(0, "%s: screen: invalid option -%c.", fn, av[0][1]);
	  break;
	}
      ++av;
    }
  num = 0;
  if (av && *av && IsNumColon(*av, 10, buf, sizeof(buf)))
    {
      if (*buf != '\0')
	nwin.aka = buf;
      num = atoi(*av);
      if (num < 0 || (maxwin && num > maxwin - 1) || (!maxwin && num > MAXWIN - 1))
	{
	  Msg(0, "%s: illegal screen number %d.", fn, num);
	  num = 0;
	}
      nwin.StartAt = num;
      ++av;
    }
  if (av && *av)
    {
      nwin.args = av;
      if (!nwin.aka)
        nwin.aka = Filename(*av);
    }
  MakeWindow(&nwin);
}

#ifdef COPY_PASTE
/*
 * CompileKeys must be called before Markroutine is first used.
 * to initialise the keys with defaults, call CompileKeys(NULL, mark_key_tab);
 *
 * s is an ascii string in a termcap-like syntax. It looks like
 *   "j=u:k=d:l=r:h=l: =.:" and so on...
 * this example rebinds the cursormovement to the keys u (up), d (down),
 * l (left), r (right). placing a mark will now be done with ".".
 */
int
CompileKeys(char *s, int sl, unsigned char *array)
{
  int i;
  unsigned char key, value;

  if (sl == 0)
    {
      for (i = 0; i < 256; i++)
        array[i] = i;
      return 0;
    }
  debug1("CompileKeys: '%s'\n", s);
  while (sl)
    {
      key = *(unsigned char *)s++;
      if (*s != '=' || sl < 3)
	return -1;
      sl--;
      do 
	{
	  s++;
	  sl -= 2;
	  value = *(unsigned char *)s++;
	  array[value] = key;
	}
      while (*s == '=' && sl >= 2);
      if (sl == 0) 
	break;
      if (*s++ != ':')
	return -1;
      sl--;
    }
  return 0;
}
#endif /* COPY_PASTE */

/*
 *  Asynchronous input functions
 */

#if defined(DETACH) && defined(POW_DETACH)
static void
pow_detach_fn(char *buf, int len, char *data)
{
  debug("pow_detach_fn called\n");
  if (len)
    {
      *buf = 0;
      return;
    }
  if (ktab[(int)(unsigned char)*buf].nr != RC_POW_DETACH)
    {
      if (display)
        write(D_userfd, "\007", 1);
      Msg(0, "Detach aborted.");
    }
  else
    Detach(D_POWER);
}
#endif /* POW_DETACH */

#ifdef COPY_PASTE
static void
copy_reg_fn(char *buf, int len, char *data)
{
  struct plop *pp = plop_tab + (int)(unsigned char)*buf;

  if (len)
    {
      *buf = 0;
      return;
    }
  if (pp->buf)
    free(pp->buf);
  pp->buf = 0;
  pp->len = 0;
  if (D_user->u_plop.len)
    {
      if ((pp->buf = (char *)malloc(D_user->u_plop.len)) == NULL)
	{
	  Msg(0, "%s", strnomem);
	  return;
	}
      bcopy(D_user->u_plop.buf, pp->buf, D_user->u_plop.len);
    }
  pp->len = D_user->u_plop.len;
#ifdef ENCODINGS
  pp->enc = D_user->u_plop.enc;
#endif
  Msg(0, "Copied %d characters into register %c", D_user->u_plop.len, *buf);
}

static void
ins_reg_fn(char *buf, int len, char *data)
{
  struct plop *pp = plop_tab + (int)(unsigned char)*buf;

  if (len)
    {
      *buf = 0;
      return;
    }
  if (!fore)
    return;	/* Input() should not call us w/o fore, but you never know... */
  if (*buf == '.')
    Msg(0, "ins_reg_fn: Warning: pasting real register '.'!");
  if (pp->buf)
    {
      MakePaster(&fore->w_paster, pp->buf, pp->len, 0);
      return;
    }
  Msg(0, "Empty register.");
}
#endif /* COPY_PASTE */

static void
process_fn(char *buf, int len, char *data)
{
  struct plop *pp = plop_tab + (int)(unsigned char)*buf;

  if (len)
    {
      *buf = 0;
      return;
    }
  if (pp->buf)
    {
      ProcessInput(pp->buf, pp->len);
      return;
    }
  Msg(0, "Empty register.");
}

static void
confirm_fn(char *buf, int len, char *data)
{
  struct action act;

  if (len || (*buf != 'y' && *buf != 'Y'))
    {
      *buf = 0;
      return;
    }
  act.nr = *(int *)data;
  act.args = noargs;
  act.argl = 0;
  act.quiet = 0;
  DoAction(&act, -1);
}

#ifdef MULTIUSER
struct inputsu
{
  struct acluser **up;
  char name[24];
  char pw1[130];	/* FreeBSD crypts to 128 bytes */
  char pw2[130];
};

static void
su_fin(char *buf, int len, char *data)
{
  struct inputsu *i = (struct inputsu *)data;
  char *p;
  int l;

  if (!*i->name)
    { p = i->name; l = sizeof(i->name) - 1; }
  else if (!*i->pw1)
    { strcpy(p = i->pw1, "\377"); l = sizeof(i->pw1) - 1; }
  else
    { strcpy(p = i->pw2, "\377"); l = sizeof(i->pw2) - 1; }
  if (buf && len)
    strncpy(p, buf, 1 + ((l < len) ? l : len));
  if (!*i->name)
    Input("Screen User: ", sizeof(i->name) - 1, INP_COOKED, su_fin, (char *)i, 0);
  else if (!*i->pw1)
    Input("User's UNIX Password: ", sizeof(i->pw1)-1, INP_COOKED|INP_NOECHO, su_fin, (char *)i, 0);
  else if (!*i->pw2)
    Input("User's Screen Password: ", sizeof(i->pw2)-1, INP_COOKED|INP_NOECHO, su_fin, (char *)i, 0);
  else
    {
      if ((p = DoSu(i->up, i->name, i->pw2, i->pw1)))
        Msg(0, "%s", p);
      free((char *)i);
    }
}
 
static int
InputSu(struct win *win, struct acluser **up, char *name)
{
  struct inputsu *i;

  if (!(i = (struct inputsu *)calloc(1, sizeof(struct inputsu))))
    return -1;

  i->up = up;
  if (name && *name)
    su_fin(name, (int)strlen(name), (char *)i); /* can also initialise stuff */
  else
    su_fin((char *)0, 0, (char *)i);
  return 0;
}
#endif	/* MULTIUSER */

#ifdef PASSWORD

static void
pass1(char *buf, int len, char *data)
{
  struct acluser *u = (struct acluser *)data;

  if (!*buf)
    return;
  ASSERT(u);
  if (u->u_password != NullStr)
    free((char *)u->u_password);
  u->u_password = SaveStr(buf);
  bzero(buf, strlen(buf));
  Input("Retype new password:", 100, INP_NOECHO, pass2, data, 0);
}

static void
pass2(char *buf, int len, char *data)
{
  int st;
  char salt[3];
  struct acluser *u = (struct acluser *)data;

  ASSERT(u);
  if (!buf || strcmp(u->u_password, buf))
    {
      Msg(0, "[ Passwords don't match - checking turned off ]");
      if (u->u_password != NullStr)
        {
          bzero(u->u_password, strlen(u->u_password));
          free((char *)u->u_password);
	}
      u->u_password = NullStr;
    }
  else if (u->u_password[0] == '\0')
    {
      Msg(0, "[ No password - no secure ]");
      if (buf)
        bzero(buf, strlen(buf));
    }
  
  if (u->u_password != NullStr)
    {
      for (st = 0; st < 2; st++)
	salt[st] = 'A' + (int)((time(0) >> 6 * st) % 26);
      salt[2] = 0;
      buf = crypt(u->u_password, salt);
      bzero(u->u_password, strlen(u->u_password));
      free((char *)u->u_password);
      if (!buf)
	{
	  Msg(0, "[ crypt() error - no secure ]");
	  u->u_password = NullStr;
	  return;
	}
      u->u_password = SaveStr(buf);
      bzero(buf, strlen(buf));
#ifdef COPY_PASTE
      if (u->u_plop.buf)
	UserFreeCopyBuffer(u);
      u->u_plop.len = strlen(u->u_password);
# ifdef ENCODINGS
      u->u_plop.enc = 0;
#endif
      if (!(u->u_plop.buf = SaveStr(u->u_password)))
	{
	  Msg(0, "%s", strnomem);
          D_user->u_plop.len = 0;
	}
      else
	Msg(0, "[ Password moved into copybuffer ]");
#else				/* COPY_PASTE */
      Msg(0, "[ Crypted password is \"%s\" ]", u->u_password);
#endif				/* COPY_PASTE */
    }
}
#endif /* PASSWORD */

static int
digraph_find(const char *buf)
{
  int i;
  for (i = 0; i < sizeof(digraphs) && digraphs[i].d[0]; i++)
    if ((digraphs[i].d[0] == (unsigned char)buf[0] && digraphs[i].d[1] == (unsigned char)buf[1]))
      break;
  return i;
}

static void
digraph_fn(char *buf, int len, char *data)
{
  int ch, i, x;

  ch = buf[len];
  if (ch)
    {
      buf[len + 1] = ch;		/* so we can restore it later */
      if (ch < ' ' || ch == '\177')
	return;
      if (len >= 1 && ((*buf == 'U' && buf[1] == '+') || (*buf == '0' && (buf[1] == 'x' || buf[1] == 'X'))))
	{
	  if (len == 1)
	    return;
	  if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f') && (ch < 'A' || ch > 'F'))
	    {
	      buf[len] = '\034';	/* ^] is ignored by Input() */
	      return;
	    }
	  if (len == (*buf == 'U' ? 5 : 3))
	    buf[len] = '\n';
	  return;
	}
      if (len && *buf == '0')
	{
	  if (ch < '0' || ch > '7')
	    {
	      buf[len] = '\034';	/* ^] is ignored by Input() */
	      return;
	    }
	  if (len == 3)
	    buf[len] = '\n';
	  return;
	}
      if (len == 1)
        buf[len] = '\n';
      return;
    }
  if (len < 1)
    return;
  if (buf[len + 1])
    {
      buf[len] = buf[len + 1];	/* stored above */
      len++;
    }
  if (len < 2)
    return;
  if (!parse_input_int(buf, len, &x))
    {
      i = digraph_find(buf);
      if ((x = digraphs[i].value) <= 0)
	{
	  Msg(0, "Unknown digraph");
	  return;
	}
    }
  i = 1;
  *buf = x;
#ifdef UTF8
  if (flayer->l_encoding == UTF8)
    i = ToUtf8(buf, x);	/* buf is big enough for all UTF-8 codes */
#endif
  while(i)
    LayProcess(&buf, &i);
}

#ifdef MAPKEYS
int
StuffKey(int i)
{
  struct action *act;
  int discard = 0;
  int keyno = i;

  debug1("StuffKey #%d", i);
#ifdef DEBUG
  if (i < KMAP_KEYS)
    debug1(" - %s", term[i + T_CAPS].tcname);
#endif

  if (i < KMAP_KEYS && D_ESCseen)
    {
      struct action *act = &D_ESCseen[i + 256];
      if (act->nr != RC_ILLEGAL)
	{
	  D_ESCseen = 0;
	  WindowChanged(fore, 'E');
          DoAction(act, i + 256);
	  return 0;
	}
      discard = 1;
    }

  if (i >= T_CURSOR - T_CAPS && i < T_KEYPAD - T_CAPS && D_cursorkeys)
    i += T_OCAPS - T_CURSOR;
  else if (i >= T_KEYPAD - T_CAPS && i < T_OCAPS - T_CAPS && D_keypad)
    i += T_OCAPS - T_CURSOR;
  debug1(" - action %d\n", i);
  flayer = D_forecv->c_layer;
  fore = D_fore;
  act = 0;
#ifdef COPY_PASTE
  if (flayer && flayer->l_mode == 1)
    act = i < KMAP_KEYS+KMAP_AKEYS ? &mmtab[i] : &kmap_exts[i - (KMAP_KEYS+KMAP_AKEYS)].mm;
#endif
  if ((!act || act->nr == RC_ILLEGAL) && !D_mapdefault)
    act = i < KMAP_KEYS+KMAP_AKEYS ? &umtab[i] : &kmap_exts[i - (KMAP_KEYS+KMAP_AKEYS)].um;
  if (!act || act->nr == RC_ILLEGAL)
    act = i < KMAP_KEYS+KMAP_AKEYS ? &dmtab[i] : &kmap_exts[i - (KMAP_KEYS+KMAP_AKEYS)].dm;

  if (discard && (!act || act->nr != RC_COMMAND))
    {
      /* if the input was just a single byte we let it through */
      if (D_tcs[keyno + T_CAPS].str && strlen(D_tcs[keyno + T_CAPS].str) == 1)
	return -1;
      if (D_ESCseen)
        {
          D_ESCseen = 0;
          WindowChanged(fore, 'E');
        }
      return 0;
    }
  D_mapdefault = 0;

  if (act == 0 || act->nr == RC_ILLEGAL)
    return -1;
  DoAction(act, 0);
  return 0;
}
#endif


static int
IsOnDisplay(struct win *win)
{
  struct canvas *cv;
  ASSERT(display);
  for (cv = D_cvlist; cv; cv = cv->c_next)
    if (Layer2Window(cv->c_layer) == win)
      return 1;
  return 0;
}

struct win *
FindNiceWindow(struct win *win, char *presel)
{
  int i;

  debug2("FindNiceWindow %d %s\n", win ? win->w_number : -1 , presel ? presel : "NULL");
  if (presel)
    {
      i = WindowByNoN(presel);
      if (i >= 0)
	win = wtab[i];
    }
  if (!display)
    return win;
#ifdef MULTIUSER
  if (win && AclCheckPermWin(D_user, ACL_READ, win))
    win = 0;
#endif
  if (!win || (IsOnDisplay(win) && !presel))
    {
      /* try to get another window */
      win = 0;
#ifdef MULTIUSER
      for (win = windows; win; win = win->w_next)
	if (!win->w_layer.l_cvlist && !AclCheckPermWin(D_user, ACL_WRITE, win))
	  break;
      if (!win)
        for (win = windows; win; win = win->w_next)
	  if (win->w_layer.l_cvlist && !IsOnDisplay(win) && !AclCheckPermWin(D_user, ACL_WRITE, win))
	    break;
      if (!win)
	for (win = windows; win; win = win->w_next)
	  if (!win->w_layer.l_cvlist && !AclCheckPermWin(D_user, ACL_READ, win))
	    break;
      if (!win)
	for (win = windows; win; win = win->w_next)
	  if (win->w_layer.l_cvlist && !IsOnDisplay(win) && !AclCheckPermWin(D_user, ACL_READ, win))
	    break;
#endif
      if (!win)
	for (win = windows; win; win = win->w_next)
	  if (!win->w_layer.l_cvlist)
	    break;
      if (!win)
	for (win = windows; win; win = win->w_next)
	  if (win->w_layer.l_cvlist && !IsOnDisplay(win))
	    break;
    }
#ifdef MULTIUSER
  if (win && AclCheckPermWin(D_user, ACL_READ, win))
    win = 0;
#endif
  return win;
}

static int
CalcSlicePercent(struct canvas *cv, int percent)
{
  int w, wsum, up;
  if (!cv || !cv->c_slback)
    return percent;
  up = CalcSlicePercent(cv->c_slback->c_slback, percent);
  w = cv->c_slweight;
  for (cv = cv->c_slback->c_slperp, wsum = 0; cv; cv = cv->c_slnext)
    wsum += cv->c_slweight;
  if (wsum == 0)
    return 0;
  return (up * w) / wsum;
}

static int
ChangeCanvasSize(struct canvas *fcv, int abs, int diff, int gflag, int percent)
//struct canvas *fcv;	/* make this canvas bigger */
//int abs;		/* mode: 0:rel 1:abs 2:max */
//int diff;		/* change this much */
//int gflag;		/* go up if neccessary */
//int percent;
{
  struct canvas *cv;
  int done, have, m, dir;

  debug3("ChangeCanvasSize abs %d diff %d percent=%d\n", abs, diff, percent);
  if (abs == 0 && diff == 0)
    return 0;
  if (abs == 2)
    {
      if (diff == 0)
	  fcv->c_slweight = 0;
      else
	{
          for (cv = fcv->c_slback->c_slperp; cv; cv = cv->c_slnext)
	    cv->c_slweight = 0;
	  fcv->c_slweight = 1;
	  cv = fcv->c_slback->c_slback;
	  if (gflag && cv && cv->c_slback)
	    ChangeCanvasSize(cv, abs, diff, gflag, percent);
	}
      return diff;
    }
  if (abs)
    {
      if (diff < 0)
	diff = 0;
      if (percent && diff > percent)
	diff = percent;
    }
  if (percent)
    {
      int wsum, up;
      for (cv = fcv->c_slback->c_slperp, wsum = 0; cv; cv = cv->c_slnext)
	wsum += cv->c_slweight;
      if (wsum)
	{
	  up = gflag ? CalcSlicePercent(fcv->c_slback->c_slback, percent) : percent;
          debug3("up=%d, wsum=%d percent=%d\n", up, wsum, percent);
	  if (wsum < 1000)
	    {
	      int scale = wsum < 10 ? 1000 : 100;
	      for (cv = fcv->c_slback->c_slperp; cv; cv = cv->c_slnext)
		cv->c_slweight *= scale;
	      wsum *= scale;
	      debug1("scaled wsum to %d\n", wsum);
	    }
	  for (cv = fcv->c_slback->c_slperp; cv; cv = cv->c_slnext)
	    {
	      if (cv->c_slweight)
		{
	          cv->c_slweight = (cv->c_slweight * up) / percent;
		  if (cv->c_slweight == 0)
		    cv->c_slweight = 1;
		}
	      debug1("  - weight %d\n", cv->c_slweight);
	    }
	  diff = (diff * wsum) / percent;
	  percent = wsum;
	}
    }
  else
    {
      if (abs && diff == (fcv->c_slorient == SLICE_VERT ? fcv->c_ye - fcv->c_ys + 2 : fcv->c_xe - fcv->c_xs + 2))
	return 0;
      /* fix weights to real size (can't be helped, sorry) */
      for (cv = fcv->c_slback->c_slperp; cv; cv = cv->c_slnext)
	{
	  cv->c_slweight = cv->c_slorient == SLICE_VERT ? cv->c_ye - cv->c_ys + 2 : cv->c_xe - cv->c_xs + 2;
	  debug1("  - weight %d\n", cv->c_slweight);
	}
    }
  if (abs)
    diff = diff - fcv->c_slweight;
  debug1("diff = %d\n", diff);
  if (diff == 0)
    return 0;
  if (diff < 0)
    {
      cv = fcv->c_slnext ? fcv->c_slnext : fcv->c_slprev;
      fcv->c_slweight += diff;
      cv->c_slweight -= diff;
      return diff;
    }
  done = 0;
  dir = 1;
  for (cv = fcv->c_slnext; diff > 0; cv = dir > 0 ? cv->c_slnext : cv->c_slprev)
    {
      if (!cv)
	{
	  debug1("reached end, dir is %d\n", dir);
	  if (dir == -1)
	    break;
	  dir = -1;
	  cv = fcv;
	  continue;
	}
      if (percent)
	m = 1;
      else
        m = cv->c_slperp ? CountCanvasPerp(cv) * 2 : 2;
      debug2("min is %d, have %d\n", m, cv->c_slweight);
      if (cv->c_slweight > m)
	{
	  have = cv->c_slweight - m;
	  if (have > diff)
	    have = diff;
	  debug1("subtract %d\n", have);
	  cv->c_slweight -= have;
	  done += have;
	  diff -= have;
	}
    }
  if (diff && gflag)
    {
      /* need more room! */
      cv = fcv->c_slback->c_slback;
      if (cv && cv->c_slback)
        done += ChangeCanvasSize(fcv->c_slback->c_slback, 0, diff, gflag, percent);
    }
  fcv->c_slweight += done;
  debug1("ChangeCanvasSize returns %d\n", done);
  return done;
}

static void
ResizeRegions(char *arg, int flags)
{
  struct canvas *cv;
  int diff, l;
  int gflag = 0, abs = 0, percent = 0;
  int orient = 0;

  ASSERT(display);
  if (!*arg)
    return;
  if (D_forecv->c_slorient == SLICE_UNKN)
    {
      Msg(0, "resize: need more than one region");
      return;
    }
  gflag = flags & RESIZE_FLAG_L ? 0 : 1;
  orient |= flags & RESIZE_FLAG_H ? SLICE_HORI : 0;
  orient |= flags & RESIZE_FLAG_V ? SLICE_VERT : 0;
  if (orient == 0)
    orient = D_forecv->c_slorient;
  l = strlen(arg);
  if (*arg == '=')
    {
      /* make all regions the same height */
      struct canvas *cv = gflag ? &D_canvas : D_forecv->c_slback;
      if (cv->c_slperp->c_slorient & orient)
	EqualizeCanvas(cv->c_slperp, gflag);
      /* can't use cv->c_slorient directly as it can be D_canvas */
      if ((cv->c_slperp->c_slorient ^ (SLICE_HORI ^ SLICE_VERT)) & orient)
        {
	  if (cv->c_slback)
	    {
	      cv = cv->c_slback;
	      EqualizeCanvas(cv->c_slperp, gflag);
	    }
	  else
	   EqualizeCanvas(cv, gflag);
        }
      ResizeCanvas(cv);
      RecreateCanvasChain();
      RethinkDisplayViewports();
      ResizeLayersToCanvases();
      return;
    }
  if (!strcmp(arg, "min") || !strcmp(arg, "0"))
    {
      abs = 2;
      diff = 0;
    }
  else if (!strcmp(arg, "max") || !strcmp(arg, "_"))
    {
      abs = 2;
      diff = 1;
    }
  else
    {
      if (l > 0 && arg[l - 1] == '%')
	percent = 1000;
      if (*arg == '+')
	diff = atoi(arg + 1);
      else if (*arg == '-')
	diff = -atoi(arg + 1);
      else
	{
	  diff = atoi(arg);		/* +1 because of caption line */
	  if (diff < 0)
	    diff = 0;
	  abs = diff == 0 ? 2 : 1;
	}
    }
  if (!abs && !diff)
    return;
  if (percent)
    diff = diff * percent / 100;
  cv = D_forecv;
  if (cv->c_slorient & orient)
    ChangeCanvasSize(cv, abs, diff, gflag, percent);
  if (cv->c_slback->c_slorient & orient)
    ChangeCanvasSize(cv->c_slback, abs, diff, gflag, percent);

  ResizeCanvas(&D_canvas);
  RecreateCanvasChain();
  RethinkDisplayViewports();
  ResizeLayersToCanvases();
  return;
}

static void
ResizeFin(char *buf, int len, char *data)
{
  int ch;
  int flags = *(int *)data;
  ch = ((unsigned char *)buf)[len];
  if (ch == 0)
    {
      ResizeRegions(buf, flags);
      return;
    }
  if (ch == 'h')
    flags ^= RESIZE_FLAG_H;
  else if (ch == 'v')
    flags ^= RESIZE_FLAG_V;
  else if (ch == 'b')
    flags |= RESIZE_FLAG_H|RESIZE_FLAG_V;
  else if (ch == 'p')
    flags ^= D_forecv->c_slorient == SLICE_VERT ? RESIZE_FLAG_H : RESIZE_FLAG_V;
  else if (ch == 'l')
    flags ^= RESIZE_FLAG_L;
  else
    return;
  inp_setprompt(resizeprompts[flags], NULL);
  *(int *)data = flags;
  buf[len] = '\034';
}

void
SetForeCanvas(struct display *d, struct canvas *cv)
{
  struct display *odisplay = display;
  if (d->d_forecv == cv)
    return;

  display = d;
  D_forecv = cv;
  if ((focusminwidth && (focusminwidth < 0 || D_forecv->c_xe - D_forecv->c_xs + 1 < focusminwidth)) ||
      (focusminheight && (focusminheight < 0 || D_forecv->c_ye - D_forecv->c_ys + 1 < focusminheight)))
    {
      ResizeCanvas(&D_canvas);
      RecreateCanvasChain();
      RethinkDisplayViewports();
      ResizeLayersToCanvases();	/* redisplays */
    }
  fore = D_fore = Layer2Window(D_forecv->c_layer);
  if (D_other == fore)
    D_other = 0;
  flayer = D_forecv->c_layer;
#ifdef RXVT_OSC
  if (D_xtermosc[2] || D_xtermosc[3])
    {
      Activate(-1);
    }
  else
#endif
    {
      RefreshHStatus();
#ifdef RXVT_OSC
      RefreshXtermOSC();
#endif
      flayer = D_forecv->c_layer;
      CV_CALL(D_forecv, LayRestore();LaySetCursor());
      WindowChanged(0, 'F');
    }

  display = odisplay;
}

#ifdef RXVT_OSC
void
RefreshXtermOSC()
{
  int i;
  struct win *p;

  p = Layer2Window(D_forecv->c_layer);
  for (i = 3; i >=0; i--)
    SetXtermOSC(i, p ? p->w_xtermosc[i] : 0);
}
#endif

int
ParseAttrColor(char *s1, char *s2, int msgok)
{
  int i, n;
  char *s, *ss;
  int r = 0;

  s = s1;
  while (*s == ' ')
    s++;
  ss = s;
  while (*ss && *ss != ' ')
    ss++;
  while (*ss == ' ')
    ss++;
  if (*s && (s2 || *ss || !((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || *s == '.')))
    {
      int mode = 0, n = 0;
      if (*s == '+')
	{
	  mode = 1;
	  s++;
	}
      else if (*s == '-')
	{
	  mode = -1;
	  s++;
	}
      else if (*s == '!')
	{
	  mode = 2;
	  s++;
	}
      else if (*s == '=')
	s++;
      if (*s >= '0' && *s <= '9')
	{
	  n = *s++ - '0';
	  if (*s >= '0' && *s <= '9')
	    n = n * 16 + (*s++ - '0');
	  else if (*s >= 'a' && *s <= 'f')
	    n = n * 16 + (*s++ - ('a' - 10));
	  else if (*s >= 'A' && *s <= 'F')
	    n = n * 16 + (*s++ - ('A' - 10));
	  else if (*s && *s != ' ')
	    {
	      if (msgok)
		Msg(0, "Illegal attribute hexchar '%c'", *s);
	      return -1;
	    }
	}
      else
	{
	  while (*s && *s != ' ')
	    {
	      if (*s == 'd')
		n |= A_DI;
	      else if (*s == 'u')
		n |= A_US;
	      else if (*s == 'b')
		n |= A_BD;
	      else if (*s == 'r')
		n |= A_RV;
	      else if (*s == 's')
		n |= A_SO;
	      else if (*s == 'B')
		n |= A_BL;
	      else
		{
		  if (msgok)
		    Msg(0, "Illegal attribute specifier '%c'", *s);
		  return -1;
		}
	      s++;
	    }
	}
      if (*s && *s != ' ')
	{
	  if (msgok)
	    Msg(0, "junk after attribute description: '%c'", *s);
	  return -1;
	}
      if (mode == -1)
	r = n << 8 | n;
      else if (mode == 1)
	r = n << 8;
      else if (mode == 2)
	r = n;
      else if (mode == 0)
	r = 0xffff ^ n;
    }
  while (*s && *s == ' ')
    s++;

  if (s2)
    {
      if (*s)
	{
	  if (msgok)
	    Msg(0, "junk after description: '%c'", *s);
	  return -1;
	}
      s = s2;
      while (*s && *s == ' ')
	s++;
    }

#ifdef COLOR
  if (*s)
    {
      static char costr[] = "krgybmcw d    i.01234567 9     f               FKRGYBMCW      I ";
      int numco = 0, j;

      n = 0;
      if (*s == '.')
	{
	  numco++;
	  n = 0x0f;
	  s++;
	}
      for (j = 0; j < 2 && *s && *s != ' '; j++)
	{
	  for (i = 0; costr[i]; i++)
	    if (*s == costr[i])
	      break;
	  if (!costr[i])
	    {
	      if (msgok)
		Msg(0, "illegal color descriptor: '%c'", *s);
	      return -1;
	    }
	  numco++;
	  n = n << 4 | (i & 15);
#ifdef COLORS16
	  if (i >= 48)
	    n = (n & 0x20ff) | 0x200;
#endif
	  s++;
	}
      if ((n & 0xf00) == 0xf00)
        n ^= 0xf00;	/* clear superflous bits */
#ifdef COLORS16
      if (n & 0x2000)
	n ^= 0x2400;	/* shift bit into right position */
#endif
      if (numco == 1)
	n |= 0xf0;	/* don't change bg color */
      if (numco != 2 && n != 0xff)
	n |= 0x100;	/* special invert mode */
      if (*s && *s != ' ')
	{
	  if (msgok)
	    Msg(0, "junk after color description: '%c'", *s);
	  return -1;
	}
      n ^= 0xff;
      r |= n << 16;
    }
#endif

  while (*s && *s == ' ')
    s++;
  if (*s)
    {
      if (msgok)
	Msg(0, "junk after description: '%c'", *s);
      return -1;
    }
  debug1("ParseAttrColor %06x\n", r);
  return r;
}

/*
 *  Color coding:
 *    0-7 normal colors
 *    9   default color
 *    e   just set intensity
 *    f   don't change anything
 *  Intensity is encoded into bits 17(fg) and 18(bg).
 */
void
ApplyAttrColor(int i, struct mchar *mc)
{
  debug1("ApplyAttrColor %06x\n", i);
  mc->attr |= i >> 8 & 255;
  mc->attr ^= i & 255;
#ifdef COLOR
  i = (i >> 16) ^ 0xff;
  if ((i & 0x100) != 0)
    {
      i &= 0xeff;
      if (mc->attr & (A_SO|A_RV))
# ifdef COLORS16
        i = ((i & 0x0f) << 4) | ((i & 0xf0) >> 4) | ((i & 0x200) << 1) | ((i & 0x400) >> 1);
# else
        i = ((i & 0x0f) << 4) | ((i & 0xf0) >> 4);
# endif
    }
# ifdef COLORS16
  if ((i & 0x0f) != 0x0f)
    mc->attr = (mc->attr & 0xbf) | ((i >> 3) & 0x40);
  if ((i & 0xf0) != 0xf0)
    mc->attr = (mc->attr & 0x7f) | ((i >> 3) & 0x80);
# endif
  mc->color = 0x99 ^ mc->color;
  if ((i & 0x0e) == 0x0e)
    i = (i & 0xf0) | (mc->color & 0x0f);
  if ((i & 0xe0) == 0xe0)
    i = (i & 0x0f) | (mc->color & 0xf0);
  mc->color = 0x99 ^ i;
  debug2("ApplyAttrColor - %02x %02x\n", mc->attr, i);
#endif
}
