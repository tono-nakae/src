#	$NetBSD: bsd.own.mk,v 1.198 2001/10/25 22:31:11 bjh21 Exp $

.if !defined(_BSD_OWN_MK_)
_BSD_OWN_MK_=1

MAKECONF?=	/etc/mk.conf

.if exists(${MAKECONF})
.include "${MAKECONF}"
.endif

# Temporary; this will become default when all platforms have migrated.
.if defined(USE_NEW_TOOLCHAIN) && ${USE_NEW_TOOLCHAIN} == "no"
.undef USE_NEW_TOOLCHAIN	# unset
.else
.if ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "sparc64" || \
    ${MACHINE} == "arm26"
USE_NEW_TOOLCHAIN=yes	# set
.endif
.endif

.if defined(BSD_PKG_MK) || !defined(USE_NEW_TOOLCHAIN)
USETOOLS:=	no
.endif
USETOOLS?=	yes

.if ${MACHINE_ARCH} == "mips" || ${MACHINE_ARCH} == "sh3"
.BEGIN:
	@echo "Must set MACHINE_ARCH to one of ${MACHINE_ARCH}eb or ${MACHINE_ARCH}el"
	@false
.elif ${USETOOLS} == "no" && defined(REQUIRETOOLS)
.BEGIN:
	@echo "USETOOLS=no, but this component requires a host toolchain to build"
	@false
.endif

.if ${USETOOLS} != "no"
# Define default locations for common tools.

