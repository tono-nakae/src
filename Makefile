#	$NetBSD: Makefile,v 1.220 2003/07/25 19:20:47 mrg Exp $

#
# This is the top-level makefile for building NetBSD. For an outline of
# how to build a snapshot or release, as well as other release engineering
# information, see http://www.netbsd.org/developers/releng/index.html
#
# Not everything you can set or do is documented in this makefile. In
# particular, you should review the files in /usr/share/mk (especially
# bsd.README) for general information on building programs and writing
# Makefiles within this structure, and see the comments in src/etc/Makefile
# for further information on installation and release set options.
#
# Variables listed below can be set on the make command line (highest
# priority), in /etc/mk.conf (middle priority), or in the environment
# (lowest priority).
#
# Variables:
#   DESTDIR is the target directory for installation of the compiled
#	software. It defaults to /. Note that programs are built against
#	libraries installed in DESTDIR.
#   MKMAN, if `no', will prevent building of manual pages.
#   MKOBJDIRS, if not `no', will build object directories at
#	an appropriate point in a build.
#   MKSHARE, if `no', will prevent building and installing
#	anything in /usr/share.
#   MKUPDATE, if not `no', will avoid a `make cleandir' at the start of
#	`make build', as well as having the effects listed in
#	/usr/share/mk/bsd.README.
#   NOCLEANDIR, if defined, will avoid a `make cleandir' at the start
#	of the `make build'.
#   NOINCLUDES will avoid the `make includes' usually done by `make build'.
#
#   See mk.conf(5) for more details.
#
#
# Targets:
#   build:
#	Builds a full release of NetBSD in DESTDIR, except for the
#	/etc configuration files.
#	If BUILD_DONE is set, this is an empty target.
#   distribution:
#	Builds a full release of NetBSD in DESTDIR, including the /etc
#	configuration files.
#   buildworld:
#	As per `make distribution', except that it ensures that DESTDIR
#	is not the root directory.
#   installworld:
#	Install the distribution from DESTDIR to INSTALLWORLDDIR (which
#	defaults to the root directory).  Ensures that INSTALLWORLDDIR
#	is the not root directory if cross compiling.
#   release:
#	Does a `make build', and then tars up the DESTDIR files
#	into RELEASEDIR/${MACHINE}, in release(7) format.
#	(See etc/Makefile for more information on this.)
#   regression-tests:
#	Runs the regression tests in "regress" on this host.
#   sets:
#	Populate ${RELEASEDIR}/${MACHINE}/binary/sets from ${DESTDIR}
#   sourcesets:
#	Populate ${RELEASEDIR}/source/sets from ${NETBSDSRCDIR}
#
# Targets invoked by `make build,' in order:
#   cleandir:        cleans the tree.
#   obj:             creates object directories.
#   do-tools:        builds host toolchain.
#   do-distrib-dirs: creates the distribution directories.
#   includes:        installs include files.
#   do-tools-compat: builds the "libnbcompat" library; needed for some
#                    random host tool programs in the source tree.
#   do-gnu-lib-libgcc: builds and installs prerequisites from gnu/lib/libgcc
#   do-lib-csu:      builds and installs prerequisites from lib/csu.
#   do-lib-libc:     builds and installs prerequisites from lib/libc.
#   do-lib-libdes:   builds and installs prerequisites from lib/libdes.
#   do-lib:          builds and installs prerequisites from lib.
#   do-gnu-lib:      builds and installs prerequisites from gnu/lib.
#   do-ld.so:        builds and installs prerequisites from libexec/ld.*_so.
#   do-build:        builds and installs the entire system.
#   do-obsolete:     builds and installs the entire system.
#

.if ${.MAKEFLAGS:M${.CURDIR}/share/mk} == ""
.MAKEFLAGS: -m ${.CURDIR}/share/mk
.endif

