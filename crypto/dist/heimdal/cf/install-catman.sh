#!/bin/sh
#
# $Id: install-catman.sh,v 1.1.1.1 2001/02/11 13:51:51 assar Exp $
#
# install preformatted manual pages

INSTALL_DATA="$1"; shift
mkinstalldirs="$1"; shift
srcdir="$1"; shift
mandir="$1"; shift
suffix="$1"; shift

for f in "$@"; do
	base=`echo "$f" | sed 's/\(.*\)\.\([^.]*\)$/\1/'`
	section=`echo "$f" | sed 's/\(.*\)\.\([^.]*\)$/\2/'`
	catdir="$mandir/cat$section"
	c="$base.cat$section"
	if test -f "$srcdir/$c"; then
		if test \! -d "$catdir"; then
			eval "$mkinstalldirs $catdir"
		fi
		eval "echo $INSTALL_DATA $srcdir/$c $catdir/$base.$suffix"
		eval "$INSTALL_DATA $srcdir/$c $catdir/$base.$suffix"
	fi
done
