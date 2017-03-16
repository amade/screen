/* Copyright (c) 2013, 2015
 *      Mike Gerwitz (mtg@gnu.org)
 * Copyright (c) 2010
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

#include "process.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>
#include <uniconv.h>
#include <unistr.h>
#include <unistdio.h>

#include "screen.h"

#include "display.h"
#include "fileio.h"
#include "help.h"
#include "input.h"
#include "kmapdef.h"
#include "layout.h"
#include "list_generic.h"
#include "logfile.h"
#include "mark.h"
#include "misc.h"
#include "resize.h"
#include "search.h"
#include "socket.h"
#include "telnet.h"
#include "termcap.h"
#include "tty.h"
#include "utmp.h"
#include "viewport.h"
#include "winmsg.h"


static int CheckArgNum(int, uint32_t **);
static void ClearAction(struct action *);
static void SaveAction(struct action *, int, uint32_t **, int *);
static uint16_t NextWindow(void);
static uint16_t PreviousWindow(void);
static int MoreWindows(void);
static void CollapseWindowlist(void);
static void LogToggle(bool);
static void ShowInfo(void);
static void ShowDInfo(void);
static Window *WindowByName(uint32_t *);
static int WindowByNumber(uint32_t *);
static int ParseSwitch(struct action *, bool *);
static int ParseOnOff(struct action *, bool *);
static int ParseWinNum(struct action *, int *);
static int ParseBase(struct action *, char *, int *, int, char *);
static int ParseSaveStr(struct action *, uint32_t **);
static int ParseNum(struct action *, int *);
static int ParseNum1000(struct action *, int *);
static uint32_t **SaveArgs(uint32_t **);
static bool IsNum(char *);
static void ColonFin(uint32_t *, size_t, void *);
static void InputSelect(void);
static void InputSetenv(uint32_t *);
static void InputAKA(void);
//static int InputSu(struct acluser **, char *);
//static void suFin(uint32_t *, size_t, void *);
static void AKAFin(uint32_t *, size_t, void *);
static void copy_reg_fn(uint32_t *, size_t, void *);
static void ins_reg_fn(uint32_t *, size_t, void *);
static void process_fn(uint32_t *, size_t, void *);
static void pow_detach_fn(uint32_t *, size_t, void *);
static void digraph_fn(uint32_t *, size_t, void *);
static int digraph_find(const char *buf);
static void confirm_fn(uint32_t *, size_t, void *);
static int IsOnDisplay(Window *);
static void ResizeRegions(uint32_t *, int);
static void ResizeFin(uint32_t *, size_t, void *);
static struct action *FindKtab(uint32_t *, int);
static void SelectFin(uint32_t *, size_t, void *);
static void SelectLayoutFin(uint32_t *, size_t, void *);
static void ShowWindowsX(char *);

char NullStr[] = "";

struct plop plop_tab[MAX_PLOP_DEFS];

#ifndef PTY_MODE
#define PTY_MODE 0622
#endif

int TtyMode = PTY_MODE;
bool hardcopy_append = false;
bool all_norefresh = 0;
int zmodem_mode = 0;
uint32_t *zmodem_sendcmd;
uint32_t *zmodem_recvcmd;
static uint32_t *zmodes[4] = { U"off", U"auto", U"catch", U"pass" };

int idletimo;
struct action idleaction;
uint32_t **blankerprg;

struct action ktab[256 + KMAP_KEYS];	/* command key translation table */
struct kclass {
	struct kclass *next;
	uint32_t *name;
	struct action ktab[256 + KMAP_KEYS];
};
struct kclass *kclasses;

struct action umtab[KMAP_KEYS + KMAP_AKEYS];
struct action dmtab[KMAP_KEYS + KMAP_AKEYS];
struct action mmtab[KMAP_KEYS + KMAP_AKEYS];
struct kmap_ext *kmap_exts;
int kmap_extn;
static int maptimeout = 300;

#ifndef MAX_DIGRAPH
#define MAX_DIGRAPH 512
#endif

struct digraph {
	unsigned char d[2];
	int value;
};

/* digraph table taken from old vim and rfc1345 */
static struct digraph digraphs[MAX_DIGRAPH + 1] = {
	{{' ', ' '}, 160},	/*   */
	{{'N', 'S'}, 160},	/*   */
	{{'~', '!'}, 161},	/* ¡ */
	{{'!', '!'}, 161},	/* ¡ */
	{{'!', 'I'}, 161},	/* ¡ */
	{{'c', '|'}, 162},	/* ¢ */
	{{'c', 't'}, 162},	/* ¢ */
	{{'$', '$'}, 163},	/* £ */
	{{'P', 'd'}, 163},	/* £ */
	{{'o', 'x'}, 164},	/* ¤ */
	{{'C', 'u'}, 164},	/* ¤ */
	{{'C', 'u'}, 164},	/* ¤ */
	{{'E', 'u'}, 164},	/* ¤ */
	{{'Y', '-'}, 165},	/* ¥ */
	{{'Y', 'e'}, 165},	/* ¥ */
	{{'|', '|'}, 166},	/* ¦ */
	{{'B', 'B'}, 166},	/* ¦ */
	{{'p', 'a'}, 167},	/* § */
	{{'S', 'E'}, 167},	/* § */
	{{'"', '"'}, 168},	/* ¨ */
	{{'\'', ':'}, 168},	/* ¨ */
	{{'c', 'O'}, 169},	/* © */
	{{'C', 'o'}, 169},	/* © */
	{{'a', '-'}, 170},	/* ª */
	{{'<', '<'}, 171},	/* « */
	{{'-', ','}, 172},	/* ¬ */
	{{'N', 'O'}, 172},	/* ¬ */
	{{'-', '-'}, 173},	/* ­ */
	{{'r', 'O'}, 174},	/* ® */
	{{'R', 'g'}, 174},	/* ® */
	{{'-', '='}, 175},	/* ¯ */
	{{'\'', 'm'}, 175},	/* ¯ */
	{{'~', 'o'}, 176},	/* ° */
	{{'D', 'G'}, 176},	/* ° */
	{{'+', '-'}, 177},	/* ± */
	{{'2', '2'}, 178},	/* ² */
	{{'2', 'S'}, 178},	/* ² */
	{{'3', '3'}, 179},	/* ³ */
	{{'3', 'S'}, 179},	/* ³ */
	{{'\'', '\''}, 180},	/* ´ */
	{{'j', 'u'}, 181},	/* µ */
	{{'M', 'y'}, 181},	/* µ */
	{{'p', 'p'}, 182},	/* ¶ */
	{{'P', 'I'}, 182},	/* ¶ */
	{{'~', '.'}, 183},	/* · */
	{{'.', 'M'}, 183},	/* · */
	{{',', ','}, 184},	/* ¸ */
	{{'\'', ','}, 184},	/* ¸ */
	{{'1', '1'}, 185},	/* ¹ */
	{{'1', 'S'}, 185},	/* ¹ */
	{{'o', '-'}, 186},	/* º */
	{{'>', '>'}, 187},	/* » */
	{{'1', '4'}, 188},	/* ¼ */
	{{'1', '2'}, 189},	/* ½ */
	{{'3', '4'}, 190},	/* ¾ */
	{{'~', '?'}, 191},	/* ¿ */
	{{'?', '?'}, 191},	/* ¿ */
	{{'?', 'I'}, 191},	/* ¿ */
	{{'A', '`'}, 192},	/* À */
	{{'A', '!'}, 192},	/* À */
	{{'A', '\''}, 193},	/* Á */
	{{'A', '^'}, 194},	/* Â */
	{{'A', '>'}, 194},	/* Â */
	{{'A', '~'}, 195},	/* Ã */
	{{'A', '?'}, 195},	/* Ã */
	{{'A', '"'}, 196},	/* Ä */
	{{'A', ':'}, 196},	/* Ä */
	{{'A', '@'}, 197},	/* Å */
	{{'A', 'A'}, 197},	/* Å */
	{{'A', 'E'}, 198},	/* Æ */
	{{'C', ','}, 199},	/* Ç */
	{{'E', '`'}, 200},	/* È */
	{{'E', '!'}, 200},	/* È */
	{{'E', '\''}, 201},	/* É */
	{{'E', '^'}, 202},	/* Ê */
	{{'E', '>'}, 202},	/* Ê */
	{{'E', '"'}, 203},	/* Ë */
	{{'E', ':'}, 203},	/* Ë */
	{{'I', '`'}, 204},	/* Ì */
	{{'I', '!'}, 204},	/* Ì */
	{{'I', '\''}, 205},	/* Í */
	{{'I', '^'}, 206},	/* Î */
	{{'I', '>'}, 206},	/* Î */
	{{'I', '"'}, 207},	/* Ï */
	{{'I', ':'}, 207},	/* Ï */
	{{'D', '-'}, 208},	/* Ð */
	{{'N', '~'}, 209},	/* Ñ */
	{{'N', '?'}, 209},	/* Ñ */
	{{'O', '`'}, 210},	/* Ò */
	{{'O', '!'}, 210},	/* Ò */
	{{'O', '\''}, 211},	/* Ó */
	{{'O', '^'}, 212},	/* Ô */
	{{'O', '>'}, 212},	/* Ô */
	{{'O', '~'}, 213},	/* Õ */
	{{'O', '?'}, 213},	/* Õ */
	{{'O', '"'}, 214},	/* Ö */
	{{'O', ':'}, 214},	/* Ö */
	{{'/', '\\'}, 215},	/* × */
	{{'*', 'x'}, 215},	/* × */
	{{'O', '/'}, 216},	/* Ø */
	{{'U', '`'}, 217},	/* Ù */
	{{'U', '!'}, 217},	/* Ù */
	{{'U', '\''}, 218},	/* Ú */
	{{'U', '^'}, 219},	/* Û */
	{{'U', '>'}, 219},	/* Û */
	{{'U', '"'}, 220},	/* Ü */
	{{'U', ':'}, 220},	/* Ü */
	{{'Y', '\''}, 221},	/* Ý */
	{{'I', 'p'}, 222},	/* Þ */
	{{'T', 'H'}, 222},	/* Þ */
	{{'s', 's'}, 223},	/* ß */
	{{'s', '"'}, 223},	/* ß */
	{{'a', '`'}, 224},	/* à */
	{{'a', '!'}, 224},	/* à */
	{{'a', '\''}, 225},	/* á */
	{{'a', '^'}, 226},	/* â */
	{{'a', '>'}, 226},	/* â */
	{{'a', '~'}, 227},	/* ã */
	{{'a', '?'}, 227},	/* ã */
	{{'a', '"'}, 228},	/* ä */
	{{'a', ':'}, 228},	/* ä */
	{{'a', 'a'}, 229},	/* å */
	{{'a', 'e'}, 230},	/* æ */
	{{'c', ','}, 231},	/* ç */
	{{'e', '`'}, 232},	/* è */
	{{'e', '!'}, 232},	/* è */
	{{'e', '\''}, 233},	/* é */
	{{'e', '^'}, 234},	/* ê */
	{{'e', '>'}, 234},	/* ê */
	{{'e', '"'}, 235},	/* ë */
	{{'e', ':'}, 235},	/* ë */
	{{'i', '`'}, 236},	/* ì */
	{{'i', '!'}, 236},	/* ì */
	{{'i', '\''}, 237},	/* í */
	{{'i', '^'}, 238},	/* î */
	{{'i', '>'}, 238},	/* î */
	{{'i', '"'}, 239},	/* ï */
	{{'i', ':'}, 239},	/* ï */
	{{'d', '-'}, 240},	/* ð */
	{{'n', '~'}, 241},	/* ñ */
	{{'n', '?'}, 241},	/* ñ */
	{{'o', '`'}, 242},	/* ò */
	{{'o', '!'}, 242},	/* ò */
	{{'o', '\''}, 243},	/* ó */
	{{'o', '^'}, 244},	/* ô */
	{{'o', '>'}, 244},	/* ô */
	{{'o', '~'}, 245},	/* õ */
	{{'o', '?'}, 245},	/* õ */
	{{'o', '"'}, 246},	/* ö */
	{{'o', ':'}, 246},	/* ö */
	{{':', '-'}, 247},	/* ÷ */
	{{'o', '/'}, 248},	/* ø */
	{{'u', '`'}, 249},	/* ù */
	{{'u', '!'}, 249},	/* ù */
	{{'u', '\''}, 250},	/* ú */
	{{'u', '^'}, 251},	/* û */
	{{'u', '>'}, 251},	/* û */
	{{'u', '"'}, 252},	/* ü */
	{{'u', ':'}, 252},	/* ü */
	{{'y', '\''}, 253},	/* ý */
	{{'i', 'p'}, 254},	/* þ */
	{{'t', 'h'}, 254},	/* þ */
	{{'y', '"'}, 255},	/* ÿ */
	{{'y', ':'}, 255},	/* ÿ */
	{{'"', '['}, 196},	/* Ä */
	{{'"', '\\'}, 214},	/* Ö */
	{{'"', ']'}, 220},	/* Ü */
	{{'"', '{'}, 228},	/* ä */
	{{'"', '|'}, 246},	/* ö */
	{{'"', '}'}, 252},	/* ü */
	{{'"', '~'}, 223}	/* ß */
};

#define RESIZE_FLAG_H 1
#define RESIZE_FLAG_V 2
#define RESIZE_FLAG_L 4

static uint32_t *resizeprompts[] = {
	U"resize # lines: ",
	U"resize -h # columns: ",
	U"resize -v # lines: ",
	U"resize -b # columns: ",
	U"resize -l # lines: ",
	U"resize -l -h # columns: ",
	U"resize -l -v # lines: ",
	U"resize -l -b # columns: ",
};

static int parse_input_int(const uint32_t *buf, size_t len, uint32_t *val)
{
	int x = 0;
	size_t i;
	if (len >= 1 && ((*buf == 'U' && buf[1] == '+') || (*buf == '0' && (buf[1] == 'x' || buf[1] == 'X')))) {
		x = 0;
		for (i = 2; i < len; i++) {
			if (buf[i] >= '0' && buf[i] <= '9')
				x = x * 16 | (buf[i] - '0');
			else if (buf[i] >= 'a' && buf[i] <= 'f')
				x = x * 16 | (buf[i] - ('a' - 10));
			else if (buf[i] >= 'A' && buf[i] <= 'F')
				x = x * 16 | (buf[i] - ('A' - 10));
			else
				return 0;
		}
	} else if (buf[0] == '0') {
		x = 0;
		for (i = 1; i < len; i++) {
			if (buf[i] < '0' || buf[i] > '7')
				return 0;
			x = x * 8 | (buf[i] - '0');
		}
	} else
		return 0;
	*val = x;
	return 1;
}

uint32_t *noargs[1];

int enter_window_name_mode = 0;

