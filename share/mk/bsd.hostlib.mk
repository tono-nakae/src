#	$NetBSD: bsd.hostlib.mk,v 1.1 2001/11/28 04:42:49 tv Exp $

.include <bsd.init.mk>
.include <bsd.sys.mk>

##### Basic targets
.PHONY:		cleanlib
clean:		cleanlib

##### Default values
CFLAGS+=	${COPTS}

# Override these:
MKDEP:=		CC=${HOST_CC:Q} ${MKDEP:NCC=*}

.if defined(USE_NEW_TOOLCHAIN)
OBJHOSTMACHINE=	# set
.endif

##### Build rules
.if defined(HOSTLIB)
DPSRCS+=	${SRCS:M*.[ly]:C/\..$/.c/}
CLEANFILES+=	${DPSRCS} ${YHEADER:D${SRCS:M*.y:.y=.h}}
.endif	# defined(HOSTLIB)

.if !empty(SRCS:N*.h:N*.sh)
OBJS+=		${SRCS:N*.h:N*.sh:R:S/$/.lo/g}
.endif

.if defined(OBJS) && !empty(OBJS)
.NOPATH: ${OBJS} ${HOSTPROG} ${SRCS:M*.[ly]:C/\..$/.c/} ${YHEADER:D${SRCS:M*.y:.y=.h}}

lib${HOSTLIB}.a: ${DPSRCS} ${OBJS} ${DPADD}
	-rm -f ${.TARGET}
	${HOST_AR} cq ${.TARGET} ${OBJS}
	@${HOST_RANLIB} ${.TARGET}

.endif	# defined(OBJS) && !empty(OBJS)

realall: lib${HOSTLIB}.a

cleanlib:
	rm -f a.out [Ee]rrs mklog core *.core \
	    lib${HOSTLIB}.a ${OBJS} ${CLEANFILES}

beforedepend:
CFLAGS:=	${HOST_CFLAGS}
CPPFLAGS:=	${HOST_CPPFLAGS}

.if defined(SRCS)
afterdepend: .depend
	@(TMP=/tmp/_depend$$$$; \
	    sed -e 's/^\([^\.]*\).o[ ]*:/\1.lo:/' \
	      < .depend > $$TMP; \
	    mv $$TMP .depend)
.endif

##### Pull in related .mk logic
.include <bsd.dep.mk>
.include <bsd.obj.mk>

${TARGETS}:	# ensure existence