#
# If _SRC_TOP_OBJ_ gets set here, we will end up with a directory that may
# not be the top level objdir, because "make obj" can happen in the *middle*
# of "make build" (long after <bsd.own.mk> is calculated it).  So, pre-set
# _SRC_TOP_OBJ_ here so it will not be added to ${.MAKEOVERRIDES}.
#
_SRC_TOP_OBJ_=

.include <bsd.own.mk>

#
# Sanity check: make sure that "make build" is not invoked simultaneously
# with a standard recursive target.
#

.if make(build) || make(release) || make(snapshot)
.for targ in ${TARGETS:Nobj:Ncleandir}
.if make(${targ}) && !target(.BEGIN)
.BEGIN:
	@echo 'BUILD ABORTED: "make build" and "make ${targ}" are mutually exclusive.'
	@false
.endif
.endfor
.endif

_SUBDIR=	tools lib include gnu bin games libexec sbin usr.bin
_SUBDIR+=	usr.sbin share rescue sys etc .WAIT distrib regress

#
# Weed out directories that don't exist.
#

.for dir in ${_SUBDIR}
.if exists(${dir}/Makefile) && (${BUILD_${dir}:Uyes} != "no")
SUBDIR+=	${dir}
.endif
.endfor

.if exists(regress)
regression-tests:
	@echo Running regression tests...
	@(cd ${.CURDIR}/regress && ${MAKE} regress)
.endif

.if ${MKUNPRIVED} != "no"
NOPOSTINSTALL=	# defined
.endif

afterinstall:
.if ${MKMAN} != "no"
	(cd ${.CURDIR}/share/man && ${MAKE} makedb)
.endif
.if (${MKUNPRIVED} != "no" && ${MKINFO} != "no")
	(cd ${.CURDIR}/gnu/usr.bin/texinfo/install-info && ${MAKE} infodir-meta)
.endif
.if !defined(NOPOSTINSTALL)
	(cd ${.CURDIR} && ${MAKE} postinstall-check)
.endif

postinstall-check:
	@echo "   === Post installation checks ==="
	${HOST_SH} ${.CURDIR}/etc/postinstall -s ${.CURDIR} -d ${DESTDIR}/ check
	@echo "   ================================"

postinstall-fix: .NOTMAIN
	@echo "   === Post installation fixes ==="
	${HOST_SH} ${.CURDIR}/etc/postinstall -s ${.CURDIR} -d ${DESTDIR}/ fix
	@echo "   ==============================="

postinstall-fix-obsolete: .NOTMAIN
	@echo "   === Removing obsolete files ==="
	${HOST_SH} ${.CURDIR}/etc/postinstall -s ${.CURDIR} -d ${DESTDIR}/ fix obsolete
	@echo "   ==============================="


#
# Targets (in order!) called by "make build".
#
.if ${USE_TOOLS_TOOLCHAIN} == "no"
LIBGCC_EXT=3
.else
LIBGCC_EXT=
.endif

BUILDTARGETS+=	check-tools
.if ${MKUPDATE} == "no" && !defined(NOCLEANDIR)
BUILDTARGETS+=	cleandir
.endif
.if ${MKOBJDIRS} != "no"
BUILDTARGETS+=	obj
.endif
.if ${USETOOLS} == "yes"
BUILDTARGETS+=	do-tools
.endif
.if !defined(NODISTRIBDIRS)
BUILDTARGETS+=	do-distrib-dirs
.endif
.if !defined(NOINCLUDES)
BUILDTARGETS+=	includes
.endif
BUILDTARGETS+=	do-tools-compat
.if ${MKGCC} != "no"
BUILDTARGETS+=	do-gnu-lib-libgcc${LIBGCC_EXT}
.endif
BUILDTARGETS+=	do-lib-csu do-lib-libc do-lib-libdes do-lib do-gnu-lib \
		do-ld.so do-build
BUILDTARGETS+=	do-obsolete

