dnl $Id: krb-sys-nextstep.m4,v 1.1.1.3 2001/09/17 12:10:07 assar Exp $
dnl
dnl NEXTSTEP is not posix compliant by default,
dnl you need a switch -posix to the compiler
dnl

AC_DEFUN(rk_SYS_NEXTSTEP, [
AC_CACHE_CHECK(for NeXTSTEP, rk_cv_sys_nextstep, [
AC_EGREP_CPP(yes, 
[#if defined(NeXT) && !defined(__APPLE__)
	yes
#endif 
], rk_cv_sys_nextstep=yes, rk_cv_sys_nextstep=no)])
if test "$rk_cv_sys_nextstep" = "yes"; then
  CFLAGS="$CFLAGS -posix"
  LIBS="$LIBS -posix"
fi
AH_BOTTOM([#if defined(HAVE_SGTTY_H) && defined(__NeXT__)
#define SGTTY
#endif])
])
