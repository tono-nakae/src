#! /usr/bin/env sh
#  $NetBSD: build.sh,v 1.76 2002/12/09 12:49:55 scw Exp $
#
# Top level build wrapper, for a system containing no tools.
#
# This script should run on any POSIX-compliant shell.  For systems
# with a strange /bin/sh, "ksh" or "bash" may be an ample alternative.
#
# Note, however, that due to the way the interpreter is invoked above,
# if a POSIX-compliant shell is the first in the PATH, you won't have
# to take any further action.
#

bomb () {
	echo ""
	echo "ERROR: $@"
	echo "*** BUILD ABORTED ***"
	exit 1
}
[ -d usr.bin/make ] || bomb "build.sh must be run from the top source level"
[ -f share/mk/bsd.own.mk ] || bomb "src/share/mk is missing; please re-fetch the source tree"

# If $PWD is a valid name of the current directory, POSIX mandates that pwd
# return it by default which causes problems in the presence of symlinks.
# Unsetting PWD is simpler than changing every occurrence of pwd to use -P.
#
# XXX Except that doesn't work on Solaris.
unset PWD
if [ "x`uname -s`" = "xSunOS" ]; then
	TOP=`pwd -P`
else
	TOP=`pwd`
fi

getarch () {
	# Translate a MACHINE into a default MACHINE_ARCH.
	case $MACHINE in
		acorn26|acorn32|cats|netwinder|shark|*arm)
			MACHINE_ARCH=arm;;

		sun2)
			MACHINE_ARCH=m68000;;

		amiga|atari|cesfic|hp300|sun3|*68k)
			MACHINE_ARCH=m68k;;

		mipsco|newsmips|sbmips|sgimips)
			MACHINE_ARCH=mipseb;;

		algor|arc|cobalt|evbmips|hpcmips|playstation2|pmax)
			MACHINE_ARCH=mipsel;;

		pc532)
			MACHINE_ARCH=ns32k;;

		bebox|prep|sandpoint|*ppc)
			MACHINE_ARCH=powerpc;;

		evbsh3|mmeye)
			MACHINE_ARCH=sh3eb;;

		dreamcast|hpcsh)
			MACHINE_ARCH=sh3el;;

		hp700)
			MACHINE_ARCH=hppa;;

		evbsh5)
			MACHINE_ARCH=sh5el;;

		alpha|i386|sparc|sparc64|vax|x86_64)
			MACHINE_ARCH=$MACHINE;;

		*)	bomb "unknown target MACHINE: $MACHINE";;
	esac
}

validatearch () {
	# Ensure that the MACHINE_ARCH exists (and is supported by build.sh).
	case $MACHINE_ARCH in
		alpha|arm|armeb|hppa|i386|m68000|m68k|mipse[bl]|ns32k|powerpc|sh[35]e[bl]|sparc|sparc64|vax|x86_64)
			;;

		*)	bomb "unknown target MACHINE_ARCH: $MACHINE_ARCH";;
	esac
}

getmakevar () {
	$make -m ${TOP}/share/mk -s -f- _x_ <<EOF
_x_:
	echo \${$1}
.include <bsd.prog.mk>
.include <bsd.kernobj.mk>
EOF
}

