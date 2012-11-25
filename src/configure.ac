dnl Process this file with autoconf to produce a configure script.
dnl
dnl $Id$ GNU
dnl
dnl Many thanks to David MacKenzie for writing autoconf and
dnl providing a sample configure.in file for screen.
dnl
AC_REVISION($Revision$)dnl
AC_INIT(screen.c)
AC_CONFIG_HEADER(config.h)

dnl
dnl Define some useful macros
dnl
AC_DEFUN([AC_PROGRAM_SOURCE],
[AC_REQUIRE([AC_PROG_CPP])AC_PROVIDE([$0])cat > conftest.c <<EOF
#include "confdefs.h"
[$1]
_CUT_HERE_
[$2]
EOF
eval "$ac_cpp conftest.c 2>&5 | sed -e '1,/_CUT_HERE_/d' -e 's/ //g' > conftest.out"
. ./conftest.out
rm -f conftest*
])dnl
dnl
define(AC_NOTE,
[echo "$1" 1>&AC_FD_MSG
])dnl

dnl
dnl Extract version from patchlevel.h
dnl
rev=`sed < ${srcdir}/patchlevel.h -n -e '/#define REV/s/#define REV  *//p'`
vers=`sed < ${srcdir}/patchlevel.h -n -e '/#define VERS/s/#define VERS  *//p'`
pat=`sed < ${srcdir}/patchlevel.h -n -e '/#define PATCHLEVEL/s/#define PATCHLEVEL  *//p'`
VERSION="$rev.$vers.$pat"
AC_NOTE(this is screen version $VERSION)
AC_SUBST(VERSION)
AC_PREFIX_PROGRAM(screen)
AC_PREFIX_PROGRAM(gzip)

old_CFLAGS="$CFLAGS"
AC_PROG_CC
AC_PROG_CPP
AC_PROG_GCC_TRADITIONAL
AC_ISC_POSIX
AC_USE_SYSTEM_EXTENSIONS

AC_TRY_RUN(main(){exit(0);},,[
if test $CC != cc ; then
AC_NOTE(Your $CC failed - restarting with CC=cc)
AC_NOTE()
CC=cc
export CC
exec $0 $configure_args
fi
])