AR=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-ar
AS=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-as
ASN1_COMPILE=	${TOOLDIR}/bin/asn1_compile
#BIB=		${TOOLDIR}/bin/bib
CC=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-gcc
COMPILE_ET=	${TOOLDIR}/bin/compile_et
CONFIG=		${TOOLDIR}/bin/config
CPP=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-cpp
CRUNCHGEN=	MAKE=${.MAKE} ${TOOLDIR}/bin/crunchgen
CXX=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-c++
#EQN=		${TOOLDIR}/bin/eqn
FC=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-g77
GENCAT=		${TOOLDIR}/bin/gencat
#GRIND=		${TOOLDIR}/bin/vgrind -f
#GROFF=		${TOOLDIR}/bin/groff -Tascii
#INDXBIB=	${TOOLDIR}/bin/indxbib
INSTALL=	STRIP=${STRIP} ${TOOLDIR}/bin/binstall
INSTALL_INFO=	${TOOLDIR}/bin/install-info
LD=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-ld
LEX=		${TOOLDIR}/bin/lex
LINT=		CC="${CC}" ${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-lint
LORDER=		NM="${NM}" ${TOOLDIR}/bin/lorder
#MAKE=		${TOOLDIR}/bin/bmake
MAKEINFO=	${TOOLDIR}/bin/makeinfo
MDSETIMAGE=	${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-mdsetimage
MENUC=		MENUDEF=${TOOLDIR}/share/misc ${TOOLDIR}/bin/menuc
MKDEP=		CC="${CC}" ${TOOLDIR}/bin/mkdep
MKLOCALE=	${TOOLDIR}/bin/mklocale
MSGC=		MSGDEF=${TOOLDIR}/share/misc ${TOOLDIR}/bin/msgc
MTREE=		${TOOLDIR}/bin/mtree
NM=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-nm
#NROFF=		${TOOLDIR}/bin/nroff -Tascii
OBJC=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-gcc
OBJCOPY=	${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-objcopy
#PIC=		${TOOLDIR}/bin/pic
RANLIB=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-ranlib
#REFER=		${TOOLDIR}/bin/refer
#ROFF=		${TOOLDIR}/bin/groff -Tps
RPCGEN=		${TOOLDIR}/bin/rpcgen
SIZE=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-size
#SOELIM=	${TOOLDIR}/bin/soelim
STRIP=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-strip
#TBL=		${TOOLDIR}/bin/tbl
TSORT=		${TOOLDIR}/bin/tsort -q
YACC=		${TOOLDIR}/bin/yacc

# Make sure DESTDIR is set, so that builds with these tools always
# get appropriate -nostdinc, -nostdlib, etc. handling.  The default is
# <empty string>, meaning start from /, the root directory.

DESTDIR?=
.endif

# Defining `SKEY' causes support for S/key authentication to be compiled in.
SKEY=		yes

# where the system object and source trees are kept; can be configurable
# by the user in case they want them in ~/foosrc and ~/fooobj, for example
BSDSRCDIR?=	/usr/src
BSDOBJDIR?=	/usr/obj

BINGRP?=	wheel
BINOWN?=	root
BINMODE?=	555
NONBINMODE?=	444

# Define MANZ to have the man pages compressed (gzip)
#MANZ=		1

MANDIR?=	/usr/share/man
MANGRP?=	wheel
MANOWN?=	root
MANMODE?=	${NONBINMODE}
MANINSTALL?=	maninstall catinstall

INFODIR?=	/usr/share/info
INFOGRP?=	wheel
INFOOWN?=	root
INFOMODE?=	${NONBINMODE}

LIBDIR?=	/usr/lib
LINTLIBDIR?=	/usr/libdata/lint
LIBGRP?=	${BINGRP}
LIBOWN?=	${BINOWN}
LIBMODE?=	${NONBINMODE}

DOCDIR?=	/usr/share/doc
HTMLDOCDIR?=	/usr/share/doc/html
DOCGRP?=	wheel
DOCOWN?=	root
DOCMODE?=	${NONBINMODE}

NLSDIR?=	/usr/share/nls
NLSGRP?=	wheel
NLSOWN?=	root
NLSMODE?=	${NONBINMODE}

KMODDIR?=	/usr/lkm
KMODGRP?=	wheel
KMODOWN?=	root
KMODMODE?=	${NONBINMODE}

LOCALEDIR?=	/usr/share/locale
LOCALEGRP?=	wheel
LOCALEOWN?=	root
LOCALEMODE?=	${NONBINMODE}

COPY?=		-c
PRESERVE?=	${UPDATE:D-p}
RENAME?=	-r
INSTPRIV?=	${UNPRIVILEGED:D-U}
STRIPFLAG?=	-s

# Define SYS_INCLUDE to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
#SYS_INCLUDE= 	symlinks

# The sh3 port is incomplete.
.if ${MACHINE_ARCH} == "sh3eb" || ${MACHINE_ARCH} == "sh3el"
NOLINT=1
NOPROFILE=1
OBJECT_FMT?=COFF
NOPIC?=1
.endif

# Profiling and linting is also off on the x86_64 port at the moment.
# The x86_64 port is incomplete.
.if ${MACHINE_ARCH} == "x86_64"
NOPROFILE=1
NOLINT=1
.endif

# The m68000 port is incomplete.
.if ${MACHINE_ARCH} == "m68000"
NOLINT=1
NOPROFILE=1
NOPIC?=1
.endif

# Data-driven table using make variables to control how 
# toolchain-dependent targets and shared libraries are built
# for different platforms and object formats.
# OBJECT_FMT:		currently either "ELF" or "a.out".
# SHLIB_TYPE:		"ELF" or "a.out" or "" to force static libraries.
#
.if ${MACHINE_ARCH} == "alpha" || \
    ${MACHINE_ARCH} == "mipsel" || ${MACHINE_ARCH} == "mipseb" || \
    ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "sparc" || \
    ${MACHINE_ARCH} == "sparc64" || \
    ${MACHINE_ARCH} == "x86_64" || \
    ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "m68000" || \
    ${MACHINE} == "next68k" || \
    ${MACHINE} == "sun3" || \
    ${MACHINE} == "mvme68k" || \
    ${MACHINE} == "hp300" || \
    ${MACHINE} == "news68k" || \
    ${MACHINE} == "cesfic" || \
    ${MACHINE} == "arm26" || \
    ${MACHINE} == "atari"
OBJECT_FMT?=ELF
.else
OBJECT_FMT?=a.out
.endif

.if ${MACHINE_ARCH} == "x86_64"
CFLAGS+=-Wno-format -fno-builtin
.endif

# Location of the file that contains the major and minor numbers of the
# version of a shared library.  If this file exists a shared library
# will be built by <bsd.lib.mk>.
SHLIB_VERSION_FILE?= ${.CURDIR}/shlib_version

# GNU sources and packages sometimes see architecture names differently.
GNU_ARCH.arm26=arm
GNU_ARCH.arm32=arm
GNU_ARCH.sh3eb=sh
GNU_ARCH.sh3el=sh
MACHINE_GNU_ARCH=${GNU_ARCH.${MACHINE_ARCH}:U${MACHINE_ARCH}}

# In order to identify NetBSD to GNU packages, we sometimes need
# an "elf" tag for historically a.out platforms.
.if ${OBJECT_FMT} == "ELF" && \
    (${MACHINE_GNU_ARCH} == "arm" || \
     ${MACHINE_ARCH} == "i386" || \
     ${MACHINE_ARCH} == "m68k" || \
     ${MACHINE_ARCH} == "sparc" || \
     ${MACHINE_ARCH} == "vax")
MACHINE_GNU_PLATFORM=${MACHINE_GNU_ARCH}--netbsdelf
.else
MACHINE_GNU_PLATFORM=${MACHINE_GNU_ARCH}--netbsd
.endif

# CPU model, derived from MACHINE_ARCH
MACHINE_CPU=	${MACHINE_ARCH:C/mipse[bl]/mips/:S/arm26/arm/:S/arm32/arm/:C/sh3e[bl]/sh3/:S/m68000/m68k/}

TARGETS+=	all clean cleandir depend dependall includes \
		install lint obj regress tags html installhtml cleanhtml
.PHONY:		all clean cleandir depend dependall distclean includes \
		install lint obj regress tags beforedepend afterdepend \
		beforeinstall afterinstall realinstall realdepend realall \
		html installhtml cheanhtml

# set NEED_OWN_INSTALL_TARGET, if it's not already set, to yes
# this is used by bsd.pkg.mk to stop "install" being defined
NEED_OWN_INSTALL_TARGET?=	yes

.if ${NEED_OWN_INSTALL_TARGET} == "yes"
.if !target(install)
install:	.NOTMAIN beforeinstall subdir-install realinstall afterinstall
beforeinstall:	.NOTMAIN
subdir-install:	.NOTMAIN beforeinstall
realinstall:	.NOTMAIN beforeinstall
afterinstall:	.NOTMAIN subdir-install realinstall
.endif
all:		.NOTMAIN realall subdir-all
subdir-all:	.NOTMAIN
realall:	.NOTMAIN
depend:		.NOTMAIN realdepend subdir-depend
subdir-depend:	.NOTMAIN
realdepend:	.NOTMAIN
distclean:	.NOTMAIN cleandir
cleandir:	.NOTMAIN clean
.endif

PRINTOBJDIR=	printf "xxx: .MAKE\n\t@echo \$${.OBJDIR}\n" | ${MAKE} -B -s -f-

# Define MKxxx variables (which are either yes or no) for users
# to set in /etc/mk.conf and override on the make commandline.
# These should be tested with `== "no"' or `!= "no"'.
# The NOxxx variables should only be used by Makefiles.
#

MKCATPAGES?=yes

.if defined(NODOC)
MKDOC=no
#.elif !defined(MKDOC)
#MKDOC=yes
.else
MKDOC?=yes
.endif

MKINFO?=yes

.if defined(NOLINKLIB)
MKLINKLIB=no
.else
MKLINKLIB?=yes
.endif
.if ${MKLINKLIB} == "no"
MKPICINSTALL=no
MKPROFILE=no
.endif

.if defined(NOLINT)
MKLINT=no
.else
MKLINT?=yes
.endif

.if defined(NOMAN)
MKMAN=no
.else
MKMAN?=yes
.endif
.if ${MKMAN} == "no"
MKCATPAGES=no
.endif

.if defined(NONLS)
MKNLS=no
.else
MKNLS?=yes
.endif

#
# MKOBJDIRS controls whether object dirs are created during "make build".
# MKOBJ controls whether the "make obj" rule does anything.
#
.if defined(NOOBJ)
MKOBJ=no
MKOBJDIRS=no
.else
MKOBJ?=yes
MKOBJDIRS?=no
.endif

.if defined(NOPIC)
MKPIC=no
.else
MKPIC?=yes
.endif

.if defined(NOPICINSTALL)
MKPICINSTALL=no
.else
MKPICINSTALL?=yes
.endif

.if defined(NOPROFILE)
MKPROFILE=no
.else
MKPROFILE?=yes
.endif

.if defined(NOSHARE)
MKSHARE=no
.else
MKSHARE?=yes
.endif
.if ${MKSHARE} == "no"
MKCATPAGES=no
MKDOC=no
MKINFO=no
MKMAN=no
MKNLS=no
.endif

.if defined(NOCRYPTO)
MKCRYPTO=no
.else
MKCRYPTO?=yes
.endif

MKCRYPTO_IDEA?=no

MKCRYPTO_RC5?=no

.if defined(NOKERBEROS) || (${MKCRYPTO} == "no")
MKKERBEROS=no
.else
MKKERBEROS?=yes
.endif

MKSOFTFLOAT?=no

.endif		# _BSD_OWN_MK_
