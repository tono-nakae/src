#	$NetBSD: mod.mk,v 1.1 2013/03/10 21:41:05 christos Exp $

WARNS=	5
MKLINT=	no
LIBISMODULE= yes
LIBROOTDIR=/lib

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.if defined(MLIBDIR)
LIBDIR=         ${LIBROOTDIR}/${MLIBDIR}/npf
SHLIBDIR=       ${LIBROOTDIR}/${MLIBDIR}/npf
SHLIBINSTALLDIR=${LIBROOTDIR}/${MLIBDIR}/npf
.else
LIBDIR=         ${LIBROOTDIR}/npf
SHLIBDIR=       ${LIBROOTDIR}/npf
SHLIBINSTALLDIR=${LIBROOTDIR}/npf
.endif

LIB=${MOD}
SRCS=npf${MOD}.c

.include <bsd.lib.mk>