#
# Enforce proper ordering of some rules.
#

.ORDER:		${BUILDTARGETS}
includes-lib:	includes-include includes-sys
includes-gnu:	includes-lib

#
# Build the system and install into DESTDIR.
#

START_TIME!=	date

build:
.if defined(BUILD_DONE)
	@echo "Build already installed into ${DESTDIR}"
.else
	@echo "Build started at: ${START_TIME}"
.for tgt in ${BUILDTARGETS}
	@(cd ${.CURDIR} && ${MAKE} ${tgt})
.endfor
	@echo   "Build started at:  ${START_TIME}"
	@printf "Build finished at: " && date
.endif

#
# Build a full distribution, but not a release (i.e. no sets into
# ${RELEASEDIR}).  "buildworld" enforces a build to ${DESTDIR} != /
#

distribution buildworld:
.if make(buildworld) && \
    (!defined(DESTDIR) || ${DESTDIR} == "" || ${DESTDIR} == "/")
	@echo "Won't make ${.TARGET} with DESTDIR=/"
	@false
.endif
	(cd ${.CURDIR} && ${MAKE} NOPOSTINSTALL=1 build)
	(cd ${.CURDIR}/etc && ${MAKE} INSTALL_DONE=1 distribution)
.if defined(DESTDIR) && ${DESTDIR} != "" && ${DESTDIR} != "/"
	(cd ${.CURDIR} && ${MAKE} postinstall-fix-obsolete)
	(cd ${.CURDIR}/distrib/sets && ${MAKE} checkflist)
.endif
	@echo   "make ${.TARGET} started at:  ${START_TIME}"
	@printf "make ${.TARGET} finished at: " && date

#
# Install the distribution from $DESTDIR to $INSTALLWORLDDIR (defaults to `/')
# If installing to /, ensures that the host's operating system is NetBSD and
# the host's `uname -m` == ${MACHINE}.
#

HOST_UNAME_S!=	uname -s
HOST_UNAME_M!=	uname -m

installworld:
.if (!defined(DESTDIR) || ${DESTDIR} == "" || ${DESTDIR} == "/")
	@echo "Can't make ${.TARGET} to DESTDIR=/"
	@false
.endif
.if !defined(INSTALLWORLDDIR) || \
    ${INSTALLWORLDDIR} == "" || ${INSTALLWORLDDIR} == "/"
.if (${HOST_UNAME_S} != "NetBSD")
	@echo "Won't cross-make ${.TARGET} from ${HOST_UNAME_S} to NetBSD with INSTALLWORLDDIR=/"
	@false
.endif
.if (${HOST_UNAME_M} != ${MACHINE})
	@echo "Won't cross-make ${.TARGET} from ${HOST_UNAME_M} to ${MACHINE} with INSTALLWORLDDIR=/"
	@false
.endif
.endif
	(cd ${.CURDIR}/distrib/sets && \
	    ${MAKE} INSTALLDIR=${INSTALLWORLDDIR:U/} INSTALLSETS= installsets)
	(cd ${.CURDIR} && \
	    ${MAKE} DESTDIR=${INSTALLWORLDDIR} postinstall-check)
	@echo   "make ${.TARGET} started at:  ${START_TIME}"
	@printf "make ${.TARGET} finished at: " && date

#
# Create sets from $DESTDIR or $NETBSDSRCDIR into $RELEASEDIR
#

.for tgt in sets sourcesets
${tgt}:
	(cd ${.CURDIR}/distrib/sets && ${MAKE} $@)
.endfor

#
# Build a release or snapshot (implies "make build").  Note that
# in this case, the set lists will be checked before the tar files
# are made.
#

release snapshot:
	(cd ${.CURDIR} && ${MAKE} NOPOSTINSTALL=1 build)
	(cd ${.CURDIR}/etc && ${MAKE} INSTALL_DONE=1 release)
	@echo   "make ${.TARGET} started at:  ${START_TIME}"
	@printf "make ${.TARGET} finished at: " && date

