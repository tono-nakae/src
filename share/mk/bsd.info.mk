#	$NetBSD: bsd.info.mk,v 1.15 2000/06/08 03:30:58 mycroft Exp $

.if !target(__initialized__)
__initialized__:
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.include <bsd.own.mk>
.include <bsd.obj.mk>
.include <bsd.depall.mk>
.MAIN:		all
.endif

MAKEINFO?=	makeinfo
INFOFLAGS?=	
INSTALL_INFO?=	install-info

.PHONY:		infoinstall cleaninfo
.if ${MKINFO} != "no"
realinstall:	infoinstall
.endif
cleandir distclean: cleaninfo

.SUFFIXES: .txi .texi .texinfo .info

.txi.info .texi.info .texinfo.info:
	@${MAKEINFO} ${INFOFLAGS} --no-split -o $@ $<

.if defined(TEXINFO) && !empty(TEXINFO)
INFOFILES=	${TEXINFO:C/\.te?xi(nfo)?$/.info/}
.NOPATH:	${INFOFILES}

.if ${MKINFO} != "no"
realall: ${INFOFILES}
.endif

cleaninfo:
	rm -f ${INFOFILES}

infoinstall:: ${INFOFILES:@F@${DESTDIR}${INFODIR_${F}:U${INFODIR}}/${INFONAME_${F}:U${INFONAME:U${F:T}}}@}
.PRECIOUS: ${INFOFILES:@F@${DESTDIR}${INFODIR_${F}:U${INFODIR}}/${INFONAME_${F}:U${INFONAME:U${F:T}}}@}
.if !defined(UPDATE)
.PHONY: ${INFOFILES:@F@${DESTDIR}${INFODIR_${F}:U${INFODIR}}/${INFONAME_${F}:U${INFONAME:U${F:T}}}@}
.endif

__infoinstall: .USE
	@${INSTALL_INFO} --remove --info-dir=${DESTDIR}${INFODIR} ${DESTDIR}${INFODIR}/${.ALLSRC}
	${INSTALL} ${RENAME} ${PRESERVE} ${COPY} ${INSTPRIV} \
	    -o ${INFOOWN_${.ALLSRC}:U${INFOOWN}} \
	    -g ${INFOGRP_${.ALLSRC}:U${INFOGRP}} \
	    -m ${INFOMODE_${.ALLSRC}:U${INFOMODE}} \
	    ${.ALLSRC} ${.TARGET}
	${INSTALL_INFO} --info-dir=${DESTDIR}${INFODIR} ${DESTDIR}${INFODIR}/${.ALLSRC}

.for F in ${INFOFILES}
.if !defined(BUILD) && !make(all) && !make(${F})
${DESTDIR}${INFODIR_${F}:U${INFODIR}}/${INFONAME_${F}:U${INFONAME:U${F:T}}}: .MADE
.endif
${DESTDIR}${INFODIR_${F}:U${INFODIR}}/${INFONAME_${F}:U${INFONAME:U${F:T}}}: ${F} __infoinstall
.endfor
.else
cleaninfo:
.endif

# Make sure all of the standard targets are defined, even if they do nothing.
clean depend includes lint regress tags:
