#! /bin/sh
#  $NetBSD: build.sh,v 1.35 2001/12/12 23:50:27 jmc Exp $
#
# Top level build wrapper, for a system containing no tools.
#
# This script should run on any POSIX-compliant shell.  For systems
# with a strange /bin/sh, "ksh" or "bash" may be an ample alternative.
#

bomb () {
	echo ""
	echo "ERROR: $@"
	echo "*** BUILD ABORTED ***"
	exit 1
}
[ -d usr.bin/make ] || bomb "build.sh must be run from the top source level"

TOP=`pwd`

getarch () {
	# Translate a MACHINE into a default MACHINE_ARCH.
	case $MACHINE in
		arm26|dnard|evbarm|hpcarm|netwinder)
			MACHINE_ARCH=arm;;

		acorn32|arm32|cats)
			MACHINE_ARCH=arm32;;

		sun2)
			MACHINE_ARCH=m68000;;

		amiga|atari|cesfic|hp300|sun3|*68k)
			MACHINE_ARCH=m68k;;

		mipsco|newsmips|sgimips)
			MACHINE_ARCH=mipseb;;

		algor|arc|cobalt|hpcmips|playstation2|pmax)
			MACHINE_ARCH=mipsel;;

		pc532)
			MACHINE_ARCH=ns32k;;

		bebox|prep|sandpoint|walnut|*ppc)
			MACHINE_ARCH=powerpc;;

		evbsh3|mmeye)
			MACHINE_ARCH=sh3eb;;

		dreamcast|hpcsh)
			MACHINE_ARCH=sh3el;;

		alpha|i386|sparc|sparc64|vax|x86_64)
			MACHINE_ARCH=$MACHINE;;

		*)	bomb "unknown target MACHINE: $MACHINE";;
	esac
}

getmakevar () {
	$make -m ${TOP}/share/mk -s -f- _x_ <<EOF
_x_:
	echo \${$1}
.include <bsd.prog.mk>
EOF
}

# Emulate "mkdir -p" for systems that have an Old "mkdir".
mkdirp () {
	oifs=$IFS
	IFS=/; set -- $@; IFS=$oifs
	_d=
	if [ -z "$1" ]; then _d=/; shift; fi

	for _f in "$@"; do
		if [ -n "$_f" ]; then
			[ -d "$_d$_f" ] || $runcmd mkdir "$_d$_f" || return 1
			_d="$_d$_f/"
		fi
	done
}

