#	$NetBSD: Makefile,v 1.77 1999/01/03 22:17:19 cjs Exp $

.include <bsd.own.mk>			# for configuration variables.

# Configurations variables (can be set either in /etc/mk.conf or
# as environement variable
# NBUILDJOBS: the number of jobs to start in parallel in a 'make build'.
#             defaults to 1
# NOMAN: if set to 1, don't build and install man pages
# NOSHARE: if set to 1, don't build or install /usr/share stuffs
# UPDATE: if set to 1, don't do a 'make cleandir' before compile
# DESTDIR: The target directory for installation (default to '/',
#          which mean the current system is updated).

HAVE_GCC28!=	${CXX} --version | egrep "^(2\.8|egcs)" ; echo

.if defined(NBUILDJOBS)
_J= -j${NBUILDJOBS}
.endif

# NOTE THAT etc *DOES NOT* BELONG IN THE LIST BELOW

SUBDIR+= lib include bin libexec sbin usr.bin usr.sbin share sys

.if exists(games)
SUBDIR+= games
.endif

SUBDIR+= gnu
# This is needed for libstdc++ and gen-params.
includes-gnu: includes-include includes-sys

.if exists(domestic) && (!defined(EXPORTABLE_SYSTEM) ||\
    make(obj) || make(clean) || make(cleandir) || make(distclean))
SUBDIR+= domestic
.endif

.if exists(regress)
.ifmake !(install)
SUBDIR+= regress
.endif

regression-tests:
	@echo Running regression tests...
	@(cd ${.CURDIR}/regress && ${MAKE} regress)
.endif

beforeinstall:
.ifmake build
	@echo -n "Build started at: "
	@date
.endif
.ifndef DESTDIR
	(cd ${.CURDIR}/etc && ${MAKE} DESTDIR=/ distrib-dirs)
.else
	(cd ${.CURDIR}/etc && ${MAKE} distrib-dirs)
.endif

afterinstall:
.if !defined(NOMAN) && !defined(NOSHARE)
	(cd ${.CURDIR}/share/man && ${MAKE} makedb)
.endif

build: beforeinstall
.if !defined(NOSHARE)
	(cd ${.CURDIR}/share/mk && ${MAKE} install)
	(cd ${.CURDIR}/share/tmac && ${MAKE} && ${MAKE} install)
.endif
.if !defined(UPDATE)
	${MAKE} cleandir
.endif
.if empty(HAVE_GCC28)
.if defined(DESTDIR)
	@echo "*** CAPUTE!"
	@echo "    You attempted to compile the world with egcs.  You must"
	@echo "    first install a native egcs compiler."
	false
.else
	(cd ${.CURDIR}/gnu/usr.bin/egcs && \
	    ${MAKE} depend && ${MAKE} ${_J} NOMAN= && \
	    ${MAKE} NOMAN= install && ${MAKE} cleandir)
.endif
.endif
	${MAKE} includes
	(cd ${.CURDIR}/lib/csu && \
	    ${MAKE} depend && ${MAKE} ${_J} NOMAN= && ${MAKE} NOMAN= install)
	(cd ${.CURDIR}/lib && \
	    ${MAKE} depend && ${MAKE} ${_J} NOMAN= && ${MAKE} NOMAN= install)
	(cd ${.CURDIR}/gnu/lib && \
	    ${MAKE} depend && ${MAKE} ${_J} NOMAN= && ${MAKE} NOMAN= install)
.if exists(domestic) && !defined(EXPORTABLE_SYSTEM)
# libtelnet depends on libdes and libkrb.  libkrb depends on
# libcom_err.
.if exists(domestic/lib/libdes)
	(cd ${.CURDIR}/domestic/lib/libdes && \
	    ${MAKE} depend && ${MAKE} ${_J} NOMAN= && ${MAKE} NOMAN= install)
.endif
.if exists(domestic/lib/libcom_err)
	(cd ${.CURDIR}/domestic/lib/libcom_err && \
	    ${MAKE} depend && ${MAKE} ${_J} NOMAN= && ${MAKE} NOMAN= install)
.endif
.if exists(domestic/lib/libkrb)
	(cd ${.CURDIR}/domestic/lib/libkrb && \
	    ${MAKE} depend && ${MAKE} ${_J} NOMAN= && ${MAKE} NOMAN= install)
.endif
	(cd ${.CURDIR}/domestic/lib && \
	    ${MAKE} depend && ${MAKE} ${_J} NOMAN= && ${MAKE} NOMAN= install)
.endif
	${MAKE} depend && ${MAKE} ${_J} && ${MAKE} install
	@echo -n "Build finished at: "
	@date

.include <bsd.subdir.mk>
