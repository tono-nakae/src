#	$NetBSD: bsd.dep.mk,v 1.59 2003/10/19 03:00:55 lukem Exp $

##### Basic targets
.PHONY:		cleandepend
cleandir:	cleandepend
realdepend:	beforedepend .depend afterdepend
.ORDER:		beforedepend .depend afterdepend

beforedepend .depend afterdepend: # ensure existence

##### Default values
MKDEP?=		mkdep

##### Build rules
# some of the rules involve .h sources, so remove them from mkdep line

.if defined(SRCS)							# {
__acpp_flags=	-traditional-cpp

__DPSRCS.all=	${SRCS:C/\.(c|m|s|S|C|cc|cpp|cxx)$/.d/} \
		${DPSRCS:C/\.(c|m|s|S|C|cc|cpp|cxx)$/.d/}
__DPSRCS.d=	${__DPSRCS.all:O:u:M*.d}
__DPSRCS.notd=	${__DPSRCS.all:O:u:N*.d}

.NOPATH: .depend ${__DPSRCS.d}

.if !empty(__DPSRCS.d)							# {
${__DPSRCS.d}: ${__DPSRCS.notd} ${DPSRCS}
.endif									# }

.depend: ${__DPSRCS.d}
	${_MKMSGCREATE}
	${_MKCMD}\
	rm -f .depend
	${_MKCMD}\
	cat ${__DPSRCS.d} /dev/null > .depend

.SUFFIXES: .d .s .S .c .C .cc .cpp .cxx .m

.c.d:
	${_MKMSGCREATE}
	${_MKCMD}\
	${MKDEP} -f ${.TARGET} ${MKDEPFLAGS} ${CFLAGS:M-[ID]*} ${CPPFLAGS} \
	    ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}

.m.d:
	${_MKMSGCREATE}
	${_MKCMD}\
	${MKDEP} -f ${.TARGET} ${MKDEPFLAGS} ${OBJCFLAGS:M-[ID]*} \
	    ${CPPFLAGS} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}

.s.d .S.d:
	${_MKMSGCREATE}
	${_MKCMD}\
	${MKDEP} -f ${.TARGET} ${MKDEPFLAGS} ${AFLAGS:M-[ID]*} ${CPPFLAGS} \
	    ${CPPFLAGS.${.IMPSRC:T}} ${__acpp_flags} ${AINC} ${.IMPSRC}

.C.d .cc.d .cpp.d .cxx.d:
	${_MKMSGCREATE}
	${_MKCMD}\
	${MKDEP} -f ${.TARGET} ${MKDEPFLAGS} ${CXXFLAGS:M-[ID]*} \
	    ${DESTDIR:D-nostdinc++ ${CPPFLAG_ISYSTEMXX} \
	    ${DESTDIR}/usr/include/g++} \
	    ${CPPFLAGS} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}

.endif # defined(SRCS)							# }

##### Clean rules
cleandepend:
.if defined(SRCS)
	${_MKCMD}\
	rm -f .depend ${__DPSRCS.d} ${.CURDIR}/tags ${CLEANDEPEND}
.endif

##### Custom rules
.if !target(tags)
tags: ${SRCS}
.if defined(SRCS)
	${_MKCMD}\
	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC:N*.h} | \
	    sed "s;\${.CURDIR}/;;" > tags
.endif
.endif
