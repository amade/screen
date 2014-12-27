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

#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "config.h"
#include "screen.h"
#include "extern.h"
#include "canvas.h"

static int CountChars(int);
static int DoAddChar(int);
static int BlankResize(int, int);
static int CallRewrite(int, int, int, int);
static void disp_readev_fn(struct event *, char *);
static void disp_writeev_fn(struct event *, char *);
static void disp_writeev_eagain(struct event *, char *);
static void disp_status_fn(struct event *, char *);
static void disp_hstatus_fn(struct event *, char *);
static void disp_blocked_fn(struct event *, char *);
static void disp_map_fn(struct event *, char *);
static void disp_idle_fn(struct event *, char *);
static void disp_blanker_fn(struct event *, char *);
static void WriteLP(int, int);
static void INSERTCHAR(int);
static void RAW_PUTCHAR(int);
static void SetBackColor(int);
static void RemoveStatusMinWait(void);

/*
 * tputs needs this to calculate the padding
 */
#ifndef NEED_OSPEED
extern
#endif				/* NEED_OSPEED */
short ospeed;

struct display *display, *displays;
int attr2color[8][4];
int nattr2color;

/*
 *  The default values
 */
int defobuflimit = OBUF_MAX;
int defnonblock = -1;
int defmousetrack = 0;
int defautonuke = 0;
int captionalways;
int hardstatusemu = HSTATUS_IGNORE;

int focusminwidth, focusminheight;

/*
 *  Default layer management
 */

void DefProcess(char **bufp, int *lenp)
{
	*bufp += *lenp;
	*lenp = 0;
}

void DefRedisplayLine(int y, int xs, int xe, int isblank)
{
	if (isblank == 0 && y >= 0)
		DefClearLine(y, xs, xe, 0);
}

void DefClearLine(int y, int xs, int xe, int bce)
{
	LClearLine(flayer, y, xs, xe, bce, (struct mline *)0);
}

 /*ARGSUSED*/ int DefRewrite(int y, int xs, int xe, struct mchar *rend, int doit)
{
	return EXPENSIVE;
}

 /*ARGSUSED*/ int DefResize(int wi, int he)
{
	return -1;
}

void DefRestore()
{
	LAY_DISPLAYS(flayer, InsertMode(0));
	/* ChangeScrollRegion(0, D_height - 1); */
	LKeypadMode(flayer, 0);
	LCursorkeysMode(flayer, 0);
	LCursorVisibility(flayer, 0);
	LMouseMode(flayer, 0);
	LSetRendition(flayer, &mchar_null);
	LSetFlow(flayer, nwin_default.flowflag & FLOW_NOW);
}

/*
 *  Blank layer management
 */

struct LayFuncs BlankLf = {
	DefProcess,
	0,
	DefRedisplayLine,
	DefClearLine,
	DefRewrite,
	BlankResize,
	DefRestore,
	0
};

 /*ARGSUSED*/ static int BlankResize(int wi, int he)
{
	flayer->l_width = wi;
	flayer->l_height = he;
	return 0;
}

/*
 *  Generate new display, start with a blank layer.
 *  The termcap arrays are not initialised here.
 *  The new display is placed in the displays list.
 */

struct display *MakeDisplay(char *uname, char *utty, char *term, int fd, int pid, struct mode *Mode)
{
	struct acluser **u;
	struct baud_values *b;

	if (!*(u = FindUserPtr(uname)) && UserAdd(uname, (char *)0, u))
		return 0;	/* could not find or add user */

	if ((display = calloc(1, sizeof(*display))) == 0)
		return 0;
	display->d_next = displays;
	displays = display;
	D_flow = 1;
	D_nonblock = defnonblock;
	D_userfd = fd;
	D_readev.fd = D_writeev.fd = fd;
	D_readev.type = EV_READ;
	D_writeev.type = EV_WRITE;
	D_readev.data = D_writeev.data = (char *)display;
	D_readev.handler = disp_readev_fn;
	D_writeev.handler = disp_writeev_fn;
	evenq(&D_readev);
	D_writeev.condpos = &D_obuflen;
	D_writeev.condneg = &D_obuffree;
	evenq(&D_writeev);
	D_statusev.type = EV_TIMEOUT;
	D_statusev.data = (char *)display;
	D_statusev.handler = disp_status_fn;
	D_hstatusev.type = EV_TIMEOUT;
	D_hstatusev.data = (char *)display;
	D_hstatusev.handler = disp_hstatus_fn;
	D_blockedev.type = EV_TIMEOUT;
	D_blockedev.data = (char *)display;
	D_blockedev.handler = disp_blocked_fn;
	D_blockedev.condpos = &D_obuffree;
	D_blockedev.condneg = &D_obuflenmax;
	D_hstatusev.handler = disp_hstatus_fn;
	D_mapev.type = EV_TIMEOUT;
	D_mapev.data = (char *)display;
	D_mapev.handler = disp_map_fn;
	D_idleev.type = EV_TIMEOUT;
	D_idleev.data = (char *)display;
	D_idleev.handler = disp_idle_fn;
	D_blankerev.type = EV_READ;
	D_blankerev.data = (char *)display;
	D_blankerev.handler = disp_blanker_fn;
	D_blankerev.fd = -1;
	D_OldMode = *Mode;
	D_status_obuffree = -1;
	Resize_obuf();		/* Allocate memory for buffer */
	D_obufmax = defobuflimit;
	D_obuflenmax = D_obuflen - D_obufmax;
	D_auto_nuke = defautonuke;
	D_obufp = D_obuf;
	D_printfd = -1;
	D_userpid = pid;

#ifdef POSIX
	if ((b = lookup_baud((int)cfgetospeed(&D_OldMode.tio))))
		D_dospeed = b->idx;
#else
#ifdef TERMIO
	if ((b = lookup_baud(D_OldMode.tio.c_cflag & CBAUD)))
		D_dospeed = b->idx;
#else
	D_dospeed = (short)D_OldMode.m_ttyb.sg_ospeed;
#endif
#endif
	debug("New displays ospeed = %d\n", D_dospeed);

	strncpy(D_usertty, utty, sizeof(D_usertty) - 1);
	D_usertty[sizeof(D_usertty) - 1] = 0;
	strncpy(D_termname, term, sizeof(D_termname) - 1);
	D_termname[sizeof(D_termname) - 1] = 0;
	D_user = *u;
	D_processinput = ProcessInput;
	D_mousetrack = defmousetrack;
	return display;
}

void FreeDisplay()
{
	struct win *p;
	struct display *d, **dp;

	FreeTransTable();
	KillBlanker();
	if (D_userfd >= 0) {
		Flush(3);
		if (!display)
			return;
		SetTTY(D_userfd, &D_OldMode);
		fcntl(D_userfd, F_SETFL, 0);
	}
	freetty();
	if (D_tentry)
		free(D_tentry);
	D_tentry = 0;
	if (D_processinputdata)
		free(D_processinputdata);
	D_processinputdata = 0;
	D_tcinited = 0;
	evdeq(&D_hstatusev);
	evdeq(&D_statusev);
	evdeq(&D_readev);
	evdeq(&D_writeev);
	evdeq(&D_blockedev);
	evdeq(&D_mapev);
	if (D_kmaps) {
		free(D_kmaps);
		D_kmaps = 0;
		D_aseqs = 0;
		D_nseqs = 0;
		D_seqp = 0;
		D_seql = 0;
		D_seqh = 0;
	}
	evdeq(&D_idleev);
	evdeq(&D_blankerev);

	for (dp = &displays; (d = *dp); dp = &d->d_next)
		if (d == display)
			break;
	ASSERT(d);
	if (D_status_lastmsg)
		free(D_status_lastmsg);
	if (D_obuf)
		free(D_obuf);
	*dp = display->d_next;

	while (D_canvas.c_slperp)
		FreeCanvas(D_canvas.c_slperp);
	D_cvlist = 0;

	for (p = windows; p; p = p->w_next) {
		if (p->w_pdisplay == display)
			p->w_pdisplay = 0;
		if (p->w_lastdisp == display)
			p->w_lastdisp = 0;
		if (p->w_readev.condneg == &D_status || p->w_readev.condneg == &D_obuflenmax)
			p->w_readev.condpos = p->w_readev.condneg = 0;
	}
	for (p = windows; p; p = p->w_next)
		if (p->w_zdisplay == display)
			zmodem_abort(p, 0);
	if (D_mousetrack) {
		D_mousetrack = 0;
		MouseMode(0);
	}
	free((char *)display);
	display = 0;
}

/*
 * if the adaptflag is on, we keep the size of this display, else
 * we may try to restore our old window sizes.
 */
void InitTerm(int adapt)
{
	ASSERT(display);
	ASSERT(D_tcinited);
	D_top = D_bot = -1;
	AddCStr(D_IS);
	AddCStr(D_TI);
	/* Check for toggle */
	if (D_IM && strcmp(D_IM, D_EI))
		AddCStr(D_EI);
	D_insert = 0;
	AddCStr(D_KS);
	AddCStr(D_CCS);
	D_keypad = 0;
	D_cursorkeys = 0;
	AddCStr(D_ME);
	AddCStr(D_EA);
	AddCStr(D_CE0);
	D_rend = mchar_null;
	D_atyp = 0;
	if (adapt == 0)
		ResizeDisplay(D_defwidth, D_defheight);
	ChangeScrollRegion(0, D_height - 1);
	D_x = D_y = 0;
	Flush(3);
	ClearAll();
	debug("we %swant to adapt all our windows to the display\n", (adapt) ? "" : "don't ");
	/* In case the size was changed by a init sequence */
	CheckScreenSize((adapt) ? 2 : 0);
}

void FinitTerm()
{
	ASSERT(display);
	KillBlanker();
	if (D_tcinited) {
		ResizeDisplay(D_defwidth, D_defheight);
		InsertMode(0);
		ChangeScrollRegion(0, D_height - 1);
		KeypadMode(0);
		CursorkeysMode(0);
		CursorVisibility(0);
		if (D_mousetrack)
			D_mousetrack = 0;
		MouseMode(0);
		SetRendition(&mchar_null);
		SetFlow(FLOW_NOW);
		AddCStr(D_KE);
		AddCStr(D_CCE);
		if (D_hstatus)
			ShowHStatus((char *)0);
		ClearAllXtermOSC();
		D_x = D_y = -1;
		GotoPos(0, D_height - 1);
		AddChar('\r');
		AddChar('\n');
		AddCStr(D_TE);
	}
	Flush(3);
}

static void INSERTCHAR(int c)
{
	ASSERT(display);
	if (!D_insert && D_x < D_width - 1) {
		if (D_IC || D_CIC) {
			if (D_IC)
				AddCStr(D_IC);
			else
				AddCStr2(D_CIC, 1);
			RAW_PUTCHAR(c);
			return;
		}
		InsertMode(1);
		if (!D_insert) {
			RefreshLine(D_y, D_x, D_width - 1, 0);
			return;
		}
	}
	RAW_PUTCHAR(c);
}

void PUTCHAR(int c)
{
	ASSERT(display);
	if (D_insert && D_x < D_width - 1)
		InsertMode(0);
	RAW_PUTCHAR(c);
}

void PUTCHARLP(int c)
{
	if (D_x < D_width - 1) {
		if (D_insert)
			InsertMode(0);
		RAW_PUTCHAR(c);
		return;
	}
	if (D_CLP || D_y != D_bot) {
		int y = D_y;
		RAW_PUTCHAR(c);
		if (D_AM && !D_CLP)
			GotoPos(D_width - 1, y);
		return;
	}
	debug("PUTCHARLP: lp_missing!\n");
	D_lp_missing = 1;
	D_rend.image = c;
	D_lpchar = D_rend;
	/* XXX -> PutChar ? */
	if (D_mbcs) {
		D_lpchar.mbcs = c;
		D_lpchar.image = D_mbcs;
		D_mbcs = 0;
		D_x--;
	}
}

