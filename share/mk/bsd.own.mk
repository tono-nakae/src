#	from: @(#)bsd.own.mk	0.1 (RGrimes) 4/4/93
#	$Id: bsd.own.mk,v 1.3 1993/08/15 19:37:09 mycroft Exp $

BINGRP?=	bin
BINOWN?=	bin
BINMODE?=	555

STRIP?=		-s

COPY?=		-c

MANDIR?=	/usr/share/man/cat
MANGRP?=	bin
MANOWN?=	bin
MANMODE?=	444

# Define SYS_INCLUDE to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
#SYS_INCLUDE= 	symlinks
