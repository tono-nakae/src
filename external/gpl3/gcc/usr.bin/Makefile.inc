#	$NetBSD: Makefile.inc,v 1.13 2013/08/05 03:22:45 matt Exp $

.ifndef _EXTERNAL_GPL3_GCC_USR_BIN_MAKEFILE_INC_
_EXTERNAL_GPL3_GCC_USR_BIN_MAKEFILE_INC_=1

.include <bsd.own.mk>

GMP_MACHINE_ARCH=${MACHINE_ARCH:C/armv[467]/arm/}

TOP=		${NETBSDSRCDIR}
DIST=		${TOP}/external/gpl3/gcc/dist

GCCARCH=	${.CURDIR}/../gcc/arch/${MACHINE_ARCH}

CPPFLAGS+=	-DLOCALEDIR=\"/usr/share/locale\" -DNETBSD_NATIVE
HOST_CPPFLAGS+=	-I${.CURDIR}/..

# Link in the GMP/MPFR/MPC headers since we don't install them
BUILDSYMLINKS+=	${NETBSDSRCDIR}/external/lgpl3/gmp/lib/libgmp/arch/${GMP_MACHINE_ARCH}/gmp.h gmp.h
BUILDSYMLINKS+=	${NETBSDSRCDIR}/external/lgpl3/mpfr/dist/mpfr.h mpfr.h
BUILDSYMLINKS+=	${NETBSDSRCDIR}/external/lgpl3/mpfr/dist/mpf2mpfr.h mpf2mpfr.h
BUILDSYMLINKS+=	${NETBSDSRCDIR}/external/lgpl2/mpc/dist/src/mpc.h mpc.h
BUILDSYMLINKS+=	${NETBSDSRCDIR}/external/lgpl2/mpc/dist/src/mpc-log.h mpc-log.h
CPPFLAGS+=	-I.
# XXX
DPSRCS+= gmp.h mpfr.h mpf2mpfr.h mpc.h mpc-log.h

.if ${USETOOLS} == "yes"
NBCOMPATLIB=   -L${TOOLDIR}/lib -lnbcompat
.endif

GNUHOSTDIST=	${DIST}
BUILD_PREFIX=	${C_BUILD_PREFIX}

LIBIBERTYOBJ!=		cd ${.CURDIR}/../../lib/libiberty && ${PRINTOBJDIR}
FRONTENDOBJ!=		cd ${.CURDIR}/../frontend && ${PRINTOBJDIR}
BACKENDOBJ!=		cd ${.CURDIR}/../backend && ${PRINTOBJDIR}
LIBCPPOBJ!=		cd ${.CURDIR}/../libcpp && ${PRINTOBJDIR}
LIBDECNUMBEROBJ!=	cd ${.CURDIR}/../libdecnumber && ${PRINTOBJDIR}

HOSTLIBIBERTYOBJ!=	cd ${.CURDIR}/../host-libiberty && ${PRINTOBJDIR}
HOSTLIBIBERTY=		${HOSTLIBIBERTYOBJ}/libiberty/libiberty.a

BASEVER!=		cat ${GNUHOSTDIST}/gcc/BASE-VER
# XXX
#DEVPHASE!=		cat ${GNUHOSTDIST}/gcc/DEV-PHASE
DEVPHASE=
.if ${DEVPHASE} == "release" || ${DEVPHASE} == ""
DATESTAMP=
.else
DATESTAMP!=		cat ${GNUHOSTDIST}/gcc/DATESTAMP
.endif

BASEVERSTR=		"\"$(BASEVER)\""
.if !empty(DEVPHASE)
DEVPHASESTR=		"\" $(DEVPHASE)\""
.else
DEVPHASESTR=		"\"\""
.endif
.if !empty(DATESTAMP)
DATESTAMPSTR=		"\" $(DATESTAMP)\""
.else
DATESTAMPSTR=		"\"\""
.endif

# XXX pull this out of our configs
G_BUGURL=<http://www.NetBSD.org/Misc/send-pr.html>
G_BUGURL_s="\"${G_BUG_URL}\""
G_PKGVERSION=(NetBSD nb1 20120916) 
G_PKGVERSION_s="\"${G_PKGVERSION} \""

VER_CPPFLAGS=			-DBUGURL=${G_BUGURL_s} \
				-DPKGVERSION=${G_PKGVERSION_s} \
				-DBASEVER="\"${BASEVER}"\" \
				-DDATESTAMP=${DATESTAMPSTR} \
				-DDEVPHASE=${DEVPHASE} \
				-DREVISION=${REVISION}
CPPFLAGS.version.c=		${VER_CPPFLAGS}

.include "${GCCARCH}/defs.mk"

.if ${MKPIC} != "no"
.if ${G_ENABLE_SHARED} == "yes" && ${G_SHLIB_LINK} != ""
CPPFLAGS+=	-DENABLE_SHARED_LIBGCC
.endif

.if empty(G_SHLIB_MULTILIB)
CPPFLAGS+=	-DNO_SHARED_LIBGCC_MULTILIB
.endif
.endif

# This depends on the "extern inline" mess, so downgrade to something safe.
# CPPFLAGS is applied after CFLAGS, which gets the -std=gnu99
CPPFLAGS+=	-std=gnu89

NOCLANGERROR=	# defined

.endif
