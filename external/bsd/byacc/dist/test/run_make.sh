#!/bin/sh
# $Id: run_make.sh,v 1.1.1.2 2011/09/10 21:19:11 christos Exp $
# vi:ts=4 sw=4:

# do a test-compile on each of the ".c" files in the test-directory

if test $# = 1
then
	PROG_DIR=`pwd`
	TEST_DIR=$1
else
	PROG_DIR=..
	TEST_DIR=.
fi

echo '** '`date`
for i in ${TEST_DIR}/*.c
do
	obj=`echo "$i" |sed -e 's/\.c$/.o/'`
	make -f $PROG_DIR/makefile $obj C_FILES=$i srcdir=$PROG_DIR
	test -f $obj && rm $obj
done