/*
 * RAW_PUTCHAR() is for all text that will be displayed.
 * NOTE: charset Nr. 0 has a conversion table, but c1, c2, ... don't.
 */

STATIC void RAW_PUTCHAR(int c)
{
	ASSERT(display);

	if (D_encoding == UTF8) {
		c = (c & 255) | (unsigned char)D_rend.font << 8 | (unsigned char)D_rend.fontx << 16;
		if (D_mbcs) {
			c = D_mbcs;
			if (D_x == D_width)
				D_x += D_AM ? 1 : -1;
			D_mbcs = 0;
		} else if (utf8_isdouble(c)) {
			D_mbcs = c;
			D_x++;
			return;
		}
		if (c < 32) {
			AddCStr2(D_CS0, '0');
			AddChar(c + 0x5f);
			AddCStr(D_CE0);
			goto addedutf8;
		}
		if (c < 0x80) {
			if (D_xtable && D_xtable[(int)(unsigned char)D_rend.font]
			    && D_xtable[(int)(unsigned char)D_rend.font][(int)(unsigned char)c])
				AddStr(D_xtable[(int)(unsigned char)D_rend.font][(int)(unsigned char)c]);
			else
				AddChar(c);
		} else
			AddUtf8(c);
		goto addedutf8;
	}
	if (is_dw_font(D_rend.font)) {
		int t = c;
		if (D_mbcs == 0) {
			D_mbcs = c;
			D_x++;
			return;
		}
		D_x--;
		if (D_x == D_width - 1)
			D_x += D_AM ? 1 : -1;
		c = D_mbcs;
		D_mbcs = t;
	}
	if (D_encoding)
		c = PrepareEncodedChar(c);
 kanjiloop:
	if (D_xtable && D_xtable[(int)(unsigned char)D_rend.font]
	    && D_xtable[(int)(unsigned char)D_rend.font][(int)(unsigned char)c])
		AddStr(D_xtable[(int)(unsigned char)D_rend.font][(int)(unsigned char)c]);
	else
		AddChar(D_rend.font != '0' ? c : D_c0_tab[(int)(unsigned char)c]);

 addedutf8:
	if (++D_x >= D_width) {
		if (D_AM == 0)
			D_x = D_width - 1;
		else if (!D_CLP || D_x > D_width) {
			D_x -= D_width;
			if (D_y < D_height - 1 && D_y != D_bot)
				D_y++;
		}
	}
	if (D_mbcs) {
		c = D_mbcs;
		D_mbcs = 0;
		goto kanjiloop;
	}
}

static int DoAddChar(int c)
{
	/* this is for ESC-sequences only (AddChar is a macro) */
	AddChar(c);
	return c;
}

void AddCStr(char *s)
{
	if (display && s && *s) {
		ospeed = D_dospeed;
		tputs(s, 1, DoAddChar);
	}
}

void AddCStr2(char *s, int c)
{
	if (display && s && *s) {
		ospeed = D_dospeed;
		tputs(tgoto(s, 0, c), 1, DoAddChar);
	}
}

/* Insert mode is a toggle on some terminals, so we need this hack:
 */
void InsertMode(int on)
{
	if (display && on != D_insert && D_IM) {
		D_insert = on;
		if (on)
			AddCStr(D_IM);
		else
			AddCStr(D_EI);
	}
}

/* ...and maybe keypad application mode is a toggle, too:
 */
void KeypadMode(int on)
{
	if (display)
		D_keypad = on;
}

void CursorkeysMode(int on)
{
	if (display)
		D_cursorkeys = on;
}

void ReverseVideo(int on)
{
	if (display && D_revvid != on && D_CVR) {
		D_revvid = on;
		if (D_revvid)
			AddCStr(D_CVR);
		else
			AddCStr(D_CVN);
	}
}

void CursorVisibility(int v)
{
	if (display && D_curvis != v) {
		if (D_curvis)
			AddCStr(D_VE);	/* do this always, just to be safe */
		D_curvis = 0;
		if (v == -1 && D_VI)
			AddCStr(D_VI);
		else if (v == 1 && D_VS)
			AddCStr(D_VS);
		else
			return;
		D_curvis = v;
	}
}

void MouseMode(int mode)
{
	if (!display)
		return;

	if (mode < D_mousetrack)
		mode = D_mousetrack;

	if (D_mouse != mode) {
		char mousebuf[20];
		if (!D_CXT)
			return;
		if (D_mouse) {
			sprintf(mousebuf, "\033[?%dl", D_mouse);
			AddStr(mousebuf);
		}
		if (mode) {
			sprintf(mousebuf, "\033[?%dh", mode);
			AddStr(mousebuf);
		}
		D_mouse = mode;
	}
}

static int StrCost;

/* ARGSUSED */
static int CountChars(int c)
{
	StrCost++;
	return c;
}

int CalcCost(register char *s)
{
	ASSERT(display);
	if (s) {
		StrCost = 0;
		ospeed = D_dospeed;
		tputs(s, 1, CountChars);
		return StrCost;
	} else
		return EXPENSIVE;
}

static int CallRewrite(int y, int xs, int xe, int doit)
{
	struct canvas *cv, *cvlist, *cvlnext;
	struct viewport *vp;
	struct layer *oldflayer;
	int cost;

	debug("CallRewrite %d %d %d\n", y, xs, xe);
	ASSERT(display);
	ASSERT(xe >= xs);

	vp = 0;
	for (cv = D_cvlist; cv; cv = cv->c_next) {
		if (y < cv->c_ys || y > cv->c_ye || xe < cv->c_xs || xs > cv->c_xe)
			continue;
		for (vp = cv->c_vplist; vp; vp = vp->v_next)
			if (y >= vp->v_ys && y <= vp->v_ye && xe >= vp->v_xs && xs <= vp->v_xe)
				break;
		if (vp)
			break;
	}
	if (doit) {
		oldflayer = flayer;
		flayer = cv->c_layer;
		cvlist = flayer->l_cvlist;
		cvlnext = cv->c_lnext;
		flayer->l_cvlist = cv;
		cv->c_lnext = 0;
		LayRewrite(y - vp->v_yoff, xs - vp->v_xoff, xe - vp->v_xoff, &D_rend, 1);
		flayer->l_cvlist = cvlist;
		cv->c_lnext = cvlnext;
		flayer = oldflayer;
		return 0;
	}
	if (cv == 0 || cv->c_layer == 0)
		return EXPENSIVE;	/* not found or nothing on it */
	if (xs < vp->v_xs || xe > vp->v_xe)
		return EXPENSIVE;	/* crosses viewport boundaries */
	if (y - vp->v_yoff < 0 || y - vp->v_yoff >= cv->c_layer->l_height)
		return EXPENSIVE;	/* line not on layer */
	if (xs - vp->v_xoff < 0 || xe - vp->v_xoff >= cv->c_layer->l_width)
		return EXPENSIVE;	/* line not on layer */
	if (D_encoding == UTF8)
		D_rend.font = 0;
	oldflayer = flayer;
	flayer = cv->c_layer;
	debug("Calling Rewrite %d %d %d\n", y - vp->v_yoff, xs - vp->v_xoff, xe - vp->v_xoff);
	cost = LayRewrite(y - vp->v_yoff, xs - vp->v_xoff, xe - vp->v_xoff, &D_rend, 0);
	flayer = oldflayer;
	if (D_insert)
		cost += D_EIcost + D_IMcost;
	return cost;
}

void GotoPos(int x2, int y2)
{
	register int dy, dx, x1, y1;
	register int costx, costy;
	register int m;
	register char *s;
	int CMcost;
	enum move_t xm = M_NONE, ym = M_NONE;

	if (!display)
		return;

	x1 = D_x;
	y1 = D_y;

	if (x1 == D_width) {
		if (D_CLP && D_AM)
			x1 = -1;	/* don't know how the terminal treats this */
		else
			x1--;
	}
	if (x2 == D_width)
		x2--;
	dx = x2 - x1;
	dy = y2 - y1;
	if (dy == 0 && dx == 0)
		return;
	debug("GotoPos (%d,%d)", x1, y1);
	debug(" -> (%d,%d)\n", x2, y2);
	if (!D_MS)		/* Safe to move ? */
		SetRendition(&mchar_null);
	if (y1 < 0		/* don't know the y position */
	    || (y2 > D_bot && y1 <= D_bot)	/* have to cross border */
	    ||(y2 < D_top && y1 >= D_top)) {	/* of scrollregion ?    */
 DoCM:
		if (D_HO && !x2 && !y2)
			AddCStr(D_HO);
		else
			AddCStr(tgoto(D_CM, x2, y2));
		D_x = x2;
		D_y = y2;
		return;
	}

	/* some scrollregion implementations don't allow movements
	 * away from the region. sigh.
	 */
	if ((y1 > D_bot && y2 > y1) || (y1 < D_top && y2 < y1))
		goto DoCM;

	/* Calculate CMcost */
	if (D_HO && !x2 && !y2)
		s = D_HO;
	else
		s = tgoto(D_CM, x2, y2);
	CMcost = CalcCost(s);

	/* Calculate the cost to move the cursor to the right x position */
	costx = EXPENSIVE;
	if (x1 >= 0) {		/* relativ x positioning only if we know where we are */
		if (dx > 0) {
			if (D_CRI && (dx > 1 || !D_ND)) {
				costx = CalcCost(tgoto(D_CRI, 0, dx));
				xm = M_CRI;
			}
			if ((m = D_NDcost * dx) < costx) {
				costx = m;
				xm = M_RI;
			}
			/* Speedup: dx <= LayRewrite() */
			if (dx < costx && (m = CallRewrite(y1, x1, x2 - 1, 0)) < costx) {
				costx = m;
				xm = M_RW;
			}
		} else if (dx < 0) {
			if (D_CLE && (dx < -1 || !D_BC)) {
				costx = CalcCost(tgoto(D_CLE, 0, -dx));
				xm = M_CLE;
			}
			if ((m = -dx * D_LEcost) < costx) {
				costx = m;
				xm = M_LE;
			}
		} else
			costx = 0;
	}
	/* Speedup: LayRewrite() >= x2 */
	if (x2 + D_CRcost < costx && (m = (x2 ? CallRewrite(y1, 0, x2 - 1, 0) : 0) + D_CRcost) < costx) {
		costx = m;
		xm = M_CR;
	}

	/* Check if it is already cheaper to do CM */
	if (costx >= CMcost)
		goto DoCM;

	/* Calculate the cost to move the cursor to the right y position */
	costy = EXPENSIVE;
	if (dy > 0) {
		if (D_CDO && dy > 1) {	/* DO & NL are always != 0 */
			costy = CalcCost(tgoto(D_CDO, 0, dy));
			ym = M_CDO;
		}
		if ((m = dy * ((x2 == 0) ? D_NLcost : D_DOcost)) < costy) {
			costy = m;
			ym = M_DO;
		}
	} else if (dy < 0) {
		if (D_CUP && (dy < -1 || !D_UP)) {
			costy = CalcCost(tgoto(D_CUP, 0, -dy));
			ym = M_CUP;
		}
		if ((m = -dy * D_UPcost) < costy) {
			costy = m;
			ym = M_UP;
		}
	} else
		costy = 0;

	/* Finally check if it is cheaper to do CM */
	if (costx + costy >= CMcost)
		goto DoCM;

	switch (xm) {
	case M_LE:
		while (dx++ < 0)
			AddCStr(D_BC);
		break;
	case M_CLE:
		AddCStr2(D_CLE, -dx);
		break;
	case M_RI:
		while (dx-- > 0)
			AddCStr(D_ND);
		break;
	case M_CRI:
		AddCStr2(D_CRI, dx);
		break;
	case M_CR:
		AddCStr(D_CR);
		D_x = 0;
		x1 = 0;
		/* FALLTHROUGH */
	case M_RW:
		if (x1 < x2)
			(void)CallRewrite(y1, x1, x2 - 1, 1);
		break;
	default:
		break;
	}

	switch (ym) {
	case M_UP:
		while (dy++ < 0)
			AddCStr(D_UP);
		break;
	case M_CUP:
		AddCStr2(D_CUP, -dy);
		break;
	case M_DO:
		s = (x2 == 0) ? D_NL : D_DO;
		while (dy-- > 0)
			AddCStr(s);
		break;
	case M_CDO:
		AddCStr2(D_CDO, dy);
		break;
	default:
		break;
	}
	D_x = x2;
	D_y = y2;
}

