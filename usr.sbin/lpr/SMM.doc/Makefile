#	$NetBSD: Makefile,v 1.5 2003/07/10 10:34:48 lukem Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

DIR=	smm/07.lpd
SRCS=	0.t 1.t 2.t 3.t 4.t 5.t 6.t 7.t
MACROS=	-ms

paper.ps: ${SRCS}
	${TOOL_SOELIM} -I${.CURDIR} ${.ALLSRC} | ${TOOL_TBL} | \
	    ${TOOL_ROFF_PS} ${MACROS} > ${.TARGET}

.include <bsd.doc.mk>
