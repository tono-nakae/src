#	$NetBSD: bsd.files.mk,v 1.23 2003/09/03 07:09:39 lukem Exp $

.if !defined(_BSD_FILES_MK_)
_BSD_FILES_MK_=1

.include <bsd.init.mk>

.if !target(__fileinstall)
##### Basic targets
.PHONY:		filesinstall
realinstall:	filesinstall

##### Default values
FILESDIR?=	${BINDIR}
FILESOWN?=	${BINOWN}
FILESGRP?=	${BINGRP}
FILESMODE?=	${NONBINMODE}

##### Install rules
filesinstall::	# ensure existence

__fileinstall: .USE
	${INSTALL_FILE} \
	    -o ${FILESOWN_${.ALLSRC:T}:U${FILESOWN}} \
	    -g ${FILESGRP_${.ALLSRC:T}:U${FILESGRP}} \
	    -m ${FILESMODE_${.ALLSRC:T}:U${FILESMODE}} \
	    ${SYSPKGTAG} ${.ALLSRC} ${.TARGET}

.endif # !target(__fileinstall)

.for F in ${FILES:O:u}
_FDIR:=		${FILESDIR_${F}:U${FILESDIR}}		# dir override
_FNAME:=	${FILESNAME_${F}:U${FILESNAME:U${F:T}}}	# name override
_F:=		${DESTDIR}${_FDIR}/${_FNAME}		# installed path

.if ${MKUPDATE} == "no"
${_F}!		${F} __fileinstall			# install rule
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}!		.MADE					# no build at install
.endif
.else
${_F}:		${F} __fileinstall			# install rule
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}:		.MADE					# no build at install
.endif
.endif

filesinstall::	${_F}
.PRECIOUS: 	${_F}					# keep if install fails
.endfor

.undef _FDIR
.undef _FNAME
.undef _F


#
# LNFILES
#
.if defined(LNFILES)						# {

.for _SL _TL in ${LNFILES}
LNFILES.s+=	${_SL}
LNFILES.t+=	${_TL}
${_TL}: ${_SL}
	rm -f ${.TARGET}
	ln -s ${.ALLSRC} ${.TARGET}
.endfor

realall: ${LNFILES.t}

.PHONY:   cleanlnfiles
cleandir: cleanlnfiles
cleanlnfiles:
	rm -f ${LNFILES.t}

.endif								# }

.endif	# !defined(_BSD_FILES_MK_)
