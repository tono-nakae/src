#	$NetBSD: bsd.info.mk,v 1.23 2001/11/02 05:21:50 tv Exp $

.include <bsd.init.mk>

##### Basic targets
.PHONY:		infoinstall cleaninfo
cleandir:	cleaninfo
realinstall:	infoinstall

##### Default values
MAKEINFO?=	makeinfo
INFOFLAGS?=
INSTALL_INFO?=	install-info

INFOFILES?=

##### Build rules
.if ${MKINFO} != "no"

INFOFILES=	${TEXINFO:C/\.te?xi(nfo)?$/.info/}

realall:	${INFOFILES}
.NOPATH:	${INFOFILES}

.SUFFIXES: .txi .texi .texinfo .info

.txi.info .texi.info .texinfo.info:
	${MAKEINFO} ${INFOFLAGS} --no-split -o $@ $<

.endif # ${MKINFO} != "no"

##### Install rules
infoinstall::	# ensure existence
.if ${MKINFO} != "no"

__infoinstall: .USE
	${INSTALL_FILE} \
	    -o ${INFOOWN_${.ALLSRC:T}:U${INFOOWN}} \
	    -g ${INFOGRP_${.ALLSRC:T}:U${INFOGRP}} \
	    -m ${INFOMODE_${.ALLSRC:T}:U${INFOMODE}} \
	    ${.ALLSRC} ${.TARGET}
	@${INSTALL_INFO} --remove --info-dir=${DESTDIR}${INFODIR} ${.TARGET} 2>/dev/null
	${INSTALL_INFO} --info-dir=${DESTDIR}${INFODIR} ${.TARGET}

.for F in ${INFOFILES:O:u}
_FDIR:=		${INFODIR_${F}:U${INFODIR}}		# dir overrides
_FNAME:=	${INFONAME_${F}:U${INFONAME:U${F:T}}}	# name overrides
_F:=		${DESTDIR}${_FDIR}/${_FNAME}		# installed path

${_F}:		${F} __infoinstall			# install rule
infoinstall::	${_F}
.PRECIOUS:	${_F}					# keep if install fails
.PHONY:		${UPDATE:U${_F}}			# clobber unless UPDATE
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}:		.MADE					# no build at install
.endif
.endfor

.undef _FDIR
.undef _FNAME
.undef _F
.endif # ${MKINFO} != "no"

##### Clean rules
cleaninfo:
.if !empty(INFOFILES)
	rm -f ${INFOFILES}
.endif

##### Pull in related .mk logic
.include <bsd.obj.mk>

${TARGETS}:	# ensure existence