#
# Special components of the "make build" process.
#

check-tools:
.if ${TOOLCHAIN_MISSING} == "yes" && !defined(EXTERNAL_TOOLCHAIN)
	@echo '*** WARNING:  Building on MACHINE=${MACHINE} with missing toolchain.'
	@echo '*** May result in a failed build or corrupt binaries!'
.elif defined(EXTERNAL_TOOLCHAIN)
	@echo '*** Using external toolchain rooted at ${EXTERNAL_TOOLCHAIN}.'
.endif
.if defined(NBUILDJOBS)
	@echo '*** WARNING: NBUILDJOBS is obsolete; use -j directly instead!'
.endif

do-distrib-dirs:
.if !defined(DESTDIR) || ${DESTDIR} == ""
	(cd ${.CURDIR}/etc && ${MAKE} DESTDIR=/ distrib-dirs)
.else
	(cd ${.CURDIR}/etc && ${MAKE} DESTDIR=${DESTDIR} distrib-dirs)
.endif

.for targ in cleandir obj includes
do-${targ}: ${targ}
	@true
.endfor

.for dir in tools tools/compat lib/csu gnu/lib/libgcc${LIBGCC_EXT} lib/libc lib/libdes lib gnu/lib
do-${dir:S/\//-/g}:
.for targ in dependall install
	(cd ${.CURDIR}/${dir} && ${MAKE} ${targ})
.endfor
.endfor

do-ld.so:
.for targ in dependall install
.if (${OBJECT_FMT} == "a.out")
	(cd ${.CURDIR}/libexec/ld.aout_so && ${MAKE} ${targ})
.endif
.if (${OBJECT_FMT} == "ELF")
	(cd ${.CURDIR}/libexec/ld.elf_so && ${MAKE} ${targ})
.endif
.endfor

do-build:
.for targ in dependall install
	(cd ${.CURDIR} && ${MAKE} ${targ} BUILD_tools=no BUILD_lib=no)
.endfor

do-obsolete:
	(cd ${.CURDIR}/etc && ${MAKE} install-obsolete-lists)

#
# Speedup stubs for some subtrees that don't need to run these rules.
# (Tells <bsd.subdir.mk> not to recurse for them.)
#

.for dir in bin etc distrib games libexec regress sbin usr.sbin tools
includes-${dir}:
	@true
.endfor
.for dir in etc distrib regress
install-${dir}:
	@true
.endfor

#
# XXX this needs to change when distrib Makefiles are recursion compliant
# XXX many distrib subdirs need "cd etc && make snap_pre snap_kern" first...
#
dependall-distrib depend-distrib all-distrib:
	@true

.include <bsd.sys.mk>
.include <bsd.obj.mk>
.include <bsd.kernobj.mk>
.include <bsd.subdir.mk>

build-docs: ${.CURDIR}/BUILDING
${.CURDIR}/BUILDING: doc/BUILDING.mdoc
	${TOOL_GROFF} -mdoc -Tascii -P-bou $> >$@


#
# Display current make(1) parameters
#
params:
.for var in	BSDSRCDIR BSDOBJDIR BUILDID DESTDIR EXTERNAL_TOOLCHAIN \
		KERNARCHDIR KERNCONFDIR KERNOBJDIR KERNSRCDIR \
		MACHINE MACHINE_ARCH MAKECONF MAKEFLAGS \
		MAKEOBJDIR MAKEOBJDIRPREFIX \
		MKOBJDIRS MKUNPRIVED MKUPDATE \
		RELEASEDIR TOOLCHAIN_MISSING TOOLDIR \
		USETOOLS
.if defined(${var})
	@printf "%20s = '%-s'\n" ${var} ${${var}:Q}
.else
	@printf "%20s = (undefined)\n" ${var}
.endif
.endfor