AC_TRY_RUN(main(){exit(0);},,
exec 5>&2
eval $ac_link
AC_NOTE(CC=$CC; CFLAGS=$CFLAGS; LIBS=$LIBS;)
AC_NOTE($ac_compile)
AC_MSG_ERROR(Can't run the compiler - sorry))

AC_TRY_RUN([
main()
{
  int __something_strange_();
  __something_strange_(0);
}
],AC_MSG_ERROR(Your compiler does not set the exit status - sorry))

AC_PROG_AWK

AC_PROG_INSTALL

if test -f etc/toolcheck; then
AC_CHECKING(for buggy tools)
sh etc/toolcheck 1>&AC_FD_MSG
fi

dnl
dnl    ****  typedefs ****
dnl
dnl (currently not used)
dnl
dnl AC_CHECKING(for pid_t)
dnl AC_EGREP_CPP(pid_t,[#include <sys/types.h>
dnl ],AC_DEFINE(PID_T_DEFINED))
dnl
dnl AC_CHECKING(for sig_t)
dnl AC_EGREP_CPP(sig_t,[#include <sys/types.h>
dnl #include <signal.h>
dnl ],AC_DEFINE(SIG_T_DEFINED))
dnl
dnl AC_CHECKING(for uid_t)
dnl AC_EGREP_CPP(uid_t,[#include <sys/types.h>
dnl ],AC_DEFINE(UID_T_DEFINED))
dnl

dnl
dnl   ****     Job control     ****
dnl

AC_CHECKING(BSD job jontrol)
AC_TRY_LINK(
[#include <sys/types.h>
#include <sys/ioctl.h>
], [
#ifdef POSIX
tcsetpgrp(0, 0);
#else
int x = TIOCSPGRP;
int y = TIOCNOTTY;
#endif
], AC_NOTE(- you have jobcontrol) AC_DEFINE(BSDJOBS), AC_NOTE(- you don't have jobcontrol))

dnl
dnl    ****  setresuid(), setreuid(), seteuid()  ****
dnl
AC_CHECKING(setresuid)
AC_TRY_LINK(,[
setresuid(0, 0, 0);
], AC_DEFINE(HAVE_SETRESUID))
AC_CHECKING(setreuid)
AC_TRY_LINK(,[
setreuid(0, 0);
], AC_DEFINE(HAVE_SETREUID))
dnl
dnl seteuid() check:
dnl   linux seteuid was broken before V1.1.11
dnl   NeXT, AUX, ISC, and ultrix are still broken (no saved uid support)
dnl   Solaris seteuid doesn't change the saved uid, bad for
dnl     multiuser screen sessions
AC_CHECKING(seteuid)
AC_TRY_LINK(,[seteuid_is_broken(0);], AC_DEFINE(HAVE_SETEUID))

dnl execvpe
AC_CHECKING(execvpe)
AC_TRY_LINK(,[
    execvpe(0, 0, 0);
], AC_DEFINE(HAVE_EXECVPE)
CFLAGS="$CFLAGS -D_GNU_SOURCE")

dnl
dnl    ****  select()  ****
dnl

AC_CHECKING(select)
AC_TRY_LINK(,[select(0, 0, 0, 0, 0);],, 
LIBS="$LIBS -lnet -lnsl"
AC_CHECKING(select with $LIBS)
AC_TRY_LINK(,[select(0, 0, 0, 0, 0);],, 
AC_MSG_ERROR(!!! no select - no screen))
)

dnl
dnl    ****  SOCKET tests  ****
dnl 
dnl 	may need  	LIBS="$LIBS -lsocket" 	here
dnl

AC_CHECKING(sockets)
AC_TRY_RUN([
/* For select - According to POSIX 1003.1-2001 */
#include <sys/select.h>

/* For select - According to earlier standards */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

char *son = "/tmp/conftest$$";

main()
{
  int s1, s2, l;
  struct sockaddr_un a;
  fd_set f;

  (void)alarm(5);
  if ((s1 = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    exit(1);
  a.sun_family = AF_UNIX;
  strcpy(a.sun_path, son);
  (void) unlink(son);
  if (bind(s1, (struct sockaddr *) &a, strlen(son)+2) == -1)
    exit(1);
  if (listen(s1, 2))
    exit(1);
  if (fork() == 0)
    {
      if ((s2 = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	kill(getppid(), 3);
      (void)connect(s2, (struct sockaddr *)&a, strlen(son) + 2);
      if (write(s2, "HELLO", 5) == -1)
	kill(getppid(), 3);
      exit(0);
    }
  l = sizeof(a);
  close(0);
  if (accept(s1, &a, &l))
    exit(1);
  FD_SET(0, &f);
  if (select(1, &f, 0, 0, 0) == -1)
    exit(1);
  exit(0);
}
], AC_NOTE(- your sockets are usable) sock=1,
AC_NOTE(- your sockets are not usable))
rm -f /tmp/conftest*

if test -n "$sock"; then
AC_CHECKING(socket implementation)
AC_TRY_RUN([
/* For select - According to POSIX 1003.1-2001 */
#include <sys/select.h>

/* For select - According to earlier standards */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

char *son = "/tmp/conftest$$";

main()
{
  int s;
  struct stat stb;
  struct sockaddr_un a;
  if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    exit(0);
  a.sun_family = AF_UNIX;
  strcpy(a.sun_path, son);
  (void) unlink(son);
  if (bind(s, (struct sockaddr *) &a, strlen(son)+2) == -1)
    exit(0);
  if (stat(son, &stb))
    exit(1);
  close(s);
  exit(0);
}
],AC_NOTE(- you are normal),
AC_NOTE(- unix domain sockets are not kept in the filesystem)
AC_DEFINE(SOCK_NOT_IN_FS) socknofs=1)
rm -f /tmp/conftest*
fi


dnl
dnl    ****  choose sockets or fifos  ****
dnl
if test -n "$sock"; then
  AC_NOTE(- using unix-domain sockets)
else
  AC_MSG_ERROR(you don't have usable sockets -> no screen)
fi

dnl
dnl    ****  check the select implementation ****
dnl

AC_CHECKING(select return value)
AC_TRY_RUN([
/* For select - According to POSIX 1003.1-2001 */
#include <sys/select.h>

/* For select - According to earlier standards */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/stat.h>
#include <fcntl.h>

char *nam = "/tmp/conftest$$";

#ifdef NAMEDPIPE

#ifndef O_NONBLOCK
#define O_NONBLOCK O_NDELAY
#endif
#ifndef S_IFIFO
#define S_IFIFO 0010000
#endif


main()
{
  fd_set f;

#ifdef __FreeBSD__
/* From Andrew A. Chernov (ache@astral.msk.su):
 * opening RDWR fifo fails in BSD 4.4, but select return values are
 * right.
 */
  exit(0);
#endif
  (void)alarm(5);
#ifdef POSIX
  if (mkfifo(nam, 0777))
#else
  if (mknod(nam, S_IFIFO|0777, 0))
#endif
    exit(1);
  close(0);
  if (open(nam, O_RDWR | O_NONBLOCK))
    exit(1);
  if (write(0, "TEST", 4) == -1)
    exit(1);

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

main()
{
  int s1, s2, l;
  struct sockaddr_un a;
  fd_set f;

  (void)alarm(5);
  if ((s1 = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    exit(1);
  a.sun_family = AF_UNIX;
  strcpy(a.sun_path, nam);
  (void) unlink(nam);
  if (bind(s1, (struct sockaddr *) &a, strlen(nam)+2) == -1)
    exit(1);
  if (listen(s1, 2))
    exit(1);
  if (fork() == 0)
    {
      if ((s2 = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	kill(getppid(), 3);
      (void)connect(s2, (struct sockaddr *)&a, strlen(nam) + 2);
      if (write(s2, "HELLO", 5) == -1)
	kill(getppid(), 3);
      exit(0);
    }
  l = sizeof(a);
  close(0);
  if (accept(s1, (struct sockaddr *)&a, &l))
    exit(1);
#endif


  FD_SET(0, &f);
  if (select(1, &f, 0, 0, 0) == -1)
    exit(1);
  if (select(1, &f, &f, 0, 0) != 2)
    exit(1);
  exit(0);
}
],AC_NOTE(- select is ok),
AC_NOTE(- select can't count) AC_DEFINE(SELECT_BROKEN))

dnl
dnl    ****  termcap or terminfo  ****
dnl
AC_CHECKING(for tgetent)
AC_TRY_LINK(,tgetent((char *)0, (char *)0);,,
olibs="$LIBS"
LIBS="-lcurses $olibs"
AC_CHECKING(libcurses)
AC_TRY_LINK(,[
#ifdef __hpux
__sorry_hpux_libcurses_is_totally_broken_in_10_10();
#else
tgetent((char *)0, (char *)0);
#endif
],,
LIBS="-ltermcap $olibs"
AC_CHECKING(libtermcap)
AC_TRY_LINK(,tgetent((char *)0, (char *)0);,,
LIBS="-ltermlib $olibs"
AC_CHECKING(libtermlib)
AC_TRY_LINK(,tgetent((char *)0, (char *)0);,,
LIBS="-lncursesw $olibs"
AC_CHECKING(libncursesw)
AC_TRY_LINK(,tgetent((char *)0, (char *)0);,,
LIBS="-lncurses $olibs"
AC_CHECKING(libncurses)
AC_TRY_LINK(,tgetent((char *)0, (char *)0);,,
AC_MSG_ERROR(!!! no tgetent - no screen)))))))

AC_CHECKING(ospeed)
AC_TRY_LINK(extern short ospeed;,ospeed=5;,,AC_DEFINE(NEED_OSPEED))

dnl
dnl    ****  PTY specific things  ****
dnl
AC_CHECKING(for /dev/ptc)
if test -r /dev/ptc; then
AC_DEFINE(HAVE_DEV_PTC)
fi

AC_CHECK_FUNCS(getpt)

dnl check for openpty()
if test -z "$sysvr4ptys"; then
AC_CHECK_FUNCS(openpty,,
[AC_CHECK_LIB(util,openpty, [AC_DEFINE(HAVE_OPENPTY)] [LIBS="$LIBS -lutil"])])
fi

AC_CHECKING(for ptyranges)
if test -d /dev/ptym ; then
pdir='/dev/ptym'
else
pdir='/dev'
fi
dnl SCO uses ptyp%d
AC_EGREP_CPP(yes,
[#ifdef M_UNIX
   yes;
#endif
], ptys=`echo /dev/ptyp??`, ptys=`echo $pdir/pty??`)
dnl if test -c /dev/ptyp19; then
dnl ptys=`echo /dev/ptyp??`
dnl else
dnl ptys=`echo $pdir/pty??`
dnl fi
if test "$ptys" != "$pdir/pty??" ; then
p0=`echo $ptys | tr ' ' '\012' | sed -e 's/^.*\(.\).$/\1/g' | sort -u | tr -d '\012'`
p1=`echo $ptys | tr ' ' '\012' | sed -e 's/^.*\(.\)$/\1/g'  | sort -u | tr -d '\012'`
AC_DEFINE_UNQUOTED(PTYRANGE0,"$p0")
AC_DEFINE_UNQUOTED(PTYRANGE1,"$p1")
fi

dnl    ****  pty mode/group handling ****
dnl
dnl support provided by Luke Mewburn <lm@rmit.edu.au>, 931222
AC_ARG_WITH(pty-mode, [  --with-pty-mode=mode    default mode for ptys], [ ptymode="${withval}" ])
AC_ARG_WITH(pty-group, [  --with-pty-group=group  default group for ptys], [ ptygrp="${withval}" ])
test -n "$ptymode" || ptymode=0620
if test -n "$ptygrp" ; then
AC_DEFINE_UNQUOTED(PTYMODE, $ptymode)
AC_DEFINE_UNQUOTED(PTYGROUP,$ptygrp)
else

AC_CHECKING(default tty permissions/group)
rm -f conftest_grp
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
main()
{
  struct stat sb;
  char *x,*ttyname();
  int om, m;
  FILE *fp;

  if (!(x = ttyname(0))) exit(1);
  if (stat(x, &sb)) exit(1);
  om = sb.st_mode;
  if (om & 002) exit(0);
  m = system("mesg y");
  if (m == -1 || m == 127) exit(1);
  if (stat(x, &sb)) exit(1);
  m = sb.st_mode;
  if (chmod(x, om)) exit(1);
  if (m & 002) exit(0);
  if (sb.st_gid == getgid()) exit(1);
  if (!(fp=fopen("conftest_grp", "w")))
    exit(1);
  fprintf(fp, "%d\n", sb.st_gid);
  fclose(fp);
  exit(0);
}
],[
    if test -f conftest_grp; then
	ptygrp=`cat conftest_grp`
	AC_NOTE([- pty mode: $ptymode, group: $ptygrp])
	AC_DEFINE_UNQUOTED(PTYMODE, $ptymode)
	AC_DEFINE_UNQUOTED(PTYGROUP,$ptygrp)
    else
	AC_NOTE(- ptys are world accessable)
    fi
],[
    WRITEPATH=''
    XTERMPATH=''
    AC_PATH_PROG(WRITEPATH, write)
    AC_PATH_PROG(XTERMPATH, xterm)
    found=
    if test -n "$WRITEPATH$XTERMPATH"; then
      findfollow=
      lsfollow=
      found=`find $WRITEPATH $XTERMPATH -follow -print 2>/dev/null`
      if test -n "$found"; then
	findfollow=-follow
	lsfollow=L
      fi
      if test -n "$XTERMPATH"; then
	ptygrpn=`ls -l$lsfollow $XTERMPATH | sed -n -e 1p | $AWK '{print $4}'`
	if test tty != "$ptygrpn"; then
	  XTERMPATH=
	fi
      fi
    fi
    if test -n "$WRITEPATH$XTERMPATH"; then
      found=`find $WRITEPATH $XTERMPATH $findfollow -perm -2000 -print` 
      if test -n "$found"; then
	ptygrp=`ls -ln$lsfollow $found | sed -n -e 1p | $AWK '{print $4}'`
	AC_NOTE([- pty mode: $ptymode, group: $ptygrp])
	AC_DEFINE_UNQUOTED(PTYMODE, $ptymode)
	AC_DEFINE_UNQUOTED(PTYGROUP,$ptygrp)
      else
	AC_NOTE(- ptys are world accessable)
      fi
    else
      AC_NOTE(- can't determine - assume ptys are world accessable)
    fi
  ]
)
rm -f conftest_grp
fi

dnl
dnl    ****  utmp handling  ****
dnl
AC_CHECKING(getutent)
AC_TRY_LINK([
#include <time.h> /* to get time_t on SCO */
#include <sys/types.h>
#if defined(SVR4) && !defined(DGUX)
#include <utmpx.h>
#define utmp utmpx
#else
#include <utmp.h>
#endif
#ifdef __hpux
#define pututline _pututline
#endif
],
[int x = DEAD_PROCESS; pututline((struct utmp *)0); getutent();], AC_DEFINE(GETUTENT),
olibs="$LIBS"
LIBS="$LIBS -lgen"
AC_CHECKING(getutent with -lgen)
AC_TRY_LINK([
#include <time.h>
#include <sys/types.h>
#if defined(SVR4) && !defined(DGUX)
#include <utmpx.h>
#define utmp utmpx
#else
#include <utmp.h>
#endif
#ifdef __hpux
#define pututline _pututline
#endif
],
[int x = DEAD_PROCESS; pututline((struct utmp *)0); getutent();], AC_DEFINE(GETUTENT), LIBS="$olibs")
)
AC_CHECKING(ut_host)
AC_TRY_COMPILE([
#include <time.h>
#include <sys/types.h>
#if defined(SVR4) && !defined(DGUX)
#include <utmpx.h>
#define utmp utmpx
#else
#include <utmp.h>
#endif
],[struct utmp u; u.ut_host[0] = 0;], AC_DEFINE(UTHOST))

dnl
dnl    ****  loadav  ****
dnl
AC_CHECKING(for libutil(s))
test -f /usr/lib/libutils.a && LIBS="$LIBS -lutils"
test -f /usr/lib/libutil.a && LIBS="$LIBS -lutil"

AC_CHECKING(getloadavg)
AC_TRY_LINK(,[getloadavg((double *)0, 0);],
AC_DEFINE(LOADAV_GETLOADAVG) load=1,
if test -f /usr/lib/libkvm.a ; then
olibs="$LIBS"
LIBS="$LIBS -lkvm"
AC_CHECKING(getloadavg with -lkvm)
AC_TRY_LINK(,[getloadavg((double *)0, 0);],
AC_DEFINE(LOADAV_GETLOADAVG) load=1, LIBS="$olibs")
fi
)

if test -z "$load" ; then
AC_EGREP_CPP(yes,[yes;], load=1)
fi
if test -z "$load" ; then
AC_CHECKING(for kernelfile)
for core in /unix /vmunix /dynix /hp-ux /xelos /dev/ksyms /kernel/unix /kernel/genunix /unicos /mach /netbsd /386bsd /dgux /bsd /stand/vmunix; do
  if test -f $core || test -c $core; then
    break
  fi
done
if test ! -f $core && test ! -c $core ; then
  AC_NOTE(- no kernelfile found)
else
  AC_NOTE(- using kernelfile '$core')
  if test -r $core ; then
  AC_DEFINE_UNQUOTED(LOADAV_UNIX,"$core")
  AC_CHECK_HEADER(nlist.h,
    [AC_DEFINE(NLIST_STRUCT)
     AC_CHECKING(n_un in struct nlist)
     AC_TRY_COMPILE([#include <nlist.h>],
       [struct nlist n; n.n_un.n_name = 0;],
        AC_DEFINE(NLIST_NAME_UNION))])

  AC_CHECKING(for nlist declaration)
  AC_EGREP_CPP([nlist(( |	)( |	)*.*\(|\()],[
#ifdef NLIST_STRUCT
# include <nlist.h>
#else
# include <a.out.h>
#endif
],AC_DEFINE(NLIST_DECLARED))

  AC_CHECKING(for avenrun symbol)
  nlist64=
  for av in avenrun _avenrun _Loadavg avenrun _avenrun _Loadavg; do
  AC_TRY_RUN([
#include <sys/types.h>
#ifdef NLIST_STRUCT
#include <nlist.h>
#else
#include <a.out.h>
#endif

$nlist64

struct nlist nl[2];

main()
{
#if !defined(_AUX_SOURCE) && !defined(AUX)
# ifdef NLIST_NAME_UNION
  nl[0].n_un.n_name = "$av";
# else
  nl[0].n_name = "$av";
# endif
#else
  strncpy(nl[0].n_name, "$av", sizeof(nl[0].n_name));
#endif
  nlist(LOADAV_UNIX, nl);
  if (nl[0].n_value == 0)
    exit(1);
  exit(0);
}
  ],avensym=$av;break)
  if test "$av" = _Loadavg; then
    nlist64='#define nlist nlist64'
  fi
  done
  if test -z "$avensym" ; then
    AC_NOTE(- no avenrun symbol found)
  else
    AC_NOTE(- using avenrun symbol '$avensym')
    AC_DEFINE_UNQUOTED(LOADAV_AVENRUN,"$avensym")
    if test -n "$nlist64"; then
      AC_NOTE(- used nlist64 to find it)
      AC_DEFINE(LOADAV_USE_NLIST64)
    fi
    load=1
  fi
  else
    AC_NOTE(  Can't configure the load average display feature)
    AC_NOTE(  because $core is not readable by you.)
    AC_NOTE(  To configure the load average display feature,)
    AC_NOTE(  re-run configure as root if possible.)
    AC_NOTE(  If you are not the system administrator then disregard)
    AC_NOTE(  this warning.  You can still use screen without)
    AC_NOTE(  the load average display feature.)
  fi
fi
fi

AC_PROGRAM_SOURCE([
#include <sys/types.h>
#include <sys/param.h>
],[
#if !defined(LOADAV_GETLOADAVG)
loadtype=long
#  if defined(FSCALE) && !defined(__osf__)
#   undef FSCALE
loadscale=FSCALE
#  else
loadscale=1000 	/* our default value */
#  endif /* not FSCALE */
#else
loadtype=double
loadscale=1
#endif
#ifdef alliant
loadnum=4
#else
loadnum=3
#endif
])

if test -n "$load" ; then AC_DEFINE(LOADAV) fi
if test -n "$loadtype" ; then AC_DEFINE_UNQUOTED(LOADAV_TYPE,$loadtype) fi
if test -n "$loadnum" ; then AC_DEFINE_UNQUOTED(LOADAV_NUM,$loadnum) fi
if test -n "$loadscale" ; then AC_DEFINE_UNQUOTED(LOADAV_SCALE,$loadscale) fi

dnl
dnl    ****  signal handling  ****
dnl
if test -n "$posix" ; then

dnl POSIX has reliable signals with void return type.
AC_NOTE(assuming posix signal definition)
AC_DEFINE(SIGVOID)

else

AC_CHECKING(return type of signal handlers)
AC_TRY_COMPILE(
[#include <sys/types.h>
#include <signal.h>
#ifdef signal
#undef signal
#endif
extern void (*signal ()) ();], [int i;], AC_DEFINE(SIGVOID))
AC_CHECKING(sigset)
AC_TRY_LINK([
#include <sys/types.h>
#include <signal.h>
],[
#ifdef SIGVOID
sigset(0, (void (*)())0);
#else
sigset(0, (int (*)())0);
#endif
], AC_DEFINE(USESIGSET))

fi



dnl
dnl    ****  libraries  ****
dnl

AC_CHECKING(for crypt and sec libraries)
test -f /lib/libcrypt_d.a || test -f /usr/lib/libcrypt_d.a && LIBS="$LIBS -lcrypt_d"
oldlibs="$LIBS"
LIBS="$LIBS -lcrypt"
AC_CHECKING(crypt)
AC_TRY_LINK(,,,LIBS="$oldlibs")
test -f /lib/libsec.a || test -f /usr/lib/libsec.a && LIBS="$LIBS -lsec"
test -f /lib/libshadow.a || test -f /usr/lib/libshadow.a && LIBS="$LIBS -lshadow"
oldlibs="$LIBS"
LIBS="$LIBS -lsun"
AC_CHECKING(IRIX sun library)
AC_TRY_LINK(,,,LIBS="$oldlibs")

AC_CHECKING(syslog)
AC_TRY_LINK(,[closelog();], , [oldlibs="$LIBS"
LIBS="$LIBS -lbsd"
AC_CHECKING(syslog in libbsd.a)
AC_TRY_LINK(, [closelog();], AC_NOTE(- found.), [LIBS="$oldlibs"
AC_NOTE(- bad news: syslog missing.) AC_DEFINE(NOSYSLOG)])])

AC_EGREP_CPP(yes,
[#ifdef M_UNIX
   yes;
#endif
], LIBS="$LIBS -lsocket -lcrypt_i")

dnl
dnl    ****  misc things  ****
dnl

if test -z "$butterfly"; then
AC_CHECKING(for termio or termios)
AC_TRY_CPP([#include <termio.h>], AC_DEFINE(TERMIO),
if test -n "$posix"; then
AC_TRY_CPP([#include <termios.h>], AC_DEFINE(TERMIO))
fi
)
fi

AC_CHECKING(getttyent)
AC_TRY_LINK(,[getttyent();], AC_DEFINE(GETTTYENT))

AC_CHECKING(fdwalk)
AC_TRY_LINK([#include <stdlib.h>], [fdwalk(NULL, NULL);],AC_DEFINE(HAVE_FDWALK))


AC_SYS_LONG_FILE_NAMES

AC_MSG_CHECKING(for vsprintf)
AC_TRY_LINK(,[vsprintf(0,0,0);], AC_MSG_RESULT(yes);AC_DEFINE(USEVARARGS), AC_MSG_RESULT(no))

AC_HEADER_DIRENT

AC_MSG_CHECKING([for nl_langinfo(CODESET)])
AC_TRY_LINK([
#include <langinfo.h>
],[nl_langinfo(CODESET);], AC_MSG_RESULT(yes);AC_DEFINE(HAVE_NL_LANGINFO), AC_MSG_RESULT(no))

AC_SEARCH_LIBS(gethostname, nsl)

AC_CHECK_FUNCS(vsnprintf strftime)

AC_ARG_ENABLE(pam, [  --enable-pam            enable PAM support])
if test "$enable_pam" = "yes"; then
    AC_MSG_CHECKING(for PAM support)
    oldlibs="$LIBS"
    LIBS="$LIBS -lpam"
    AC_TRY_LINK([#include <security/pam_appl.h>], [
	pam_start(0, 0, 0, 0);
	pam_authenticate(0, 0);
	pam_end(0,0);
    ], AC_MSG_RESULT(yes);AC_DEFINE(USE_PAM),
       AC_MSG_RESULT(no);LIBS="$oldlibs")
fi

AC_ARG_ENABLE(use_locale, [  --enable-locale         use localized month/day names])
if test "$enable_use_locale" = "yes"; then
  AC_DEFINE(USE_LOCALE)
fi

AC_TRY_RUN(main(){exit(0);},,AC_MSG_ERROR(Can't run the compiler - internal error. Sorry.))

ETCSCREENRC=
AC_MSG_CHECKING(for the global screenrc file)
AC_ARG_WITH(sys-screenrc, [  --with-sys-screenrc=path to the global screenrc file], [ ETCSCREENRC="${withval}" ])
AC_SUBST(ETCSCREENRC)

AC_OUTPUT(Makefile doc/Makefile, [[
# a hook for preserving undef directive in config.h
mv config.h conftest
sed -e 's@^\(.*\)defin.\( .*\) .*/\*\(.*KEEP_UNDEF_HERE\)@\1undef\2	/\*\3@' < conftest > config.h
rm -f conftest
]])

echo ""
if test -z "$AWK"; then
echo "!!! Since you have no awk you must copy the files 'comm.h.dist'"
echo "!!! and 'term.h.dist' to 'comm.h' and 'term.h'."
echo "!!! Do _not_ change the user configuration section in config.h!"
echo "Please check the pathnames in the Makefile."
else
echo "Now please check the pathnames in the Makefile and in the user"
echo "configuration section in config.h."
fi
echo "Then type 'make' to make screen. Good luck."
echo ""
