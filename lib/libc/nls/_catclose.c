/*	$NetBSD: _catclose.c,v 1.5 2005/06/12 05:21:27 lukem Exp $	*/

/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: _catclose.c,v 1.5 2005/06/12 05:21:27 lukem Exp $");
#endif /* LIBC_SCCS and not lint */

#ifdef __indr_reference
__indr_reference(_catclose,catclose)
#else

#include <nl_types.h>
int	_catclose __P((nl_catd));	/* XXX */

int
catclose(catd)
	nl_catd catd;
{
	return _catclose(catd);
}

#endif
