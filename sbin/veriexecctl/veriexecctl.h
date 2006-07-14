/*	$NetBSD: veriexecctl.h,v 1.6 2006/07/14 18:41:40 elad Exp $	*/

/*-
 * Copyright 2005 Elad Efrat <elad@bsd.org.il>
 * Copyright 2005 Brett Lymn <blymn@netbsd.org> 
 *
 * All rights reserved.
 *
 * This code has been donated to The NetBSD Foundation by the Author.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#ifndef _VERIEXECCTL_H_
#define _VERIEXECCTL_H_

#define	STATUS_STRING(status)	((status) == FINGERPRINT_NOTEVAL ?	\
					     "not evaluated" :		\
				 (status) == FINGERPRINT_VALID ?	\
					     "valid" :			\
				 (status) == FINGERPRINT_NOMATCH ?	\
					     "mismatch" :		\
					     "<unknown>")

CIRCLEQ_HEAD(veriexec_ups, veriexec_up) params_list;
struct veriexec_up {
        struct veriexec_sizing_params vu_param;
        CIRCLEQ_ENTRY(veriexec_up) vu_list;
};

extern int gfd, verbose, phase;
extern size_t line;
extern char *infile;
extern FILE *yyin;

int yywrap(void);
int yylex(void);
int yyparse(void);
void yyerror(const char *);
#if 0
struct veriexec_up *dev_lookup(dev_t);
struct veriexec_up *dev_add(dev_t);
#endif
struct veriexec_up *dev_lookup(char *);
struct veriexec_up *dev_add(dev_t, char *);
void phase2_load(void);

#endif /* _VERIEXECCTL_H_ */
