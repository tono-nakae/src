/*	$NetBSD: exec.h,v 1.19 2002/11/24 22:35:39 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)exec.h	8.3 (Berkeley) 6/8/95
 */

/* values of cmdtype */
#define CMDUNKNOWN -1		/* no entry in table for command */
#define CMDNORMAL 0		/* command is an executable program */
#define CMDFUNCTION 1		/* command is a shell function */
#define CMDBUILTIN 2		/* command is a shell builtin */
#define CMDSPLBLTIN 3		/* command is a special shell builtin */


struct cmdentry {
	int cmdtype;
	union param {
		int index;
		int (*bltin)(int, char**);
		union node *func;
	} u;
};


#define DO_ERR	1		/* find_command prints errors */
#define DO_ABS	2		/* find_command checks absolute paths */

extern const char *pathopt;	/* set by padvance */

void shellexec(char **, char **, const char *, int, int)
    __attribute__((__noreturn__));
char *padvance(const char **, const char *);
int hashcmd(int, char **);
void find_command(char *, struct cmdentry *, int, const char *);
int (*find_builtin(char *))(int, char **);
int (*find_splbltin(char *))(int, char **);
void hashcd(void);
void changepath(const char *);
void deletefuncs(void);
void getcmdentry(char *, struct cmdentry *);
void addcmdentry(char *, struct cmdentry *);
void defun(char *, union node *);
int unsetfunc(char *);
int typecmd(int, char **);
void hash_special_builtins(void);
