/*	$NetBSD: nan.c,v 1.2 2003/10/22 23:57:04 kleink Exp $	*/

/*
 * This file is in the Public Domain.
 *
 * Blatently copied from the infinity test by Ben Harris.
 *
 * Simon Burge, 2001
 */

/*
 * Check that NAN (alias __nanf) really is not-a-number.
 * Alternatively, check that isnan() minimally works.
 */

#include <err.h>
#include <math.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{

	/* NAN is meant to be a NaN. */
	if (!isnan(NAN))
		errx(1, "NAN is a number");
	return 0;
}