void InitKeytab()
{
	unsigned int i;
	uint32_t *argarr[2];

	for (i = 0; i < sizeof(ktab) / sizeof(*ktab); i++) {
		ktab[i].nr = RC_ILLEGAL;
		ktab[i].args = noargs;
		ktab[i].argl = 0;
	}
	for (i = 0; i < KMAP_KEYS + KMAP_AKEYS; i++) {
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
	for (i = 0; i < NKMAPDEF; i++) {
		if (i + KMAPDEFSTART < T_CAPS)
			continue;
		if (i + KMAPDEFSTART >= T_CAPS + KMAP_KEYS)
			continue;
		if (kmapdef[i] == 0)
			continue;
		argarr[0] = kmapdef[i];
		SaveAction(dmtab + i + (KMAPDEFSTART - T_CAPS), RC_STUFF, argarr, 0);
	}
	for (i = 0; i < NKMAPADEF; i++) {
		if (i + KMAPADEFSTART < T_CURSOR)
			continue;
		if (i + KMAPADEFSTART >= T_CURSOR + KMAP_AKEYS)
			continue;
		if (kmapadef[i] == 0)
			continue;
		argarr[0] = kmapadef[i];
		SaveAction(dmtab + i + (KMAPADEFSTART - T_CURSOR + KMAP_KEYS), RC_STUFF, argarr, 0);
	}
	for (i = 0; i < NKMAPMDEF; i++) {
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

	ktab['h'].nr = RC_HARDCOPY;
	ktab['z'].nr = ktab[Ctrl('z')].nr = RC_SUSPEND;
	ktab['c'].nr = ktab[Ctrl('c')].nr = RC_SCREEN;
	ktab[' '].nr = ktab[Ctrl(' ')].nr = ktab['n'].nr = ktab[Ctrl('n')].nr = RC_NEXT;
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
#if defined(ENABLE_UTMP)
	ktab['L'].nr = RC_LOGIN;
#endif
	ktab[','].nr = RC_LICENSE;
	ktab['W'].nr = RC_WIDTH;
	ktab['.'].nr = RC_DUMPTERMCAP;
	ktab[Ctrl('\\')].nr = RC_QUIT;
	ktab['d'].nr = ktab[Ctrl('d')].nr = RC_DETACH;
	ktab['D'].nr = RC_POW_DETACH;
	ktab['r'].nr = ktab[Ctrl('r')].nr = RC_WRAP;
	ktab['f'].nr = ktab[Ctrl('f')].nr = RC_FLOW;
	ktab['C'].nr = RC_CLEAR;
	ktab['Z'].nr = RC_RESET;
	ktab['H'].nr = RC_LOG;
	ktab['M'].nr = RC_MONITOR;
	ktab['?'].nr = RC_HELP;
	ktab['*'].nr = RC_DISPLAYS;
	{
		char *args[2];
		args[0] = "-";
		args[1] = NULL;
		SaveAction(ktab + '-', RC_SELECT, args, 0);
	}
	for (i = 0; i < ((maxwin && maxwin < 10) ? maxwin : 10); i++) {
		char *args[2], arg1[10];
		args[0] = arg1;
		args[1] = 0;
		sprintf(arg1, "%d", i);
		SaveAction(ktab + '0' + i, RC_SELECT, args, 0);
	}
	ktab['\''].nr = RC_SELECT;	/* calling a window by name */
	{
		char *args[2];
		args[0] = "-b";
		args[1] = 0;
		SaveAction(ktab + '"', RC_WINDOWLIST, args, 0);
	}
	ktab[Ctrl('G')].nr = RC_VBELL;
	ktab[':'].nr = RC_COLON;
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
	ktab['D'].nr = RC_POW_DETACH;
	ktab['x'].nr = ktab[Ctrl('x')].nr = RC_LOCKSCREEN;
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
	if (DefaultEsc >= 0) {
		ClearAction(&ktab[DefaultEsc]);
		ktab[DefaultEsc].nr = RC_OTHER;
	}
	if (DefaultMetaEsc >= 0) {
		ClearAction(&ktab[DefaultMetaEsc]);
		ktab[DefaultMetaEsc].nr = RC_META;
	}

	idleaction.nr = RC_BLANKER;
	idleaction.args = noargs;
	idleaction.argl = 0;
}

static struct action *FindKtab(uint32_t *class, int create)
{
	struct kclass *kp, **kpp;
	int i;

	if (class == 0)
		return ktab;
	for (kpp = &kclasses; (kp = *kpp) != 0; kpp = &kp->next)
		if (!u32_strcmp(kp->name, class))
			break;
	if (kp == 0) {
		if (!create)
			return 0;
		if (u32_strlen(class) > 80) {
			Msg(0, U"Command class name too long.");
			return 0;
		}
		kp = malloc(sizeof(*kp) * sizeof(uint32_t));
		if (kp == 0) {
			Msg(0, U"%s", strnomem);
			return 0;
		}
		kp->name = SaveStr(class);
		for (i = 0; i < (int)(sizeof(kp->ktab) / sizeof(*kp->ktab)); i++) {
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

static void ClearAction(struct action *act)
{
	uint32_t **p;

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

/*
 *  This ProcessInput just does the keybindings and passes
 *  everything else on to ProcessInput2.
 */

void ProcessInput(uint32_t *ibuf, size_t ilen)
{
	uint32_t ch;
	size_t slen;
	uint32_t *s, *q;
	int i, l;
	uint32_t *p;

	if (display == 0 || ilen == 0)
		return;
	if (D_seql)
		evdeq(&D_mapev);
	slen = ilen;
	s = ibuf;
	while (ilen-- > 0) {
		ch = *s++;
		if (D_dontmap || !D_nseqs) {
			D_dontmap = 0;
			continue;
		}
		for (;;) {
			if (*D_seqp != ch) {
				l = D_seqp[D_seqp[-D_seql - 1] + 1];
				if (l) {
					D_seqp += l * 2 + 4;
					continue;
				}
				D_mapdefault = 0;
				l = D_seql;
				p = D_seqp - l;
				D_seql = 0;
				D_seqp = D_kmaps + 3;
				if (l == 0)
					break;
				if ((q = D_seqh) != 0) {
					D_seqh = 0;
					i = q[0] << 8 | q[1];
					i &= ~KMAP_NOTIMEOUT;
					if (StuffKey(i))
						ProcessInput2(q + 3, q[2]);
					if (display == 0)
						return;
					l -= q[2];
					p += q[2];
				} else
					D_dontmap = 1;
				ProcessInput(p, l);
				if (display == 0)
					return;
				evdeq(&D_mapev);
				continue;
			}
			if (D_seql++ == 0) {
				/* Finish old stuff */
				slen -= ilen + 1;
				if (slen)
					ProcessInput2(ibuf, slen);
				if (display == 0)
					return;
				D_seqh = 0;
			}
			ibuf = s;
			slen = ilen;
			D_seqp++;
			l = D_seql;
			if ((uint32_t)l == D_seqp[-l - 1]) {
				if (D_seqp[l] != (uint32_t)l) {
					q = D_seqp + 1 + l;
					if (D_kmaps + D_nseqs > q && q[2] > (uint32_t)l && !memcmp(D_seqp - l, q + 3, l)) {
						D_seqh = D_seqp - 3 - l;
						D_seqp = q + 3 + l;
						break;
					}
				}
				i = D_seqp[-l - 3] << 8 | D_seqp[-l - 2];
				i &= ~KMAP_NOTIMEOUT;
				p = D_seqp - l;
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
	if (D_seql) {
		l = D_seql;
		for (s = D_seqp;; s += i * 2 + 4) {
			if (s[-l - 3] & KMAP_NOTIMEOUT >> 8)
				break;
			if ((i = s[s[-l - 1] + 1]) == 0) {
				SetTimeout(&D_mapev, maptimeout);
				evenq(&D_mapev);
				break;
			}
		}
	}
	ProcessInput2(ibuf, slen);
}

/*
 *  Here only the screen escape commands are handled.
 */

void ProcessInput2(uint32_t *ibuf, size_t ilen)
{
	uint32_t *s;
	uint32_t ch;
	size_t slen;
	struct action *ktabp;

	if (!display)
		return;
	while (ilen) {
		flayer = D_forecv->c_layer;
		fore = D_fore;
		slen = ilen;
		s = ibuf;
		if (!D_ESCseen) {
			while (ilen) {
				if (*s++ == D_user->u_Esc)
					break;
				ilen--;
			}
			slen -= ilen;
			if (slen)
				DoProcess(fore, &ibuf, &slen, 0);
			if (ilen == 1) {
				D_ESCseen = ktab;
				WindowChanged(fore, WINESC_ESC_SEEN);
				ilen--;
			}
		}
		if (ilen == 0)
			return;
		ktabp = D_ESCseen ? D_ESCseen : ktab;
		if (D_ESCseen) {
			D_ESCseen = 0;
			WindowChanged(fore, WINESC_ESC_SEEN);
		}
		ch = *s;

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

		/*XXX if (ch >= 0) */
			DoAction(&ktabp[ch], ch);
		ibuf = s + 1;
		ilen--;
	}
}

void DoProcess(Window *window, uint32_t **bufp, size_t *lenp, struct paster *pa)
{
	size_t oldlen;
	Display *d = display;

	/* XXX -> PasteStart */
	if (pa && *lenp > 1 && window && window->w_slowpaste) {
		/* schedule slowpaste event */
		SetTimeout(&window->w_paster.pa_slowev, window->w_slowpaste);
		evenq(&window->w_paster.pa_slowev);
		return;
	}
	while (flayer && *lenp) {
		if (!pa && window && window->w_paster.pa_pastelen && flayer == window->w_paster.pa_pastelayer) {
			WBell(window, visual_bell);
			*bufp += *lenp;
			*lenp = 0;
			display = d;
			return;
		}
		oldlen = *lenp;
		LayProcess(bufp, lenp);
		if (pa && !pa->pa_pastelayer)
			break;	/* flush rest of paste */
		if (*lenp == oldlen) {
			if (pa) {
				display = d;
				return;
			}
			/* We're full, let's beep */
			WBell(window, visual_bell);
			break;
		}
	}
	*bufp += *lenp;
	*lenp = 0;
	display = d;
	if (pa && pa->pa_pastelen == 0)
		FreePaster(pa);
}

int FindCommnr(const uint32_t *str)
{
	int x, m, l = 0, r = RC_LAST;
	while (l <= r) {
		m = (l + r) / 2;
		x = u32_strcmp(str, comms[m].name);
		if (x > 0)
			l = m + 1;
		else if (x < 0)
			r = m - 1;
		else
			return m;
	}
	return RC_ILLEGAL;
}

static int CheckArgNum(int nr, uint32_t **args)
{
	int i, n;
	static uint32_t *argss[] = { U"no", U"one", U"two", U"three", U"four", U"OOPS" };
	static uint32_t *orformat[] = {
		U"%s: %s: %s argument%s required",
		U"%s: %s: %s or %s argument%s required",
		U"%s: %s: %s, %s or %s argument%s required",
		U"%s: %s: %s, %s, %s or %s argument%s required"
	};

	n = comms[nr].flags & ARGS_MASK;
	for (i = 0; args[i]; i++) ;
	if (comms[nr].flags & ARGS_ORMORE) {
		if (i < n) {
			Msg(0, U"%s: %s: at least %s argument%s required",
			    rc_name, comms[nr].name, argss[n], n != 1 ? "s" : "");
			return -1;
		}
	} else if ((comms[nr].flags & ARGS_PLUS1) && (comms[nr].flags & ARGS_PLUS2) && (comms[nr].flags & ARGS_PLUS3)) {
		if (i != n && i != n + 1 && i != n + 2 && i != n + 3) {
			Msg(0, orformat[3], rc_name, comms[nr].name, argss[n],
			    argss[n + 1], argss[n + 2], argss[n + 3], "");
			return -1;
		}
	} else if ((comms[nr].flags & ARGS_PLUS1) && (comms[nr].flags & ARGS_PLUS2)) {
		if (i != n && i != n + 1 && i != n + 2) {
			Msg(0, orformat[2], rc_name, comms[nr].name, argss[n], argss[n + 1], argss[n + 2], "");
			return -1;
		}
	} else if ((comms[nr].flags & ARGS_PLUS1) && (comms[nr].flags & ARGS_PLUS3)) {
		if (i != n && i != n + 1 && i != n + 3) {
			Msg(0, orformat[2], rc_name, comms[nr].name, argss[n], argss[n + 1], argss[n + 3], "");
			return -1;
		}
	} else if ((comms[nr].flags & ARGS_PLUS2) && (comms[nr].flags & ARGS_PLUS3)) {
		if (i != n && i != n + 2 && i != n + 3) {
			Msg(0, orformat[2], rc_name, comms[nr].name, argss[n], argss[n + 2], argss[n + 3], "");
			return -1;
		}
	} else if (comms[nr].flags & ARGS_PLUS1) {
		if (i != n && i != n + 1) {
			Msg(0, orformat[1], rc_name, comms[nr].name, argss[n], argss[n + 1], n != 0 ? "s" : "");
			return -1;
		}
	} else if (comms[nr].flags & ARGS_PLUS2) {
		if (i != n && i != n + 2) {
			Msg(0, orformat[1], rc_name, comms[nr].name, argss[n], argss[n + 2], "s");
			return -1;
		}
	} else if (comms[nr].flags & ARGS_PLUS3) {
		if (i != n && i != n + 3) {
			Msg(0, orformat[1], rc_name, comms[nr].name, argss[n], argss[n + 3], "");
			return -1;
		}
	} else if (i != n) {
		Msg(0, orformat[0], rc_name, comms[nr].name, argss[n], n != 1 ? "s" : "");
		return -1;
	}
	return i;
}

static void StuffFin(uint32_t *buf, size_t len, void *data)
{
	(void)data; /* unused */

	if (!flayer)
		return;
	while (len)
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

void DoAction(struct action *act, int key)
{
	int nr = act->nr;
	uint32_t **args = act->args;
	int *argl = act->argl;
	Window *p;
	bool b;
	int argc, n, msgok;
	int64_t i;
	int j;
	uint32_t *s = NULL;
	char ch;
	Display *odisplay = display;
	struct acluser *user;
	size_t len;

	user = display ? D_user : users;
	if (nr == RC_ILLEGAL) {
		return;
	}
	n = comms[nr].flags;
	/* Commands will have a CAN_QUERY flag, depending on whether they have
	 * something to return on a query. For example, 'windows' can return a result,
	 * but 'other' cannot.
	 * If some command causes an error, then it should reset queryflag to -1, so that
	 * the process requesting the query can be notified that an error happened.
	 */
	if (!(n & CAN_QUERY) && queryflag >= 0) {
		/* Query flag is set, but this command cannot be queried. */
		OutputMsg(0, U"%s command cannot be queried.", comms[nr].name);
		queryflag = -1;
		return;
	}
	if ((n & NEED_DISPLAY) && display == 0) {
		OutputMsg(0, U"%s: %s: display required", rc_name, comms[nr].name);
		queryflag = -1;
		return;
	}
	if ((n & NEED_FORE) && fore == 0) {
		OutputMsg(0, U"%s: %s: window required", rc_name, comms[nr].name);
		queryflag = -1;
		return;
	}
	if ((n & NEED_LAYER) && flayer == 0) {
		OutputMsg(0, U"%s: %s: display or window required", rc_name, comms[nr].name);
		queryflag = -1;
		return;
	}
	if ((argc = CheckArgNum(nr, args)) < 0)
		return;
	if (display) {
		if (AclCheckPermCmd(D_user, ACL_EXEC, &comms[nr])) {
			OutputMsg(0, U"%s: %s: permission denied (user %s)",
				  rc_name, comms[nr].name, (EffectiveAclUser ? EffectiveAclUser : D_user)->u_name);
			queryflag = -1;
			return;
		}
	}

	msgok = display && !*rc_name;
	switch (nr) {
	case RC_SELECT:
		if (!*args)
			InputSelect();
		else if (args[0][0] == '-' && !args[0][1]) {
			SetForeWindow((Window *)0);
			Activate(0);
		} else if (args[0][0] == '.' && !args[0][1]) {
			if (!fore) {
				OutputMsg(0, U"select . needs a window");
				queryflag = -1;
			} else {
				SetForeWindow(fore);
				Activate(0);
			}
		} else if (ParseWinNum(act, &n) == 0)
			SwitchWindow(n);
		else if (queryflag >= 0)
			queryflag = -1;	/* ParseWinNum already prints out an appropriate error message. */
		break;
	case RC_MULTIINPUT:
		if (!*args) {
			if (!fore)
				OutputMsg(0, U"multiinput needs a window");
			else
				fore->w_miflag = fore->w_miflag ? 0 : 1;
		} else {
			if (ParseWinNum(act, &n) == 0) {
				Window *p;
				if ((p = wtab[n]) == 0) {
					ShowWindows(n);
					break;
				} else {
					p->w_miflag = p->w_miflag ? 0 : 1;
				}
			}
		}
		break;
	case RC_DEFAUTONUKE:
		if (ParseOnOff(act, &defautonuke) == 0 && msgok)
			OutputMsg(0, U"Default autonuke turned %s", defautonuke ? "on" : "off");
		if (display && *rc_name)
			D_auto_nuke = defautonuke;
		break;
	case RC_AUTONUKE:
		if (ParseOnOff(act, &D_auto_nuke) == 0 && msgok)
			OutputMsg(0, U"Autonuke turned %s", D_auto_nuke ? "on" : "off");
		break;
	case RC_DEFOBUFLIMIT:
		if (ParseNum(act, &defobuflimit) == 0 && msgok)
			OutputMsg(0, U"Default limit set to %d", defobuflimit);
		if (display && *rc_name) {
			D_obufmax = defobuflimit;
			D_obuflenmax = D_obuflen - D_obufmax;
		}
		break;
	case RC_OBUFLIMIT:
		if (*args == 0)
			OutputMsg(0, U"Limit is %d, current buffer size is %d", D_obufmax, D_obuflen);
		else if (ParseNum(act, &D_obufmax) == 0 && msgok)
			OutputMsg(0, U"Limit set to %d", D_obufmax);
		D_obuflenmax = D_obuflen - D_obufmax;
		break;
	case RC_DUMPTERMCAP:
		WriteFile(user, (char *)0, DUMP_TERMCAP);
		break;
	case RC_HARDCOPY:
		{
			int mode = DUMP_HARDCOPY;
			uint32_t *file = NULL;

			if (args[0]) {
				if (!u32_strcmp(*args, U"-h")) {
					mode = DUMP_SCROLLBACK;
					file = args[1];
				} else if (!u32_strcmp(*args, U"--") && args[1])
					file = args[1];
				else
					file = args[0];
			}

			if (args[0] && file == args[0] && args[1]) {
				OutputMsg(0, U"%s: hardcopy: too many arguments", rc_name);
				break;
			}
			WriteFile(user, file, mode);
		}
		break;
	case RC_DEFLOG:
		(void)ParseOnOff(act, &nwin_default.Lflag);
		break;
	case RC_LOG:
		b = fore->w_log ? true : false;
		ParseSwitch(act, &b);
		LogToggle(b);
		break;
	case RC_SUSPEND:
		Detach(D_STOP);
		break;
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
			uint32_t *name;

			if (key >= 0) {
				Input(fore->w_pwin ? U"Really kill this filter [y/n]" : U"Really kill this window [y/n]",
				      1, INP_RAW, confirm_fn, NULL, RC_KILL);
				break;
			}
			n = fore->w_number;
			if (fore->w_pwin) {
				FreePseudowin(fore);
				OutputMsg(0, U"Filter removed.");
				break;
			}
			name = u32_strdup(fore->w_title);
			KillWindow(fore);
			OutputMsg(0, U"Window %d (%llU) killed.", n, name);
			if (name)
				free(name);
			break;
		}
	case RC_QUIT:
		if (key >= 0) {
			Input(U"Really quit and kill all your windows [y/n]", 1, INP_RAW, confirm_fn, NULL, RC_QUIT);
			break;
		}
		Finit(0);
		/* NOTREACHED */
	case RC_DETACH:
		if (*args && !u32_strcmp(*args, U"-h"))
			Hangup();
		else
			Detach(D_DETACH);
		break;
	case RC_POW_DETACH:
		if (key >= 0) {
			static uint32_t buf[2];

			buf[0] = key;
			Input(buf, 1, INP_RAW, pow_detach_fn, NULL, 0);
		} else
			Detach(D_POWER);	/* detach and kill Attacher's parent */
		break;
	case RC_ZMODEM:
		if (*args && !u32_strcmp(*args, U"sendcmd")) {
			if (args[1]) {
				free(zmodem_sendcmd);
				zmodem_sendcmd = u32_SaveStr(args[1]);
			}
			if (msgok)
				OutputMsg(0, U"zmodem sendcmd: %llU", zmodem_sendcmd);
			break;
		}
		if (*args && !u32_strcmp(*args, U"recvcmd")) {
			if (args[1]) {
				free(zmodem_recvcmd);
				zmodem_recvcmd = u32_SaveStr(args[1]);
			}
			if (msgok)
				OutputMsg(0, U"zmodem recvcmd: %llU", zmodem_recvcmd);
			break;
		}
		if (*args) {
			for (i = 0; i < 4; i++)
				if (!u32_strcmp(zmodes[i], *args))
					break;
			if (i == 4 && !u32_strcmp(*args, U"on"))
				i = 1;
			if (i == 4) {
				OutputMsg(0, U"usage: zmodem off|auto|catch|pass");
				break;
			}
			zmodem_mode = i;
		}
		if (msgok)
			OutputMsg(0, U"zmodem mode is %s", zmodes[zmodem_mode]);
		break;
	case RC_UNBINDALL:
		{
			unsigned int i;

			for (i = 0; i < sizeof(ktab) / sizeof(*ktab); i++)
				ClearAction(&ktab[i]);
			OutputMsg(0, U"Unbound all keys.");
			break;
		}
	case RC_ZOMBIE:
		{
			if (!(s = *args)) {
				ZombieKey_destroy = 0;
				break;
			}
			if (*argl == 0 || *argl > 2) {
				OutputMsg(0, U"%s:zombie: one or two characters expected.", rc_name);
				break;
			}
			if (args[1]) {
				if (!u32_strcmp(args[1], U"onerror")) {
					ZombieKey_onerror = 1;
				} else {
					OutputMsg(0, U"usage: zombie [keys [onerror]]");
					break;
				}
			} else
				ZombieKey_onerror = 0;
			ZombieKey_destroy = args[0][0];
			ZombieKey_resurrect = *argl == 2 ? args[0][1] : 0;
		}
		break;
	case RC_WALL:
		s = D_user->u_name;
		{
			OutputMsg(0, U"%s: %s", s, *args);
		}
		break;
	case RC_AT:
		/* where this AT command comes from: */
		if (!user)
			break;
		s = SaveStr(user->u_name);
		/* DO NOT RETURN FROM HERE WITHOUT RESETTING THIS: */
		EffectiveAclUser = user;
		n = u32_strlen(args[0]);
		if (n)
			n--;
		/*
		 * the windows/displays loops are quite dangerous here, take extra
		 * care not to trigger landmines. Things may appear/disappear while
		 * we are walking along.
		 */
		switch (args[0][n]) {
		case '*':	/* user */
			{
				Display *nd;
				struct acluser *u;

				if (!n)
					u = user;
				else {
					for (u = users; u; u = u->u_next) {
						if (!u32_strncmp(*args, u->u_name, n))
							break;
					}
					if (!u) {
						args[0][n] = '\0';
						OutputMsg(0, U"Did not find any user matching '%s'", args[0]);
						break;
					}
				}
				for (display = displays; display; display = nd) {
					nd = display->d_next;
					if (D_forecv == 0)
						continue;
					flayer = D_forecv->c_layer;
					fore = D_fore;
					if (D_user != u)
						continue;
					DoCommand(args + 1, argl + 1);
					if (display)
						OutputMsg(0, U"command from %s: %s %s",
							  s, args[1], args[2] ? args[2] : "");
					display = NULL;
					flayer = 0;
					fore = NULL;
				}
				break;
			}
		case '%':	/* display */
			{
				Display *nd;

				for (display = displays; display; display = nd) {
					nd = display->d_next;
					if (D_forecv == 0)
						continue;
					fore = D_fore;
					flayer = D_forecv->c_layer;
					if (u32_strncmp(args[0], D_usertty, n) &&
					    (strncmp("/dev/", D_usertty, 5) ||
					     u32_strncmp(args[0], D_usertty + 5, n)) &&
					    (strncmp("/dev/tty", D_usertty, 8) || u32_strncmp(args[0], D_usertty + 8, n)))
						continue;
					DoCommand(args + 1, argl + 1);
					if (display)
						OutputMsg(0, U"command from %s: %s %s",
							  s, args[1], args[2] ? args[2] : "");
					display = NULL;
					fore = NULL;
					flayer = 0;
				}
				break;
			}
		case '#':	/* window */
			n--;
			/* FALLTHROUGH */
		default:
			{
				Window *nw;
				int ch;

				n++;
				ch = args[0][n];
				args[0][n] = '\0';
				if (!*args[0] || (i = WindowByNumber(args[0])) < 0) {
					args[0][n] = ch;	/* must restore string in case of bind */
					/* try looping over titles */
					for (fore = windows; fore; fore = nw) {
						nw = fore->w_next;
						if (u32_strncmp(args[0], fore->w_title, n))
							continue;
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
						if (fore && fore->w_layer.l_cvlist) {
							display = fore->w_layer.l_cvlist->c_display;
							OutputMsg(0, U"command from %s: %s %s",
								  s, args[1], args[2] ? args[2] : "");
						}
					}
					display = NULL;
					fore = NULL;
					if (i < 0)
						OutputMsg(0, U"%s: at '%s': no such window.\n", rc_name, args[0]);
					break;
				} else if (i < maxwin && (fore = wtab[i])) {
					args[0][n] = ch;	/* must restore string in case of bind */
					if (fore->w_layer.l_cvlist)
						display = fore->w_layer.l_cvlist->c_display;
					flayer = fore->w_savelayer ? fore->w_savelayer : &fore->w_layer;
					DoCommand(args + 1, argl + 1);
					if (fore && fore->w_layer.l_cvlist) {
						display = fore->w_layer.l_cvlist->c_display;
						OutputMsg(0, U"command from %s: %s %s",
							  s, args[1], args[2] ? args[2] : "");
					}
					display = NULL;
					fore = NULL;
				} else
					OutputMsg(0, U"%s: at [identifier][%%|*|#] command [args]", rc_name);
				break;
			}
		}
		free(s);
		EffectiveAclUser = NULL;
		break;

	case RC_READREG:
		/*
		 * Without arguments we prompt for a destination register.
		 * It will receive the copybuffer contents.
		 * This is not done by RC_PASTE, as we prompt for source
		 * (not dest) there.
		 */
		if ((s = *args) == NULL) {
			Input(U"Copy to register:", 1, INP_RAW, copy_reg_fn, NULL, 0);
			break;
		}
		if (*argl != 1) {
			OutputMsg(0, U"%s: copyreg: character, ^x, or (octal) \\032 expected.", rc_name);
			break;
		}
		ch = args[0][0];
		/*
		 * With two arguments we *really* read register contents from file
		 */
		if (args[1]) {
			if (args[2]) {
				OutputMsg(0, U"%s: readreg: too many arguments", rc_name);
				break;
			}
			if ((s = ReadFile(args[1], &n))) {
				struct plop *pp = plop_tab + (int)(unsigned char)ch;

				if (pp->buf)
					free(pp->buf);
				pp->buf = s;
				pp->len = n;
			}
		} else
			/*
			 * with one argument we copy the copybuffer into a specified register
			 * This could be done with RC_PASTE too, but is here to be consistent
			 * with the zero argument call.
			 */
			copy_reg_fn(&ch, 0, NULL);
		break;
	case RC_REGISTER:
		if (argc != 2) {
			OutputMsg(0, U"%s: register: illegal number of arguments.", rc_name);
			break;
		}
		if (*argl != 1) {
			OutputMsg(0, U"%s: register: character, ^x, or (octal) \\032 expected.", rc_name);
			break;
		}
		ch = args[0][0];
		if (ch == '.') {
			if (user->u_plop.buf != NULL)
				UserFreeCopyBuffer(user);
			if (args[1] && args[1][0]) {
				user->u_plop.buf = SaveStrn(args[1], argl[1]);
				user->u_plop.len = argl[1];
			}
		} else {
			struct plop *plp = plop_tab + (int)(unsigned char)ch;

			if (plp->buf)
				free(plp->buf);
			plp->buf = SaveStrn(args[1], argl[1]);
			plp->len = argl[1];
		}
		break;
	case RC_PROCESS:
		if ((s = *args) == NULL) {
			Input(U"Process register:", 1, INP_RAW, process_fn, NULL, 0);
			break;
		}
		if (*argl != 1) {
			OutputMsg(0, U"%s: process: character, ^x, or (octal) \\032 expected.", rc_name);
			break;
		}
		ch = args[0][0];
		process_fn(&ch, 0, NULL);
		break;
	case RC_STUFF:
		s = *args;
		if (!args[0]) {
			Input(U"Stuff:", 100, INP_COOKED, StuffFin, NULL, 0);
			break;
		}
		len = *argl;
		if (args[1]) {
			if (u32_strcmp(s, U"-k")) {
				OutputMsg(0, U"%s: stuff: invalid option %s", rc_name, s);
				break;
			}
			s = args[1];
			for (i = T_CAPS; i < T_OCAPS; i++)
				if (strcmp(term[i].tcname, s) == 0)
					break;
			if (i == T_OCAPS) {
				OutputMsg(0, U"%s: stuff: unknown key '%s'", rc_name, s);
				break;
			}
			if (StuffKey(i - T_CAPS) == 0)
				break;
			s = display ? D_tcs[i].str : 0;
			if (s == 0)
				break;
			len = u32_strlen(s);
		}
		while (len)
			LayProcess(&s, &len);
		break;
	case RC_REDISPLAY:
		Activate(-1);
		break;
	case RC_WINDOWS:
		if (args[0]) {
			ShowWindowsX(args[0]);
			break;
		}
		ShowWindows(-1);
		break;
	case RC_VERSION:
		OutputMsg(0, U"screen %s", version);
		break;
	case RC_TIME:
		if (*args) {
			timestring = SaveStr(*args);
			break;
		}
		OutputMsg(0, U"%s", MakeWinMsg(timestring, fore, '%'));
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
			if (argc == 2 && !u32_strcmp(*args, U"-c")) {
				if ((ktabp = FindKtab(args[1], 0)) == 0) {
					OutputMsg(0, U"Unknown command class '%s'", args[1]);
					break;
				}
			}
			if (D_ESCseen != ktab || ktabp != ktab) {
				if (D_ESCseen != ktabp) {
					D_ESCseen = ktabp;
					WindowChanged(fore, WINESC_ESC_SEEN);
				}
				break;
			}
			if (D_ESCseen) {
				D_ESCseen = 0;
				WindowChanged(fore, WINESC_ESC_SEEN);
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
		len = 1;
		LayProcess(&s, &len);
		break;
	case RC_XON:
		ch = Ctrl('q');
		s = &ch;
		len = 1;
		LayProcess(&s, &len);
		break;
	case RC_XOFF:
		ch = Ctrl('s');
		s = &ch;
		len = 1;
		LayProcess(&s, &len);
		break;
	case RC_DEFBREAKTYPE:
	case RC_BREAKTYPE:
		{
			static char *types[] = { "TIOCSBRK", "TCSBRK", "tcsendbreak", NULL };

			if (*args) {
				if (ParseNum(act, &n))
					for (n = 0; n < (int)(sizeof(types) / sizeof(*types)); n++) {
						for (i = 0; i < 4; i++) {
							ch = args[0][i];
							if (ch >= 'a' && ch <= 'z')
								ch -= 'a' - 'A';
							if (ch != types[n][i] && (ch + ('a' - 'A')) != types[n][i])
								break;
						}
						if (i == 4)
							break;
					}
				if (n < 0 || n >= (int)(sizeof(types) / sizeof(*types)))
					OutputMsg(0, U"%s invalid, chose one of %s, %s or %s", *args, types[0], types[1],
						  types[2]);
				else {
					breaktype = n;
					OutputMsg(0, U"breaktype set to (%d) %s", n, types[n]);
				}
			}
		}
		break;
	case RC_POW_BREAK:
	case RC_BREAK:
		n = 0;
		if (*args && ParseNum(act, &n))
			break;
		SendBreak(fore, n, nr == RC_POW_BREAK);
		break;
	case RC_LOCKSCREEN:
		Detach(D_LOCK);
		break;
	case RC_WIDTH:
	case RC_HEIGHT:
		{
			int w, h;
			int what = 0;

			if (*args && !u32_strcmp(*args, U"-w"))
				what = 1;
			else if (*args && !u32_strcmp(*args, U"-d"))
				what = 2;
			if (what)
				args++;
			if (what == 0 && flayer && !display)
				what = 1;
			if (what == 1) {
				if (!flayer) {
					OutputMsg(0, U"%s: %s: window required", rc_name, comms[nr].name);
					break;
				}
				w = flayer->l_width;
				h = flayer->l_height;
			} else {
				if (!display) {
					OutputMsg(0, U"%s: %s: display required", rc_name, comms[nr].name);
					break;
				}
				w = D_width;
				h = D_height;
			}
			if (*args && args[0][0] == '-') {
				OutputMsg(0, U"%s: %s: unknown option %s", rc_name, comms[nr].name, *args);
				break;
			}
			if (nr == RC_HEIGHT) {
				if (!*args) {
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
				} else {
					h = u32_atoi(*args);
					if (args[1])
						w = u32_atoi(args[1]);
				}
			} else {
				if (!*args) {
					if (w == Z0width)
						w = Z1width;
					else if (w == Z1width)
						w = Z0width;
					else if (w > (Z0width + Z1width) / 2)
						w = Z0width;
					else
						w = Z1width;
				} else {
					w = u32_atoi(*args);
					if (args[1])
						h = u32_atoi(args[1]);
				}
			}
			if (*args && args[1] && args[2]) {
				OutputMsg(0, U"%s: %s: too many arguments", rc_name, comms[nr].name);
				break;
			}
			if (w <= 0) {
				OutputMsg(0, U"Illegal width");
				break;
			}
			if (h <= 0) {
				OutputMsg(0, U"Illegal height");
				break;
			}
			if (what == 1) {
				if (flayer->l_width == w && flayer->l_height == h)
					break;
				ResizeLayer(flayer, w, h, (Display *)0);
				break;
			}
			if (D_width == w && D_height == h)
				break;
			if (what == 2) {
				ChangeScreenSize(w, h, 1);
			} else {
				if (ResizeDisplay(w, h) == 0) {
					Activate(D_fore ? D_fore->w_norefresh : 0);
					/* autofit */
					ResizeLayer(D_forecv->c_layer, D_forecv->c_xe - D_forecv->c_xs + 1,
						    D_forecv->c_ye - D_forecv->c_ys + 1, 0);
					break;
				}
				if (h == D_height)
					OutputMsg(0,
						  U"Your termcap does not specify how to change the terminal's width to %d.",
						  w);
				else if (w == D_width)
					OutputMsg(0,
						  U"Your termcap does not specify how to change the terminal's height to %d.",
						  h);
				else
					OutputMsg(0,
						  U"Your termcap does not specify how to change the terminal's resolution to %dx%d.",
						  w, h);
			}
		}
		break;
	case RC_DEFDYNAMICTITLE:
		(void)ParseOnOff(act, &nwin_default.dynamicaka);
		break;
	case RC_DYNAMICTITLE:
		(void)ParseOnOff(act, &fore->w_dynamicaka);
		break;
	case RC_TITLE:
		if (queryflag >= 0) {
			if (fore)
				OutputMsg(0, U"%s", fore->w_title);
			else
				queryflag = -1;
			break;
		}
		if (*args == 0)
			InputAKA();
		else
			ChangeAKA(fore, *args, u32_strlen(*args));
		break;
	case RC_COLON:
		Input(U":", MAXSTR, INP_EVERY, ColonFin, NULL, 0);
		if (*args && **args) {
			s = *args;
			len = u32_strlen(s);
			LayProcess(&s, &len);
		}
		break;
	case RC_LASTMSG:
		if (D_status_lastmsg)
			OutputMsg(0, U"%s", D_status_lastmsg);
		break;
	case RC_SCREEN:
		DoScreen("key", args);
		break;
	case RC_WRAP:
		if (ParseSwitch(act, &fore->w_wrap) == 0 && msgok)
			OutputMsg(0, U"%cwrap", fore->w_wrap ? '+' : '-');
		break;
	case RC_FLOW:
		if (*args) {
			if (args[0][0] == 'a') {
				fore->w_flow =
				    (fore->w_flow & FLOW_AUTO) ? FLOW_AUTOFLAG | FLOW_AUTO | FLOW_ON : FLOW_AUTOFLAG;
			} else 	if (ParseOnOff(act, &b) == 0)
				fore->w_flow = (fore->w_flow & FLOW_AUTO) | b ? FLOW_ON : FLOW_OFF;
		} else {
			if (fore->w_flow & FLOW_AUTOFLAG)
				fore->w_flow = (fore->w_flow & FLOW_AUTO) | FLOW_ON;
			else if (fore->w_flow & FLOW_ON)
				fore->w_flow &= ~FLOW_ON;
			else
				fore->w_flow = fore->w_flow ? FLOW_AUTOFLAG | FLOW_AUTO | FLOW_ON : FLOW_AUTOFLAG;
		}
		SetFlow(fore->w_flow & FLOW_ON);
		if (msgok)
			OutputMsg(0, U"%cflow%s", (fore->w_flow & FLOW_ON) ? '+' : '-',
				  (fore->w_flow & FLOW_AUTOFLAG) ? "(auto)" : "");
		break;
	case RC_DEFWRITELOCK:
		if (args[0][0] == 'a')
			nwin_default.wlock = WLOCK_AUTO;
		else if (ParseOnOff(act, &b) == 0)
			nwin_default.wlock = b ? WLOCK_ON : WLOCK_OFF;
		break;
	case RC_WRITELOCK:
		if (*args) {
			if (args[0][0] == 'a') {
				fore->w_wlock = WLOCK_AUTO;
			} else {
				if (ParseOnOff(act, &b))
					break;
				fore->w_wlock = b ? WLOCK_ON : WLOCK_OFF;
			}
			/* 
			 * user may have permission to change the writelock setting, 
			 * but he may never aquire the lock himself without write permission
			 */
			if (!AclCheckPermWin(D_user, ACL_WRITE, fore))
				fore->w_wlockuser = D_user;
		}
		OutputMsg(0, U"writelock %s", (fore->w_wlock == WLOCK_AUTO) ? "auto" :
			  ((fore->w_wlock == WLOCK_OFF) ? "off" : "on"));
		break;
	case RC_CLEAR:
		ResetAnsiState(fore);
		WriteString(fore, U"\033[H\033[J", 6);
		break;
	case RC_RESET:
		ResetAnsiState(fore);
		if (fore->w_zdisplay)
			zmodem_abort(fore, fore->w_zdisplay);
		WriteString(fore, U"\033c", 2);
		break;
	case RC_MONITOR:
		b = fore->w_monitor != MON_OFF;
		if (display)
			b = b && (ACLBYTE(fore->w_mon_notify, D_user->u_id) & ACLBIT(D_user->u_id));
		if (ParseSwitch(act, &b))
			break;
		if (b) {
			if (display)	/* we tell only this user */
				ACLBYTE(fore->w_mon_notify, D_user->u_id) |= ACLBIT(D_user->u_id);
			else
				for (i = 0; i < maxusercount; i++)
					ACLBYTE(fore->w_mon_notify, i) |= ACLBIT(i);
			if (fore->w_monitor == MON_OFF)
				fore->w_monitor = MON_ON;
			OutputMsg(0, U"Window %d (%s) is now being monitored for all activity.", fore->w_number,
				  fore->w_title);
		} else {
			if (display)	/* we remove only this user */
				ACLBYTE(fore->w_mon_notify, D_user->u_id)
				    &= ~ACLBIT(D_user->u_id);
			else
				for (i = 0; i < maxusercount; i++)
					ACLBYTE(fore->w_mon_notify, i) &= ~ACLBIT(i);
			for (i = maxusercount - 1; i >= 0; i--)
				if (ACLBYTE(fore->w_mon_notify, i))
					break;
			if (i < 0)
				fore->w_monitor = MON_OFF;
			OutputMsg(0, U"Window %d (%s) is no longer being monitored for activity.", fore->w_number,
				  fore->w_title);
		}
		break;
	case RC_DISPLAYS:
		display_displays();
		break;
	case RC_WINDOWLIST:
		if (!*args)
			display_windows(0, WLIST_NUM, (Window *)0);
		else if (!u32_strcmp(*args, U"string")) {
			if (args[1]) {
				if (wliststr)
					free(wliststr);
				wliststr = SaveStr(args[1]);
			}
			if (msgok)
				OutputMsg(0, U"windowlist string is '%s'", wliststr);
		} else if (!u32_strcmp(*args, U"title")) {
			if (args[1]) {
				if (wlisttit)
					free(wlisttit);
				wlisttit = SaveStr(args[1]);
			}
			if (msgok)
				OutputMsg(0, U"windowlist title is '%s'", wlisttit);
		} else {
			int flag = 0;
			int blank = 0;
			for (i = 0; i < argc; i++)
				if (!args[i])
					continue;
				else if (!u32_strcmp(args[i], U"-m"))
					flag |= WLIST_MRU;
				else if (!u32_strcmp(args[i], U"-b"))
					blank = 1;
				else if (!u32_strcmp(args[i], U"-g"))
					flag |= WLIST_NESTED;
				else {
					OutputMsg(0,
						  U"usage: windowlist [-b] [-g] [-m] [string [string] | title [title]]");
					break;
				}
			if (i == argc)
				display_windows(blank, flag, (Window *)0);
		}
		break;
	case RC_HELP:
		if (argc == 2 && !u32_strcmp(*args, U"-c")) {
			struct action *ktabp;
			if ((ktabp = FindKtab(args[1], 0)) == 0) {
				OutputMsg(0, U"Unknown command class '%s'", args[1]);
				break;
			}
			display_help(args[1], ktabp);
		} else
			display_help((char *)0, ktab);
		break;
	case RC_LICENSE:
		display_copyright();
		break;
	case RC_COPY:
		if (flayer->l_layfn != &WinLf) {
			OutputMsg(0, U"Must be on a window layer");
			break;
		}
		MarkRoutine();
		WindowChanged(fore, WINESC_COPY_MODE);
		break;
	case RC_HISTORY:
		{
			static uint32_t *pasteargs[] = { U".", 0 };
			static int pasteargl[] = { 1 };

			if (flayer->l_layfn != &WinLf) {
				OutputMsg(0, U"Must be on a window layer");
				break;
			}
			if (GetHistory() == 0)
				break;
			if (user->u_plop.buf == NULL)
				break;
			args = pasteargs;
			argl = pasteargl;
		}
	 /*FALLTHROUGH*/ case RC_PASTE:
		{
			uint32_t *ss, *dbuf, dch;
			size_t l = 0;

			/*
			 * without args we prompt for one(!) register to be pasted in the window
			 */
			if ((s = *args) == NULL) {
				Input(U"Paste from register:", 1, INP_RAW, ins_reg_fn, NULL, 0);
				break;
			}
			if (args[1] == 0 && !fore)	/* no window? */
				break;
			/*
			 * with two arguments we paste into a destination register
			 * (no window needed here).
			 */
			if (args[1] && argl[1] != 1) {
				OutputMsg(0, U"%s: paste destination: character, ^x, or (octal) \\032 expected.",
					  rc_name);
				break;
			}

			/*
			 * measure length of needed buffer
			 */
			for (ss = s = *args; (ch = *ss); ss++) {
				if (ch == '.') {
					l += user->u_plop.len;
				} else {
					l += plop_tab[(int)(unsigned char)ch].len;
				}
			}
			if (l == 0) {
				OutputMsg(0, U"empty buffer");
				break;
			}
			/*
			 * shortcut:
			 * if there is only one source and the destination is a window, then
			 * pass a pointer rather than duplicating the buffer.
			 */
			if (s[1] == 0 && args[1] == 0) {
				MakePaster(&fore->w_paster,
					   *s == '.' ? user->u_plop.buf : plop_tab[(int)(unsigned char)*s].buf, l, 0);
				break;
			}
			/*
			 * if no shortcut, we construct a buffer
			 */
			if ((dbuf = malloc(l)) == 0) {
				OutputMsg(0, U"%s", strnomem);
				break;
			}
			l = 0;
			/*
			 * concatenate all sources into our own buffer, copy buffer is
			 * special and is skipped if no display exists.
			 */
			for (ss = s; (ch = *ss); ss++) {
				struct plop *pp = (ch == '.' ? &user->u_plop : &plop_tab[(int)(unsigned char)ch]);
				memmove(dbuf + l, pp->buf, pp->len);
				l += pp->len;
			}
			/*
			 * when called with one argument we paste our buffer into the window
			 */
			if (args[1] == 0) {
				MakePaster(&fore->w_paster, dbuf, l, 1);
			} else {
				/*
				 * we have two arguments, the second is already in dch.
				 * use this as destination rather than the window.
				 */
				dch = args[1][0];
				if (dch == '.') {
					if (user->u_plop.buf != NULL)
						UserFreeCopyBuffer(user);
					user->u_plop.buf = dbuf;
					user->u_plop.len = l;
				} else {
					struct plop *pp = plop_tab + (int)(unsigned char)dch;
					if (pp->buf)
						free(pp->buf);
					pp->buf = dbuf;
					pp->len = l;
				}
			}
			break;
		}
	case RC_WRITEBUF:
		if (!user->u_plop.buf) {
			OutputMsg(0, U"empty buffer");
			break;
		}
		if (args[0] && args[1])
			OutputMsg(0, U"%s: writebuf: too many arguments", rc_name);
		else
			WriteFile(user, args[0], DUMP_EXCHANGE);
		break;
	case RC_READBUF:
		if (args[0] && args[1]) {
			OutputMsg(0, U"%s: readbuf: too many arguments", rc_name);
			break;
		}
		if ((s = ReadFile(args[0] ? args[0] : BufferFile, &n))) {
			if (user->u_plop.buf)
				UserFreeCopyBuffer(user);
			user->u_plop.len = n;
			user->u_plop.buf = s;
			OutputMsg(0, U"Read contents of %s into copybuffer",
                                  args[0] ? args[0] : BufferFile);
		}
		break;
	case RC_REMOVEBUF:
		KillBuffers();
		break;
	case RC_IGNORECASE:
		(void)ParseSwitch(act, &search_ic);
		if (msgok)
			OutputMsg(0, U"Will %signore case in searches", search_ic ? "" : "not ");
		break;
	case RC_ESCAPE:
		if (*argl == 0)
			SetEscape(user, -1, -1);
		else if (*argl == 2)
			SetEscape(user, (int)(unsigned char)args[0][0], (int)(unsigned char)args[0][1]);
		else {
			OutputMsg(0, U"%s: two characters required after escape.", rc_name);
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
		else {
			OutputMsg(0, U"%s: two characters required after defescape.", rc_name);
			break;
		}
		CheckEscape();
		break;
	case RC_CHDIR:
		s = *args ? *args : home;
		if (chdir(s) == -1)
			OutputMsg(errno, U"%s", s);
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
			OutputMsg(0, U"hardcopydir is %s\n", hardcopydir && *hardcopydir ? hardcopydir : "<cwd>");
		break;
	case RC_LOGFILE:
		if (*args) {
			char buf[1024];
			if (args[1] && !(u32_strcmp(*args, U"flush"))) {
				log_flush = u32_atoi(args[1]);
				if (msgok)
					OutputMsg(0, U"log flush timeout set to %ds\n", log_flush);
				break;
			}
			if (ParseSaveStr(act, &screenlogfile))
				break;
			if (fore && fore->w_log)
				if (DoStartLog(fore, buf, sizeof(buf)))
					OutputMsg(0, U"Error opening logfile \"%s\"", buf);
			if (!msgok)
				break;
		}
		OutputMsg(0, U"logfile is '%s'", screenlogfile);
		break;
	case RC_LOGTSTAMP:
		if (!*args || !u32_strcmp(*args, U"on") || !u32_strcmp(*args, U"off")) {
			if (ParseSwitch(act, &logtstamp_on) == 0 && msgok)
				OutputMsg(0, U"timestamps turned %s", logtstamp_on ? "on" : "off");
		} else if (!u32_strcmp(*args, U"string")) {
			if (args[1]) {
				if (logtstamp_string)
					free(logtstamp_string);
				logtstamp_string = SaveStr(args[1]);
			}
			if (msgok)
				OutputMsg(0, U"logfile timestamp is '%s'", logtstamp_string);
		} else if (!u32_strcmp(*args, U"after")) {
			if (args[1]) {
				logtstamp_after = u32_atoi(args[1]);
				if (!msgok)
					break;
			}
			OutputMsg(0, U"timestamp printed after %ds\n", logtstamp_after);
		} else
			OutputMsg(0, U"usage: logtstamp [after [n]|string [str]|on|off]");
		break;
	case RC_SHELLTITLE:
		(void)ParseSaveStr(act, &nwin_default.aka);
		break;
	case RC_TERMCAP:
	case RC_TERMCAPINFO:
	case RC_TERMINFO:
		if (!rc_name || !*rc_name)
			OutputMsg(0, U"Sorry, too late now. Place that in your .screenrc file.");
		break;
	case RC_SLEEP:
		break;		/* Already handled */
	case RC_TERM:
		s = NULL;
		if (ParseSaveStr(act, &s))
			break;
		if (u32_strlen(s) > MAXTERMLEN) {
			OutputMsg(0, U"%s: term: argument too long ( < %d)", rc_name, MAXTERMLEN);
			free(s);
			break;
		}
		strncpy(screenterm, s, MAXTERMLEN);
		screenterm[MAXTERMLEN] = '\0';
		free(s);
		MakeTermcap((display == 0));
		break;
	case RC_ECHO:
		if (!msgok && (!rc_name || strcmp(rc_name, "-X")))
			break;
		/*
		 * user typed ^A:echo... well, echo isn't FinishRc's job,
		 * but as he wanted to test us, we show good will
		 */
		if (argc > 1 && !u32_strcmp(*args, U"-n")) {
			args++;
			argc--;
		}
		s = *args;
		if (argc > 1 && !u32_strcmp(*args, U"-p")) {
			args++;
			argc--;
			s = *args;
			if (s)
				s = MakeWinMsg(s, fore, '%');
		}
		if (s)
			OutputMsg(0, U"%s", s);
		else {
			OutputMsg(0, U"%s: 'echo [-n] [-p] \"string\"' expected.", rc_name);
			queryflag = -1;
		}
		break;
	case RC_BELL:
	case RC_BELL_MSG:
		if (*args == 0) {
			char buf[256];
			AddXChars(buf, sizeof(buf), BellString);
			OutputMsg(0, U"bell_msg is '%s'", buf);
			break;
		}
		(void)ParseSaveStr(act, &BellString);
		break;
	case RC_BUFFERFILE:
		if (*args == 0)
			BufferFile = SaveStr(DEFAULT_BUFFERFILE);
		else if (ParseSaveStr(act, &BufferFile))
			break;
		if (msgok)
			OutputMsg(0, U"Bufferfile is now '%s'", BufferFile);
		break;
	case RC_ACTIVITY:
		(void)ParseSaveStr(act, &ActivityString);
		break;
	case RC_POW_DETACH_MSG:
		if (*args == 0) {
			char buf[256];
			AddXChars(buf, sizeof(buf), PowDetachString);
			OutputMsg(0, U"pow_detach_msg is '%s'", buf);
			break;
		}
		(void)ParseSaveStr(act, &PowDetachString);
		break;
#if defined(ENABLE_UTMP) && defined(LOGOUTOK)
	case RC_LOGIN:
		b = fore->w_slot != (slot_t)(-1);
		if (*args && !u32_strcmp(*args, U"always")) {
			fore->w_lflag = 3;
			if (!displays && b)
				SlotToggle(b);
			break;
		}
		if (*args && !u32_strcmp(*args, U"attached")) {
			fore->w_lflag = 1;
			if (!displays && b)
				SlotToggle(0);
			break;
		}
		if (ParseSwitch(act, &b) == 0)
			SlotToggle(b);
		break;
	case RC_DEFLOGIN:
		if (!u32_strcmp(*args, U"always"))
			nwin_default.lflag |= 2;
		else if (!u32_strcmp(*args, U"attached"))
			nwin_default.lflag &= ~2;
		else if (ParseOnOff(act, &b) == 0)
			nwin_default.lflag = b ? 1 : 0;
		break;
#endif
	case RC_DEFFLOW:
		if (args[0] && args[1] && args[1][0] == 'i') {
			iflag = true;
			for (display = displays; display; display = display->d_next) {
				if (!D_flow)
					continue;
				D_NewMode.tio.c_cc[VINTR] = D_OldMode.tio.c_cc[VINTR];
				D_NewMode.tio.c_lflag |= ISIG;
				SetTTY(D_userfd, &D_NewMode);
			}
		}
		if (args[0] && args[0][0] == 'a')
			nwin_default.flowflag = FLOW_AUTOFLAG;
		else if (ParseOnOff(act, &b) == 0) 
			nwin_default.flowflag = b ? FLOW_ON : FLOW_OFF;
		break;
	case RC_DEFWRAP:
		(void)ParseOnOff(act, &nwin_default.wrap);
		break;
	case RC_DEFC1:
		(void)ParseOnOff(act, &nwin_default.c1);
		break;
	case RC_DEFBCE:
		if (ParseOnOff(act, &b) == 0)
			nwin_default.bce = b ? 1 : 0;
		break;
	case RC_DEFGR:
		if (ParseOnOff(act, &b) == 0)
			nwin_default.gr = b ? 1 : 0;
		break;
	case RC_DEFMONITOR:
		if (ParseOnOff(act, &b) == 0)
			nwin_default.monitor = b ? MON_ON : MON_OFF;
		break;
	case RC_DEFMOUSETRACK:
		if (ParseOnOff(act, &b) == 0)
			defmousetrack = b ? 1000 : 0;
		break;
	case RC_MOUSETRACK:
		if (!args[0]) {
			OutputMsg(0, U"Mouse tracking for this display is turned %s", D_mousetrack ? "on" : "off");
		} else if (ParseOnOff(act, &b) == 0) {
			D_mousetrack = b ? 1000 : 0;
			if (D_fore)
				MouseMode(D_fore->w_mouse);
		}
		break;
	case RC_DEFSILENCE:
		if (ParseOnOff(act, &b) == 0)
			nwin_default.silence = b ? SILENCE_ON : SILENCE_OFF;
		break;
	case RC_VERBOSE:
		if (!*args)
			OutputMsg(0, U"W%s echo command when creating windows.", VerboseCreate ? "ill" : "on't");
		else
			ParseOnOff(act, &VerboseCreate);
		break;
	case RC_HARDSTATUS:
		if (display) {
			OutputMsg(0, U"%s", "");	/* wait till mintime (keep gcc quiet) */
			RemoveStatus();
		}
		if (args[0] && u32_strcmp(args[0], U"on") && u32_strcmp(args[0], U"off")) {
			Display *olddisplay = display;
			int old_use, new_use = -1;

			s = args[0];
			if (!u32_strncmp(s, U"always", 6))
				s += 6;
			if (!u32_strcmp(s, U"firstline"))
				new_use = HSTATUS_FIRSTLINE;
			else if (!u32_strcmp(s, U"lastline"))
				new_use = HSTATUS_LASTLINE;
			else if (!u32_strcmp(s, U"ignore"))
				new_use = HSTATUS_IGNORE;
			else if (!u32_strcmp(s, U"message"))
				new_use = HSTATUS_MESSAGE;
			else if (!u32_strcmp(args[0], U"string")) {
				if (!args[1]) {
					uint32_t buf[256];
					AddXChars(buf, sizeof(buf), hstatusstring);
					OutputMsg(0, U"hardstatus string is '%s'", buf);
					break;
				}
			} else {
				OutputMsg(0, U"%s: usage: hardstatus [always]lastline|ignore|message|string [string]",
					  rc_name);
				break;
			}
			if (new_use != -1) {
				hardstatusemu = new_use | (s == args[0] ? 0 : HSTATUS_ALWAYS);
				for (display = displays; display; display = display->d_next) {
					RemoveStatus();
					new_use = hardstatusemu & ~HSTATUS_ALWAYS;
					if (D_HS && s == args[0])
						new_use = HSTATUS_HS;
					ShowHStatus((uint32_t *)0);
					old_use = D_has_hstatus;
					D_has_hstatus = new_use;
					if ((new_use == HSTATUS_LASTLINE && old_use != HSTATUS_LASTLINE)
					    || (new_use != HSTATUS_LASTLINE && old_use == HSTATUS_LASTLINE))
						ChangeScreenSize(D_width, D_height, 1);
					if ((new_use == HSTATUS_FIRSTLINE && old_use != HSTATUS_FIRSTLINE)
					    || (new_use != HSTATUS_FIRSTLINE && old_use == HSTATUS_FIRSTLINE))
						ChangeScreenSize(D_width, D_height, 1);
					RefreshHStatus();
				}
			}
			if (args[1]) {
				if (hstatusstring)
					free(hstatusstring);
				hstatusstring = u32_strdup(args[1]);
				for (display = displays; display; display = display->d_next)
					RefreshHStatus();
			}
			display = olddisplay;
			break;
		}
		(void)ParseSwitch(act, &use_hardstatus);
		if (msgok)
			OutputMsg(0, U"messages displayed on %s", use_hardstatus ? "hardstatus line" : "window");
		break;
	case RC_STATUS:
		if (display) {
			Msg(0, U"%s", "");	/* wait till mintime (keep gcc quiet) */
			RemoveStatus();
		}
		{
			int	i = 0;
			while ( (i <= 1) && args[i]) {
				if ( (u32_strcmp(args[i], U"top") == 0) || (u32_strcmp(args[i], U"up") == 0) ) {
					statuspos.row = STATUS_TOP;
				} else if ( (u32_strcmp(args[i], U"bottom") == 0) || (u32_strcmp(args[i], U"down") == 0) ) {
					statuspos.row = STATUS_BOTTOM;
				} else if (u32_strcmp(args[i], U"left") == 0) {
					statuspos.col = STATUS_LEFT;
				} else if (u32_strcmp(args[i], U"right") == 0) {
					statuspos.col = STATUS_RIGHT;
				} else {
					Msg(0, U"%s: usage: status [top|up|down|bottom] [left|right]", rc_name);
					break;
				}
				i++;
			}
		}
		break;
	case RC_CAPTION:
		if (u32_strcmp(args[0], U"top") == 0) {
			captiontop = 1;
			args++;
		} else if(u32_strcmp(args[0], U"bottom") == 0) {
			captiontop = 0;
			args++;
		}
		if (u32_strcmp(args[0], U"always") == 0 || u32_strcmp(args[0], U"splitonly") == 0) {
			Display *olddisplay = display;

			captionalways = args[0][0] == 'a';
			for (display = displays; display; display = display->d_next)
				ChangeScreenSize(D_width, D_height, 1);
			display = olddisplay;
		} else if (u32_strcmp(args[0], U"string") == 0) {
			if (!args[1]) {
				char buf[256];
				AddXChars(buf, sizeof(buf), captionstring);
				OutputMsg(0, U"caption string is '%s'", buf);
				break;
			}
		} else {
			OutputMsg(0, U"%s: usage: caption [ top | bottom ] always|splitonly|string <string>", rc_name);
			break;
		}
		if (!args[1])
			break;
		if (captionstring)
			free(captionstring);
		captionstring = u32_strdup(args[1]);
		RedisplayDisplays(0);
		break;
	case RC_CONSOLE:
		b = (console_window != 0);
		if (ParseSwitch(act, &b))
			break;
		if (TtyGrabConsole(fore->w_ptyfd, b, rc_name))
			break;
		if (b == 0)
			OutputMsg(0, U"%s: releasing console %s", rc_name, HostName);
		else if (console_window)
			OutputMsg(0, U"%s: stealing console %s from window %d (%s)", rc_name,
				  HostName, console_window->w_number, console_window->w_title);
		else
			OutputMsg(0, U"%s: grabbing console %s", rc_name, HostName);
		console_window = b ? fore : 0;
		break;
	case RC_ALLPARTIAL:
		if (ParseOnOff(act, &all_norefresh))
			break;
		if (!all_norefresh && fore)
			Activate(-1);
		if (msgok)
			OutputMsg(0, all_norefresh ? U"No refresh on window change!\n" : U"Window specific refresh\n");
		break;
	case RC_PARTIAL:
		(void)ParseSwitch(act, &b);
		fore->w_norefresh = b;
		break;
	case RC_VBELL:
		if (ParseSwitch(act, &visual_bell) || !msgok)
			break;
		if (visual_bell == 0)
			OutputMsg(0, U"switched to audible bell.");
		else
			OutputMsg(0, U"switched to visual bell.");
		break;
	case RC_VBELLWAIT:
		if (ParseNum1000(act, &VBellWait) == 0 && msgok)
			OutputMsg(0, U"vbellwait set to %.10g seconds", VBellWait / 1000.);
		break;
	case RC_MSGWAIT:
		if (ParseNum1000(act, &MsgWait) == 0 && msgok)
			OutputMsg(0, U"msgwait set to %.10g seconds", MsgWait / 1000.);
		break;
	case RC_MSGMINWAIT:
		if (ParseNum1000(act, &MsgMinWait) == 0 && msgok)
			OutputMsg(0, U"msgminwait set to %.10g seconds", MsgMinWait / 1000.);
		break;
	case RC_SILENCEWAIT:
		if (ParseNum(act, &SilenceWait))
			break;
		if (SilenceWait < 1)
			SilenceWait = 1;
		for (p = windows; p; p = p->w_next)
			p->w_silencewait = SilenceWait;
		if (msgok)
			OutputMsg(0, U"silencewait set to %d seconds", SilenceWait);
		break;
	case RC_BUMPRIGHT:
		if (fore->w_number < NextWindow())
			SwapWindows(fore->w_number, NextWindow());
		break;
	case RC_BUMPLEFT:
		if (fore->w_number > PreviousWindow())
			SwapWindows(fore->w_number, PreviousWindow());
		break;
	case RC_COLLAPSE:
		CollapseWindowlist();
		break;
	case RC_NUMBER:
		if (*args == 0)
			OutputMsg(0, queryflag >= 0 ? U"%d (%s)" : U"This is window %d (%s).", fore->w_number,
				  fore->w_title);
		else {
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
			if (!SwapWindows(old, n)) {
				/* Window number could not be changed. */
				queryflag = -1;
				return;
			}
		}
		break;

	case RC_ZOMBIE_TIMEOUT:
		if (argc != 1) {
			Msg(0, U"Setting zombie polling needs a timeout arg\n");
			break;
		}

		nwin_default.poll_zombie_timeout = u32_atoi(args[0]);
		if (fore)
			fore->w_poll_zombie_timeout = nwin_default.poll_zombie_timeout;
		break;
	case RC_SORT:
		i = 0;
		if (!wtab[i] || !wtab[i + 1]) {
			Msg(0, U"Less than two windows, sorting makes no sense.\n");
			break;
		}
		for (i = 0; wtab[i + 1] != NULL; i++) {
			for (n = i, nr = i; wtab[n + 1] != NULL; n++) {
				if (u32_strcmp(wtab[nr]->w_title, wtab[n + 1]->w_title) > 0) {
					nr = n + 1;
				}
			}
			if (nr != i) {
				SwapWindows(nr, i);
			}
		}
		WindowChanged((Window *)0, 0);
		break;
	case RC_SILENCE:
		b = fore->w_silence != 0;
		j = fore->w_silencewait;
		if (args[0] && (args[0][0] == '-' || (args[0][0] >= '0' && args[0][0] <= '9'))) {
			if (ParseNum(act, &j))
				break;
			n = j > 0;
		} else if (ParseSwitch(act, &b))
			break;
		if (b) {
			if (display)	/* we tell only this user */
				ACLBYTE(fore->w_lio_notify, D_user->u_id) |= ACLBIT(D_user->u_id);
			else
				for (n = 0; n < maxusercount; n++)
					ACLBYTE(fore->w_lio_notify, n) |= ACLBIT(n);
			fore->w_silencewait = j;
			fore->w_silence = SILENCE_ON;
			SetTimeout(&fore->w_silenceev, fore->w_silencewait * 1000);
			evenq(&fore->w_silenceev);

			if (!msgok)
				break;
			OutputMsg(0, U"The window is now being monitored for %d sec. silence.", fore->w_silencewait);
		} else {
			if (display)	/* we remove only this user */
				ACLBYTE(fore->w_lio_notify, D_user->u_id)
				    &= ~ACLBIT(D_user->u_id);
			else
				for (n = 0; n < maxusercount; n++)
					ACLBYTE(fore->w_lio_notify, n) &= ~ACLBIT(n);
			for (i = maxusercount - 1; i >= 0; i--)
				if (ACLBYTE(fore->w_lio_notify, i))
					break;
			if (i < 0) {
				fore->w_silence = SILENCE_OFF;
				evdeq(&fore->w_silenceev);
			}
			if (!msgok)
				break;
			OutputMsg(0, U"The window is no longer being monitored for silence.");
		}
		break;
	case RC_DEFSCROLLBACK:
		(void)ParseNum(act, &nwin_default.histheight);
		break;
	case RC_SCROLLBACK:
		if (flayer->l_layfn == &MarkLf) {
			OutputMsg(0, U"Cannot resize scrollback buffer in copy/scrollback mode.");
			break;
		}
		(void)ParseNum(act, &n);
		ChangeWindowSize(fore, fore->w_width, fore->w_height, n);
		if (msgok)
			OutputMsg(0, U"scrollback set to %d", fore->w_histheight);
		break;
	case RC_SESSIONNAME:
		if (*args == 0)
			OutputMsg(0, U"This session is named '%s'\n", SocketName);
		else {
			char buf[MAXPATHLEN];

			s = 0;
			if (ParseSaveStr(act, &s))
				break;
			if (!*s || strlen(s) + (SocketName - SocketPath) > MAXPATHLEN - 13 || strchr(s, '/')) {
				OutputMsg(0, U"%s: bad session name '%s'\n", rc_name, s);
				free(s);
				break;
			}
			strncpy(buf, SocketPath, SocketName - SocketPath);
			sprintf(buf + (SocketName - SocketPath), "%d.%s", (int)getpid(), s);
			free(s);
			if ((access(buf, F_OK) == 0) || (errno != ENOENT)) {
				OutputMsg(0, U"%s: inappropriate path: '%s'.", rc_name, buf);
				break;
			}
			if (rename(SocketPath, buf)) {
				OutputMsg(errno, U"%s: failed to rename(%s, %s)", rc_name, SocketPath, buf);
				break;
			}
			strncpy(SocketPath, buf, MAXPATHLEN + 2 * MAXSTR);
			MakeNewEnv();
			WindowChanged((Window *)0, WINESC_SESS_NAME);
		}
		break;
	case RC_SETENV:
		if (!args[0] || !args[1]) {
			InputSetenv(args[0]);
		} else {
			setenv(args[0], args[1], 1);
			MakeNewEnv();
		}
		break;
	case RC_UNSETENV:
		unsetenv(*args);
		MakeNewEnv();
		break;
	case RC_DEFSLOWPASTE:
		(void)ParseNum(act, &nwin_default.slow);
		break;
	case RC_SLOWPASTE:
		if (*args == 0)
			OutputMsg(0, fore->w_slowpaste ?
				  U"Slowpaste in window %d is %d milliseconds." :
				  U"Slowpaste in window %d is unset.", fore->w_number, fore->w_slowpaste);
		else if (ParseNum(act, &fore->w_slowpaste) == 0 && msgok)
			OutputMsg(0, fore->w_slowpaste ?
				  U"Slowpaste in window %d set to %d milliseconds." :
				  U"Slowpaste in window %d now unset.", fore->w_number, fore->w_slowpaste);
		break;
	case RC_MARKKEYS:
		if (CompileKeys(*args, *argl, mark_key_tab)) {
			OutputMsg(0, U"%s: markkeys: syntax error.", rc_name);
			break;
		}
		break;
	case RC_CRLF:
		(void)ParseSwitch(act, &join_with_cr);
		break;
	case RC_COMPACTHIST:
		if (ParseSwitch(act, &compacthist) == 0 && msgok)
			OutputMsg(0, U"%scompacting history lines", compacthist ? "" : "not ");
		break;
	case RC_HARDCOPY_APPEND:
		(void)ParseOnOff(act, &hardcopy_append);
		break;
	case RC_VBELL_MSG:
		if (*args == 0) {
			char buf[256];
			AddXChars(buf, sizeof(buf), VisualBellString);
			OutputMsg(0, U"vbell_msg is '%s'", buf);
			break;
		}
		(void)ParseSaveStr(act, &VisualBellString);
		break;
	case RC_DEFMODE:
		if (ParseBase(act, *args, &n, 8, "octal"))
			break;
		if (n < 0 || n > 0777) {
			OutputMsg(0, U"%s: mode: Invalid tty mode %o", rc_name, n);
			break;
		}
		TtyMode = n;
		if (msgok)
			OutputMsg(0, U"Ttymode set to %03o", TtyMode);
		break;
	case RC_AUTODETACH:
		(void)ParseOnOff(act, &auto_detach);
		break;
	case RC_STARTUP_MESSAGE:
		(void)ParseOnOff(act, &default_startup);
		break;
	case RC_BIND:
		{
			struct action *ktabp = ktab;
			int kflag = 0;

			for (;;) {
				if (argc > 2 && !u32_strcmp(*args, U"-c")) {
					ktabp = FindKtab(args[1], 1);
					if (ktabp == 0)
						break;
					args += 2;
					argl += 2;
					argc -= 2;
				} else if (argc > 1 && !u32_strcmp(*args, U"-k")) {
					kflag = 1;
					args++;
					argl++;
					argc--;
				} else
					break;
			}
			if (kflag) {
				for (n = 0; n < KMAP_KEYS; n++)
					if (u32_strcmp(term[n + T_CAPS].tcname, *args) == 0)
						break;
				if (n == KMAP_KEYS) {
					OutputMsg(0, U"%s: bind: unknown key '%s'", rc_name, *args);
					break;
				}
				n += 256;
			} else if (*argl != 1) {
				OutputMsg(0, U"%s: bind: character, ^x, or (octal) \\032 expected.", rc_name);
				break;
			} else
				n = (unsigned char)args[0][0];

			if (args[1]) {
				if ((i = FindCommnr(args[1])) == RC_ILLEGAL) {
					OutputMsg(0, U"%s: bind: unknown command '%s'", rc_name, args[1]);
					break;
				}
				if (CheckArgNum(i, args + 2) < 0)
					break;
				ClearAction(&ktabp[n]);
				SaveAction(ktabp + n, i, args + 2, argl + 2);
			} else
				ClearAction(&ktabp[n]);
		}
		break;
	case RC_BINDKEY:
		{
			struct action *newact;
			int newnr, fl = 0, kf = 0, af = 0, df = 0, mf = 0;
			Display *odisp = display;
			int used = 0;
			struct kmap_ext *kme = NULL;

			for (; *args && **args == '-'; args++, argl++) {
				if (u32_strcmp(*args, U"-t") == 0)
					fl = KMAP_NOTIMEOUT;
				else if (u32_strcmp(*args, U"-k") == 0)
					kf = 1;
				else if (u32_strcmp(*args, U"-a") == 0)
					af = 1;
				else if (u32_strcmp(*args, U"-d") == 0)
					df = 1;
				else if (u32_strcmp(*args, U"-m") == 0)
					mf = 1;
				else if (u32_strcmp(*args, U"--") == 0) {
					args++;
					argl++;
					break;
				} else {
					OutputMsg(0, U"%s: bindkey: invalid option %s", rc_name, *args);
					return;
				}
			}
			if (df && mf) {
				OutputMsg(0, U"%s: bindkey: -d does not work with -m", rc_name);
				break;
			}
			if (*args == 0) {
				if (mf)
					display_bindkey("Edit mode", mmtab);
				else if (df)
					display_bindkey("Default", dmtab);
				else
					display_bindkey("User", umtab);
				break;
			}
			if (kf == 0) {
				if (af) {
					OutputMsg(0, U"%s: bindkey: -a only works with -k", rc_name);
					break;
				}
				if (*argl == 0) {
					OutputMsg(0, U"%s: bindkey: empty string makes no sense", rc_name);
					break;
				}
				for (i = 0, kme = kmap_exts; i < kmap_extn; i++, kme++)
					if (kme->str == 0) {
						if (args[1])
							break;
					} else
					    if (*argl == (kme->fl & ~KMAP_NOTIMEOUT)
						&& memcmp(kme->str, *args, *argl) == 0)
						break;
				if (i == kmap_extn) {
					if (!args[1]) {
						OutputMsg(0, U"%s: bindkey: keybinding not found", rc_name);
						break;
					}
					kmap_extn += 8;
					kmap_exts = xrealloc((char *)kmap_exts, kmap_extn * sizeof(*kmap_exts));
					kme = kmap_exts + i;
					memset((char *)kme, 0, 8 * sizeof(*kmap_exts));
					for (; i < kmap_extn; i++, kme++) {
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
			} else {
				for (i = T_CAPS; i < T_OCAPS; i++)
					if (strcmp(term[i].tcname, *args) == 0)
						break;
				if (i == T_OCAPS) {
					OutputMsg(0, U"%s: bindkey: unknown key '%s'", rc_name, *args);
					break;
				}
				if (af && i >= T_CURSOR && i < T_OCAPS)
					i -= T_CURSOR - KMAP_KEYS;
				else
					i -= T_CAPS;
				newact = df ? &dmtab[i] : mf ? &mmtab[i] : &umtab[i];
			}
			if (args[1]) {
				if ((newnr = FindCommnr(args[1])) == RC_ILLEGAL) {
					OutputMsg(0, U"%s: bindkey: unknown command '%s'", rc_name, args[1]);
					break;
				}
				if (CheckArgNum(newnr, args + 2) < 0)
					break;
				ClearAction(newact);
				SaveAction(newact, newnr, args + 2, argl + 2);
				if (kf == 0 && args[1]) {
					if (kme->str)
						free(kme->str);
					kme->str = SaveStrn(*args, *argl);
					kme->fl = fl | *argl;
				}
			} else
				ClearAction(newact);
			for (display = displays; display; display = display->d_next)
				remap(i, args[1] ? 1 : 0);
			if (kf == 0 && !args[1]) {
				if (!used && kme->str) {
					free(kme->str);
					kme->str = 0;
					kme->fl = 0;
				}
			}
			display = odisp;
		}
		break;
	case RC_MAPTIMEOUT:
		if (*args) {
			if (ParseNum(act, &n))
				break;
			if (n < 0) {
				OutputMsg(0, U"%s: maptimeout: illegal time %d", rc_name, n);
				break;
			}
			maptimeout = n;
		}
		if (*args == 0 || msgok)
			OutputMsg(0, U"maptimeout is %dms", maptimeout);
		break;
	case RC_MAPNOTNEXT:
		D_dontmap = 1;
		break;
	case RC_MAPDEFAULT:
		D_mapdefault = 1;
		break;
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
			OutputMsg(0, U"%s removed from acl database", args[0]);
		break;
	case RC_ACLGRP:
		/*
		 * modify a user to gain or lose rights granted to a group.
		 * This group is actually a normal user whose rights were defined
		 * with chacl in the usual way.
		 */
		if (args[1]) {
			if (u32_strcmp(args[1], U"none")) {	/* link a user to another user */
				if (AclLinkUser(args[0], args[1]))
					break;
				if (msgok)
					OutputMsg(0, U"User %s joined acl-group %s", args[0], args[1]);
			} else {	/* remove all groups from user */

				struct acluser *u;
				struct aclusergroup *g;

				if (!(u = *FindUserPtr(args[0])))
					break;
				while ((g = u->u_group)) {
					u->u_group = g->next;
					free((char *)g);
				}
			}
		} else {	/* show all groups of user */

			char buf[256], *p = buf;
			int ngroups = 0;
			struct acluser *u;
			struct aclusergroup *g;

			if (!(u = *FindUserPtr(args[0]))) {
				if (msgok)
					OutputMsg(0, U"User %s does not exist.", args[0]);
				break;
			}
			g = u->u_group;
			while (g) {
				ngroups++;
				sprintf(p, "%s ", g->u->u_name);
				p += strlen(p);
				if (p > buf + 200)
					break;
				g = g->next;
			}
			if (ngroups)
				*(--p) = '\0';
			OutputMsg(0, U"%s's group%s: %s.", args[0], (ngroups == 1) ? "" : "s",
				  (ngroups == 0) ? "none" : buf);
		}
		break;
	case RC_ACLUMASK:
	case RC_UMASK:
		while ((s = *args++)) {
			char *err = 0;

			if (AclUmask(display ? D_user : users, s, &err))
				OutputMsg(0, U"umask: %s\n", err);
		}
		break;
	case RC_MULTIUSER:
		if (ParseOnOff(act, &b))
			break;
		multi = b ? "" : 0;
		chsock();
		if (msgok)
			OutputMsg(0, U"Multiuser mode %s", multi ? "enabled" : "disabled");
		break;
	case RC_EXEC:
		winexec(args);
		break;
	case RC_NONBLOCK:
		j = D_nonblock >= 0;
		if (*args && ((args[0][0] >= '0' && args[0][0] <= '9') || args[0][0] == '.')) {
			if (ParseNum1000(act, &j))
				break;
		} else if (!ParseSwitch(act, &b))
			j = b == 0 ? -1 : 1000;
		else
			break;
		if (msgok && j == -1)
			OutputMsg(0, U"display set to blocking mode");
		else if (msgok && j == 0)
			OutputMsg(0, U"display set to nonblocking mode, no timeout");
		else if (msgok)
			OutputMsg(0, U"display set to nonblocking mode, %.10gs timeout", j / 1000.);
		D_nonblock = j;
		if (D_nonblock <= 0)
			evdeq(&D_blockedev);
		break;
	case RC_DEFNONBLOCK:
		if (*args && ((args[0][0] >= '0' && args[0][0] <= '9') || args[0][0] == '.')) {
			if (ParseNum1000(act, &defnonblock))
				break;
		} else if (!ParseOnOff(act, &b))
			defnonblock = b == 0 ? -1 : 1000;
		else
			break;
		if (display && *rc_name) {
			D_nonblock = defnonblock;
			if (D_nonblock <= 0)
				evdeq(&D_blockedev);
		}
		break;
	case RC_GR:
		if (ParseSwitch(act, &b) == 0 && msgok) {
			fore->w_gr = b ? 1 : 0;
			OutputMsg(0, U"Will %suse GR", fore->w_gr ? "" : "not ");
		}
		break;
	case RC_C1:
		if (ParseSwitch(act, &fore->w_c1) == 0 && msgok)
			OutputMsg(0, U"Will %suse C1", fore->w_c1 ? "" : "not ");
		break;
	case RC_PRINTCMD:
		if (*args) {
			if (printcmd)
				free(printcmd);
			printcmd = 0;
			if (**args)
				printcmd = SaveStr(*args);
		}
		if (*args == 0 || msgok) {
			if (printcmd)
				OutputMsg(0, U"using '%s' as print command", printcmd);
			else
				OutputMsg(0, U"using termcap entries for printing");
			break;
		}
		break;

	case RC_DIGRAPH:
		if (argl && argl[0] > 0 && args[1] && argl[1] > 0) {
			if (argl[0] != 2) {
				OutputMsg(0, U"Two characters expected to define a digraph");
				break;
			}
			i = digraph_find(args[0]);
			digraphs[i].d[0] = args[0][0];
			digraphs[i].d[1] = args[0][1];
			if (!parse_input_int(args[1], argl[1], &digraphs[i].value)) {
				if (!(digraphs[i].value = u32_atoi(args[1]))) {
					if (!args[1][1])
						digraphs[i].value = (int)args[1][0];
				}
			}
			break;
		}
		Input(U"Enter digraph: ", 10, INP_EVERY, digraph_fn, NULL, 0);
		if (*args && **args) {
			s = *args;
			len = u32_strlen(s);
			LayProcess(&s, &len);
		}
		break;

	case RC_DEFHSTATUS:
		if (*args == 0) {
			char buf[256];
			*buf = 0;
			if (nwin_default.hstatus)
				AddXChars(buf, sizeof(buf), nwin_default.hstatus);
			OutputMsg(0, U"default hstatus is '%s'", buf);
			break;
		}
		(void)ParseSaveStr(act, &nwin_default.hstatus);
		if (*nwin_default.hstatus == 0) {
			free(nwin_default.hstatus);
			nwin_default.hstatus = 0;
		}
		break;
	case RC_HSTATUS:
		(void)ParseSaveStr(act, &fore->w_hstatus);
		if (*fore->w_hstatus == 0) {
			free(fore->w_hstatus);
			fore->w_hstatus = 0;
		}
		WindowChanged(fore, WINESC_HSTATUS);
		break;
	case RC_RENDITION:
		i = -1;
		if (u32_strcmp(args[0], U"bell") == 0) {
			i = REND_BELL;
		} else if (u32_strcmp(args[0], U"monitor") == 0) {
			i = REND_MONITOR;
		} else if (u32_strcmp(args[0], U"silence") == 0) {
			i = REND_SILENCE;
		} else if (u32_strcmp(args[0], U"so") != 0) {
			OutputMsg(0, U"Invalid option '%s' for rendition", args[0]);
			break;
		}

		++args;
		++argl;

		if (i != -1) {
			renditions[i] = ParseAttrColor(args[0], 1);
			WindowChanged((Window *)0, WINESC_WIN_NAMES);
			WindowChanged((Window *)0, WINESC_WIN_NAMES_NOCUR);
			WindowChanged((Window *)0, 0);
			break;
		}

		/* We are here, means we want to set the sorendition. */
		/* FALLTHROUGH */
	case RC_SORENDITION:
		if (args[0]) {
			i = ParseAttrColor(args[0], 1);
			if (i == 0)
				break;
			ApplyAttrColor(i, &mchar_so);
			WindowChanged((Window *)0, 0);
		}
		if (msgok)
			OutputMsg(0, U"Standout attributes 0x%02x  colorbg 0x%02x  colorfg 0x%02x", (unsigned char)mchar_so.attr,
				  (unsigned char)mchar_so.colorbg, (unsigned char)mchar_so.colorfg);
		break;

	case RC_SOURCE:
		do_source(*args);
		break;

	case RC_SU:
		s = NULL;
		if (!*args) //{
			OutputMsg(0, U"%s:%s screen login", HostName, SocketPath);
			/*InputSu(&D_user, NULL);
		} else if (!args[1])
			InputSu(&D_user, args[0]);
		else if (!args[2])
			s = DoSu(&D_user, args[0], args[1], "\377");
		else
			s = DoSu(&D_user, args[0], args[1], args[2]);*/
		if (s)
			OutputMsg(0, U"%s", s);
		break;
	case RC_SPLIT:
		s = args[0];
		if (s && !u32_strcmp(s, U"-v"))
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
		ResizeLayer(D_forecv->c_layer, D_forecv->c_xe - D_forecv->c_xs + 1, D_forecv->c_ye - D_forecv->c_ys + 1,
			    0);
		flayer = D_forecv->c_layer;
		LaySetCursor();
		break;
	case RC_FOCUS:
		{
			Canvas *cv = 0;
			if (!*args || !u32_strcmp(*args, U"next"))
				cv = D_forecv->c_next ? D_forecv->c_next : D_cvlist;
			else if (!u32_strcmp(*args, U"prev")) {
				for (cv = D_cvlist; cv->c_next && cv->c_next != D_forecv; cv = cv->c_next) ;
			} else if (!u32_strcmp(*args, U"top"))
				cv = D_cvlist;
			else if (!u32_strcmp(*args, U"bottom")) {
				for (cv = D_cvlist; cv->c_next; cv = cv->c_next) ;
			} else if (!u32_strcmp(*args, U"up"))
				cv = FindCanvas(D_forecv->c_xs, D_forecv->c_ys - 1);
			else if (!u32_strcmp(*args, U"down"))
				cv = FindCanvas(D_forecv->c_xs, D_forecv->c_ye + 2);
			else if (!u32_strcmp(*args, U"left"))
				cv = FindCanvas(D_forecv->c_xs - 1, D_forecv->c_ys);
			else if (!u32_strcmp(*args, U"right"))
				cv = FindCanvas(D_forecv->c_xe + 1, D_forecv->c_ys);
			else {
				OutputMsg(0, U"%s: usage: focus [next|prev|up|down|left|right|top|bottom]", rc_name);
				break;
			}
			SetForeCanvas(display, cv);
		}
		break;
	case RC_RESIZE:
		i = 0;
		if (D_forecv->c_slorient == SLICE_UNKN) {
			OutputMsg(0, U"resize: need more than one region");
			break;
		}
		for (; *args; args++) {
			if (!u32_strcmp(*args, U"-h"))
				i |= RESIZE_FLAG_H;
			else if (!u32_strcmp(*args, U"-v"))
				i |= RESIZE_FLAG_V;
			else if (!u32_strcmp(*args, U"-b"))
				i |= RESIZE_FLAG_H | RESIZE_FLAG_V;
			else if (!u32_strcmp(*args, U"-p"))
				i |= D_forecv->c_slorient == SLICE_VERT ? RESIZE_FLAG_H : RESIZE_FLAG_V;
			else if (!u32_strcmp(*args, U"-l"))
				i |= RESIZE_FLAG_L;
			else
				break;
		}
		if (*args && args[1]) {
			OutputMsg(0, U"%s: usage: resize [-h] [-v] [-l] [num]\n", rc_name);
			break;
		}
		if (*args)
			ResizeRegions(*args, i);
		else
			Input(resizeprompts[i], 20, INP_EVERY, ResizeFin, (char *)0, i);
		break;
	case RC_SETSID:
		(void)ParseSwitch(act, &separate_sids);
		break;
	case RC_EVAL:
		args = SaveArgs(args);
		for (i = 0; args[i]; i++) {
			if (args[i][0])
				ColonFin(args[i], u32_strlen(args[i]), (char *)0);
			free(args[i]);
		}
		free(args);
		break;
	case RC_ALTSCREEN:
		(void)ParseSwitch(act, &use_altscreen);
		if (msgok)
			OutputMsg(0, U"Will %sdo alternate screen switching", use_altscreen ? "" : "not ");
		break;
	case RC_MAXWIN:
		if (!args[0]) {
			OutputMsg(0, U"maximum windows allowed: %d", maxwin);
			break;
		}
		if (ParseNum(act, &n))
			break;
		if (n < 1)
			OutputMsg(0, U"illegal maxwin number specified");
		else if (n > 2048)
			OutputMsg(0, U"maximum 2048 windows allowed");
		else if (n > maxwin && windows)
			OutputMsg(0, U"may increase maxwin only when there's no window");
		else {
			if (!windows) {
				wtab = realloc(wtab, n * sizeof(Window *));
				memset(wtab, 0, n * sizeof(Window *));
			}
			maxwin = n;
		}
		break;
	case RC_BACKTICK:
		if (ParseBase(act, *args, &n, 10, "decimal"))
			break;
		if (!args[1])
			setbacktick(n, 0, 0, (char **)0);
		else {
			int lifespan, tick;
			if (argc < 4) {
				OutputMsg(0, U"%s: usage: backtick num [lifespan tick cmd args...]", rc_name);
				break;
			}
			if (ParseBase(act, args[1], &lifespan, 10, "decimal"))
				break;
			if (ParseBase(act, args[2], &tick, 10, "decimal"))
				break;
			setbacktick(n, lifespan, tick, SaveArgs(args + 3));
		}
		WindowChanged(0, WINESC_BACKTICK);
		break;
	case RC_BLANKER:
		if (blankerprg) {
			RunBlanker(blankerprg);
			break;
		}
		ClearAll();
		CursorVisibility(-1);
		D_blocked = 4;
		break;
	case RC_BLANKERPRG:
		if (!args[0]) {
			if (blankerprg) {
				char path[MAXPATHLEN];
				char *p = path, **pp;
				for (pp = blankerprg; *pp; pp++)
					p += snprintf(p, sizeof(path) - (p - path) - 1, "%s ", *pp);
				*(p - 1) = '\0';
				OutputMsg(0, U"blankerprg: %s", path);
			} else
				OutputMsg(0, U"No blankerprg set.");
			break;
		}
		if (blankerprg) {
			char **pp;
			for (pp = blankerprg; *pp; pp++)
				free(*pp);
			free(blankerprg);
			blankerprg = 0;
		}
		if (args[0][0])
			blankerprg = SaveArgs(args);
		break;
	case RC_IDLE:
		if (*args) {
			Display *olddisplay = display;
			if (!u32_strcmp(*args, U"off"))
				idletimo = 0;
			else if (args[0][0])
				idletimo = u32_atoi(*args) * 1000;
			if (argc > 1) {
				if ((i = FindCommnr(args[1])) == RC_ILLEGAL) {
					OutputMsg(0, U"%s: idle: unknown command '%s'", rc_name, args[1]);
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
		if (msgok) {
			if (idletimo)
				OutputMsg(0, U"idle timeout %ds, %s", idletimo / 1000, comms[idleaction.nr].name);
			else
				OutputMsg(0, U"idle off");
		}
		break;
	case RC_FOCUSMINSIZE:
		for (i = 0; i < 2 && args[i]; i++) {
			if (!u32_strcmp(args[i], U"max") || !u32_strcmp(args[i], U"_"))
				n = -1;
			else
				n = u32_atoi(args[i]);
			if (i == 0)
				focusminwidth = n;
			else
				focusminheight = n;
		}
		if (msgok) {
			char b[2][20];
			for (i = 0; i < 2; i++) {
				n = i == 0 ? focusminwidth : focusminheight;
				if (n == -1)
					strncpy(b[i], "max", 20);
				else
					sprintf(b[i], "%d", n);
			}
			OutputMsg(0, U"focus min size is %s %s\n", b[0], b[1]);
		}
		break;
	case RC_GROUP:
		if (*args) {
			fore->w_group = 0;
			if (args[0][0]) {
				fore->w_group = WindowByName(*args);
				if (fore->w_group == fore || (fore->w_group && fore->w_group->w_type != W_TYPE_GROUP))
					fore->w_group = 0;
			}
			WindowChanged((Window *)0, WINESC_WIN_NAMES);
			WindowChanged((Window *)0, WINESC_WIN_NAMES_NOCUR);
			WindowChanged((Window *)0, 0);
		}
		if (msgok) {
			if (fore->w_group)
				OutputMsg(0, U"window group is %d (%s)\n", fore->w_group->w_number,
					  fore->w_group->w_title);
			else
				OutputMsg(0, U"window belongs to no group");
		}
		break;
	case RC_LAYOUT:
		// A number of the subcommands for "layout" are ignored, or not processed correctly when there
		// is no attached display.

		if (!u32_strcmp(args[0], U"title")) {
			if (!display) {
				if (!args[1])	// There is no display, and there is no new title. Ignore.
					break;
				if (!layout_attach || layout_attach == &layout_last_marker)
					layout_attach = CreateLayout(args[1], 0);
				else
					RenameLayout(layout_attach, args[1]);
				break;
			}

			if (!D_layout) {
				OutputMsg(0, U"not on a layout");
				break;
			}
			if (!args[1]) {
				OutputMsg(0, U"current layout is %d (%s)", D_layout->lay_number, D_layout->lay_title);
				break;
			}
			RenameLayout(D_layout, args[1]);
		} else if (!u32_strcmp(args[0], U"number")) {
			if (!display) {
				if (args[1] && layout_attach && layout_attach != &layout_last_marker)
					RenumberLayout(layout_attach, u32_atoi(args[1]));
				break;
			}

			if (!D_layout) {
				OutputMsg(0, U"not on a layout");
				break;
			}
			if (!args[1]) {
				OutputMsg(0, U"This is layout %d (%s).\n", D_layout->lay_number, D_layout->lay_title);
				break;
			}
			RenumberLayout(D_layout, u32_atoi(args[1]));
			break;
		} else if (!u32_strcmp(args[0], U"autosave")) {
			if (!display) {
				if (args[1] && layout_attach && layout_attach != &layout_last_marker) {
					if (!u32_strcmp(args[1], U"on"))
						layout_attach->lay_autosave = 1;
					else if (!u32_strcmp(args[1], U"off"))
						layout_attach->lay_autosave = 0;
				}
				break;
			}

			if (!D_layout) {
				OutputMsg(0, U"not on a layout");
				break;
			}
			if (args[1]) {
				if (!u32_strcmp(args[1], U"on"))
					D_layout->lay_autosave = 1;
				else if (!u32_strcmp(args[1], U"off"))
					D_layout->lay_autosave = 0;
				else {
					OutputMsg(0, U"invalid argument. Give 'on' or 'off");
					break;
				}
			}
			if (msgok)
				OutputMsg(0, U"autosave is %s", D_layout->lay_autosave ? "on" : "off");
		} else if (!u32_strcmp(args[0], U"new")) {
			uint32_t *t = args[1];
			n = 0;
			if (t) {
				while (*t >= '0' && *t <= '9')
					t++;
				if (t != args[1] && (!*t || *t == ':')) {
					n = u32_atoi(args[1]);
					if (*t)
						t++;
				} else
					t = args[1];
			}
			if (!t || !*t)
				t = "layout";
			NewLayout(t, n);
			Activate(-1);
		} else if (!u32_strcmp(args[0], U"save")) {
			if (!args[1]) {
				OutputMsg(0, U"usage: layout save <name>");
				break;
			}
			if (display)
				SaveLayout(args[1], &D_canvas);
		} else if (!u32_strcmp(args[0], U"select")) {
			if (!display) {
				if (args[1])
					layout_attach = FindLayout(args[1]);
				break;
			}
			if (!args[1]) {
				Input(U"Switch to layout: ", 20, INP_COOKED, SelectLayoutFin, NULL, 0);
				break;
			}
			SelectLayoutFin(args[1], u32_strlen(args[1]), (char *)0);
		} else if (!u32_strcmp(args[0], U"next")) {
			if (!display) {
				if (layout_attach && layout_attach != &layout_last_marker)
					layout_attach = layout_attach->lay_next ? layout_attach->lay_next : layouts;;
				break;
			}
			Layout *lay = D_layout;
			if (lay)
				lay = lay->lay_next ? lay->lay_next : layouts;
			else
				lay = layouts;
			if (!lay) {
				OutputMsg(0, U"no layout defined");
				break;
			}
			if (lay == D_layout)
				break;
			LoadLayout(lay);
			Activate(-1);
		} else if (!u32_strcmp(args[0], U"prev")) {
			Layout *lay = display ? D_layout : layout_attach;
			Layout *target = lay;
			if (lay) {
				for (lay = layouts; lay->lay_next && lay->lay_next != target; lay = lay->lay_next) ;
			} else
				lay = layouts;

			if (!display) {
				layout_attach = lay;
				break;
			}

			if (!lay) {
				OutputMsg(0, U"no layout defined");
				break;
			}
			if (lay == D_layout)
				break;
			LoadLayout(lay);
			Activate(-1);
		} else if (!u32_strcmp(args[0], U"attach")) {
			if (!args[1]) {
				if (!layout_attach)
					OutputMsg(0, U"no attach layout set");
				else if (layout_attach == &layout_last_marker)
					OutputMsg(0, U"will attach to last layout");
				else
					OutputMsg(0, U"will attach to layout %d (%s)", layout_attach->lay_number,
						  layout_attach->lay_title);
				break;
			}
			if (!u32_strcmp(args[1], U":last"))
				layout_attach = &layout_last_marker;
			else if (!args[1][0])
				layout_attach = 0;
			else {
				Layout *lay;
				lay = FindLayout(args[1]);
				if (!lay) {
					OutputMsg(0, U"unknown layout '%s'", args[1]);
					break;
				}
				layout_attach = lay;
			}
		} else if (!u32_strcmp(args[0], U"show")) {
			ShowLayouts(-1);
		} else if (!u32_strcmp(args[0], U"remove")) {
			Layout *lay = display ? D_layout : layouts;
			if (args[1]) {
				lay = layouts ? FindLayout(args[1]) : (Layout *)0;
				if (!lay) {
					OutputMsg(0, U"unknown layout '%s'", args[1]);
					break;
				}
			}
			if (lay)
				RemoveLayout(lay);
		} else if (!u32_strcmp(args[0], U"dump")) {
			if (!display)
				OutputMsg(0, U"Must have a display for 'layout dump'.");
			else if (!LayoutDumpCanvas(&D_canvas, args[1] ? args[1] : "layout-dump"))
				OutputMsg(errno, U"Error dumping layout.");
			else
				OutputMsg(0, U"Layout dumped to \"%s\"", args[1] ? args[1] : "layout-dump");
		} else
			OutputMsg(0, U"unknown layout subcommand");
		break;
	case RC_TRUECOLOR:
		ParseOnOff(act, &hastruecolor);
		Activate(-1); /* redisplay (check RC_REDISPLAY) */
		break;
	default:
		break;
	}
	if (display != odisplay) {
		for (display = displays; display; display = display->d_next)
			if (display == odisplay)
				break;
	}
}

#undef OutputMsg

void CollapseWindowlist()
/* renumber windows from 0, leaving no gaps */
{
	int pos, moveto = 0;

	for (pos = 1; pos < MAXWIN; pos++)
		if (wtab[pos])
			for (; moveto < pos; moveto++)
				if (!wtab[moveto]) {
					SwapWindows(pos, moveto);
					break;
				}
}

void DoCommand(uint32_t **argv, int *argl)
{
	struct action act;
	const uint32_t *cmd = *argv;

	act.quiet = 0;
	/* For now, we actually treat both 'supress error' and 'suppress normal message' as the
	 * same, and ignore all messages on either flag. If we wanted to do otherwise, we would
	 * need to change the definition of 'OutputMsg' slightly. */
	if (*cmd == '@') {	/* Suppress error */
		act.quiet |= 0x01;
		cmd++;
	}
	if (*cmd == '-') {	/* Suppress normal message */
		act.quiet |= 0x02;
		cmd++;
	}

	if ((act.nr = FindCommnr(cmd)) == RC_ILLEGAL) {
		Msg(0, U"%s: unknown command '%s'", rc_name, cmd);
		return;
	}
	act.args = argv + 1;
	act.argl = argl + 1;
	DoAction(&act, -1);
}

static void SaveAction(struct action *act, int nr, uint32_t **args, int *argl)
{
	int argc = 0;
	uint32_t **pp;
	int *lp;

	if (args)
		while (args[argc])
			argc++;
	if (argc == 0) {
		act->nr = nr;
		act->args = noargs;
		act->argl = 0;
		return;
	}
	if ((pp = malloc((unsigned)(argc + 1) * sizeof(uint32_t *))) == 0)
		Panic(0, U"%s", strnomem);
	if ((lp = malloc((unsigned)(argc) * sizeof(int))) == 0)
		Panic(0, U"%s", strnomem);
	act->nr = nr;
	act->args = pp;
	act->argl = lp;
	while (argc--) {
		*lp = argl ? *argl++ : (int)u32_strlen(*args);
		*pp++ = SaveStrn(*args++, *lp++);
	}
	*pp = 0;
}

static uint32_t **SaveArgs(uint32_t **args)
{
	uint32_t **ap, **pp;
	int argc = 0;

	while (args[argc])
		argc++;
	if ((pp = ap = malloc((unsigned)(argc + 1) * sizeof(uint32_t *))) == 0)
		Panic(0, U"%s", strnomem);
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
int Parse(uint32_t *buf, int bufl, uint32_t **args, int *argl)
{
	uint32_t *p = buf, **ap = args, *pp;
	uint32_t delim;
	int argc;
	int *lp = argl;

	argc = 0;
	pp = buf;
	delim = 0;
	for (;;) {
		*lp = 0;
		while (*p && (*p == ' ' || *p == '\t'))
			++p;
		if (argc == 0 && *p == '!') {
			*ap++ = U"exec";
			*lp++ = 4;
			p++;
			argc++;
			continue;
		}
		if (*p == '\0' || *p == '#' || *p == '\n') {
			*p = '\0';
			args[argc] = 0;
			return argc;
		}
		if (++argc >= MAXARGS) {
			Msg(0, U"%s: too many tokens.", rc_name);
			return 0;
		}
		*ap++ = pp;

		while (*p) {
			if (*p == delim)
				delim = 0;
			else if (delim != '\'' && *p == '\\'
				 && (p[1] == 'n' || p[1] == 'r' || p[1] == 't' || p[1] == '\'' || p[1] == '"'
				     || p[1] == '\\' || p[1] == '$' || p[1] == '#' || p[1] == '^' || (p[1] >= '0'
												      && p[1] <=
												      '7'))) {
				p++;
				if (*p >= '0' && *p <= '7') {
					*pp = *p - '0';
					if (p[1] >= '0' && p[1] <= '7') {
						p++;
						*pp = (*pp << 3) | (*p - '0');
						if (p[1] >= '0' && p[1] <= '7') {
							p++;
							*pp = (*pp << 3) | (*p - '0');
						}
					}
					pp++;
				} else {
					switch (*p) {
					case 'n':
						*pp = '\n';
						break;
					case 'r':
						*pp = '\r';
						break;
					case 't':
						*pp = '\t';
						break;
					default:
						*pp = *p;
						break;
					}
					pp++;
				}
			} else if (delim != '\'' && *p == '$'
				   && (p[1] == '{' || p[1] == ':' || (p[1] >= 'a' && p[1] <= 'z')
				       || (p[1] >= 'A' && p[1] <= 'Z') || (p[1] >= '0' && p[1] <= '9') || p[1] == '_'))
			{
				uint32_t *ps, *pe, op, *v, xbuf[11], path[MAXPATHLEN];
				int vl;

				ps = ++p;
				p++;
				while (*p) {
					if (*ps == '{' && *p == '}')
						break;
					if (*ps == ':' && *p == ':')
						break;
					if (*ps != '{' && *ps != ':' && (*p < 'a' || *p > 'z') && (*p < 'A' || *p > 'Z')
					    && (*p < '0' || *p > '9') && *p != '_')
						break;
					p++;
				}
				pe = p;
				if (*ps == '{' || *ps == ':') {
					if (!*p) {
						Msg(0, U"%s: bad variable name.", rc_name);
						return 0;
					}
					p++;
				}
				op = *pe;
				*pe = 0;
				if (*ps == ':')
					v = gettermcapstring(ps + 1);
				else {
					if (*ps == '{')
						ps++;
					v = xbuf;
					if (!u32_strcmp(ps, U"TERM"))
						v = display ? D_termname : U"unknown";
					else if (!u32_strcmp(ps, U"COLUMNS"))
						u32_sprintf(xbuf, "%d", display ? D_width : -1);
					else if (!u32_strcmp(ps, U"LINES"))
						u32_sprintf(xbuf, "%d", display ? D_height : -1);
					else if (!u32_strcmp(ps, U"PID"))
						u32_sprintf(xbuf, "%d", getpid());
					else if (!u32_strcmp(ps, U"PWD")) {
						if (getcwd(path, sizeof(path) - 1) == 0)
							v = U"?";
						else
							v = path;
					} else if (!u32_strcmp(ps, U"STY")) {
						if ((v = u32_strchr(SocketName, '.')))	/* Skip the PID */
							v++;
						else
							v = SocketName;
					} else
						v = getenv(ps);
				}
				*pe = op;
				vl = v ? u32_strlen(v) : 0;
				if (vl) {
					if (p - pp < vl) {
						ptrdiff_t right = buf + bufl - (p + u32_strlen(p) + 1);
						if (right > 0) {
							u32_move(p + right, p, u32_strlen(p) + 1);
							p += right;
						}
					}
					if (p - pp < vl) {
						Msg(0, U"%s: no space left for variable expansion.", rc_name);
						return 0;
					}
					u32_move(pp, v, vl);
					pp += vl;
				}
				continue;
			} else if (delim != '\'' && *p == '^' && p[1]) {
				p++;
				*pp++ = *p == '?' ? '\177' : *p & 0x1f;
			} else if (delim == 0 && (*p == '\'' || *p == '"'))
				delim = *p;
			else if (delim == 0 && (*p == ' ' || *p == '\t' || *p == '\n'))
				break;
			else
				*pp++ = *p;
			p++;
		}
		if (delim) {
			Msg(0, U"%s: Missing %c quote.", rc_name, delim);
			return 0;
		}
		if (*p)
			p++;
		*pp = 0;
		*lp++ = pp - ap[-1];
		pp++;
	}
}

void SetEscape(struct acluser *u, int e, int me)
{
	if (u) {
		u->u_Esc = e;
		u->u_MetaEsc = me;
	} else {
		if (users) {
			if (DefaultEsc >= 0)
				ClearAction(&ktab[DefaultEsc]);
			if (DefaultMetaEsc >= 0)
				ClearAction(&ktab[DefaultMetaEsc]);
		}
		DefaultEsc = e;
		DefaultMetaEsc = me;
		if (users) {
			if (DefaultEsc >= 0) {
				ClearAction(&ktab[DefaultEsc]);
				ktab[DefaultEsc].nr = RC_OTHER;
			}
			if (DefaultMetaEsc >= 0) {
				ClearAction(&ktab[DefaultMetaEsc]);
				ktab[DefaultMetaEsc].nr = RC_META;
			}
		}
	}
}

static int ParseSwitch(struct action *act, bool *var)
{
	if (*act->args == 0) {
		*var ^= true;
		return 0;
	}
	return ParseOnOff(act, var);
}

static int ParseOnOff(struct action *act, bool *var)
{
	int num = -1;
	uint32_t **args = act->args;

	if (args[1] == 0) {
		if (u32_strcmp(args[0], U"on") == 0)
			num = true;
		else if (u32_strcmp(args[0], U"off") == 0)
			num = false;
	}
	if (num < 0) {
		Msg(0, U"%s: %s: invalid argument. Give 'on' or 'off'", rc_name, comms[act->nr].name);
		return -1;
	}
	*var = num;
	return 0;
}

static int ParseSaveStr(struct action *act, uint32_t **var)
{
	uint32_t **args = act->args;
	if (*args == 0 || args[1]) {
		Msg(0, U"%s: %s: one argument required.", rc_name, comms[act->nr].name);
		return -1;
	}
	if (*var)
		free(*var);
	*var = SaveStr(*args);
	return 0;
}

static int ParseNum(struct action *act, int *var)
{
	int i;
	uint32_t *p, **args = act->args;

	p = *args;
	if (p == 0 || *p == 0 || args[1]) {
		Msg(0, U"%s: %s: invalid argument. Give one argument.", rc_name, comms[act->nr].name);
		return -1;
	}
	i = 0;
	while (*p) {
		if (*p >= '0' && *p <= '9')
			i = 10 * i + (*p - '0');
		else {
			Msg(0, U"%s: %s: invalid argument. Give numeric argument.", rc_name, comms[act->nr].name);
			return -1;
		}
		p++;
	}
	*var = i;
	return 0;
}

static int ParseNum1000(struct action *act, int *var)
{
	int i;
	uint32_t *p, **args = act->args;
	int dig = 0;

	p = *args;
	if (p == 0 || *p == 0 || args[1]) {
		Msg(0, U"%s: %s: invalid argument. Give one argument.", rc_name, comms[act->nr].name);
		return -1;
	}
	i = 0;
	while (*p) {
		if (*p >= '0' && *p <= '9') {
			if (dig < 4)
				i = 10 * i + (*p - '0');
			else if (dig == 4 && *p >= '5')
				i++;
			if (dig)
				dig++;
		} else if (*p == '.' && !dig)
			dig++;
		else {
			Msg(0, U"%s: %s: invalid argument. Give floating point argument.", rc_name, comms[act->nr].name);
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
	*var = i;
	return 0;
}

static Window *WindowByName(uint32_t *s)
{
	Window *window;

	for (window = windows; window; window = window->w_next)
		if (!u32_strcmp(window->w_title, s))
			return window;
	for (window = windows; window; window = window->w_next)
		if (!u32_strncmp(window->w_title, s, u32_strlen(s)))
			return window;
	return 0;
}

static int WindowByNumber(uint32_t *string)
{
	int i;
	uint32_t *s;

	for (i = 0, s = string; *s; s++) {
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
int WindowByNoN(uint32_t *string)
{
	int i;
	Window *window;

	if ((i = WindowByNumber(string)) < 0 || i >= maxwin) {
		if ((window = WindowByName(string)))
			return window->w_number;
		return -1;
	}
	return i;
}

static int ParseWinNum(struct action *act, int *var)
{
	char **args = act->args;
	int i = 0;

	if (*args == 0 || args[1]) {
		Msg(0, U"%s: %s: one argument required.", rc_name, comms[act->nr].name);
		return -1;
	}

	i = WindowByNoN(*args);
	if (i < 0) {
		Msg(0, U"%s: %s: invalid argument. Give window number or name.", rc_name, comms[act->nr].name);
		return -1;
	}
	*var = i;
	return 0;
}

static int ParseBase(struct action *act, char *p, int *var, int base, char *bname)
{
	int i = 0;
	int c;

	if (*p == 0) {
		Msg(0, U"%s: %s: empty argument.", rc_name, comms[act->nr].name);
		return -1;
	}
	while ((c = *p++)) {
		if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
		if (c >= 'A' && c <= 'Z')
			c -= 'A' - ('0' + 10);
		c -= '0';
		if (c < 0 || c >= base) {
			Msg(0, U"%s: %s: argument is not %s.", rc_name, comms[act->nr].name, bname);
			return -1;
		}
		i = base * i + c;
	}
	*var = i;
	return 0;
}

static bool IsNum(char *s)
{
	for (; *s; ++s)
		if (*s < '0' || *s > '9')
			return false;
	return true;
}

int IsNumColon(char *s, char *p, int psize)
{
	char *q;
	if ((q = strrchr(s, ':')) != 0) {
		strncpy(p, q + 1, psize - 1);
		p[psize - 1] = '\0';
		*q = '\0';
	} else
		*p = '\0';
	return IsNum(s);
}

void SwitchWindow(int n)
{
	Window *window;

	if (n < 0 || n >= maxwin) {
		ShowWindows(-1);
		return;
	}
	if ((window = wtab[n]) == 0) {
		ShowWindows(n);
		return;
	}
	if (display == 0) {
		fore = window;
		return;
	}
	if (window == D_fore) {
		Msg(0, U"This IS window %d (%s).", n, window->w_title);
		return;
	}
	if (AclCheckPermWin(D_user, ACL_READ, window)) {
		Msg(0, U"Access to window %d denied.", window->w_number);
		return;
	}
	SetForeWindow(window);
	Activate(fore->w_norefresh);
}

/*
 * SetForeWindow changes the window in the input focus of the display.
 * Puts window wi in canvas display->d_forecv.
 */
void SetForeWindow(Window *window)
{
	Window *oldfore;

	if (display == 0) {
		fore = window;
		return;
	}
	oldfore = Layer2Window(D_forecv->c_layer);
	SetCanvasWindow(D_forecv, window);
	if (oldfore)
		WindowChanged(oldfore, 'u');
	if (window)
		WindowChanged(window, 'u');
	flayer = D_forecv->c_layer;
	/* Activate called afterwards, so no RefreshHStatus needed */
}

/*****************************************************************/

/*
 *  Activate - make fore window active
 *  norefresh = -1 forces a refresh, disregard all_norefresh then.
 */
void Activate(int norefresh)
{
	if (display == 0)
		return;
	if (D_status) {
		Msg(0, U"%s", "");	/* wait till mintime (keep gcc quiet) */
		RemoveStatus();
	}

	if (MayResizeLayer(D_forecv->c_layer))
		ResizeLayer(D_forecv->c_layer, D_forecv->c_xe - D_forecv->c_xs + 1, D_forecv->c_ye - D_forecv->c_ys + 1,
			    display);

	fore = D_fore;
	if (fore) {
		/* XXX ? */
		if (fore->w_monitor != MON_OFF)
			fore->w_monitor = MON_ON;
		fore->w_bell = BELL_ON;
		WindowChanged(fore, WINESC_WFLAGS);
	}
	Redisplay(norefresh + all_norefresh);
}

static uint16_t NextWindow()
{
	Window **pp;
	int n = fore ? fore->w_number : maxwin;
	Window *group = fore ? fore->w_group : 0;

	for (pp = fore ? wtab + n + 1 : wtab; pp != wtab + n; pp++) {
		if (pp == wtab + maxwin)
			pp = wtab;
		if (*pp) {
			if (!fore || group == (*pp)->w_group)
				break;
		}
	}
	if (pp == wtab + n)
		return -1;
	return pp - wtab;
}

static uint16_t PreviousWindow()
{
	Window **pp;
	int n = fore ? fore->w_number : 0;
	Window *group = fore ? fore->w_group : 0;

	for (pp = wtab + n - 1; pp != wtab + n; pp--) {
		if (pp == wtab - 1)
			pp = wtab + maxwin - 1;
		if (*pp) {
			if (!fore || group == (*pp)->w_group)
				break;
		}
	}
	if (pp == wtab + n)
		return 0;
	return pp - wtab;
}

static int MoreWindows()
{
	uint32_t *m = U"No other window.";
	if (windows && (fore == 0 || windows->w_next))
		return 1;
	if (fore == 0) {
		Msg(0, U"No window available");
		return 0;
	}
	Msg(0, m, fore->w_number);
	return 0;
}

void KillWindow(Window *window)
{
	Window **pp, *p;
	Canvas *cv;
	int gotone;
	Layout *lay;

	/*
	 * Remove window from linked list.
	 */
	for (pp = &windows; (p = *pp); pp = &p->w_next)
		if (p == window)
			break;
	*pp = p->w_next;
	window->w_inlen = 0;
	wtab[window->w_number] = 0;

	if (windows == 0) {
		FreeWindow(window);
		Finit(0);
	}

	/*
	 * switch to different window on all canvases
	 */
	for (display = displays; display; display = display->d_next) {
		gotone = 0;
		for (cv = D_cvlist; cv; cv = cv->c_next) {
			if (Layer2Window(cv->c_layer) != window)
				continue;
			/* switch to other window */
			SetCanvasWindow(cv, FindNiceWindow(D_other, 0));
			gotone = 1;
		}
		if (gotone) {
			if (window->w_zdisplay == display) {
				D_blocked = 0;
				D_readev.condpos = D_readev.condneg = 0;
			}
			Activate(-1);
		}
	}

	/* do the same for the layouts */
	for (lay = layouts; lay; lay = lay->lay_next)
		UpdateLayoutCanvas(&lay->lay_canvas, window);

	FreeWindow(window);
	WindowChanged((Window *)0, WINESC_WIN_NAMES);
	WindowChanged((Window *)0, WINESC_WIN_NAMES_NOCUR);
	WindowChanged((Window *)0, 0);
}

static void LogToggle(bool on)
{
	char buf[1024];

	if ((fore->w_log != 0) == on) {
		if (display && !*rc_name)
			Msg(0, U"You are %s logging.", on ? "already" : "not");
		return;
	}
	if (fore->w_log != 0) {
		Msg(0, U"Logfile \"%s\" closed.", fore->w_log->name);
		logfclose(fore->w_log);
		fore->w_log = 0;
		WindowChanged(fore, WINESC_WFLAGS);
		return;
	}
	if (DoStartLog(fore, buf, sizeof(buf))) {
		Msg(errno, U"Error opening logfile \"%s\"", buf);
		return;
	}
	if (ftell(fore->w_log->fp) == 0)
		Msg(0, U"Creating logfile \"%s\".", fore->w_log->name);
	else
		Msg(0, U"Appending to logfile \"%s\".", fore->w_log->name);
	WindowChanged(fore, WINESC_WFLAGS);
}

/* TODO: wmb encapsulation; flags enum; update all callers */
char *AddWindows(WinMsgBufContext *wmbc, int len, int flags, int where)
{
	char *s, *ss;
	Window **pp, *p;
	char *cmd;
	char *buf = wmbc->p;
	int l;

	s = ss = buf;
	if ((flags & 8) && where < 0) {
		*s = 0;
		return ss;
	}

	for (pp = ((flags & 4) && where >= 0) ? wtab + where + 1 : wtab; pp < wtab + maxwin; pp++) {
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
		if (s > buf || (flags & 4)) {
			*s++ = ' ';
			*s++ = ' ';
		}
		if (p->w_number == where) {
			ss = s;
			if (flags & 8)
				break;
		}
		if (!(flags & 4) || where < 0 || ((flags & 4) && where < p->w_number)) {
			if (p->w_monitor == MON_DONE && renditions[REND_MONITOR] != 0)
				rend = renditions[REND_MONITOR];
			else if ((p->w_bell == BELL_DONE || p->w_bell == BELL_FOUND) && renditions[REND_BELL] != 0)
				rend = renditions[REND_BELL];
			else if ((p->w_silence == SILENCE_FOUND || p->w_silence == SILENCE_DONE)
				 && renditions[REND_SILENCE] != 0)
				rend = renditions[REND_SILENCE];
		}
		if (rend != -1)
			AddWinMsgRend(wmbc->buf, s, rend);
		sprintf(s, "%d", p->w_number);
		s += strlen(s);
		if (!(flags & 2)) {
			s = AddWindowFlags(s, len, p);
		}
		*s++ = ' ';
		strncpy(s, cmd, l);
		s += l;
		if (rend != -1)
			AddWinMsgRend(wmbc->buf, s, 0);
	}
	*s = 0;
	return ss;
}

char *AddWindowFlags(uint32_t *buf, int len, Window *p)
{
	uint32_t *s = buf;
	if (p == 0 || len < 12) {
		*s = 0;
		return s;
	}
	if (display && p == D_fore)
		*s++ = '*';
	if (display && p == D_other)
		*s++ = '-';
	if (p->w_layer.l_cvlist && p->w_layer.l_cvlist->c_lnext)
		*s++ = '&';
	if (p->w_monitor == MON_DONE && (ACLBYTE(p->w_mon_notify, D_user->u_id) & ACLBIT(D_user->u_id))
	    )
		*s++ = '@';
	if (p->w_bell == BELL_DONE)
		*s++ = '!';
#ifdef ENABLE_UTMP
	if (p->w_slot != (slot_t) 0 && p->w_slot != (slot_t) - 1)
		*s++ = '$';
#endif
	if (p->w_log != 0) {
		u32_strcpy(s, U"(L)");
		s += 3;
	}
	if (p->w_ptyfd < 0 && p->w_type != W_TYPE_GROUP)
		*s++ = 'Z';
	if (p->w_miflag)
		*s++ = '>';
	*s = 0;
	return s;
}

char *AddOtherUsers(char *buf, int len, Window *p)
{
	Display *d, *olddisplay = display;
	Canvas *cv;
	char *s;
	int l;

	s = buf;
	for (display = displays; display; display = display->d_next) {
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
		if (len > 1 && s != buf) {
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


/* Display window list as a message.  WHERE denotes the active window
 * number; if -1, then the active window will be determined using the
 * current foreground window, if available. */
void ShowWindows(int where)
{
	const char *buf, *s, *ss;

	WinMsgBuf *wmb = wmb_create();
	WinMsgBufContext *wmbc = wmbc_create(wmb);
	size_t max = wmbc_bytesleft(wmbc);

	if (display && where == -1 && D_fore)
		where = D_fore->w_number;

	/* TODO: this is a confusing mix of old and new; modernize */
	ss = AddWindows(wmbc, max, 0, where);
	wmbc_fastfw0(wmbc);
	buf = wmbc_finish(wmbc);
	s = buf + strlen(buf);

	if (display && ss - buf > D_width / 2) {
		ss -= D_width / 2;
		if (s - ss < D_width) {
			ss = s - D_width;
			if (ss < buf)
				ss = buf;
		}
	} else
		ss = buf;
	Msg(0, U"%s", ss);

	wmbc_free(wmbc);
	wmb_free(wmb);
}

/*
* String Escape based windows listing
* mls: currently does a Msg() call for each(!) window, dunno why
*/
static void ShowWindowsX(str)
char *str;
{
	int i;
	for (i = 0; i < maxwin; i++) {
		if (!wtab[i])
			continue;
		Msg(0, U"%s", MakeWinMsg(str, wtab[i], '%'));
	}
}

static void ShowInfo()
{
	uint32_t buf[512], *p;
	Window *wp = fore;
	int i;

	if (wp == 0) {
		Msg(0, U"(%d,%d)/(%d,%d) no window", D_x + 1, D_y + 1, D_width, D_height);
		return;
	}
	p = buf;
	if (buf < (p += GetAnsiStatus(wp, p)))
		*p++ = ' ';
	u32_sprintf(p, "(%d,%d)/(%d,%d)", wp->w_x + 1, wp->w_y + 1, wp->w_width, wp->w_height);
	u32_sprintf(p += u32_strlen(p), "+%d", wp->w_histheight);
	u32_sprintf(p += u32_strlen(p), " %c%sflow",
		(wp->w_flow & FLOW_ON) ? '+' : '-',
		(wp->w_flow & FLOW_AUTOFLAG) ? "" : ((wp->w_flow & FLOW_AUTO) ? "(+)" : "(-)"));
	if (!wp->w_wrap)
		u32_sprintf(p += u32_strlen(p), " -wrap");
	if (wp->w_insert)
		u32_sprintf(p += u32_strlen(p), " ins");
	if (wp->w_origin)
		u32_sprintf(p += u32_strlen(p), " org");
	if (wp->w_keypad)
		u32_sprintf(p += u32_strlen(p), " app");
	if (wp->w_log)
		u32_sprintf(p += u32_strlen(p), " log");
	if (wp->w_monitor != MON_OFF && (ACLBYTE(wp->w_mon_notify, D_user->u_id) & ACLBIT(D_user->u_id))
	    )
		u32_sprintf(p += u32_strlen(p), " mon");
	if (wp->w_mouse)
		u32_sprintf(p += u32_strlen(p), " mouse");
	if (!wp->w_c1)
		u32_sprintf(p += u32_strlen(p), " -c1");
	if (wp->w_norefresh)
		u32_sprintf(p += u32_strlen(p), " nored");

	p += u32_strlen(p);

	if (wp->w_type == W_TYPE_PLAIN) {
		/* add info about modem control lines */
		*p++ = ' ';
		TtyGetModemStatus(wp->w_ptyfd, p);
	}
#ifdef ENABLE_TELNET
	else if (wp->w_type == W_TYPE_TELNET) {
		*p++ = ' ';
		TelStatus(wp, p, sizeof(buf) - 1 - (p - buf));
	}
#endif
	Msg(0, U"%s %d(%s)", buf, wp->w_number, wp->w_title);
}

static void ShowDInfo()
{
	char buf[128], *p;
	int l;
	if (display == 0)
		return;
	p = buf;
	l = 512;
	sprintf(p, "(%d,%d)", D_width, D_height), l -= strlen(p);
	p += l;
	if (D_CXT) {
		strncpy(p, " xterm", l);
		l -= strlen(p);
		p += l;
	}
	if (D_hascolor) {
		strncpy(p, " color", l);
		l -= strlen(p);
		p += l;
	}
	if (D_CG0)
		strncpy(p, " iso2022", l);
	else if (D_CS0 && *D_CS0)
		strncpy(p, " altchar", l);
	Msg(0, U"%s", buf);
}

static void AKAFin(uint32_t *buf, size_t len, void *data)
{
	(void)data; /* unused */

	if (len && fore)
		ChangeAKA(fore, buf, u32_strlen(buf));

	enter_window_name_mode = 0;
}

static void InputAKA()
{
	char *s, *ss;
	size_t len;

	if (enter_window_name_mode == 1)
		return;

	enter_window_name_mode = 1;

	Input(U"Set window's title to: ", sizeof(fore->w_akabuf) - 1, INP_COOKED, AKAFin, NULL, 0);
	s = fore->w_title;
	if (!s)
		return;
	for (; *s; s++) {
		if ((*(unsigned char *)s & 0x7f) < 0x20 || *s == 0x7f)
			continue;
		ss = s;
		len = 1;
		LayProcess(&ss, &len);
	}
}

static void ColonFin(uint32_t *buf, size_t len, void *data)
{
	uint32_t mbuf[256];

	(void)data; /* unused */

	RemoveStatus();
	if (buf[len] == '\t') {
		int m, x;
		int l = 0, r = RC_LAST;
		int showmessage = 0;
		uint32_t *s = buf;

		while (*s && (uintptr_t)(s - buf) < len)
			if (*s++ == ' ')
				return;

		/* Showing a message when there's no hardstatus or caption cancels the input */
		if (display &&
		    (captionalways || D_has_hstatus == HSTATUS_LASTLINE
		     || (D_canvas.c_slperp && D_canvas.c_slperp->c_slnext)))
			showmessage = 1;

		while (l <= r) {
			m = (l + r) / 2;
			x = u32_strncmp(buf, comms[m].name, len);
			if (x > 0)
				l = m + 1;
			else if (x < 0)
				r = m - 1;
			else {
				s = mbuf;
				for (l = m - 1; l >= 0 && u32_strncmp(buf, comms[l].name, len) == 0; l--) ;
				for (m = ++l;
				     m <= r && u32_strncmp(buf, comms[m].name, len) == 0 && (uintptr_t)(s - mbuf) < sizeof(mbuf); m++)
					s += u32_snprintf(s, sizeof(mbuf) - (s - mbuf), " %s", comms[m].name);
				if (l < m - 1) {
					if (showmessage)
						Msg(0, U"Possible commands:%s", mbuf);
				} else {
					s = mbuf;
					len = u32_snprintf(mbuf, sizeof(mbuf), "%s \t", comms[l].name + len);
					if (len > 0 && len < sizeof(mbuf))
						LayProcess(&s, &len);
				}
				break;
			}
		}
		if (l > r && showmessage)
			Msg(0, U"No commands matching '%*s'", (int)len, buf);
		return;
	}

	if (!len || buf[len])
		return;

	len = u32_strlen(buf) + 1;
	if (len > (int)sizeof(mbuf))
		RcLine(buf, len);
	else {
		u32_move(mbuf, buf, len);
		RcLine(mbuf, sizeof mbuf);
	}
}

static void SelectFin(uint32_t *buf, size_t len, void *data)
{
	int n;

	(void)data; /* unused */

	if (!len || !display)
		return;
	if (len == 1 && *buf == '-') {
		SetForeWindow((Window *)0);
		Activate(0);
		return;
	}
	if ((n = WindowByNoN(buf)) < 0)
		return;
	SwitchWindow(n);
}

static void SelectLayoutFin(uint32_t *buf, size_t len, void *data)
{
	Layout *lay;

	(void)data; /* unused */

	if (!len || !display)
		return;
	if (len == 1 && *buf == '-') {
		LoadLayout((Layout *)0);
		Activate(0);
		return;
	}
	lay = FindLayout(buf);
	if (!lay)
		Msg(0, U"No such layout\n");
	else if (lay == D_layout)
		Msg(0, U"This IS layout %d (%s).\n", lay->lay_number, lay->lay_title);
	else {
		LoadLayout(lay);
		Activate(0);
	}
}

static void InputSelect()
{
	Input(U"Switch to window: ", 20, INP_COOKED, SelectFin, NULL, 0);
}

static uint32_t setenv_var[31];

static void SetenvFin1(uint32_t *buf, size_t len, void *data)
{
	(void)data; /* unused */

	if (!len || !display)
		return;
	InputSetenv(buf);
}

static void SetenvFin2(uint32_t *buf, size_t len, void *data)
{
	(void)data; /* unused */
	const char *lc = locale_charset();

	char envbuf[1000];  /* FIXME */
	size_t envlen = 1000;

	u32_conv_to_encoding(lc, iconveh_question_mark,
		buf, len, 0, envbuf, &envlen);

	if (!len || !display)
		return;
	setenv(setenv_var, envbuf, 1);
	MakeNewEnv();
}

static void InputSetenv(uint32_t *arg)
{
	static uint32_t setenv_buf[50 + sizeof(setenv_var)];	/* need to be static here, cannot be freed */

	if (arg) {
		u32_strncpy(setenv_var, arg, sizeof(setenv_var) - 1);
		u32_sprintf(setenv_buf, "Enter value for %s: ", setenv_var);
		Input(setenv_buf, 30, INP_COOKED, SetenvFin2, NULL, 0);
	} else
		Input(U"Setenv: Enter variable name: ", 30, INP_COOKED, SetenvFin1, NULL, 0);
}

/*
 * the following options are understood by this parser:
 * -f, -f0, -f1, -fy, -fa
 * -t title, -T terminal-type, -h height-of-scrollback,
 * -ln, -l0, -ly, -l1, -l
 * -a, -M, -L
 */
void DoScreen(char *fn, char **av)
{
	struct NewWindow nwin;
	int num;
	char buf[20];

	nwin = nwin_undef;
	while (av && *av && av[0][0] == '-') {
		if (av[0][1] == '-') {
			av++;
			break;
		}
		switch (av[0][1]) {
		case 'f':
			switch (av[0][2]) {
			case 'n':
			case '0':
				nwin.flowflag = FLOW_OFF;
				break;
			case 'y':
			case '1':
			case '\0':
				nwin.flowflag = FLOW_ON;
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
#if defined(ENABLE_UTMP)
		case 'l':
			switch (av[0][2]) {
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
			nwin.aflag = true;
			break;
		case 'M':
			nwin.monitor = MON_ON;
			break;
		case 'L':
			nwin.Lflag = true;
			break;
		default:
			Msg(0, U"%s: screen: invalid option -%c.", fn, av[0][1]);
			break;
		}
		++av;
	}
	if (av && *av && IsNumColon(*av, buf, sizeof(buf))) {
		if (*buf != '\0')
			nwin.aka = buf;
		num = atoi(*av);
		if (num < 0 || (maxwin && num > maxwin - 1) || (!maxwin && num > MAXWIN - 1)) {
			Msg(0, U"%s: illegal screen number %d.", fn, num);
			num = 0;
		}
		nwin.StartAt = num;
		++av;
	}
	if (av && *av) {
		nwin.args = av;
		if (!nwin.aka)
			nwin.aka = Filename(*av);
	}
	MakeWindow(&nwin);
}

/*
 * CompileKeys must be called before Markroutine is first used.
 * to initialise the keys with defaults, call CompileKeys(NULL, mark_key_tab);
 *
 * s is an ascii string in a termcap-like syntax. It looks like
 *   "j=u:k=d:l=r:h=l: =.:" and so on...
 * this example rebinds the cursormovement to the keys u (up), d (down),
 * l (left), r (right). placing a mark will now be done with ".".
 */
int CompileKeys(char *s, int sl, unsigned char *array)
{
	int i;
	unsigned char key, value;

	if (sl == 0) {
		for (i = 0; i < 256; i++)
			array[i] = i;
		return 0;
	}
	while (sl) {
		key = *(unsigned char *)s++;
		if (*s != '=' || sl < 3)
			return -1;
		sl--;
		do {
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

/*
 *  Asynchronous input functions
 */

static void pow_detach_fn(uint32_t *buf, size_t len, void *data)
{
	(void)data; /* unused */

	if (len) {
		*buf = 0;
		return;
	}
	if (ktab[(int)*buf].nr != RC_POW_DETACH) {
		if (display)
			write(D_userfd, "\007", 1);
		Msg(0, U"Detach aborted.");
	} else
		Detach(D_POWER);
}

static void copy_reg_fn(uint32_t *buf, size_t len, void *data)
{
	(void)data; /* unused */

	struct plop *pp = plop_tab + (int)*buf;

	if (len) {
		*buf = 0;
		return;
	}
	if (pp->buf)
		free(pp->buf);
	pp->buf = 0;
	pp->len = 0;
	if (D_user->u_plop.len) {
		if ((pp->buf = malloc(D_user->u_plop.len * sizeof(uint32_t))) == NULL) {
			Msg(0, U"%s", strnomem);
			return;
		}
		u32_move(pp->buf, D_user->u_plop.buf, D_user->u_plop.len);
	}
	pp->len = D_user->u_plop.len;
	Msg(0, U"Copied %zu characters into register %c", D_user->u_plop.len, *buf);
}

static void ins_reg_fn(uint32_t *buf, size_t len, void *data)
{
	(void)data; /* unused */

	struct plop *pp = plop_tab + (int)*buf;

	if (len) {
		*buf = 0;
		return;
	}
	if (!fore)
		return;		/* Input() should not call us w/o fore, but you never know... */
	if (*buf == '.')
		Msg(0, U"ins_reg_fn: Warning: pasting real register '.'!");
	if (pp->buf) {
		MakePaster(&fore->w_paster, pp->buf, pp->len, 0);
		return;
	}
	Msg(0, U"Empty register.");
}

static void process_fn(uint32_t *buf, size_t len, void *data)
{
	struct plop *pp = plop_tab + (int)*buf;

	(void)data; /* unused */

	if (len) {
		*buf = 0;
		return;
	}
	if (pp->buf) {
		ProcessInput(pp->buf, pp->len);
		return;
	}
	Msg(0, U"Empty register.");
}

static void confirm_fn(uint32_t *buf, size_t len, void *data)
{
	struct action act;

	if (len || (*buf != 'y' && *buf != 'Y')) {
		*buf = 0;
		return;
	}
	act.nr = *(int *)data;
	act.args = noargs;
	act.argl = 0;
	act.quiet = 0;
	DoAction(&act, -1);
}

#if 0
struct inputsu {
	struct acluser **up;
	uint32_t name[24];
	uint32_t pw1[130];		/* FreeBSD crypts to 128 bytes */
	uint32_t pw2[130];
};

static void suFin(uint32_t *buf, size_t len, void *data)
{
	struct inputsu *i = (struct inputsu *)data;
	uint32_t *p;
	size_t l;

	if (!*i->name) {
		p = i->name;
		l = sizeof(i->name) - 1;
	} else if (!*i->pw1) {
		u32_strcpy(p = i->pw1, U"\377");
		l = sizeof(i->pw1) - 1;
	} else {
		u32_strcpy(p = i->pw2, U"\377");
		l = sizeof(i->pw2) - 1;
	}
	if (buf && len)
		u32_strncpy(p, buf, 1 + ((l < len) ? l : len));
	if (!*i->name)
		Input(U"Screen User: ", sizeof(i->name) - 1, INP_COOKED, suFin, (char *)i, 0);
	else if (!*i->pw1)
		Input(U"User's UNIX Password: ", sizeof(i->pw1) - 1, INP_COOKED | INP_NOECHO, suFin, (char *)i, 0);
	else if (!*i->pw2)
		Input(U"User's Screen Password: ", sizeof(i->pw2) - 1, INP_COOKED | INP_NOECHO, suFin, (char *)i, 0);
	else {
		if ((p = DoSu(i->up, i->name, i->pw2, i->pw1)))
			Msg(0, U"%s", p);
		free((char *)i);
	}
}

static int InputSu(struct acluser **up, char *name)
{
	struct inputsu *i;

	if (!(i = (struct inputsu *)calloc(1, sizeof(struct inputsu))))
		return -1;

	i->up = up;
	if (name && *name)
		suFin(name, (int)strlen(name), (char *)i);	/* can also initialise stuff */
	else
		suFin((uint32_t *)0, 0, (char *)i);
	return 0;
}
#endif

static int digraph_find(const char *buf)
{
	uint32_t i;
	for (i = 0; i < sizeof(digraphs) && digraphs[i].d[0]; i++)
		if ((digraphs[i].d[0] == (unsigned char)buf[0] && digraphs[i].d[1] == (unsigned char)buf[1]))
			break;
	return i;
}

static void digraph_fn(uint32_t *buf, size_t len, void *data)
{
	int i;
	uint32_t ch, x;
	size_t l;

	(void)data; /* unused */

	ch = buf[len];
	if (ch) {
		buf[len + 1] = ch;	/* so we can restore it later */
		if (ch < ' ' || ch == '\177')
			return;
		if (len >= 1 && ((*buf == 'U' && buf[1] == '+') || (*buf == '0' && (buf[1] == 'x' || buf[1] == 'X')))) {
			if (len == 1)
				return;
			if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f') && (ch < 'A' || ch > 'F')) {
				buf[len] = '\034';	/* ^] is ignored by Input() */
				return;
			}
			if (len == (*buf == 'U' ? 5 : 3))
				buf[len] = '\n';
			return;
		}
		if (len && *buf == '0') {
			if (ch < '0' || ch > '7') {
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
	if (buf[len + 1]) {
		buf[len] = buf[len + 1];	/* stored above */
		len++;
	}
	if (len < 2)
		return;
	if (!parse_input_int(buf, len, &x)) {
		i = digraph_find(buf);
		if ((x = digraphs[i].value) <= 0) {
			Msg(0, U"Unknown digraph");
			return;
		}
	}
	l = 1;
	*buf = x;
	while (l)
		LayProcess(&buf, &l);
}

int StuffKey(int i)
{
	struct action *act;
	int discard = 0;
	int keyno = i;

	if (i < KMAP_KEYS && D_ESCseen) {
		struct action *act = &D_ESCseen[i + 256];
		if (act->nr != RC_ILLEGAL) {
			D_ESCseen = 0;
			WindowChanged(fore, WINESC_ESC_SEEN);
			DoAction(act, i + 256);
			return 0;
		}
		discard = 1;
	}

	if (i >= T_CURSOR - T_CAPS && i < T_KEYPAD - T_CAPS && D_cursorkeys)
		i += T_OCAPS - T_CURSOR;
	else if (i >= T_KEYPAD - T_CAPS && i < T_OCAPS - T_CAPS && D_keypad)
		i += T_OCAPS - T_CURSOR;
	flayer = D_forecv->c_layer;
	fore = D_fore;
	act = 0;
	if (flayer && flayer->l_mode == 1)
		act = i < KMAP_KEYS + KMAP_AKEYS ? &mmtab[i] : &kmap_exts[i - (KMAP_KEYS + KMAP_AKEYS)].mm;
	if ((!act || act->nr == RC_ILLEGAL) && !D_mapdefault)
		act = i < KMAP_KEYS + KMAP_AKEYS ? &umtab[i] : &kmap_exts[i - (KMAP_KEYS + KMAP_AKEYS)].um;
	if (!act || act->nr == RC_ILLEGAL)
		act = i < KMAP_KEYS + KMAP_AKEYS ? &dmtab[i] : &kmap_exts[i - (KMAP_KEYS + KMAP_AKEYS)].dm;

	if (discard && (!act || act->nr != RC_COMMAND)) {
		/* if the input was just a single byte we let it through */
		if (D_tcs[keyno + T_CAPS].str && strlen(D_tcs[keyno + T_CAPS].str) == 1)
			return -1;
		if (D_ESCseen) {
			D_ESCseen = 0;
			WindowChanged(fore, WINESC_ESC_SEEN);
		}
		return 0;
	}
	D_mapdefault = 0;

	if (act == 0 || act->nr == RC_ILLEGAL)
		return -1;
	DoAction(act, 0);
	return 0;
}

static int IsOnDisplay(Window *win)
{
	Canvas *cv;
	for (cv = D_cvlist; cv; cv = cv->c_next)
		if (Layer2Window(cv->c_layer) == win)
			return 1;
	return 0;
}

Window *FindNiceWindow(Window *win, char *presel)
{
	int i;

	if (presel) {
		i = WindowByNoN(presel);
		if (i >= 0)
			win = wtab[i];
	}
	if (!display)
		return win;
	if (win && AclCheckPermWin(D_user, ACL_READ, win))
		win = 0;
	if (!win || (IsOnDisplay(win) && !presel)) {
		/* try to get another window */
		win = 0;
		for (win = windows; win; win = win->w_next)
			if (!win->w_layer.l_cvlist && !AclCheckPermWin(D_user, ACL_WRITE, win))
				break;
		if (!win)
			for (win = windows; win; win = win->w_next)
				if (win->w_layer.l_cvlist && !IsOnDisplay(win)
				    && !AclCheckPermWin(D_user, ACL_WRITE, win))
					break;
		if (!win)
			for (win = windows; win; win = win->w_next)
				if (!win->w_layer.l_cvlist && !AclCheckPermWin(D_user, ACL_READ, win))
					break;
		if (!win)
			for (win = windows; win; win = win->w_next)
				if (win->w_layer.l_cvlist && !IsOnDisplay(win)
				    && !AclCheckPermWin(D_user, ACL_READ, win))
					break;
		if (!win)
			for (win = windows; win; win = win->w_next)
				if (!win->w_layer.l_cvlist)
					break;
		if (!win)
			for (win = windows; win; win = win->w_next)
				if (win->w_layer.l_cvlist && !IsOnDisplay(win))
					break;
	}
	if (win && AclCheckPermWin(D_user, ACL_READ, win))
		win = 0;
	return win;
}

static int CalcSlicePercent(Canvas *cv, int percent)
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

static int ChangeCanvasSize(Canvas *fcv, int abs, int diff, bool gflag, int percent)
/* Canvas *fcv;	 make this canvas bigger
   int abs;		 mode: 0:rel 1:abs 2:max
   int diff;		 change this much
   bool gflag;		 go up if neccessary
   int percent; */
{
	Canvas *cv;
	int done, have, m, dir;

	if (abs == 0 && diff == 0)
		return 0;
	if (abs == 2) {
		if (diff == 0)
			fcv->c_slweight = 0;
		else {
			for (cv = fcv->c_slback->c_slperp; cv; cv = cv->c_slnext)
				cv->c_slweight = 0;
			fcv->c_slweight = 1;
			cv = fcv->c_slback->c_slback;
			if (gflag && cv && cv->c_slback)
				ChangeCanvasSize(cv, abs, diff, gflag, percent);
		}
		return diff;
	}
	if (abs) {
		if (diff < 0)
			diff = 0;
		if (percent && diff > percent)
			diff = percent;
	}
	if (percent) {
		int wsum, up;
		for (cv = fcv->c_slback->c_slperp, wsum = 0; cv; cv = cv->c_slnext)
			wsum += cv->c_slweight;
		if (wsum) {
			up = gflag ? CalcSlicePercent(fcv->c_slback->c_slback, percent) : percent;
			if (wsum < 1000) {
				int scale = wsum < 10 ? 1000 : 100;
				for (cv = fcv->c_slback->c_slperp; cv; cv = cv->c_slnext)
					cv->c_slweight *= scale;
				wsum *= scale;
			}
			for (cv = fcv->c_slback->c_slperp; cv; cv = cv->c_slnext) {
				if (cv->c_slweight) {
					cv->c_slweight = (cv->c_slweight * up) / percent;
					if (cv->c_slweight == 0)
						cv->c_slweight = 1;
				}
			}
			diff = (diff * wsum) / percent;
			percent = wsum;
		}
	} else {
		if (abs
		    && diff == (fcv->c_slorient == SLICE_VERT ? fcv->c_ye - fcv->c_ys + 2 : fcv->c_xe - fcv->c_xs + 2))
			return 0;
		/* fix weights to real size (can't be helped, sorry) */
		for (cv = fcv->c_slback->c_slperp; cv; cv = cv->c_slnext) {
			cv->c_slweight =
			    cv->c_slorient == SLICE_VERT ? cv->c_ye - cv->c_ys + 2 : cv->c_xe - cv->c_xs + 2;
		}
	}
	if (abs)
		diff = diff - fcv->c_slweight;
	if (diff == 0)
		return 0;
	if (diff < 0) {
		cv = fcv->c_slnext ? fcv->c_slnext : fcv->c_slprev;
		fcv->c_slweight += diff;
		cv->c_slweight -= diff;
		return diff;
	}
	done = 0;
	dir = 1;
	for (cv = fcv->c_slnext; diff > 0; cv = dir > 0 ? cv->c_slnext : cv->c_slprev) {
		if (!cv) {
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
		if (cv->c_slweight > m) {
			have = cv->c_slweight - m;
			if (have > diff)
				have = diff;
			cv->c_slweight -= have;
			done += have;
			diff -= have;
		}
	}
	if (diff && gflag) {
		/* need more room! */
		cv = fcv->c_slback->c_slback;
		if (cv && cv->c_slback)
			done += ChangeCanvasSize(fcv->c_slback->c_slback, 0, diff, gflag, percent);
	}
	fcv->c_slweight += done;
	return done;
}

static void ResizeRegions(uint32_t *arg, int flags)
{
	Canvas *cv;
	int diff, l;
	bool gflag = 0;
	int abs = 0, percent = 0;
	int orient = 0;

	if (!*arg)
		return;
	if (D_forecv->c_slorient == SLICE_UNKN) {
		Msg(0, U"resize: need more than one region");
		return;
	}
	gflag = flags & RESIZE_FLAG_L ? 0 : 1;
	orient |= flags & RESIZE_FLAG_H ? SLICE_HORI : 0;
	orient |= flags & RESIZE_FLAG_V ? SLICE_VERT : 0;
	if (orient == 0)
		orient = D_forecv->c_slorient;
	l = u32_strlen(arg);
	if (*arg == '=') {
		/* make all regions the same height */
		Canvas *cv = gflag ? &D_canvas : D_forecv->c_slback;
		if (cv->c_slperp->c_slorient & orient)
			EqualizeCanvas(cv->c_slperp, gflag);
		/* can't use cv->c_slorient directly as it can be D_canvas */
		if ((cv->c_slperp->c_slorient ^ (SLICE_HORI ^ SLICE_VERT)) & orient) {
			if (cv->c_slback) {
				cv = cv->c_slback;
				EqualizeCanvas(cv->c_slperp, gflag);
			} else
				EqualizeCanvas(cv, gflag);
		}
		ResizeCanvas(cv);
		RecreateCanvasChain();
		RethinkDisplayViewports();
		ResizeLayersToCanvases();
		return;
	}
	if (!u32_strcmp(arg, U"min") || !u32_strcmp(arg, U"0")) {
		abs = 2;
		diff = 0;
	} else if (!u32_strcmp(arg, U"max") || !u32_strcmp(arg, U"_")) {
		abs = 2;
		diff = 1;
	} else {
		if (l > 0 && arg[l - 1] == '%')
			percent = 1000;
#if 0 /* FIXME: we need something like atoi but for UTF-32 string */
		if (*arg == '+')
			diff = atoi(arg + 1);
		else if (*arg == '-')
			diff = -atoi(arg + 1);
		else {
			diff = atoi(arg);	/* +1 because of caption line */
			if (diff < 0)
				diff = 0;
			abs = diff == 0 ? 2 : 1;
		}
#endif
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

static void ResizeFin(uint32_t *buf, size_t len, void *data)
{
	uint32_t ch;
	int flags = *(int *)data;
	ch = buf[len];
	if (ch == 0) {
		ResizeRegions(buf, flags);
		return;
	}
	if (ch == 'h')
		flags ^= RESIZE_FLAG_H;
	else if (ch == 'v')
		flags ^= RESIZE_FLAG_V;
	else if (ch == 'b')
		flags |= RESIZE_FLAG_H | RESIZE_FLAG_V;
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

void SetForeCanvas(Display *d, Canvas *cv)
{
	Display *odisplay = display;
	if (d->d_forecv == cv)
		return;

	display = d;
	D_forecv = cv;
	if ((focusminwidth && (focusminwidth < 0 || D_forecv->c_xe - D_forecv->c_xs + 1 < focusminwidth)) ||
	    (focusminheight && (focusminheight < 0 || D_forecv->c_ye - D_forecv->c_ys + 1 < focusminheight))) {
		ResizeCanvas(&D_canvas);
		RecreateCanvasChain();
		RethinkDisplayViewports();
		ResizeLayersToCanvases();	/* redisplays */
	}
	fore = D_fore = Layer2Window(D_forecv->c_layer);
	if (D_other == fore)
		D_other = 0;
	flayer = D_forecv->c_layer;
	if (D_xtermosc[2] || D_xtermosc[3]) {
		Activate(-1);
	} else {
		RefreshHStatus();
		RefreshXtermOSC();
		flayer = D_forecv->c_layer;
		CV_CALL(D_forecv, LayRestore();
			LaySetCursor());
		WindowChanged(0, WINESC_FOCUS);
	}

	display = odisplay;
}

void RefreshXtermOSC()
{
	int i;
	Window *p;

	p = Layer2Window(D_forecv->c_layer);
	for (i = 3; i >= 0; i--)
		SetXtermOSC(i, p ? p->w_xtermosc[i] : 0);
}

/*
 *  ParseAttrColor - parses attributes and color
 *  	str - string containing attributes and/or colors
 *  	        d - dim
 *  	        u - underscore
 *  	        b - bold
 *  	        r - reverse
 *  	        s - standout
 *  	        l - blinking
 *  	        0-255;0-255 - foreground;background
 *  	        xABCDEF;xABCDEF - truecolor foreground;background
 *  	msgok - can we be verbose if something is wrong
 *
 *  returns value representing encoded value
 */
uint64_t ParseAttrColor(uint32_t *str, int msgok)
{
	uint64_t r;

	uint32_t attr = 0;
	uint32_t bg = 0, fg = 0;
	uint8_t bm = 0, fm = 0;

	uint32_t *cl;
	uint8_t *cm;
	cl = &fg;
	cm = &fm;


	while (*str) {
		switch (*str) {
		case 'd':
			attr |= A_DI;
			break;
		case 'u':
			attr |= A_US;
			break;
		case 'b':
			attr |= A_BD;
			break;
		case 'r':
			attr |= A_RV;
			break;
		case 's':
			attr |= A_SO;
			break;
		case 'l':
			attr |= A_BL;
			break;
		case '-':
			*cm = 0;
			break;
		case 'x':
			*cm = 2;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (!*cm) *cm = 1;
			*cl = *cl * 10 + (*str - '0');
			break;
		case ';':
			cl = &bg;
			cm = &bm;
			break;
		case ' ':
			break;
		default:
			if (msgok)
				Msg(0, U"junk after description: '%c'\n", *str);
			break;
		}
		str++;
	}

	if (fg > 255) {
		fg = fm = 0;
	}
	if (bg > 255) {
		bg = bm = 0;
	}

	r = (((uint64_t)attr & 0x0FF) << 56);

	r |= (((uint64_t)bg & 0x0FFFFFF) << 24);
	r |= ((uint64_t)fg & 0x0FFFFFF);
	r |= ((uint64_t)fm << 48);
	r |= ((uint64_t)bm << 52);

	return r;
}

/*
 *   ApplyAttrColor - decodes color attributes and sets them in structure
 *   	i - encoded attributes and color
 *   	00 00 00 00 00 00 00 00
 *	xx 00 00 00 00 00 00 00 - attr
 *	00 x0 00 00 00 00 00 00 - what kind of background
 *	00 0x 00 00 00 00 00 00 - what kind of foreground
 *	                          0 - default, 1 - 256, 2 - truecolor
 *	00 00 xx xx xx 00 00 00 - background
 *	00 00 00 00 00 xx xx xx - foreground
 *   	mc -structure to modify
 */
void ApplyAttrColor(uint64_t i, struct mchar *mc)
{
	uint32_t a, b, f;
	unsigned char h;
	a = (0xFF00000000000000 & i) >> 56;
	b = (0x0000FFFFFF000000 & i) >> 24;
	f = (0x0000000000FFFFFF & i);

	h = (0x00FF000000000000 & i) >> 48;

	if (h & 0x20) b |= 0x02000000;
	if (h & 0x10) b |= 0x01000000;
	if (h & 0x02) f |= 0x02000000;
	if (h & 0x01) f |= 0x01000000;
	
	mc->attr	= a;
	mc->colorbg	= b;
	mc->colorfg	= f;
}