void ClearAll()
{
	ASSERT(display);
	ClearArea(0, 0, 0, D_width - 1, D_width - 1, D_height - 1, 0, 0);
}

void ClearArea(int x1, int y1, int xs, int xe, int x2, int y2, int bce, int uselayfn)
{
	int y, xxe;
	struct canvas *cv;
	struct viewport *vp;

	debug("Clear %d,%d", x1, y1);
	debug(" %d-%d", xs, xe);
	debug(" %d,%d", x2, y2);
	debug(" uselayfn=%d bce=%d\n", uselayfn, bce);
	ASSERT(display);
	if (x1 == D_width)
		x1--;
	if (x2 == D_width)
		x2--;
	if (xs == -1)
		xs = x1;
	if (xe == -1)
		xe = x2;
	if (D_UT)		/* Safe to erase ? */
		SetRendition(&mchar_null);
	if (D_BE)
		SetBackColor(bce);
	if (D_lp_missing && y1 <= D_bot && xe >= D_width - 1) {
		if (y2 > D_bot || (y2 == D_bot && x2 >= D_width - 1))
			D_lp_missing = 0;
	}
	if (x2 == D_width - 1 && (xs == 0 || y1 == y2) && xe == D_width - 1 && y2 == D_height - 1 && (!bce || D_BE)) {
		if (x1 == 0 && y1 == 0 && D_auto_nuke)
			NukePending();
		if (x1 == 0 && y1 == 0 && D_CL) {
			AddCStr(D_CL);
			D_y = D_x = 0;
			return;
		}
		/*
		 * Workaround a hp700/22 terminal bug. Do not use CD where CE
		 * is also appropriate.
		 */
		if (D_CD && (y1 < y2 || !D_CE)) {
			GotoPos(x1, y1);
			AddCStr(D_CD);
			return;
		}
	}
	if (x1 == 0 && xs == 0 && (xe == D_width - 1 || y1 == y2) && y1 == 0 && D_CCD && (!bce || D_BE)) {
		GotoPos(x1, y1);
		AddCStr(D_CCD);
		return;
	}
	xxe = xe;
	for (y = y1; y <= y2; y++, x1 = xs) {
		if (y == y2)
			xxe = x2;
		if (x1 == 0 && D_CB && (xxe != D_width - 1 || (D_x == xxe && D_y == y)) && (!bce || D_BE)) {
			GotoPos(xxe, y);
			AddCStr(D_CB);
			continue;
		}
		if (xxe == D_width - 1 && D_CE && (!bce || D_BE)) {
			GotoPos(x1, y);
			AddCStr(D_CE);
			continue;
		}
		if (uselayfn) {
			vp = 0;
			for (cv = D_cvlist; cv; cv = cv->c_next) {
				if (y < cv->c_ys || y > cv->c_ye || xxe < cv->c_xs || x1 > cv->c_xe)
					continue;
				for (vp = cv->c_vplist; vp; vp = vp->v_next)
					if (y >= vp->v_ys && y <= vp->v_ye && xxe >= vp->v_xs && x1 <= vp->v_xe)
						break;
				if (vp)
					break;
			}
			if (cv && cv->c_layer && x1 >= vp->v_xs && xxe <= vp->v_xe &&
			    y - vp->v_yoff >= 0 && y - vp->v_yoff < cv->c_layer->l_height &&
			    xxe - vp->v_xoff >= 0 && x1 - vp->v_xoff < cv->c_layer->l_width) {
				struct layer *oldflayer = flayer;
				struct canvas *cvlist, *cvlnext;
				flayer = cv->c_layer;
				cvlist = flayer->l_cvlist;
				cvlnext = cv->c_lnext;
				flayer->l_cvlist = cv;
				cv->c_lnext = 0;
				LayClearLine(y - vp->v_yoff, x1 - vp->v_xoff, xxe - vp->v_xoff, bce);
				flayer->l_cvlist = cvlist;
				cv->c_lnext = cvlnext;
				flayer = oldflayer;
				continue;
			}
		}
		ClearLine((struct mline *)0, y, x1, xxe, bce);
	}
}

/*
 * if cur_only > 0, we only redisplay current line, as a full refresh is
 * too expensive over a low baud line.
 */
void Redisplay(int cur_only)
{
	ASSERT(display);

	/* XXX do em all? */
	InsertMode(0);
	ChangeScrollRegion(0, D_height - 1);
	KeypadMode(0);
	CursorkeysMode(0);
	CursorVisibility(0);
	MouseMode(0);
	SetRendition(&mchar_null);
	SetFlow(FLOW_NOW);

	ClearAll();
	RefreshXtermOSC();
	if (cur_only > 0 && D_fore)
		RefreshArea(0, D_fore->w_y, D_width - 1, D_fore->w_y, 1);
	else
		RefreshAll(1);
	RefreshHStatus();
	CV_CALL(D_forecv, LayRestore(); LaySetCursor());
}

void RedisplayDisplays(int cur_only)
{
	struct display *olddisplay = display;
	for (display = displays; display; display = display->d_next)
		Redisplay(cur_only);
	display = olddisplay;
}

/* XXX: use oml! */
void ScrollH(int y, int xs, int xe, int n, int bce, struct mline *oml)
{
	int i;

	if (n == 0)
		return;
	if (xe != D_width - 1) {
		RefreshLine(y, xs, xe, 0);
		/* UpdateLine(oml, y, xs, xe); */
		return;
	}
	GotoPos(xs, y);
	if (D_UT)
		SetRendition(&mchar_null);
	if (D_BE)
		SetBackColor(bce);
	if (n > 0) {
		if (n >= xe - xs + 1)
			n = xe - xs + 1;
		if (D_CDC && !(n == 1 && D_DC))
			AddCStr2(D_CDC, n);
		else if (D_DC) {
			for (i = n; i--;)
				AddCStr(D_DC);
		} else {
			RefreshLine(y, xs, xe, 0);
			/* UpdateLine(oml, y, xs, xe); */
			return;
		}
	} else {
		if (-n >= xe - xs + 1)
			n = -(xe - xs + 1);
		if (!D_insert) {
			if (D_CIC && !(n == -1 && D_IC))
				AddCStr2(D_CIC, -n);
			else if (D_IC) {
				for (i = -n; i--;)
					AddCStr(D_IC);
			} else if (D_IM) {
				InsertMode(1);
				SetRendition(&mchar_null);
				SetBackColor(bce);
				for (i = -n; i--;)
					INSERTCHAR(' ');
				bce = 0;	/* all done */
			} else {
				/* UpdateLine(oml, y, xs, xe); */
				RefreshLine(y, xs, xe, 0);
				return;
			}
		} else {
			SetRendition(&mchar_null);
			SetBackColor(bce);
			for (i = -n; i--;)
				INSERTCHAR(' ');
			bce = 0;	/* all done */
		}
	}
	if (bce && !D_BE) {
		if (n > 0)
			ClearLine((struct mline *)0, y, xe - n + 1, xe, bce);
		else
			ClearLine((struct mline *)0, y, xs, xs - n - 1, bce);
	}
	if (D_lp_missing && y == D_bot) {
		if (n > 0)
			WriteLP(D_width - 1 - n, y);
		D_lp_missing = 0;
	}
}

void ScrollV(int xs, int ys, int xe, int ye, int n, int bce)
{
	int i;
	int up;
	int oldtop, oldbot;
	int alok, dlok, aldlfaster;
	int missy = 0;

	ASSERT(display);
	if (n == 0)
		return;
	if (n >= ye - ys + 1 || -n >= ye - ys + 1) {
		ClearArea(xs, ys, xs, xe, xe, ye, bce, 0);
		return;
	}
	if (xs > D_vpxmin || xe < D_vpxmax) {
		RefreshArea(xs, ys, xe, ye, 0);
		return;
	}

	if (D_lp_missing) {
		if (D_bot > ye || D_bot < ys)
			missy = D_bot;
		else {
			missy = D_bot - n;
			if (missy > ye || missy < ys)
				D_lp_missing = 0;
		}
	}

	up = 1;
	if (n < 0) {
		up = 0;
		n = -n;
	}
	if (n >= ye - ys + 1)
		n = ye - ys + 1;

	oldtop = D_top;
	oldbot = D_bot;
	if (ys < D_top || D_bot != ye)
		ChangeScrollRegion(ys, ye);
	alok = (D_AL || D_CAL || (ys >= D_top && ye == D_bot && up));
	dlok = (D_DL || D_CDL || (ys >= D_top && ye == D_bot && !up));
	if (D_top != ys && !(alok && dlok))
		ChangeScrollRegion(ys, ye);

	if (D_lp_missing && (oldbot != D_bot || (oldbot == D_bot && up && D_top == ys && D_bot == ye))) {
		WriteLP(D_width - 1, oldbot);
		if (oldbot == D_bot) {	/* have scrolled */
			if (--n == 0) {
/* XXX
	      ChangeScrollRegion(oldtop, oldbot);
*/
				if (bce && !D_BE)
					ClearLine((struct mline *)0, ye, xs, xe, bce);
				return;
			}
		}
	}

	if (D_UT)
		SetRendition(&mchar_null);
	if (D_BE)
		SetBackColor(bce);

	aldlfaster = (n > 1 && ys >= D_top && ye == D_bot && ((up && D_CDL) || (!up && D_CAL)));

	if ((up || D_SR) && D_top == ys && D_bot == ye && !aldlfaster) {
		if (up) {
			GotoPos(0, ye);
			for (i = n; i-- > 0;)
				AddCStr(D_NL);	/* was SF, I think NL is faster */
		} else {
			GotoPos(0, ys);
			for (i = n; i-- > 0;)
				AddCStr(D_SR);
		}
	} else if (alok && dlok) {
		if (up || ye != D_bot) {
			GotoPos(0, up ? ys : ye + 1 - n);
			if (D_CDL && !(n == 1 && D_DL))
				AddCStr2(D_CDL, n);
			else
				for (i = n; i--;)
					AddCStr(D_DL);
		}
		if (!up || ye != D_bot) {
			GotoPos(0, up ? ye + 1 - n : ys);
			if (D_CAL && !(n == 1 && D_AL))
				AddCStr2(D_CAL, n);
			else
				for (i = n; i--;)
					AddCStr(D_AL);
		}
	} else {
		RefreshArea(xs, ys, xe, ye, 0);
		return;
	}
	if (bce && !D_BE) {
		if (up)
			ClearArea(xs, ye - n + 1, xs, xe, xe, ye, bce, 0);
		else
			ClearArea(xs, ys, xs, xe, xe, ys + n - 1, bce, 0);
	}
	if (D_lp_missing && missy != D_bot)
		WriteLP(D_width - 1, missy);
/* XXX
  ChangeScrollRegion(oldtop, oldbot);
  if (D_lp_missing && missy != D_bot)
    WriteLP(D_width - 1, missy);
*/
}