resolvepath () {
	case $OPTARG in
	/*)	;;
	*)	OPTARG="$TOP/$OPTARG";;
	esac
}

usage () {
	cat <<_usage_
Usage:
`basename $0` [-bdEnortUu] [-a arch] [-B buildid] [-D dest] [-j njob] [-k kernel]
	   [-M obj] [-m mach] [-O obj] [-R release] [-T tools] [-w wrapper]

    -a arch	set MACHINE_ARCH to arch (otherwise deduced from MACHINE)
    -B buildid	set BUILDID to buildid
    -b		build nbmake and nbmake wrapper script, if needed
    -D dest	set DESTDIR to dest
    -d		build a full distribution into DESTDIR (including etc files)
    -E		set "expert" mode; disables some DESTDIR checks
    -j njob	run up to njob jobs in parallel; see make(1)
    -k kernel	build a kernel using the named configuration file
    -M obj	set obj root directory to obj (sets MAKEOBJDIRPREFIX)
    -m mach	set MACHINE to mach (not required if NetBSD native)
    -n		show commands that would be executed, but do not execute them
    -O obj	set obj root directory to obj (sets a MAKEOBJDIR pattern)
    -o		set MKOBJDIRS=no (do not create objdirs at start of build)
    -R release	build a release (and set RELEASEDIR to release)
    -r		remove contents of TOOLDIR and DESTDIR before building
    -T tools	set TOOLDIR to tools
    -t		build and install tools only (implies -b)
    -U		set UNPRIVED
    -u		set UPDATE
    -w wrapper	create nbmake script at wrapper
		(default TOOLDIR/bin/nbmake-MACHINE)

Note: if -T is unset and TOOLDIR is not set in the environment,
      nbmake will be [re]built unconditionally.
_usage_
	exit 1
}

# Set defaults.
MAKEFLAGS=
buildtarget=build
do_buildsystem=true
do_buildkernel=false
do_buildtools=false
do_rebuildmake=false
do_removedirs=false
expert_mode=false
makeenv=
makewrapper=
opt_a=no
opts='a:B:bD:dEhj:k:M:m:nO:oR:rT:tUuw:'
runcmd=

if type getopts >/dev/null 2>&1; then
	# Use POSIX getopts.
	getoptcmd='getopts $opts opt && opt=-$opt'
	optargcmd=':'
else
	type getopt >/dev/null 2>&1 || bomb "/bin/sh shell is too old; try ksh or bash"

	# Use old-style getopt(1) (doesn't handle whitespace in args).
	args="`getopt $opts $*`"
	[ $? = 0 ] || usage
	set -- $args

	getoptcmd='[ $# -gt 0 ] && opt="$1" && shift'
	optargcmd='OPTARG="$1"; shift'
fi

# Parse command line options.
while eval $getoptcmd; do case $opt in
	-a)	eval $optargcmd
		MACHINE_ARCH=$OPTARG; opt_a=yes;;

	-B)	eval $optargcmd
		BUILDID=$OPTARG;;

	-b)	do_buildsystem=false;;

	-D)	eval $optargcmd; resolvepath
		DESTDIR="$OPTARG"; export DESTDIR
		makeenv="$makeenv DESTDIR";;

	-d)	buildtarget=distribution;;

	-E)	expert_mode=true;;

	-j)	eval $optargcmd
		parallel="-j $OPTARG";;

	-k)	do_buildkernel=true; do_buildsystem=false
		eval $optargcmd
		kernconfname=$OPTARG;;

	-M)	eval $optargcmd; resolvepath
		MAKEOBJDIRPREFIX="$OPTARG"; export MAKEOBJDIRPREFIX
		makeobjdir=$OPTARG
		makeenv="$makeenv MAKEOBJDIRPREFIX";;

	# -m overrides MACHINE_ARCH unless "-a" is specified
	-m)	eval $optargcmd
		MACHINE=$OPTARG; [ "$opt_a" != "yes" ] && getarch;;

	-n)	runcmd=echo;;

	-O)	eval $optargcmd; resolvepath
		MAKEOBJDIR="\${.CURDIR:C,^$TOP,$OPTARG,}"; export MAKEOBJDIR
		makeobjdir=$OPTARG
		makeenv="$makeenv MAKEOBJDIR";;

	-o)	MKOBJDIRS=no;;

	-R)	eval $optargcmd; resolvepath
		RELEASEDIR=$OPTARG; export RELEASEDIR
		makeenv="$makeenv RELEASEDIR"
		buildtarget=release;;

	-r)	do_removedirs=true; do_rebuildmake=true;;

	-T)	eval $optargcmd; resolvepath
		TOOLDIR="$OPTARG"; export TOOLDIR;;

	-t)	do_buildtools=true; do_buildsystem=false;;

	-U)	UNPRIVED=yes; export UNPRIVED
		makeenv="$makeenv UNPRIVED";;

	-u)	UPDATE=yes; export UPDATE
		makeenv="$makeenv UPDATE";;

	-w)	eval $optargcmd; resolvepath
		makewrapper="$OPTARG";;

	--)		break;;
	-'?'|-h)	usage;;
esac; done

# Set up MACHINE*.  On a NetBSD host, these are allowed to be unset.
if [ -z "$MACHINE" ]; then
	if [ "`uname -s 2>/dev/null`" != "NetBSD" ]; then
		echo "MACHINE must be set, or -m must be used, for cross builds."
		echo ""; usage
	fi
	MACHINE=`uname -m`
fi
[ -n "$MACHINE_ARCH" ] || getarch
validatearch

# Set up default make(1) environment.
makeenv="$makeenv TOOLDIR MACHINE MACHINE_ARCH MAKEFLAGS"
if [ ! -z "$BUILDID" ]; then
	makeenv="$makeenv BUILDID"
fi
MAKEFLAGS="-m $TOP/share/mk $MAKEFLAGS MKOBJDIRS=${MKOBJDIRS-yes}"
export MAKEFLAGS MACHINE MACHINE_ARCH

# Test make source file timestamps against installed nbmake binary,
# if TOOLDIR is pre-set.
#
# Note that we do NOT try to grovel "mk.conf" here to find out if TOOLDIR
# is set there, because it can contain make variable expansions and other
# stuff only parseable *after* we have a working nbmake.  So this logic
# can only work if the user has pre-set TOOLDIR in the environment or
# used the -T option to build.sh.
#
make="${TOOLDIR-nonexistent}/bin/nbmake"
if [ -x $make ]; then
	for f in usr.bin/make/*.[ch] usr.bin/make/lst.lib/*.[ch]; do
		if [ $f -nt $make ]; then
			do_rebuildmake=true; break
		fi
	done
else
	do_rebuildmake=true
fi

# Build bootstrap nbmake if needed.
if $do_rebuildmake; then
	$runcmd echo "===> Bootstrapping nbmake"
	tmpdir="${TMPDIR-/tmp}/nbbuild$$"

	$runcmd mkdir "$tmpdir" || bomb "cannot mkdir: $tmpdir"
	trap "cd /; rm -r -f \"$tmpdir\"" 0
	trap "exit 1" 1 2 3 15
	$runcmd cd "$tmpdir"

	$runcmd env CC="${HOST_CC-cc}" CPPFLAGS="${HOST_CPPFLAGS}" \
		CFLAGS="${HOST_CFLAGS--O}" LDFLAGS="${HOST_LDFLAGS}" \
		"$TOP/tools/make/configure" \
		|| bomb "configure of nbmake failed"
	$runcmd sh buildmake.sh || bomb "build of nbmake failed"

	make="$tmpdir/nbmake"
	$runcmd cd "$TOP"
	$runcmd rm -f usr.bin/make/*.o usr.bin/make/lst.lib/*.o
fi

EXTERNAL_TOOLCHAIN=`getmakevar EXTERNAL_TOOLCHAIN`
if [ "$runcmd" = "echo" ]; then
	TOOLCHAIN_MISSING=no
else
	TOOLCHAIN_MISSING=`getmakevar TOOLCHAIN_MISSING`
fi
if [ "${TOOLCHAIN_MISSING}" = "yes" -a \
     "${EXTERNAL_TOOLCHAIN}" = "" ]; then
	echo "ERROR: build.sh (in-tree cross-toolchain) is not yet available for"
	echo
	echo "MACHINE: ${MACHINE}"
	echo "MACHINE_ARCH: ${MACHINE_ARCH}"
	echo
	echo "All builds for this platform should be done via a traditional make"
	echo
	echo "If you wish to use an external cross-toolchain, set"
	echo
	echo "EXTERNAL_TOOLCHAIN=<path to toolchain root>"
	echo
	echo "in either the environment or mk.conf and rerun"
	echo
	echo "$0 $*"
	exit 1
fi

# If TOOLDIR isn't already set, make objdirs in "tools" in case the
# default setting from <bsd.own.mk> is used.
if [ -z "$TOOLDIR" ] && [ "$MKOBJDIRS" != "no" ]; then
	$runcmd cd tools
	$runcmd $make -m ${TOP}/share/mk obj NOSUBDIR= \
		|| bomb "make obj failed in tools"
	$runcmd cd "$TOP"
fi

#
# If setting -M or -O to root an obj dir make sure the base directory is made
# before continuing as bsd.own.mk will need this to pick up _SRC_TOP_OBJ_
#
if [ "$MKOBJDIRS" != "no" ] && [ ! -z "$makeobjdir" ]; then
	$runcmd mkdir -p "$makeobjdir"
fi

# Find DESTDIR and TOOLDIR.
if [ "$runcmd" = "echo" ]; then
	# shown symbolically with -n because these may come from mk.conf
	DESTDIR='$DESTDIR'
	TOOLDIR='$TOOLDIR'
else
	DESTDIR=`getmakevar DESTDIR`;
	[ $? = 0 ] || bomb "getmakevar DESTDIR failed";
	$runcmd echo "===> DESTDIR path: $DESTDIR"

	TOOLDIR=`getmakevar TOOLDIR`;
	[ $? = 0 ] || bomb "getmakevar TOOLDIR failed";
	$runcmd echo "===> TOOLDIR path: $TOOLDIR"

	export DESTDIR TOOLDIR
fi

# Check validity of TOOLDIR and DESTDIR.
if [ -z "$TOOLDIR" ] || [ "$TOOLDIR" = "/" ]; then
	bomb "TOOLDIR '$TOOLDIR' invalid"
fi
removedirs="$TOOLDIR"

if [ -z "$DESTDIR" ] || [ "$DESTDIR" = "/" ]; then
	if $do_buildsystem; then
		if [ "$buildtarget" != "build" ] || \
		   [ "`uname -s 2>/dev/null`" != "NetBSD" ] || \
		   [ "`uname -m`" != "$MACHINE" ]; then
			bomb "DESTDIR must be set to a non-root path for cross builds or -d or -R."
		fi
		if ! $expert_mode; then
			bomb "DESTDIR must be set to a non-root path for non -E (expert) builds"
		fi
		$runcmd echo "===> WARNING: Building to /, in expert mode."
		$runcmd echo "===>          This may cause your system to break!  Reasons include:"
		$runcmd echo "===>             - your kernel is not up to date"
		$runcmd echo "===>             - the libraries or toolchain have changed"
		$runcmd echo "===>          YOU HAVE BEEN WARNED!"
	fi
else
	removedirs="$removedirs $DESTDIR"
fi

# Remove the target directories.
if $do_removedirs; then
	for f in $removedirs; do
		$runcmd echo "===> Removing $f"
		$runcmd rm -r -f $f
	done
fi

# Recreate $TOOLDIR.
$runcmd mkdir -p "$TOOLDIR/bin" || bomb "mkdir of '$TOOLDIR/bin' failed"

# Install nbmake if it was built.
if $do_rebuildmake; then
	$runcmd rm -f "$TOOLDIR/bin/nbmake"
	$runcmd cp $make "$TOOLDIR/bin/nbmake" \
		|| bomb "failed to install \$TOOLDIR/bin/nbmake"
	make="$TOOLDIR/bin/nbmake"
	$runcmd rm -r -f "$tmpdir"
	trap 0 1 2 3 15
fi

# Build a nbmake wrapper script, usable by hand as well as by build.sh.
if [ -z "$makewrapper" ]; then
	makewrapper="$TOOLDIR/bin/nbmake-$MACHINE"
	if [ ! -z "$BUILDID" ]; then
		makewrapper="$makewrapper-$BUILDID"
	fi
fi

$runcmd rm -f "$makewrapper"
if [ "$runcmd" = "echo" ]; then
	echo 'cat <<EOF >'$makewrapper
	makewrapout=
else
	makewrapout=">>\$makewrapper"
fi

eval cat <<EOF $makewrapout
#! /bin/sh
# Set proper variables to allow easy "make" building of a NetBSD subtree.
# Generated from:  \$NetBSD: build.sh,v 1.76 2002/12/09 12:49:55 scw Exp $
#

EOF
for f in $makeenv; do
	eval echo "$f=\'\$`echo $f`\'\;\ export\ $f" $makewrapout
done
eval echo "USETOOLS=yes\; export USETOOLS" $makewrapout

eval cat <<'EOF' $makewrapout

exec "$TOOLDIR/bin/nbmake" ${1+"$@"}
EOF
[ "$runcmd" = "echo" ] && echo EOF
$runcmd chmod +x "$makewrapper"

if $do_buildsystem; then
	# Build everything.
	${runcmd-exec} "$makewrapper" $parallel $buildtarget \
		|| bomb "failed to make $buildtarget"
else
	# One or more of do_buildtools and do_buildkernel
	# might be set.  Do them in the appropriate order.
	if $do_buildtools; then
		if [ "$MKOBJDIRS" != "no" ]; then
			$runcmd "$makewrapper" $parallel obj-tools \
				|| bomb "failed to make obj-tools"
		fi
		$runcmd cd tools
		if [ "$UPDATE" = "" ]; then
			$runcmd "$makewrapper" cleandir dependall install \
				|| bomb "failed to make tools"
		else
			$runcmd "$makewrapper" dependall install \
				|| bomb "failed to make tools"
		fi
	fi
	if $do_buildkernel; then
		if ! $do_buildtools; then
			# Building tools every time we build a kernel
			# is clearly unnecessary.  We could try to
			# figure out whether rebuilding the tools is
			# necessary this time, but it doesn't seem
			# worth the trouble.  Instead, we say it's the
			# user's responsibility to rebuild the tools if
			# necessary.
			$runcmd echo "===> Building kernel" \
				"without building new tools"
		fi
		$runcmd echo "===> Building kernel ${kernconfname}"
		# The correct value of KERNOBJDIR might depend on a
		# prior "make obj" in TOP/etc.
		if [ "$MKOBJDIRS" != "no" ] && [ ! -z "$makeobjdir" ]; then
			$runcmd cd "$TOP/etc"
			$runcmd "$makewrapper" obj \
				|| bomb "failed to make obj in etc"
			$runcmd cd "$TOP"
		fi
		if [ "$runcmd" = "echo" ]; then
			# shown symbolically with -n
			# because getmakevar might not work yet
			KERNCONFDIR='$KERNCONFDIR'
			KERNOBJDIR='$KERNOBJDIR'
		else
			KERNCONFDIR="$( getmakevar KERNCONFDIR )"
			[ $? = 0 ] || bomb "getmakevar KERNCONFDIR failed";
			KERNOBJDIR="$( getmakevar KERNOBJDIR )"
			[ $? = 0 ] || bomb "getmakevar KERNOBJDIR failed";
		fi
		case "${kernconfname}" in
		*/*)
			kernconfpath="${kernconfname}"
			kernconfbase="$( basename "${kernconfname}" )"
			;;
		*)
			kernconfpath="${KERNCONFDIR}/${kernconfname}"
			kernconfbase="${kernconfname}"
			;;
		esac
		kernbuilddir="${KERNOBJDIR}/${kernconfbase}"
		$runcmd echo "===> Kernel build directory: ${kernbuilddir}"
		$runcmd mkdir -p "${kernbuilddir}" \
			|| bomb "cannot mkdir: ${kernbuilddir}"
		if [ "$UPDATE" = "" ]; then
			$runcmd cd "${kernbuilddir}"
			$runcmd "$makewrapper" cleandir \
				|| bomb "make cleandir failed in " \
					"${kernbuilddir}"
			$runcmd cd "$TOP"
		fi
		$runcmd "${TOOLDIR}/bin/nbconfig" \
			-b "${kernbuilddir}" \
			-s "${TOP}/sys" "${kernconfpath}" \
			|| bomb "nbconfig failed for ${kernconfname}"
		$runcmd cd "${kernbuilddir}"
		$runcmd "$makewrapper" depend \
			|| bomb "make depend failed in ${kernbuilddir}"
		$runcmd "$makewrapper" $parallel all \
			|| bomb "make all failed in ${kernbuilddir}"
		$runcmd echo "===> New kernel should be in ${kernbuilddir}"
	fi
fi
