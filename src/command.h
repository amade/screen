/* Copyright (C) 2019
 *      Amadeusz Sławiński
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "comm.h" /* temporarily for struct action */

typedef struct Command Command;
struct Command {
	char *name;
	void (*handler)(struct action *, int);
};

/* tempoary defs until functions are moved to command.c */

void DoCommandAclchg(struct action *act, int quiet);
void DoCommandAcldel(struct action *act, int quiet);
void DoCommandAclgrp(struct action *act, int quiet);
void DoCommandAclumask(struct action *act, int quiet);
void DoCommandActivity(struct action *act, int quiet);
void DoCommandAllpartial(struct action *act, int quiet);
void DoCommandAltscreen(struct action *act, int quiet);
void DoCommandAt(struct action *act, int quiet);
void DoCommandAutodetach(struct action *act, int quiet);
void DoCommandAutonuke(struct action *act, int quiet);
void DoCommandBacktick(struct action *act, int quiet);
void DoCommandBell(struct action *act, int quiet);
void DoCommandBind(struct action *act, int quiet);
void DoCommandBindkey(struct action *act, int quiet);
void DoCommandBlanker(struct action *act, int quiet);
void DoCommandBlankerprg(struct action *act, int quiet);
void DoCommandBreak(struct action *act, int quiet);
void DoCommandBreaktype(struct action *act, int quiet);
void DoCommandBufferfile(struct action *act, int quiet);
void DoCommandBumpleft(struct action *act, int quiet);
void DoCommandBumpright(struct action *act, int quiet);
void DoCommandC1(struct action *act, int quiet);
void DoCommandCaption(struct action *act, int quiet);
void DoCommandCharset(struct action *act, int quiet);
void DoCommandChdir(struct action *act, int quiet);
void DoCommandCjkwidth(struct action *act, int quiet);
void DoCommandClear(struct action *act, int quiet);
void DoCommandCollapse(struct action *act, int quiet);
void DoCommandColon(struct action *act, int quiet);
void DoCommandCommand(struct action *act, int quiet);
void DoCommandCompacthist(struct action *act, int quiet);
void DoCommandConsole(struct action *act, int quiet);
void DoCommandCopy(struct action *act, int quiet);
void DoCommandCrlf(struct action *act, int quiet);
void DoCommandDefautonuke(struct action *act, int quiet);
void DoCommandDefbce(struct action *act, int quiet);
void DoCommandDefc1(struct action *act, int quiet);
void DoCommandDefcharset(struct action *act, int quiet);
void DoCommandDefdynamictitle(struct action *act, int quiet);
void DoCommandDefencoding(struct action *act, int quiet);
void DoCommandDefescape(struct action *act, int quiet);
void DoCommandDefflow(struct action *act, int quiet);
void DoCommandDefgr(struct action *act, int quiet);
void DoCommandDefhstatus(struct action *act, int quiet);
void DoCommandDeflog(struct action *act, int quiet);
void DoCommandDeflogin(struct action *act, int quiet);
void DoCommandDefmode(struct action *act, int quiet);
void DoCommandDefmonitor(struct action *act, int quiet);
void DoCommandDefmousetrack(struct action *act, int quiet);
void DoCommandDefnonblock(struct action *act, int quiet);
void DoCommandDefobuflimit(struct action *act, int quiet);
void DoCommandDefscrollback(struct action *act, int quiet);
void DoCommandDefsilence(struct action *act, int quiet);
void DoCommandDefslowpaste(struct action *act, int quiet);
void DoCommandDefutf8(struct action *act, int quiet);
void DoCommandDefwrap(struct action *act, int quiet);
void DoCommandDefwritelock(struct action *act, int quiet);
void DoCommandDetach(struct action *act, int quiet);
void DoCommandDigraph(struct action *act, int quiet);
void DoCommandDinfo(struct action *act, int quiet);
void DoCommandDisplays(struct action *act, int quiet);
void DoCommandDumptermcap(struct action *act, int quiet);
void DoCommandDynamictitle(struct action *act, int quiet);
void DoCommandEcho(struct action *act, int quiet);
void DoCommandEncoding(struct action *act, int quiet);
void DoCommandEscape(struct action *act, int quiet);
void DoCommandEval(struct action *act, int quiet);
void DoCommandExec(struct action *act, int quiet);
void DoCommandFit(struct action *act, int quiet);
void DoCommandFlow(struct action *act, int quiet);
void DoCommandFocus(struct action *act, int quiet);
void DoCommandFocusminsize(struct action *act, int quiet);
void DoCommandGr(struct action *act, int quiet);
void DoCommandGroup(struct action *act, int quiet);
void DoCommandHardcopy(struct action *act, int quiet);
void DoCommandHardcopy_append(struct action *act, int quiet);
void DoCommandHardcopydir(struct action *act, int quiet);
void DoCommandHardstatus(struct action *act, int quiet);
void DoCommandHeight(struct action *act, int quiet);
void DoCommandHelp(struct action *act, int quiet);
void DoCommandHistory(struct action *act, int quiet);
void DoCommandHstatus(struct action *act, int quiet);
void DoCommandIdle(struct action *act, int quiet);
void DoCommandIgnorecase(struct action *act, int quiet);
void DoCommandInfo(struct action *act, int quiet);
void DoCommandKill(struct action *act, int quiet);
void DoCommandLastmsg(struct action *act, int quiet);
void DoCommandLayout(struct action *act, int quiet);
void DoCommandLicense(struct action *act, int quiet);
void DoCommandLockscreen(struct action *act, int quiet);
void DoCommandLog(struct action *act, int quiet);
void DoCommandLogfile(struct action *act, int quiet);
void DoCommandLogin(struct action *act, int quiet);
void DoCommandLogtstamp(struct action *act, int quiet);
void DoCommandMapdefault(struct action *act, int quiet);
void DoCommandMapnotnext(struct action *act, int quiet);
void DoCommandMaptimeout(struct action *act, int quiet);
void DoCommandMarkkeys(struct action *act, int quiet);
void DoCommandMeta(struct action *act, int quiet);
void DoCommandMonitor(struct action *act, int quiet);
void DoCommandMousetrack(struct action *act, int quiet);
void DoCommandMsgminwait(struct action *act, int quiet);
void DoCommandMsgwait(struct action *act, int quiet);
void DoCommandMultiinput(struct action *act, int quiet);
void DoCommandMultiuser(struct action *act, int quiet);
void DoCommandNext(struct action *act, int quiet);
void DoCommandNonblock(struct action *act, int quiet);
void DoCommandNumber(struct action *act, int quiet);
void DoCommandObuflimit(struct action *act, int quiet);
void DoCommandOnly(struct action *act, int quiet);
void DoCommandOther(struct action *act, int quiet);
void DoCommandParent(struct action *act, int quiet);
void DoCommandPartial(struct action *act, int quiet);
void DoCommandPaste(struct action *act, int quiet);
void DoCommandPastefont(struct action *act, int quiet);
void DoCommandPow_break(struct action *act, int quiet);
void DoCommandPow_detach(struct action *act, int quiet);
void DoCommandPow_detach_msg(struct action *act, int quiet);
void DoCommandPrev(struct action *act, int quiet);
void DoCommandPrintcmd(struct action *act, int quiet);
void DoCommandProcess(struct action *act, int quiet);
void DoCommandQuit(struct action *act, int quiet);
void DoCommandReadbuf(struct action *act, int quiet);
void DoCommandReadreg(struct action *act, int quiet);
void DoCommandRedisplay(struct action *act, int quiet);
void DoCommandRegister(struct action *act, int quiet);
void DoCommandRemove(struct action *act, int quiet);
void DoCommandRemovebuf(struct action *act, int quiet);
void DoCommandRendition(struct action *act, int quiet);
void DoCommandReset(struct action *act, int quiet);
void DoCommandResize(struct action *act, int quiet);
void DoCommandScreen(struct action *act, int quiet);
void DoCommandScrollback(struct action *act, int quiet);
void DoCommandSelect(struct action *act, int quiet);
void DoCommandSessionname(struct action *act, int quiet);
void DoCommandSetenv(struct action *act, int quiet);
void DoCommandSetsid(struct action *act, int quiet);
void DoCommandShell(struct action *act, int quiet);
void DoCommandShelltitle(struct action *act, int quiet);
void DoCommandSilence(struct action *act, int quiet);
void DoCommandSilencewait(struct action *act, int quiet);
void DoCommandSleep(struct action *act, int quiet);
void DoCommandSlowpaste(struct action *act, int quiet);
void DoCommandSorendition(struct action *act, int quiet);
void DoCommandSort(struct action *act, int quiet);
void DoCommandSource(struct action *act, int quiet);
void DoCommandSplit(struct action *act, int quiet);
void DoCommandStartup_message(struct action *act, int quiet);
void DoCommandStatus(struct action *act, int quiet);
void DoCommandStuff(struct action *act, int quiet);
void DoCommandSu(struct action *act, int quiet);
void DoCommandSuspend(struct action *act, int quiet);
void DoCommandTerm(struct action *act, int quiet);
void DoCommandTerminfo(struct action *act, int quiet);
void DoCommandTitle(struct action *act, int quiet);
void DoCommandTruecolor(struct action *act, int quiet);
void DoCommandUnbindall(struct action *act, int quiet);
void DoCommandUnsetenv(struct action *act, int quiet);
void DoCommandUtf8(struct action *act, int quiet);
void DoCommandVbell(struct action *act, int quiet);
void DoCommandVbell_msg(struct action *act, int quiet);
void DoCommandVbellwait(struct action *act, int quiet);
void DoCommandVerbose(struct action *act, int quiet);
void DoCommandVersion(struct action *act, int quiet);
void DoCommandWall(struct action *act, int quiet);
void DoCommandWidth(struct action *act, int quiet);
void DoCommandWindowlist(struct action *act, int quiet);
void DoCommandWindows(struct action *act, int quiet);
void DoCommandWrap(struct action *act, int quiet);
void DoCommandWritebuf(struct action *act, int quiet);
void DoCommandWritelock(struct action *act, int quiet);
void DoCommandXoff(struct action *act, int quiet);
void DoCommandXon(struct action *act, int quiet);
void DoCommandZmodem(struct action *act, int quiet);
void DoCommandZombie(struct action *act, int quiet);
void DoCommandZombie_timeout(struct action *act, int quiet);


int FindCommand(const char *command);


Command Commands[];