void SetAttr(register int new)
{
	register int i, j, old, typ;

	if (!display || (old = D_rend.attr) == new)
		return;
	D_col16change = (old ^ new) & (A_BFG | A_BBG);
	new ^= D_col16change;
	if (old == new)
		return;
#if defined(TERMINFO) && defined(USE_SGR)
	if (D_SA) {
		char *tparm();
		SetFont(ASCII);
		ospeed = D_dospeed;
		tputs(tparm(D_SA, new & A_SO, new & A_US, new & A_RV, new & A_BL,
			    new & A_DI, new & A_BD, 0, 0, 0), 1, DoAddChar);
		D_rend.attr = new;
		D_atyp = 0;
		if (D_hascolor)
			rend_setdefault(&D_rend);
		return;
	}
#endif
	D_rend.attr = new;
	typ = D_atyp;
	if ((new & old) != old) {
		if ((typ & ATYP_U))
			AddCStr(D_UE);
		if ((typ & ATYP_S))
			AddCStr(D_SE);
		if ((typ & ATYP_M)) {
			AddCStr(D_ME);
			/* ansi attrib handling: \E[m resets color, too */
			if (D_hascolor)
				rend_setdefault(&D_rend);
			if (!D_CG0) {
				/* D_ME may also reset the alternate charset */
				D_rend.font = 0;
				D_realfont = 0;
			}
		}
		old = 0;
		typ = 0;
	}
	old ^= new;
	for (i = 0, j = 1; old && i < NATTR; i++, j <<= 1) {
		if ((old & j) == 0)
			continue;
		old ^= j;
		if (D_attrtab[i]) {
			AddCStr(D_attrtab[i]);
			typ |= D_attrtyp[i];
		}
	}
	D_atyp = typ;
}

void SetFont(int new)
{
	int old = D_rend.font;
	if (!display || old == new)
		return;
	D_rend.font = new;
	if (D_encoding && CanEncodeFont(D_encoding, new))
		return;
	if (new == D_realfont)
		return;
	D_realfont = new;
	if (D_xtable && D_xtable[(int)(unsigned char)new] && D_xtable[(int)(unsigned char)new][256]) {
		AddCStr(D_xtable[(int)(unsigned char)new][256]);
		return;
	}

	if (!D_CG0 && new != '0') {
		new = ASCII;
		if (old == new)
			return;
	}

	if (new == ASCII)
		AddCStr(D_CE0);
	else if (new < ' ') {
		AddStr("\033$");
		if (new > 2)
			AddChar('(');
		AddChar(new + '@');
	} else
		AddCStr2(D_CS0, new);
}

int color256to16(int jj)
{
	int min, max;
	int r, g, b;

	if (jj >= 232) {
		jj = (jj - 232) / 6;
		jj = (jj & 1) << 3 | (jj & 2 ? 7 : 0);
	} else if (jj >= 16) {
		jj -= 16;
		r = jj / 36;
		g = (jj / 6) % 6;
		b = jj % 6;
		min = r < g ? (r < b ? r : b) : (g < b ? g : b);
		max = r > g ? (r > b ? r : b) : (g > b ? g : b);
		if (min == max)
			jj = ((max + 1) & 2) << 2 | ((max + 1) & 4 ? 7 : 0);
		else
			jj = (b - min) / (max - min) << 2 | (g - min) / (max - min) << 1 | (r -
											    min) / (max - min) | (max >
														  3 ? 8
														  : 0);
	}
	return jj;
}

int color256to88(int jj)
{
	int r, g, b;

	if (jj >= 232)
		return (jj - 232) / 3 + 80;
	if (jj >= 16) {
		jj -= 16;
		r = jj / 36;
		g = (jj / 6) % 6;
		b = jj % 6;
		return ((r + 1) / 2) * 16 + ((g + 1) / 2) * 4 + ((b + 1) / 2) + 16;
	}
	return jj;
}

