#	$NetBSD: Makefile,v 1.3 2001/11/22 02:47:25 lukem Exp $
#

PROG=	makefs
SRCS=	makefs.c walk.c \
	ffs.c mkfs.c buf.c \
	misc.c spec.c pack_dev.c stat_flags.c \
	ffs_alloc.c ffs_balloc.c ffs_bswap.c ffs_subr.c ffs_tables.c ufs_bmap.c 
MAN=	makefs.8
DPADD+=	${LIBUTIL}
LDADD+=	-lutil

LSSRC=		${.CURDIR}/../../bin/ls
MKNODSRC=	${.CURDIR}/../../sbin/mknod
MTREESRC=	${.CURDIR}/../../usr.sbin/mtree
SYSSRC=		${.CURDIR}/../../sys

CPPFLAGS+=	-I${.CURDIR} -I${SYSSRC} -I${LSSRC} -I${MKNODSRC} -I${MTREESRC}
.PATH:		${.CURDIR}/ffs ${SYSSRC}/ufs/ffs \
		${LSSRC} ${MKNODSRC} ${MTREESRC}

WARNS?=	2

.include <bsd.prog.mk>