resolvepath () {
	case $OPTARG in
	/*)	;;
	*)	OPTARG="$cwd/$OPTARG";;
	esac
}

usage () {
	echo "Usage:"
	echo "$0 [-bdoru] [-a arch] [-j njob] [-m mach] [-w wrapper]"
	echo "   [-D dest] [-O obj] [-R release] [-T tools]"
	echo ""
	echo "    -a: set MACHINE_ARCH to arch (otherwise deduced from MACHINE)"
	echo "    -b: build nbmake and nbmake wrapper script, if needed"
	echo "    -d: build a full distribution into DESTDIR (including etc files)"
	echo "    -j: set NBUILDJOBS to njob"
	echo "    -m: set MACHINE to mach (not required if NetBSD native)"
	echo "    -n: show commands that would be executed, but do not execute them"
	echo "    -o: set MKOBJDIRS=no (do not create objdirs at start of build)"
	echo "    -r: remove contents of TOOLDIR and DESTDIR before building"
	echo "    -t: build and install tools only (implies -b)"
	echo "    -u: set UPDATE"
	echo "    -w: create nbmake script at wrapper (default TOOLDIR/bin/nbmake-MACHINE)"
	echo "    -D: set DESTDIR to dest"
	echo "    -O: set obj root directory to obj (sets a MAKEOBJDIR pattern)"
	echo "    -R: build a release (and set RELEASEDIR to release)"
	echo "    -T: set TOOLDIR to tools"
	echo ""
	echo "Note: if -T is unset and TOOLDIR is not set in the environment,"
	echo "      nbmake will be [re]built unconditionally."
	exit 1
}

# Set defaults.
MAKEFLAGS=
buildtarget=build
cwd=`pwd`
do_buildsystem=true
do_buildonlytools=false
do_rebuildmake=false
do_removedirs=false
makeenv=
makewrapper=
opt_a=no
opts='a:bdhj:m:nortuw:D:O:R:T:'
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

	-b)	do_buildsystem=false;;

	-d)	buildtarget=distribution;;

	-j)	eval $optargcmd
		MAKEFLAGS="$MAKEFLAGS NBUILDJOBS=$OPTARG"; export MAKEFLAGS;;

	# -m overrides MACHINE_ARCH unless "-a" is specified
	-m)	eval $optargcmd
		MACHINE=$OPTARG; [ "$opt_a" != "yes" ] && getarch;;

	-n)	runcmd=echo;;

	-o)	MKOBJDIRS=no;;

	-r)	do_removedirs=true; do_rebuildmake=true;;

	-t)	do_buildonlytools=true; do_buildsystem=false;;

	-u)	UPDATE=yes; export UPDATE
		makeenv="$makeenv UPDATE";;

	-w)	eval $optargcmd; resolvepath
		makewrapper="$OPTARG";;

	-D)	eval $optargcmd; resolvepath
		DESTDIR="$OPTARG"; export DESTDIR
		makeenv="$makeenv DESTDIR";;

	-O)	eval $optargcmd; resolvepath
		MAKEOBJDIR="\${.CURDIR:C,^$cwd,$OPTARG,}"; export MAKEOBJDIR
		makeobjdir=$OPTARG
		makeenv="$makeenv MAKEOBJDIR";;

	-R)	eval $optargcmd; resolvepath
		RELEASEDIR=$OPTARG; export RELEASEDIR
		makeenv="$makeenv RELEASEDIR"
		buildtarget=release;;

	-T)	eval $optargcmd; resolvepath
		TOOLDIR="$OPTARG"; export TOOLDIR;;

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

# Set up default make(1) environment.
makeenv="$makeenv TOOLDIR MACHINE MACHINE_ARCH MAKEFLAGS"
MAKEFLAGS="-m $cwd/share/mk $MAKEFLAGS MKOBJDIRS=${MKOBJDIRS-yes}"
export MAKEFLAGS MACHINE MACHINE_ARCH

# Test make source file timestamps against installed nbmake binary,
# if TOOLDIR is pre-set.
make=${TOOLDIR-nonexistent}/bin/nbmake
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
	tmpdir=${TMPDIR-/tmp}/nbbuild$$

	$runcmd mkdir $tmpdir || bomb "cannot mkdir: $tmpdir"
	trap "rm -r -f $tmpdir" 0
	trap "exit 1" 1 2 3 15
	$runcmd cd $tmpdir

	$runcmd ${HOST_CC-cc} ${HOST_CFLAGS} -DMAKE_BOOTSTRAP \
		-o nbmake -I$cwd/usr.bin/make \
		$cwd/usr.bin/make/*.c $cwd/usr.bin/make/lst.lib/*.c \
		${HOST_LDFLAGS} || bomb "build of nbmake failed"

	make=$tmpdir/nbmake
	$runcmd cd $cwd
	$runcmd rm -f usr.bin/make/*.o usr.bin/make/lst.lib/*.o
fi

USE_NEW_TOOLCHAIN=`getmakevar USE_NEW_TOOLCHAIN`
if [ "${USE_NEW_TOOLCHAIN}" = "" ]; then
	echo "ERROR: build.sh (new toolchain) is not yet enabled for"
	echo
	echo "MACHINE: ${MACHINE}"
	echo "MACHINE_ARCH: ${MACHINE_ARCH}"
	echo
	echo "All builds for this platform should be done via a traditional make"
	echo
	echo "If you wish to test using the new toolchain set"
	echo
	echo "USE_NEW_TOOLCHAIN=yes"
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
	$runcmd $make -m ${TOP}/share/mk obj NOSUBDIR= || exit 1
	$runcmd cd ..
fi

#
# If setting -O to root an obj dir make sure the base directory is made
# before continuing as bsd.own.mk will need this to pick up _SRC_TOP_OBJ_
#
if [ "$MKOBJDIRS" != "no" ] && [ ! -z "$makeobjdir" ]; then
	$runcmd mkdirp $makeobjdir
fi

# Find DESTDIR and TOOLDIR.
if [ "$runcmd" = "echo" ]; then
	# shown symbolically with -n because these may come from mk.conf
	DESTDIR='$DESTDIR'
	TOOLDIR='$TOOLDIR'
else
	DESTDIR=`getmakevar DESTDIR`; echo "===> DESTDIR path: $DESTDIR"
	TOOLDIR=`getmakevar TOOLDIR`; echo "===> TOOLDIR path: $TOOLDIR"
	export DESTDIR TOOLDIR
fi

# Check validity of TOOLDIR and DESTDIR.
if [ -z "$TOOLDIR" ] || [ "$TOOLDIR" = "/" ]; then
	bomb "TOOLDIR '$TOOLDIR' invalid"
fi
removedirs="$TOOLDIR"

if [ -z "$DESTDIR" ] || [ "$DESTDIR" = "/" ]; then
	if $do_buildsystem && \
	   ([ "$buildtarget" != "build" ] || \
	    [ "`uname -s 2>/dev/null`" != "NetBSD" ] || \
	    [ "`uname -m`" != "$MACHINE" ]); then
		bomb "DESTDIR must be set to a non-root path for cross builds or -d or -R."
	elif $do_buildsystem; then
		echo "===> WARNING: Building to /."
		echo "===> If your kernel is not up to date, this may cause the system to break!"
	fi
else
	removedirs="$removedirs $DESTDIR"
fi

# Remove the target directories.
if $do_removedirs; then
	for f in $removedirs; do
		echo "===> Removing $f"
		$runcmd rm -r -f $f
	done
fi

# Recreate $TOOLDIR.
mkdirp $TOOLDIR/bin || bomb "mkdir of $TOOLDIR/bin failed"

# Install nbmake if it was built.
if $do_rebuildmake; then
	$runcmd rm -f $TOOLDIR/bin/nbmake
	$runcmd cp $make $TOOLDIR/bin/nbmake
	$runcmd rm -r -f $tmpdir
	trap 0 1 2 3 15
fi

# Build a nbmake wrapper script, usable by hand as well as by build.sh.
if [ -z "$makewrapper" ]; then
	makewrapper=$TOOLDIR/bin/nbmake-$MACHINE
fi

$runcmd rm -f $makewrapper
if [ "$runcmd" = "echo" ]; then
	echo 'cat <<EOF >'$makewrapper
	makewrapout=
else
	makewrapout=">>$makewrapper"
fi

eval cat <<EOF $makewrapout
#! /bin/sh
# Set proper variables to allow easy "make" building of a NetBSD subtree.
# Generated from:  \$NetBSD: build.sh,v 1.35 2001/12/12 23:50:27 jmc Exp $
#

EOF
for f in $makeenv; do
	eval echo "$f=\'\$`echo $f`\'\;\ export\ $f" $makewrapout
done
eval echo "USETOOLS=yes\; export USETOOLS" $makewrapout

eval cat <<EOF $makewrapout

exec \$TOOLDIR/bin/nbmake \${1+"\$@"}
EOF
[ "$runcmd" = "echo" ] && echo EOF
$runcmd chmod +x $makewrapper

if $do_buildsystem; then
	${runcmd-exec} $makewrapper $buildtarget
elif $do_buildonlytools; then
	if [ "$MKOBJDIRS" != "no" ]; then
		$runcmd $makewrapper obj-tools || exit 1
	fi
	$runcmd cd tools
	if [ "$UPDATE" = "" ]; then
		${runcmd-exec} $makewrapper cleandir dependall install
	else
		${runcmd-exec} $makewrapper dependall install
	fi
fi