void SetColor(int f, int b)
{
	int of, ob;
	static unsigned char sftrans[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

	if (!display)
		return;

	of = rend_getfg(&D_rend);
	ob = rend_getbg(&D_rend);

	/* intense default not invented yet */
	if (f == 0x100)
		f = 0;
	if (b == 0x100)
		b = 0;
	debug("SetColor %d %d", coli2e(of), coli2e(ob));
	debug(" -> %d %d\n", coli2e(f), coli2e(b));
	debug("(%d %d", of, ob);
	debug(" -> %d %d)\n", f, b);

	if (!D_CAX && D_hascolor && ((f == 0 && f != of) || (b == 0 && b != ob))) {
		if (D_OP)
			AddCStr(D_OP);
		else {
			int oattr;
			oattr = D_rend.attr;
			AddCStr(D_ME ? D_ME : "\033[m");
			if (D_ME && !D_CG0) {
				/* D_ME may also reset the alternate charset */
				D_rend.font = 0;
				D_realfont = 0;
			}
			D_atyp = 0;
			D_rend.attr = 0;
			SetAttr(oattr);
		}
		of = ob = 0;
	}
	rend_setfg(&D_rend, f);
	rend_setbg(&D_rend, b);
	D_col16change = 0;
	if (!D_hascolor)
		return;
	f = f ? coli2e(f) : -1;
	b = b ? coli2e(b) : -1;
	of = of ? coli2e(of) : -1;
	ob = ob ? coli2e(ob) : -1;
	if (f != of && f > 15 && D_CCO != 256)
		f = D_CCO == 88 && D_CAF ? color256to88(f) : color256to16(f);
	if (f != of && f > 15 && D_CAF) {
		AddCStr2(D_CAF, f);
		of = f;
	}
	if (b != ob && b > 15 && D_CCO != 256)
		b = D_CCO == 88 && D_CAB ? color256to88(b) : color256to16(b);
	if (b != ob && b > 15 && D_CAB) {
		AddCStr2(D_CAB, b);
		ob = b;
	}
	if (f != of && f != (of | 8)) {
		if (f == -1)
			AddCStr("\033[39m");	/* works because AX is set */
		else if (D_CAF)
			AddCStr2(D_CAF, f & 7);
		else if (D_CSF)
			AddCStr2(D_CSF, sftrans[f & 7]);
	}
	if (b != ob && b != (ob | 8)) {
		if (b == -1)
			AddCStr("\033[49m");	/* works because AX is set */
		else if (D_CAB)
			AddCStr2(D_CAB, b & 7);
		else if (D_CSB)
			AddCStr2(D_CSB, sftrans[b & 7]);
	}
	if (f != of && D_CXT && (f & 8) != 0 && f != -1) {
#ifdef TERMINFO
		AddCStr2("\033[9%p1%dm", f & 7);
#else
		AddCStr2("\033[9%dm", f & 7);
#endif
	}
	if (b != ob && D_CXT && (b & 8) != 0 && b != -1) {
#ifdef TERMINFO
		AddCStr2("\033[10%p1%dm", b & 7);
#else
		AddCStr2("\033[10%dm", b & 7);
#endif
	}
}

static void SetBackColor(int new)
{
	if (!display)
		return;
	SetColor(rend_getfg(&D_rend), new);
}

void SetRendition(struct mchar *mc)
{
	if (!display)
		return;
	if (nattr2color && D_hascolor && (mc->attr & nattr2color) != 0) {
		static struct mchar mmc;
		int i;
		mmc = *mc;
		/*
		for (i = 0; i < 8; i++)
			if (attr2color[i] && (mc->attr & (1 << i)) != 0) {
				if (mc->color == 0 && attr2color[i][3])
					ApplyAttrColor(attr2color[i][3], &mmc);
				else if ((mc->color & 0x0f) == 0 && attr2color[i][2])
					ApplyAttrColor(attr2color[i][2], &mmc);
				else if ((mc->color & 0xf0) == 0 && attr2color[i][1])
					ApplyAttrColor(attr2color[i][1], &mmc);
				else
					ApplyAttrColor(attr2color[i][0], &mmc);
			}*/
		mc = &mmc;
		debug("SetRendition: mapped to %02x %02x\n", (unsigned char)mc->attr, 0x99 - (unsigned char)mc->color);
	}
	if (D_hascolor && D_CC8 && (mc->attr & (A_BFG | A_BBG))) {
		int a = mc->attr;
		if ((mc->attr & A_BFG) && D_MD)
			a |= A_BD;
		if ((mc->attr & A_BBG) && D_MB)
			a |= A_BL;
		if (D_rend.attr != a)
			SetAttr(a);
	} else if (D_rend.attr != mc->attr)
		SetAttr(mc->attr);

	if (D_rend.colorbg != mc->colorbg || D_rend.colorfg != mc->colorfg || D_col16change)
		SetColor(rend_getfg(mc), rend_getbg(mc));
	if (D_rend.font != mc->font)
		SetFont(mc->font);
	if (D_encoding == UTF8)
		D_rend.fontx = mc->fontx;
}

void SetRenditionMline(struct mline *ml, int x)
{
	if (!display)
		return;
	if (nattr2color && D_hascolor && (ml->attr[x] & nattr2color) != 0) {
		struct mchar mc;
		copy_mline2mchar(&mc, ml, x);
		SetRendition(&mc);
		return;
	}
	if (D_hascolor && D_CC8 && (ml->attr[x] & (A_BFG | A_BBG))) {
		int a = ml->attr[x];
		if ((ml->attr[x] & A_BFG) && D_MD)
			a |= A_BD;
		if ((ml->attr[x] & A_BBG) && D_MB)
			a |= A_BL;
		if (D_rend.attr != a)
			SetAttr(a);
	} else if (D_rend.attr != ml->attr[x])
		SetAttr(ml->attr[x]);
	if (D_rend.colorbg != ml->colorbg[x]
	    || D_rend.colorfg != ml->colorfg[x]
	    || D_col16change) {
		struct mchar mc;
		copy_mline2mchar(&mc, ml, x);
		SetColor(rend_getfg(&mc), rend_getbg(&mc));
	}
	if (D_rend.font != ml->font[x])
		SetFont(ml->font[x]);
	if (D_encoding == UTF8)
		D_rend.fontx = ml->fontx[x];
}

void MakeStatus(char *msg)
{
	register char *s, *t;
	register int max;

	if (!display)
		return;

	if (D_blocked)
		return;
	if (!D_tcinited) {
		debug("tc not inited, just writing msg\n");
		if (D_processinputdata)
			return;	/* XXX: better */
		AddStr(msg);
		AddStr("\r\n");
		Flush(0);
		return;
	}
	if (!use_hardstatus || !D_HS) {
		max = D_width;
		if (D_CLP == 0)
			max--;
	} else
		max = D_WS > 0 ? D_WS : (D_width - !D_CLP);
	if (D_status) {
		/* same message? */
		if (strcmp(msg, D_status_lastmsg) == 0) {
			debug("same message - increase timeout");
			if (!D_status_obufpos)
				SetTimeout(&D_statusev, MsgWait);
			return;
		}
		RemoveStatusMinWait();
	}
	for (s = t = msg; *s && t - msg < max; ++s)
		if (*s == BELL)
			AddCStr(D_BL);
		else if ((unsigned char)*s >= ' ' && *s != 0177)
			*t++ = *s;
	*t = '\0';
	if (t == msg)
		return;
	if (t - msg >= D_status_buflen) {
		char *buf;
		if (D_status_lastmsg)
			buf = realloc(D_status_lastmsg, t - msg + 1);
		else
			buf = malloc(t - msg + 1);
		if (buf) {
			D_status_lastmsg = buf;
			D_status_buflen = t - msg + 1;
		}
	}
	if (t - msg < D_status_buflen)
		strncpy(D_status_lastmsg, msg, D_status_buflen);
	D_status_len = t - msg;
	D_status_lastx = D_x;
	D_status_lasty = D_y;
	if (!use_hardstatus || D_has_hstatus == HSTATUS_IGNORE || D_has_hstatus == HSTATUS_MESSAGE) {
		D_status = STATUS_ON_WIN;
		debug("using STATLINE %d\n", STATLINE);
		GotoPos(0, STATLINE);
		SetRendition(&mchar_so);
		InsertMode(0);
		AddStr(msg);
		if (D_status_len < max) {
			/* Wayne Davison: add extra space for readability */
			D_status_len++;
			SetRendition(&mchar_null);
			AddChar(' ');
			if (D_status_len < max) {
				D_status_len++;
				AddChar(' ');
				AddChar('\b');
			}
			AddChar('\b');
		}
		D_x = -1;
	} else {
		D_status = STATUS_ON_HS;
		ShowHStatus(msg);
	}

	D_status_obufpos = D_obufp - D_obuf;
	ASSERT(D_status_obufpos > 0);

	if (D_status == STATUS_ON_WIN) {
		struct display *olddisplay = display;
		struct layer *oldflayer = flayer;

		/* this is copied over from RemoveStatus() */
		D_status = 0;
		GotoPos(0, STATLINE);
		RefreshLine(STATLINE, 0, D_status_len - 1, 0);
		GotoPos(D_status_lastx, D_status_lasty);
		flayer = D_forecv ? D_forecv->c_layer : 0;
		if (flayer)
			LaySetCursor();
		display = olddisplay;
		flayer = oldflayer;
		D_status = STATUS_ON_WIN;
	}
}

void RemoveStatus()
{
	struct display *olddisplay;
	struct layer *oldflayer;
	int where;

	if (!display)
		return;
	if (!(where = D_status))
		return;

	debug("RemoveStatus\n");
	if (D_status_obuffree >= 0) {
		D_obuflen = D_status_obuflen;
		D_obuffree = D_status_obuffree;
		D_status_obuffree = -1;
	}
	D_status = 0;
	D_status_obufpos = 0;
	D_status_bell = 0;
	evdeq(&D_statusev);
	olddisplay = display;
	oldflayer = flayer;
	if (where == STATUS_ON_WIN) {
		if (captionalways || (D_canvas.c_slperp && D_canvas.c_slperp->c_slnext)) {
			GotoPos(0, STATLINE);
			RefreshLine(STATLINE, 0, D_status_len - 1, 0);
			GotoPos(D_status_lastx, D_status_lasty);
		}
	} else
		RefreshHStatus();
	flayer = D_forecv ? D_forecv->c_layer : 0;
	if (flayer)
		LaySetCursor();
	display = olddisplay;
	flayer = oldflayer;
}

/* Remove the status but make sure that it is seen for MsgMinWait ms */
static void RemoveStatusMinWait()
{
	/* XXX: should flush output first if D_status_obufpos is set */
	if (!D_status_bell && !D_status_obufpos) {
		struct timeval now;
		int ti;
		gettimeofday(&now, NULL);
		ti = (now.tv_sec - D_status_time.tv_sec) * 1000 + (now.tv_usec - D_status_time.tv_usec) / 1000;
		if (ti < MsgMinWait)
			DisplaySleep1000(MsgMinWait - ti, 0);
	}
	RemoveStatus();
}

static int strlen_onscreen(unsigned char *c, unsigned char *end)
{
	int len = 0;
	while (*c && (!end || c < end)) {
		int v, dec = 0;
		do {
			v = FromUtf8(*c++, &dec);
			if (v == -2)
				c--;
		}
		while (v < 0 && (!end || c < end));
		if (!utf8_iscomb(v)) {
			if (utf8_isdouble(v))
				len++;
			len++;
		}
	}

	return len;
}

static int PrePutWinMsg(char *s, int start, int max)
{
	/* Avoid double-encoding problem for a UTF-8 message on a UTF-8 locale.
	   Ideally, this would not be necessary. But fixing it the Right Way will
	   probably take way more time. So this will have to do for now. */
	if (D_encoding == UTF8) {
		int chars = strlen_onscreen((unsigned char *)(s + start), (unsigned char *)(s + max));
		D_encoding = 0;
		PutWinMsg(s, start, max + ((max - start) - chars));	/* Multibyte count */
		D_encoding = UTF8;
		D_x -= (max - chars);	/* Yak! But this is necessary to count for
					   the fact that not every byte represents a
					   character. */
		return start + chars;
	} else {
		PutWinMsg(s, start, max);
		return max;
	}
}

/* refresh the display's hstatus line */
void ShowHStatus(char *str)
{
	int l, ox, oy, max;

	if (D_status == STATUS_ON_WIN && D_has_hstatus == HSTATUS_LASTLINE && STATLINE == D_height - 1)
		return;		/* sorry, in use */
	if (D_blocked)
		return;

	if (D_HS && D_has_hstatus == HSTATUS_HS) {
		if (!D_hstatus && (str == 0 || *str == 0))
			return;
		debug("ShowHStatus: using HS\n");
		SetRendition(&mchar_null);
		InsertMode(0);
		if (D_hstatus)
			AddCStr(D_DS);
		D_hstatus = 0;
		if (str == 0 || *str == 0)
			return;
		AddCStr2(D_TS, 0);
		max = D_WS > 0 ? D_WS : (D_width - !D_CLP);
		if ((int)strlen(str) > max)
			AddStrn(str, max);
		else
			AddStr(str);
		AddCStr(D_FS);
		D_hstatus = 1;
	} else if (D_has_hstatus == HSTATUS_LASTLINE) {
		debug("ShowHStatus: using last line\n");
		ox = D_x;
		oy = D_y;
		str = str ? str : "";
		l = strlen(str);
		if (l > D_width)
			l = D_width;
		GotoPos(0, D_height - 1);
		SetRendition(captionalways || D_cvlist == 0 || D_cvlist->c_next ? &mchar_null : &mchar_so);
		l = PrePutWinMsg(str, 0, l);
		if (!captionalways && D_cvlist && !D_cvlist->c_next)
			while (l++ < D_width)
				PUTCHARLP(' ');
		if (l < D_width)
			ClearArea(l, D_height - 1, l, D_width - 1, D_width - 1, D_height - 1, 0, 0);
		if (ox != -1 && oy != -1)
			GotoPos(ox, oy);
		D_hstatus = *str ? 1 : 0;
		SetRendition(&mchar_null);
	} else if (D_has_hstatus == HSTATUS_FIRSTLINE) {
		debug("ShowHStatus: using first line\n");
		ox = D_x;
		oy = D_y;
		str = str ? str : "";
		l = strlen(str);
		if (l > D_width)
			l = D_width;
		GotoPos(0, 0);
		SetRendition(captionalways || D_cvlist == 0 || D_cvlist->c_next ? &mchar_null : &mchar_so);
		l = PrePutWinMsg(str, 0, l);
		if (!captionalways || (D_cvlist && !D_cvlist->c_next))
			while (l++ < D_width)
				PUTCHARLP(' ');
		if (l < D_width)
			ClearArea(l, 0, l, D_width - 1, D_width - 1, 0, 0, 0);
		if (ox != -1 && oy != -1)
			GotoPos(ox, oy);
		D_hstatus = *str ? 1 : 0;
		SetRendition(&mchar_null);
	} else if (str && *str && D_has_hstatus == HSTATUS_MESSAGE) {
		debug("ShowHStatus: using message\n");
		Msg(0, "%s", str);
	}
}

/*
 *  Refreshes the harstatus of the fore window. Shouldn't be here...
 */
void RefreshHStatus()
{
	char *buf;
#ifdef UTF8
	int extrabytes = strlen(hstatusstring) - strlen_onscreen(hstatusstring, NULL);
#else
	int extrabytes = 0;
#endif
	evdeq(&D_hstatusev);
	if (D_status == STATUS_ON_HS)
		return;
	buf =
	    MakeWinMsgEv(hstatusstring, D_fore, '%',
			 (D_HS && D_has_hstatus == HSTATUS_HS
			  && D_WS > 0) ? D_WS : D_width - !D_CLP + extrabytes, &D_hstatusev, 0);
	if (buf && *buf) {
		ShowHStatus(buf);
		if (D_has_hstatus != HSTATUS_IGNORE && D_hstatusev.timeout.tv_sec)
			evenq(&D_hstatusev);
	} else
		ShowHStatus((char *)0);
}

/*********************************************************************/
/*
 *  Here come the routines that refresh an arbitrary part of the screen.
 */

void RefreshAll(int isblank)
{
	struct canvas *cv;

	ASSERT(display);
	debug("Signalling full refresh!\n");
	for (cv = D_cvlist; cv; cv = cv->c_next) {
		CV_CALL(cv, LayRedisplayLine(-1, -1, -1, isblank));
		display = cv->c_display;	/* just in case! */
	}
	RefreshArea(0, 0, D_width - 1, D_height - 1, isblank);
}

void RefreshArea(int xs, int ys, int xe, int ye, int isblank)
{
	int y;
	ASSERT(display);
	debug("Refresh Area: %d,%d", xs, ys);
	debug(" - %d,%d (isblank=%d)\n", xe, ye, isblank);
	if (!isblank && xs == 0 && xe == D_width - 1 && ye == D_height - 1 && (ys == 0 || D_CD)) {
		ClearArea(xs, ys, xs, xe, xe, ye, 0, 0);
		isblank = 1;
	}
	for (y = ys; y <= ye; y++)
		RefreshLine(y, xs, xe, isblank);
}

void RefreshLine(int y, int from, int to, int isblank)
{
	struct viewport *vp, *lvp;
	struct canvas *cv, *lcv, *cvlist, *cvlnext;
	struct layer *oldflayer;
	int xx, yy, l;
	char *buf;
	struct win *p;

	ASSERT(display);

	debug("RefreshLine %d %d", y, from);
	debug(" %d %d\n", to, isblank);

	if (D_status == STATUS_ON_WIN && y == STATLINE) {
		if (to >= D_status_len)
			D_status_len = to + 1;
		return;		/* can't refresh status */
	}

	if (isblank == 0 && D_CE && to == D_width - 1 && from < to && D_status != STATUS_ON_HS) {
		GotoPos(from, y);
		if (D_UT || D_BE)
			SetRendition(&mchar_null);
		AddCStr(D_CE);
		isblank = 1;
	}

	if ((y == D_height - 1 && D_has_hstatus == HSTATUS_LASTLINE) || (y == 0 && D_has_hstatus == HSTATUS_FIRSTLINE)) {
		RefreshHStatus();
		return;
	}

	while (from <= to) {
		lcv = 0;
		lvp = 0;
		for (cv = display->d_cvlist; cv; cv = cv->c_next) {
			if (y == cv->c_ye + 1 && from >= cv->c_xs && from <= cv->c_xe) {
#ifdef UTF8
				int extrabytes = strlen(captionstring) - strlen_onscreen(captionstring, NULL);
#else
				int extrabytes = 0;
#endif
				p = Layer2Window(cv->c_layer);
				buf =
				    MakeWinMsgEv(captionstring, p, '%',
						 cv->c_xe - cv->c_xs + (cv->c_xe + 1 < D_width
									|| D_CLP) + extrabytes, &cv->c_captev, 0);
				if (cv->c_captev.timeout.tv_sec)
					evenq(&cv->c_captev);
				xx = to > cv->c_xe ? cv->c_xe : to;
				l = strlen(buf);
				GotoPos(from, y);
				SetRendition(&mchar_so);
				if (l > xx - cv->c_xs + 1)
					l = xx - cv->c_xs + 1;
				l = PrePutWinMsg(buf, from - cv->c_xs, l + extrabytes);
				from = cv->c_xs + l;
				for (; from <= xx; from++)
					PUTCHARLP(' ');
				break;
			}
			if (from == cv->c_xe + 1 && y >= cv->c_ys && y <= cv->c_ye + 1) {
				GotoPos(from, y);
				SetRendition(&mchar_so);
				PUTCHARLP(' ');
				from++;
				break;
			}
			if (y < cv->c_ys || y > cv->c_ye || to < cv->c_xs || from > cv->c_xe)
				continue;
			debug("- canvas hit: %d %d", cv->c_xs, cv->c_ys);
			debug("  %d %d\n", cv->c_xe, cv->c_ye);
			for (vp = cv->c_vplist; vp; vp = vp->v_next) {
				debug("  - vp: %d %d", vp->v_xs, vp->v_ys);
				debug("  %d %d\n", vp->v_xe, vp->v_ye);
				/* find leftmost overlapping vp */
				if (y >= vp->v_ys && y <= vp->v_ye && from <= vp->v_xe && to >= vp->v_xs
				    && (lvp == 0 || lvp->v_xs > vp->v_xs)) {
					lcv = cv;
					lvp = vp;
				}
			}
		}
		if (cv)
			continue;	/* we advanced from */
		if (lvp == 0)
			break;
		if (from < lvp->v_xs) {
			if (!isblank)
				DisplayLine(&mline_null, &mline_blank, y, from, lvp->v_xs - 1);
			from = lvp->v_xs;
		}

		/* call LayRedisplayLine on canvas lcv viewport lvp */
		yy = y - lvp->v_yoff;
		xx = to < lvp->v_xe ? to : lvp->v_xe;

		if (lcv->c_layer && lcv->c_xoff + lcv->c_layer->l_width == from) {
			GotoPos(from, y);
			SetRendition(&mchar_blank);
			PUTCHARLP('|');
			from++;
		}
		if (lcv->c_layer && yy == lcv->c_layer->l_height) {
			GotoPos(from, y);
			SetRendition(&mchar_blank);
			while (from <= lvp->v_xe && from - lvp->v_xoff < lcv->c_layer->l_width) {
				PUTCHARLP('-');
				from++;
			}
			if (from >= lvp->v_xe + 1)
				continue;
		}
		if (lcv->c_layer == 0 || yy >= lcv->c_layer->l_height || from - lvp->v_xoff >= lcv->c_layer->l_width) {
			if (!isblank)
				DisplayLine(&mline_null, &mline_blank, y, from, lvp->v_xe);
			from = lvp->v_xe + 1;
			continue;
		}

		if (xx - lvp->v_xoff >= lcv->c_layer->l_width)
			xx = lcv->c_layer->l_width + lvp->v_xoff - 1;
		oldflayer = flayer;
		flayer = lcv->c_layer;
		cvlist = flayer->l_cvlist;
		cvlnext = lcv->c_lnext;
		flayer->l_cvlist = lcv;
		lcv->c_lnext = 0;
		LayRedisplayLine(yy, from - lvp->v_xoff, xx - lvp->v_xoff, isblank);
		flayer->l_cvlist = cvlist;
		lcv->c_lnext = cvlnext;
		flayer = oldflayer;

		from = xx + 1;
	}
	if (!isblank && from <= to)
		DisplayLine(&mline_null, &mline_blank, y, from, to);
}

/*********************************************************************/

/* clear lp_missing by writing the char on the screen. The
 * position must be safe.
 */
static void WriteLP(int x2, int y2)
{
	struct mchar oldrend;

	ASSERT(display);
	ASSERT(D_lp_missing);
	oldrend = D_rend;
	debug("WriteLP(%d,%d)\n", x2, y2);
	if (D_lpchar.mbcs) {
		if (x2 > 0)
			x2--;
		else
			D_lpchar = mchar_blank;
	}
	/* Can't use PutChar */
	GotoPos(x2, y2);
	SetRendition(&D_lpchar);
	PUTCHAR(D_lpchar.image);
	if (D_lpchar.mbcs)
		PUTCHAR(D_lpchar.mbcs);
	D_lp_missing = 0;
	SetRendition(&oldrend);
}

void ClearLine(struct mline *oml, int y, int from, int to, int bce)
{
	int x;
	struct mchar bcechar;

	debug("ClearLine %d,%d-%d\n", y, from, to);
	if (D_UT)		/* Safe to erase ? */
		SetRendition(&mchar_null);
	if (D_BE)
		SetBackColor(bce);
	if (from == 0 && D_CB && (to != D_width - 1 || (D_x == to && D_y == y)) && (!bce || D_BE)) {
		GotoPos(to, y);
		AddCStr(D_CB);
		return;
	}
	if (to == D_width - 1 && D_CE && (!bce || D_BE)) {
		GotoPos(from, y);
		AddCStr(D_CE);
		return;
	}
	if (oml == 0)
		oml = &mline_null;
	if (!bce) {
		DisplayLine(oml, &mline_blank, y, from, to);
		return;
	}
	bcechar = mchar_null;
	rend_setbg(&bcechar, bce);
	for (x = from; x <= to; x++)
		copy_mchar2mline(&bcechar, &mline_old, x);
	DisplayLine(oml, &mline_old, y, from, to);
}

void DisplayLine(struct mline *oml, struct mline *ml, int y, int from, int to)
{
	register int x;
	int last2flag = 0, delete_lp = 0;

	ASSERT(display);
	ASSERT(y >= 0 && y < D_height);
	ASSERT(from >= 0 && from < D_width);
	ASSERT(to >= 0 && to < D_width);
	if (!D_CLP && y == D_bot && to == D_width - 1) {
		if (D_lp_missing || !cmp_mline(oml, ml, to)) {
			if ((D_IC || D_IM) && from < to && !dw_left(ml, to, D_encoding)) {
				last2flag = 1;
				D_lp_missing = 0;
				to--;
			} else {
				delete_lp = !cmp_mchar_mline(&mchar_blank, oml, to) && (D_CE || D_DC || D_CDC);
				D_lp_missing = !cmp_mchar_mline(&mchar_blank, ml, to);
				copy_mline2mchar(&D_lpchar, ml, to);
			}
		}
		to--;
	}
	if (D_mbcs) {
		/* finish dw-char (can happen after a wrap) */
		debug("DisplayLine finishing kanji\n");
		SetRenditionMline(ml, from);
		PUTCHAR(ml->image[from]);
		from++;
	}
	for (x = from; x <= to; x++) {
		{
			if (ml != NULL && (x < to || x != D_width - 1 || ml->image[x + 1]))
				if (cmp_mline(oml, ml, x))
					continue;
			GotoPos(x, y);
		}
		if (dw_right(ml, x, D_encoding)) {
			if (x > 0) {
				x--;
			} else {
				x++;
			}
			debug("DisplayLine on right side of dw char- x now %d\n", x);
			GotoPos(x, y);
		}
		if (x == to && dw_left(ml, x, D_encoding))
			break;	/* don't start new kanji */
		SetRenditionMline(ml, x);
		PUTCHAR(ml->image[x]);
		if (dw_left(ml, x, D_encoding))
			PUTCHAR(ml->image[++x]);
	}
	if (last2flag) {
		GotoPos(x, y);
		SetRenditionMline(ml, x + 1);
		PUTCHAR(ml->image[x + 1]);
		GotoPos(x, y);
		SetRenditionMline(ml, x);
		INSERTCHAR(ml->image[x]);
	} else if (delete_lp) {
		if (D_UT)
			SetRendition(&mchar_null);
		if (D_DC)
			AddCStr(D_DC);
		else if (D_CDC)
			AddCStr2(D_CDC, 1);
		else if (D_CE)
			AddCStr(D_CE);
	}
}

void PutChar(struct mchar *c, int x, int y)
{
	GotoPos(x, y);
	SetRendition(c);
	PUTCHARLP(c->image);
	if (c->mbcs) {
		if (D_encoding == UTF8)
			D_rend.font = 0;
		PUTCHARLP(c->mbcs);
	}
}

void InsChar(struct mchar *c, int x, int xe, int y, struct mline *oml)
{
	GotoPos(x, y);
	if (y == D_bot && !D_CLP) {
		if (x == D_width - 1) {
			D_lp_missing = 1;
			D_lpchar = *c;
			return;
		}
		if (xe == D_width - 1)
			D_lp_missing = 0;
	}
	if (x == xe) {
		SetRendition(c);
		PUTCHARLP(c->image);
		return;
	}
	if (!(D_IC || D_CIC || D_IM) || xe != D_width - 1) {
		RefreshLine(y, x, xe, 0);
		GotoPos(x + 1, y);
		/* UpdateLine(oml, y, x, xe); */
		return;
	}
	InsertMode(1);
	if (!D_insert) {
		if (c->mbcs && D_IC)
			AddCStr(D_IC);
		if (D_IC)
			AddCStr(D_IC);
		else
			AddCStr2(D_CIC, c->mbcs ? 2 : 1);
	}
	SetRendition(c);
	RAW_PUTCHAR(c->image);
	if (c->mbcs) {
		if (D_encoding == UTF8)
			D_rend.font = 0;
		if (D_x == D_width - 1)
			PUTCHARLP(c->mbcs);
		else
			RAW_PUTCHAR(c->mbcs);
	}
}

void WrapChar(struct mchar *c, int x, int y, int xs, int ys, int xe, int ye, int ins)
{
	int bce;

	bce = rend_getbg(c);
	debug("WrapChar:");
	debug("  x %d  y %d", x, y);
	debug("  Dx %d  Dy %d", D_x, D_y);
	debug("  xs %d  ys %d", xs, ys);
	debug("  xe %d  ye %d ins %d\n", xe, ye, ins);
	if (xs != 0 || x != D_width || !D_AM) {
		if (y == ye)
			ScrollV(xs, ys, xe, ye, 1, bce);
		else if (y < D_height - 1)
			y++;
		if (ins)
			InsChar(c, xs, xe, y, 0);
		else
			PutChar(c, xs, y);
		return;
	}
	if (y == ye) {		/* we have to scroll */
		debug("- scrolling\n");
		ChangeScrollRegion(ys, ye);
		if (D_bot != y || D_x != D_width || (!bce && !D_BE)) {
			debug("- have to call ScrollV\n");
			ScrollV(xs, ys, xe, ye, 1, bce);
			y--;
		}
	} else if (y == D_bot)	/* remove unusable region? */
		ChangeScrollRegion(0, D_height - 1);
	if (D_x != D_width || D_y != y) {
		if (D_CLP && y >= 0)	/* don't even try if !LP */
			RefreshLine(y, D_width - 1, D_width - 1, 0);
		debug("- refresh last char -> x,y now %d,%d\n", D_x, D_y);
		if (D_x != D_width || D_y != y) {	/* sorry, no bonus */
			if (y == ye)
				ScrollV(xs, ys, xe, ye, 1, bce);
			GotoPos(xs, y == ye || y == D_height - 1 ? y : y + 1);
		}
	}
	debug("- writeing new char");
	if (y != ye && y < D_height - 1)
		y++;
	if (ins != D_insert)
		InsertMode(ins);
	if (ins && !D_insert) {
		InsChar(c, 0, xe, y, 0);
		debug(" -> done with insert (%d,%d)\n", D_x, D_y);
		return;
	}
	D_y = y;
	D_x = 0;
	SetRendition(c);
	RAW_PUTCHAR(c->image);
	if (c->mbcs) {
		if (D_encoding == UTF8)
			D_rend.font = 0;
		RAW_PUTCHAR(c->mbcs);
	}
	debug(" -> done (%d,%d)\n", D_x, D_y);
}

int ResizeDisplay(int wi, int he)
{
	ASSERT(display);
	debug("ResizeDisplay: to (%d,%d).\n", wi, he);
	if (D_width == wi && D_height == he) {
		debug("ResizeDisplay: No change\n");
		return 0;
	}
	if (D_width != wi && (D_height == he || !D_CWS) && D_CZ0 && (wi == Z0width || wi == Z1width)) {
		debug("ResizeDisplay: using Z0/Z1\n");
		AddCStr(wi == Z0width ? D_CZ0 : D_CZ1);
		ChangeScreenSize(wi, D_height, 0);
		return (he == D_height) ? 0 : -1;
	}
	if (D_CWS) {
		debug("ResizeDisplay: using WS\n");
		AddCStr(tgoto(D_CWS, wi, he));
		ChangeScreenSize(wi, he, 0);
		return 0;
	}
	return -1;
}

void ChangeScrollRegion(int newtop, int newbot)
{
	if (display == 0)
		return;
	if (newtop == newbot)
		return;		/* xterm etc can't do it */
	if (newtop == -1)
		newtop = 0;
	if (newbot == -1)
		newbot = D_height - 1;
	if (D_CS == 0) {
		D_top = 0;
		D_bot = D_height - 1;
		return;
	}
	if (D_top == newtop && D_bot == newbot)
		return;
	debug("ChangeScrollRegion: (%d - %d)\n", newtop, newbot);
	AddCStr(tgoto(D_CS, newbot, newtop));
	D_top = newtop;
	D_bot = newbot;
	D_y = D_x = -1;		/* Just in case... */
}

#define WT_FLAG "2"		/* change to "0" to set both title and icon */

void SetXtermOSC(int i, char *s)
{
	static char *oscs[][2] = {
		{WT_FLAG ";", "screen"},	/* set window title */
		{"20;", ""},	/* background */
		{"39;", "black"},	/* default foreground (black?) */
		{"49;", "white"}	/* default background (white?) */
	};

	ASSERT(display);
	if (!D_CXT)
		return;
	if (!s)
		s = "";
	if (!D_xtermosc[i] && !*s)
		return;
	if (i == 0 && !D_xtermosc[0])
		AddStr("\033[22;" WT_FLAG "t");	/* stack titles (xterm patch #251) */
	if (!*s)
		s = oscs[i][1];
	D_xtermosc[i] = 1;
	AddStr("\033]");
	AddStr(oscs[i][0]);
	AddStr(s);
	AddChar(7);
}

void ClearAllXtermOSC()
{
	int i;
	for (i = 3; i >= 0; i--)
		SetXtermOSC(i, 0);
	if (D_xtermosc[0])
		AddStr("\033[23;" WT_FLAG "t");	/* unstack titles (xterm patch #251) */
}

#undef WT_FLAG

/*
 *  Output buffering routines
 */

void AddStr(char *str)
{
	register char c;

	ASSERT(display);

	if (D_encoding == UTF8) {
		while ((c = *str++))
			AddUtf8((unsigned char)c);
		return;
	}
	while ((c = *str++))
		AddChar(c);
}

void AddStrn(char *str, int n)
{
	register char c;

	ASSERT(display);
	if (D_encoding == UTF8) {
		while ((c = *str++) && n-- > 0)
			AddUtf8((unsigned char)c);
	} else
		while ((c = *str++) && n-- > 0)
			AddChar(c);
	while (n-- > 0)
		AddChar(' ');
}

void Flush(int progress)
{
	register int l;
	int wr;
	register char *p;

	ASSERT(display);
	l = D_obufp - D_obuf;
	debug("Flush(): %d\n", l);
	if (l == 0)
		return;
	ASSERT(l + D_obuffree == D_obuflen);
	if (D_userfd < 0) {
		D_obuffree += l;
		D_obufp = D_obuf;
		return;
	}
	p = D_obuf;
	if (!progress) {
		if (fcntl(D_userfd, F_SETFL, 0))
			debug("Warning: BLOCK fcntl failed: %d\n", errno);
	}
	while (l) {
		if (progress) {
			fd_set w;
			FD_ZERO(&w);
			FD_SET(D_userfd, &w);
			struct timeval t;
			t.tv_sec = progress;
			t.tv_usec = 0;
			wr = select(FD_SETSIZE, (fd_set *) 0, &w, (fd_set *) 0, &t);
			if (wr == -1) {
				if (errno == EINTR)
					continue;
				debug("Warning: select failed: %d\n", errno);
				break;
			}
			if (wr == 0) {
				/* no progress after 3 seconds. sorry. */
				debug("Warning: no progress after %d seconds\n", progress);
				break;
			}
		}
		wr = write(D_userfd, p, l);
		if (wr <= 0) {
			if (errno == EINTR)
				continue;
			debug("Writing to display: %d\n", errno);
			break;
		}
		D_obuffree += wr;
		p += wr;
		l -= wr;
	}
	if (l)
		debug("Warning: Flush could not write %d bytes\n", l);
	D_obuffree += l;
	D_obufp = D_obuf;
	if (!progress) {
		if (fcntl(D_userfd, F_SETFL, FNBLOCK))
			debug("Warning: NBLOCK fcntl failed: %d\n", errno);
	}
	if (D_blocked == 1)
		D_blocked = 0;
	D_blocked_fuzz = 0;
}

void freetty()
{
	if (D_userfd >= 0)
		close(D_userfd);
	debug("did freetty %d\n", D_userfd);
	D_userfd = -1;
	D_obufp = 0;
	D_obuffree = 0;
	if (D_obuf)
		free(D_obuf);
	D_obuf = 0;
	D_obuflen = 0;
	D_obuflenmax = -D_obufmax;
	D_blocked = 0;
	D_blocked_fuzz = 0;
}

/*
 *  Asynchronous output routines by
 *  Tim MacKenzie (tym@dibbler.cs.monash.edu.au)
 */

void Resize_obuf()
{
	register int ind;

	ASSERT(display);
	if (D_status_obuffree >= 0) {
		ASSERT(D_obuffree == -1);
		RemoveStatusMinWait();
		if (--D_obuffree > 0)	/* redo AddChar decrement */
			return;
	}
	if (D_obuflen && D_obuf) {
		ind = D_obufp - D_obuf;
		D_obuflen += GRAIN;
		D_obuffree += GRAIN;
		D_obuf = realloc(D_obuf, D_obuflen);
	} else {
		ind = 0;
		D_obuflen = GRAIN;
		D_obuffree = GRAIN;
		D_obuf = malloc(D_obuflen);
	}
	if (!D_obuf)
		Panic(0, "Out of memory");
	D_obufp = D_obuf + ind;
	D_obuflenmax = D_obuflen - D_obufmax;
	debug("ResizeObuf: resized to %d\n", D_obuflen);
}

void DisplaySleep1000(int n, int eat)
{
	char buf;
	fd_set r;
	struct timeval t;

	if (n <= 0)
		return;
	if (!display) {
		debug("DisplaySleep has no display sigh\n");
		sleep1000(n);
		return;
	}
	t.tv_usec = (n % 1000) * 1000;
	t.tv_sec = n / 1000;
	FD_ZERO(&r);
	FD_SET(D_userfd, &r);
	if (select(FD_SETSIZE, &r, (fd_set *) 0, (fd_set *) 0, &t) > 0) {
		debug("display activity stopped sleep\n");
		if (eat)
			read(D_userfd, &buf, 1);
	}
	debug("DisplaySleep(%d) ending, eat was %d\n", n, eat);
}

void NukePending()
{				/* Nuke pending output in current display, clear screen */
	register int len;
	int oldtop = D_top, oldbot = D_bot;
	struct mchar oldrend;
	int oldkeypad = D_keypad, oldcursorkeys = D_cursorkeys;
	int oldcurvis = D_curvis;
	int oldmouse = D_mouse;

	oldrend = D_rend;
	len = D_obufp - D_obuf;
	debug("NukePending: nuking %d chars\n", len);

	/* Throw away any output that we can... */
#ifdef POSIX
	tcflush(D_userfd, TCOFLUSH);
#else
#ifdef TCFLSH
	(void)ioctl(D_userfd, TCFLSH, (char *)1);
#endif
#endif

	D_obufp = D_obuf;
	D_obuffree += len;
	D_top = D_bot = -1;
	AddCStr(D_IS);
	AddCStr(D_TI);
	/* Turn off all attributes. (Tim MacKenzie) */
	if (D_ME)
		AddCStr(D_ME);
	else {
		if (D_hascolor)
			AddStr("\033[m");	/* why is D_ME not set? */
		AddCStr(D_SE);
		AddCStr(D_UE);
	}
	/* Check for toggle */
	if (D_IM && strcmp(D_IM, D_EI))
		AddCStr(D_EI);
	D_insert = 0;
	/* Check for toggle */
	if (D_KS && strcmp(D_KS, D_KE))
		AddCStr(D_KS);
	if (D_CCS && strcmp(D_CCS, D_CCE))
		AddCStr(D_CCS);
	AddCStr(D_CE0);
	D_rend = mchar_null;
	D_atyp = 0;
	AddCStr(D_DS);
	D_hstatus = 0;
	AddCStr(D_VE);
	D_curvis = 0;
	ChangeScrollRegion(oldtop, oldbot);
	SetRendition(&oldrend);
	KeypadMode(oldkeypad);
	CursorkeysMode(oldcursorkeys);
	CursorVisibility(oldcurvis);
	MouseMode(oldmouse);
	if (D_CWS) {
		debug("ResizeDisplay: using WS\n");
		AddCStr(tgoto(D_CWS, D_width, D_height));
	} else if (D_CZ0 && (D_width == Z0width || D_width == Z1width)) {
		debug("ResizeDisplay: using Z0/Z1\n");
		AddCStr(D_width == Z0width ? D_CZ0 : D_CZ1);
	}
}

/* linux' select can't handle flow control, so wait 100ms if
 * we get EAGAIN
 */
static void disp_writeev_eagain(struct event *ev, char *data)
{
	display = (struct display *)data;
	evdeq(&D_writeev);
	D_writeev.type = EV_WRITE;
	D_writeev.handler = disp_writeev_fn;
	evenq(&D_writeev);
}

static void disp_writeev_fn(struct event *ev, char *data)
{
	int len, size = OUTPUT_BLOCK_SIZE;

	display = (struct display *)data;
	len = D_obufp - D_obuf;
	if (len < size)
		size = len;
	if (D_status_obufpos && size > D_status_obufpos)
		size = D_status_obufpos;
	ASSERT(len >= 0);
	size = write(D_userfd, D_obuf, size);
	if (size >= 0) {
		len -= size;
		if (len) {
			memmove(D_obuf, D_obuf + size, len);
			debug("ASYNC: wrote %d - remaining %d\n", size, len);
		}
		D_obufp -= size;
		D_obuffree += size;
		if (D_status_obufpos) {
			D_status_obufpos -= size;
			if (!D_status_obufpos) {
				debug("finished writing the status message\n");
				/* we're finished displaying the message! */
				if (D_status == STATUS_ON_WIN) {
					/* setup continue trigger */
					D_status_obuflen = D_obuflen;
					D_status_obuffree = D_obuffree;
					/* setting obbuffree to 0 will make AddChar call
					 * ResizeObuf */
					D_obuffree = D_obuflen = 0;
				}
				gettimeofday(&D_status_time, NULL);
				SetTimeout(&D_statusev, MsgWait);
				evenq(&D_statusev);
			}
		}
		if (D_blocked_fuzz) {
			D_blocked_fuzz -= size;
			if (D_blocked_fuzz < 0)
				D_blocked_fuzz = 0;
		}
		if (D_blockedev.queued) {
			if (D_obufp - D_obuf > D_obufmax / 2) {
				debug("%s: resetting timeout to %g secs\n", D_usertty, D_nonblock / 1000.);
				SetTimeout(&D_blockedev, D_nonblock);
			} else {
				debug("%s: deleting blocked timeout\n", D_usertty);
				evdeq(&D_blockedev);
			}
		}
		if (D_blocked == 1 && D_obuf == D_obufp) {
			/* empty again, restart output */
			debug("%s: buffer empty, unblocking\n", D_usertty);
			D_blocked = 0;
			Activate(D_fore ? D_fore->w_norefresh : 0);
			D_blocked_fuzz = D_obufp - D_obuf;
		}
	} else {
		/* linux flow control is badly broken */
		if (errno == EAGAIN) {
			evdeq(&D_writeev);
			D_writeev.type = EV_TIMEOUT;
			D_writeev.handler = disp_writeev_eagain;
			SetTimeout(&D_writeev, 100);
			evenq(&D_writeev);
		}
		if (errno != EINTR && errno != EAGAIN)
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
			if (errno != EWOULDBLOCK)
#endif
				Msg(errno, "Error writing output to display");
	}
}

static void disp_readev_fn(struct event *ev, char *data)
{
	int size;
	char buf[IOSIZE];
	struct canvas *cv;

	display = (struct display *)data;

	/* Hmmmm... a bit ugly... */
	if (D_forecv)
		for (cv = D_forecv->c_layer->l_cvlist; cv; cv = cv->c_lnext) {
			display = cv->c_display;
			if (D_status == STATUS_ON_WIN)
				RemoveStatus();
		}

	display = (struct display *)data;
	if (D_fore == 0)
		size = IOSIZE;
	else {
		if (W_UWP(D_fore))
			size = sizeof(D_fore->w_pwin->p_inbuf) - D_fore->w_pwin->p_inlen;
		else
			size = sizeof(D_fore->w_inbuf) - D_fore->w_inlen;
	}

	if (size > IOSIZE)
		size = IOSIZE;
	if (size <= 0)
		size = 1;	/* Always allow one char for command keys */

	size = read(D_userfd, buf, size);
	if (size < 0) {
		if (errno == EINTR || errno == EAGAIN)
			return;
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
		if (errno == EWOULDBLOCK)
			return;
#endif
		debug("Read error: %d - hangup!\n", errno);
		Hangup();
		sleep(1);
		return;
	} else if (size == 0) {
		debug("Found EOF - hangup!\n");
		Hangup();
		sleep(1);
		return;
	}
	if (D_blocked == 4) {
		D_blocked = 0;
		KillBlanker();
		Activate(D_fore ? D_fore->w_norefresh : 0);
		ResetIdle();
		return;
	}
	if (D_blocked > 1) {	/* 2, 3 */
		char *bufp;
		struct win *p;

		flayer = 0;
		for (p = windows; p; p = p->w_next)
			if (p->w_zdisplay == display) {
				flayer = &p->w_layer;
				bufp = buf;
				while (size > 0)
					LayProcess(&bufp, &size);
				return;
			}
		debug("zmodem window gone, deblocking display");
		zmodem_abort(0, display);
	}
	if (idletimo > 0)
		ResetIdle();
	if (D_fore)
		D_fore->w_lastdisp = display;
	if (D_mouse && D_forecv) {
		unsigned char *bp = (unsigned char *)buf;
		int x, y, i = size;

		/* XXX this assumes that the string is read in as a whole... */
		for (i = size; i > 0; i--, bp++) {
			if (i > 5 && bp[0] == 033 && bp[1] == '[' && bp[2] == 'M') {
				bp++;
				i--;
			} else if (i < 5 || bp[0] != 0233 || bp[1] != 'M')
				continue;
			x = bp[3] - 33;
			y = bp[4] - 33;
			if (x >= D_forecv->c_xs && x <= D_forecv->c_xe && y >= D_forecv->c_ys && y <= D_forecv->c_ye) {
				if ((D_fore && D_fore->w_mouse) || (D_mousetrack && D_forecv->c_layer->l_mode == 1)) {
					/* Send clicks only if the window is expecting clicks */
					x -= D_forecv->c_xoff;
					y -= D_forecv->c_yoff;
					if (x >= 0 && x < D_forecv->c_layer->l_width && y >= 0
					    && y < D_forecv->c_layer->l_height) {
						bp[3] = x + 33;
						bp[4] = y + 33;
						i -= 4;
						bp += 4;
						continue;
					}
				}
			} else if (D_mousetrack && bp[2] == '#') {
				/* 'focus' to the clicked region, only on mouse up */
				struct canvas *cv = FindCanvas(x, y);
				if (cv) {
					SetForeCanvas(display, cv);
					/* XXX: Do we want to reset the input buffer? */
				}
			}
			if (bp[0] == '[') {
				memmove((char *)bp, (char *)bp + 1, i);
				bp--;
				size--;
			}
			if (i > 5)
				memmove((char *)bp, (char *)bp + 5, i - 5);
			bp--;
			i -= 4;
			size -= 5;
		}
	}
	if (D_encoding != (D_forecv ? D_forecv->c_layer->l_encoding : 0)) {
		int i, j, c, enc;
		char buf2[IOSIZE * 2 + 10];
		enc = D_forecv ? D_forecv->c_layer->l_encoding : 0;
		for (i = j = 0; i < size; i++) {
			c = ((unsigned char *)buf)[i];
			c = DecodeChar(c, D_encoding, &D_decodestate);
			if (c == -2)
				i--;	/* try char again */
			if (c < 0)
				continue;
			if (pastefont) {
				int font = 0;
				j += EncodeChar(buf2 + j, c, enc, &font);
				j += EncodeChar(buf2 + j, -1, enc, &font);
			} else
				j += EncodeChar(buf2 + j, c, enc, 0);
			if (j > (int)sizeof(buf2) - 10)	/* just in case... */
				break;
		}
		(*D_processinput) (buf2, j);
		return;
	}
	(*D_processinput) (buf, size);
}

static void disp_status_fn(struct event *ev, char *data)
{
	display = (struct display *)data;
	debug("disp_status_fn for display %x\n", (int)display);
	if (D_status)
		RemoveStatus();
}

static void disp_hstatus_fn(struct event *ev, char *data)
{
	display = (struct display *)data;
	if (D_status == STATUS_ON_HS) {
		SetTimeout(ev, 1);
		evenq(ev);
		return;
	}
	RefreshHStatus();
}

static void disp_blocked_fn(struct event *ev, char *data)
{
	struct win *p;

	display = (struct display *)data;
	debug("blocked timeout %s\n", D_usertty);
	if (D_obufp - D_obuf > D_obufmax + D_blocked_fuzz) {
		debug("stopping output to display\n");
		D_blocked = 1;
		/* re-enable all windows */
		for (p = windows; p; p = p->w_next)
			if (p->w_readev.condneg == &D_obuflenmax) {
				debug("freeing window #%d\n", p->w_number);
				p->w_readev.condpos = p->w_readev.condneg = 0;
			}
	}
}

static void disp_map_fn(struct event *ev, char *data)
{
	char *p;
	int l, i;
	unsigned char *q;
	display = (struct display *)data;
	debug("Flushing map sequence\n");
	if (!(l = D_seql))
		return;
	p = (char *)D_seqp - l;
	D_seqp = D_kmaps + 3;
	D_seql = 0;
	if ((q = D_seqh) != 0) {
		D_seqh = 0;
		i = q[0] << 8 | q[1];
		i &= ~KMAP_NOTIMEOUT;
		debug("Mapping former hit #%d - ", i);
		debug("%d(%s) - ", q[2], q + 3);
		if (StuffKey(i))
			ProcessInput2((char *)q + 3, q[2]);
		if (display == 0)
			return;
		l -= q[2];
		p += q[2];
	} else
		D_dontmap = 1;
	ProcessInput(p, l);
}

static void disp_idle_fn(struct event *ev, char *data)
{
	struct display *olddisplay;
	display = (struct display *)data;
	debug("idle timeout\n");
	if (idletimo <= 0 || idleaction.nr == RC_ILLEGAL)
		return;
	olddisplay = display;
	flayer = D_forecv->c_layer;
	fore = D_fore;
	DoAction(&idleaction, -1);
	if (idleaction.nr == RC_BLANKER)
		return;
	for (display = displays; display; display = display->d_next)
		if (olddisplay == display)
			break;
	if (display)
		ResetIdle();
}

void ResetIdle()
{
	if (idletimo > 0) {
		SetTimeout(&D_idleev, idletimo);
		if (!D_idleev.queued)
			evenq(&D_idleev);
	} else
		evdeq(&D_idleev);
}

static void disp_blanker_fn(struct event *ev, char *data)
{
	char buf[IOSIZE], *b;
	int size;

	display = (struct display *)data;
	size = read(D_blankerev.fd, buf, IOSIZE);
	if (size <= 0) {
		evdeq(&D_blankerev);
		close(D_blankerev.fd);
		D_blankerev.fd = -1;
		return;
	}
	for (b = buf; size; size--)
		AddChar(*b++);
}

void KillBlanker()
{
	int oldtop = D_top, oldbot = D_bot;
	struct mchar oldrend;

	if (D_blankerev.fd == -1)
		return;
	if (D_blocked == 4)
		D_blocked = 0;
	evdeq(&D_blankerev);
	close(D_blankerev.fd);
	D_blankerev.fd = -1;
	Kill(D_blankerpid, SIGHUP);
	D_top = D_bot = -1;
	oldrend = D_rend;
	if (D_ME) {
		AddCStr(D_ME);
		AddCStr(D_ME);
	} else {
		if (D_hascolor)
			AddStr("\033[m\033[m");	/* why is D_ME not set? */
		AddCStr(D_SE);
		AddCStr(D_UE);
	}
	AddCStr(D_VE);
	AddCStr(D_CE0);
	D_rend = mchar_null;
	D_atyp = 0;
	D_curvis = 0;
	D_x = D_y = -1;
	ChangeScrollRegion(oldtop, oldbot);
	SetRendition(&oldrend);
	ClearAll();
}

void RunBlanker(char **cmdv)
{
	char *m;
	int pid;
	int slave = -1;
	char termname[NAME_MAX + 1];
#ifndef TIOCSWINSZ
	char libuf[20], cobuf[20];
#endif
	char **np;

	strncpy(termname, "TERM=", 6);
	strncpy(termname + 5, D_termname, sizeof(termname) - 6);
	termname[sizeof(termname) - 1] = 0;
	KillBlanker();
	D_blankerpid = -1;
	if ((D_blankerev.fd = OpenPTY(&m)) == -1) {
		Msg(0, "OpenPty failed");
		return;
	}
#ifdef O_NOCTTY
	if (pty_preopen) {
		if ((slave = open(m, O_RDWR | O_NOCTTY)) == -1) {
			Msg(errno, "%s", m);
			close(D_blankerev.fd);
			D_blankerev.fd = -1;
			return;
		}
	}
#endif
	switch (pid = (int)fork()) {
	case -1:
		Msg(errno, "fork");
		close(D_blankerev.fd);
		D_blankerev.fd = -1;
		close(slave);
		return;
	case 0:
		displays = 0;
#ifdef DEBUG
		if (dfp && dfp != stderr) {
			fclose(dfp);
			dfp = 0;
		}
#endif
		if (setgid(real_gid) || setuid(real_uid))
			Panic(errno, "setuid/setgid");
		brktty(D_userfd);
		freetty();
		close(0);
		close(1);
		close(2);
		closeallfiles(slave);
		if (open(m, O_RDWR))
			Panic(errno, "Cannot open %s", m);
		dup(0);
		dup(0);
		if (slave != -1)
			close(slave);
		InitPTY(0);
		fgtty(0);
		SetTTY(0, &D_OldMode);
		np = NewEnv + 3;
		*np++ = NewEnv[0];
		*np++ = termname;
#ifdef TIOCSWINSZ
		glwz.ws_col = D_width;
		glwz.ws_row = D_height;
		(void)ioctl(0, TIOCSWINSZ, (char *)&glwz);
#else
		sprintf(libuf, "LINES=%d", D_height);
		sprintf(cobuf, "COLUMNS=%d", D_width);
		*np++ = libuf;
		*np++ = cobuf;
#endif
#ifdef SIGPIPE
		signal(SIGPIPE, SIG_DFL);
#endif
		display = 0;
		execvpe(*cmdv, cmdv, NewEnv + 3);
		Panic(errno, "%s", *cmdv);
	default:
		break;
	}
	D_blankerpid = pid;
	evenq(&D_blankerev);
	D_blocked = 4;
	ClearAll();
	close(slave);
}

void ClearScrollbackBuffer()
{
	if (D_CE3)
		AddCStr(D_CE3);
}
