#	$NetBSD: bsd.obj.mk,v 1.30 2001/11/11 22:39:23 tv Exp $

.if !target(__initialized_obj__)
__initialized_obj__:
.include <bsd.own.mk>

__curdir:=	${.CURDIR}

.if ${MKOBJ} == "no"
obj:
.else
.if defined(MAKEOBJDIRPREFIX) || defined(MAKEOBJDIR)
.if defined(MAKEOBJDIRPREFIX)
__objdir:= ${MAKEOBJDIRPREFIX}${.CURDIR}
.else
__objdir:= ${MAKEOBJDIR}
.endif
# MAKEOBJDIR and MAKEOBJDIRPREFIX are env variables supported
# by make(1).  We simply mkdir -p the specified path.
# If that fails - we do a mkdir to get the appropriate error message
# before bailing out.
obj:
	@if [ ! -d ${__objdir} ]; then \
		mkdir -p ${__objdir}; \
		if [ ! -d ${__objdir} ]; then \
			mkdir ${__objdir}; exit 1; \
		fi; \
		echo "${__curdir} -> ${__objdir}"; \
	fi
.else
.if defined(OBJMACHINE)
__objdir=	obj.${MACHINE}
.else
__objdir=	obj
.endif

.if defined(USR_OBJMACHINE)
__usrobjdir=	${BSDOBJDIR}.${MACHINE}
__usrobjdirpf=	
.else
__usrobjdir=	${BSDOBJDIR}
.if defined(OBJMACHINE)
__usrobjdirpf=	.${MACHINE}
.else
__usrobjdirpf=
.endif
.endif

PAWD?=		/bin/pwd

obj:
	@cd ${__curdir}; \
	here=`${PAWD}`/; subdir=$${here#${BSDSRCDIR}/}; \
	if [ "$$here" != "$$subdir/" ]; then \
		if [ ! -d ${__usrobjdir} ]; then \
			echo "BSDOBJDIR ${__usrobjdir} does not exist, bailing..."; \
			exit 1; \
		fi; \
		subdir=$${subdir%/}; \
		dest=${__usrobjdir}/$$subdir${__usrobjdirpf}; \
		if [ -h $$here${__objdir} ]; then \
			curtarg=`ls -ld $$here${__objdir} | awk '{print $$NF}'` ; \
			if [ "$$curtarg" = "$$dest" ]; then \
				: ; \
			else \
				echo "$$here""${__objdir} -> $$dest"; \
				rm -rf ${__objdir}; \
				ln -s $$dest ${__objdir}; \
			fi; \
		else \
			echo "$$here""${__objdir} -> $$dest"; \
			rm -rf ${__objdir}; \
			ln -s $$dest ${__objdir}; \
		fi; \
		if [ ! -d $$dest ]; then \
			mkdir -p $$dest; \
		else \
			true; \
		fi; \
	else \
		true ; \
		dest=$$here${__objdir} ; \
		if [ ! -d ${__objdir} ] || [ -h ${__objdir} ]; then \
			echo "making $$dest" ; \
			rm -f ${__objdir}; \
			mkdir $$dest; \
		fi ; \
	fi;
.endif
.endif

print-objdir:
	@echo ${.OBJDIR}
.endif
