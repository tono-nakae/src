/*	$NetBSD: fpsetmask.c,v 1.3 2005/12/24 21:41:01 perry Exp $	*/

/*	$OpenBSD: fpsetmask.c,v 1.4 2004/01/05 06:06:16 otto Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpsetmask.c,v 1.3 2005/12/24 21:41:01 perry Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <ieeefp.h>

fp_except
fpsetmask(fp_except mask)
{
	uint64_t fpsr;
	fp_except old;

	__asm __volatile("fstd %%fr0,0(%1)" : "=m"(fpsr) : "r"(&fpsr));
	old = (fpsr >> 32) & 0x1f;
	fpsr = (fpsr & 0xffffffe000000000LL) | ((uint64_t)(mask & 0x1f) << 32);
	__asm __volatile("fldd 0(%0),%%fr0" : : "r"(&fpsr));
	return (old);
}
