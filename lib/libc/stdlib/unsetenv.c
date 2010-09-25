/*	$NetBSD: unsetenv.c,v 1.7 2010/09/25 18:37:24 tron Exp $	*/

/*
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "from: @(#)setenv.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: unsetenv.c,v 1.7 2010/09/25 18:37:24 tron Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <bitstring.h>
#include "reentrant.h"
#include "local.h"

/*
 * unsetenv(name) --
 *	Delete environmental variable "name".
 */
int
unsetenv(const char *name)
{
	int offset;

	_DIAGASSERT(name != NULL);

	if (name == NULL || *name == '\0' || strchr(name, '=') != NULL) {
		errno = EINVAL;
		return -1;
	}

	if (rwlock_wrlock(&__environ_lock) != 0)
		return -1;

	if (__allocenv(-1) == -1) {
		rwlock_unlock(&__environ_lock);
		return -1;
	}

	while (__findenv(name, &offset) != NULL ) { /* if set multiple times */
		free(__environ_malloced[offset]);

		while (environ[offset] != NULL) {
			environ[offset] = environ[offset + 1];
			__environ_malloced[offset] =
			    __environ_malloced[offset + 1];
			offset++;
		}
		__environ_malloced[offset] = NULL;
	}
	rwlock_unlock(&__environ_lock);

	return 0;
}
