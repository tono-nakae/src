/*	$NetBSD: _warnx.c,v 1.9 2005/07/16 18:01:38 christos Exp $	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: _warnx.c,v 1.9 2005/07/16 18:01:38 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#if defined(__indr_reference) && !defined(__lint__)
__indr_reference(_warnx, warnx)
#else

#define	__NO_NAMESPACE_H	/* XXX */
#define _warnx	warnx
#define	_vwarnx	vwarnx
#define rcsid   _rcsid
#include "warnx.c"

#endif
