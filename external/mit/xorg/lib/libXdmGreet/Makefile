#	$NetBSD: Makefile,v 1.3 2013/06/06 09:13:41 mrg Exp $

.include <bsd.own.mk>

LIB=	XdmGreet
.PATH:	${X11SRCDIR.xdm}/greeter

SRCS=	\
	Login.c \
	greet.c \
	verify.c

CPPFLAGS+=	-I${X11SRCDIR.xdm}/include -DRETSIGTYPE=void
# XXX: Enabling this, which should allow --export-dynamic to
# be elided, causes xdm to fail to accept password input and
# then crash at login "failure".
#CPPFLAGS+=	-DGREET_LIB

LIBDPLIBS=\
	Xext	${.CURDIR}/../libXext \
	Xt	${.CURDIR}/../libXt \
	Xdmcp	${.CURDIR}/../libXdmcp \
	Xinerama	${.CURDIR}/../libXinerama \
	Xft	${.CURDIR}/../libXft \
	Xpm	${.CURDIR}/../libXpm \
	X11	${.CURDIR}/../libX11/dynamic \
	pam	${.CURDIR}/../../../../../lib/libpam/libpam \
	crypt	${.CURDIR}/../../../../../lib/libcrypt \
	util	${.CURDIR}/../../../../../lib/libutil

.include "../../bin/xdm/Makefile.xdm"

.include <bsd.x11.mk>
.include <bsd.lib.mk>
