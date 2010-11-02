#	$NetBSD: compatsubdir.mk,v 1.4 2010/11/02 16:34:33 joerg Exp $

# Build netbsd libraries.

.include <bsd.own.mk>

.if ${MKCOMPAT} != "no"
.if !make(includes)

# make sure we get an objdir built early enough
.include <bsd.prog.mk>

# XXX make this use MAKEOBJDIR
MAKEDIRTARGETENV=	MAKEOBJDIRPREFIX=${.OBJDIR} MKOBJDIRS=yes MKSHARE=no BSD_MK_COMPAT_FILE=${BSD_MK_COMPAT_FILE}

# XXX fix the "library" list to include all 'external' libs?
.if defined(BOOTSTRAP_SUBDIRS)
SUBDIR=	${BOOTSTRAP_SUBDIRS}
.else
SUBDIR= ../../../gnu/lib/crtstuff4 .WAIT \
	../../../lib/csu .WAIT \
	../../../gnu/lib/libgcc4 .WAIT \
	../../../lib/libc .WAIT \
	../../../lib/libutil .WAIT \
	../../../lib .WAIT \
	../../../gnu/lib \
	../../../external/bsd/bind/lib \
	../../../external/bsd/libevent/lib \
	../../../external/bsd/file/lib \
	../../../external/public-domain/xz/lib \
	../../../libexec/ld.elf_so

.if ${MKATF} != "no"
SUBDIR+= ../../../external/bsd/atf/lib
.endif

.if (${MKLDAP} != "no")
SUBDIR+= ../../../external/bsd/openldap/lib
.endif

.if (${MKBINUTILS} != "no")
SUBDIR+= ../../../external/gpl3/binutils/lib
.endif

.if (${MKISCSI} != "no")
SUBDIR+= ../../../external/bsd/iscsi/lib
.endif

.endif

.include <bsd.subdir.mk>

.endif
.endif
