/*	$NetBSD: crtend.c,v 1.10 2002/11/22 06:44:59 thorpej Exp $	*/

#include <sys/cdefs.h>
#include "dot_init.h"

static void (*__CTOR_LIST__[1]) __P((void))
    __attribute__((__unused__))
    __attribute__((section(".ctors"))) = { (void *)0 };		/* XXX */
static void (*__DTOR_LIST__[1]) __P((void))
    __attribute__((__unused__))
    __attribute__((section(".dtors"))) = { (void *)0 };		/* XXX */

#ifdef DWARF2_EH
static unsigned int __FRAME_END__[]
    __attribute__((__unused__))
    __attribute__((section(".eh_frame"))) = { 0 };
#endif

#if defined(JCR) && defined(__GNUC__)
static void *__JCR_END__[1]
    __attribute__((__unused__, section(".jcr"))) = { (void *) 0 };
#endif
