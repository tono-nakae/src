/* $NetBSD: expr.y,v 1.22 2000/10/29 17:16:02 thorpej Exp $ */

/*_
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek <jdolecek@NetBSD.org> and J.T. Conklin <jtc@netbsd.org>.
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
 *      This product includes software developed by the NetBSD  
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 */

%{
#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: expr.y,v 1.22 2000/10/29 17:16:02 thorpej Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char **av;

static void yyerror(const char *, ...);
static int yylex(void);
static int is_zero_or_null(const char *);
static int is_integer(const char *);
static int yyparse(void);

#define YYSTYPE	const char *

%}
%token STRING
%left SPEC_OR
%left SPEC_AND
%left COMPARE ARITH_OPERATOR SPEC_REG
%left LEFT_PARENT RIGHT_PARENT

%%

exp:	expr = {
		(void) printf("%s\n", $1);
		return (is_zero_or_null($1));
		}
	;

expr:	item	{ $$ = $1; }
	| expr SPEC_OR expr = {
		/*
		 * Return evaluation of first expression if it is neither
		 * an empty string nor zero; otherwise, returns the evaluation
		 * of second expression.
		 */
		if (!is_zero_or_null($1))
			$$ = $1;
		else
			$$ = $3;
		}
	| expr SPEC_AND expr = {
		/*
		 * Returns the evaluation of first expr if neither expression
		 * evaluates to an empty string or zero; otherwise, returns
		 * zero.
		 */
		if (!is_zero_or_null($1) && !is_zero_or_null($3))
			$$ = $1;
		else
			$$ = "0";
		}
	| expr SPEC_REG expr = {
		/*
		 * The ``:'' operator matches first expr against the second,
		 * which must be a regular expression.
		 */
		regex_t rp;
		regmatch_t rm[2];
		int eval;

		/* compile regular expression */
		if ((eval = regcomp(&rp, $3, 0)) != 0) {
			char errbuf[256];
			(void)regerror(eval, &rp, errbuf, sizeof(errbuf));
			yyerror("%s", errbuf);
			/* NOT REACHED */
		}
		
		/* compare string against pattern --  remember that patterns 
		   are anchored to the beginning of the line */
		if (regexec(&rp, $1, 2, rm, 0) == 0 && rm[0].rm_so == 0) {
			char *val;
			if (rm[1].rm_so >= 0) {
				(void) asprintf(&val, "%.*s",
					(int) (rm[1].rm_eo - rm[1].rm_so),
					$1 + rm[1].rm_so);
			} else {
				(void) asprintf(&val, "%d",
					(int)(rm[0].rm_eo - rm[0].rm_so));
			}
			$$ = val;
		} else {
			if (rp.re_nsub == 0) {
				$$ = "0";
			} else {
				$$ = "";
			}
		}

		}
	| expr ARITH_OPERATOR expr = {
		/*
		 * Returns the results of multiplication, division,
		 * addition, subtraction, remainder of numeric-valued arguments.
		 */
		char *val;
		int64_t res, l, r;

		if (!is_integer($1)) {
			yyerror("non-integer argument '%s'", $1);
			/* NOTREACHED */
		}
		if (!is_integer($3)) {
			yyerror("non-integer argument '%s'", $3);
			/* NOTREACHED */
		}

		errno = 0;
		l = strtoll($1, NULL, 10);
		if (errno == ERANGE) {
			yyerror("value '%s' is %s is %lld", $1,
				(l > 0)
				? "too big, maximum" : "too small, minimum",
				(l > 0) ? LLONG_MAX : LLONG_MIN
			);
			/* NOTREACHED */
		}

		errno = 0;
		r = strtoll($3, NULL, 10);
		if (errno == ERANGE) {
			yyerror("value '%s' is %s is %lld", $3,
				(l > 0)
				? "too big, maximum" : "too small, minimum",
				(l > 0) ? LLONG_MAX : LLONG_MIN
			);
			/* NOTREACHED */
		}

		switch($2[0]) {
		case '+':
			res = l + r;
			/* very simplistic check for over-& underflow */
			if ((res < 0 && l > 0 && r > 0 && l > r)
				|| (res > 0 && l < 0 && r < 0 && l < r)) {
				yyerror("integer overflow or underflow occured\
 for operation '%s %s %s'", $1, $2, $3);
				/* NOTREACHED */
			}
			break;
		case '-':
			res = l - r;
			/* very simplistic check for over-& underflow */
			if ((res < 0 && l > 0 && l > r)
				|| (res > 0 && l < 0 && l < r) ) {
				yyerror("integer overflow or underflow occured\
 for operation '%s %s %s'", $1, $2, $3);
				/* NOTREACHED */
			}
			break;
		case '/':
			if (r == 0) {
				yyerror("second argument to '%s' must not be\
 zero", $2);
				/* NOTREACHED */
			}
			res = l / r;
				
			break;
		case '%':
			if (r == 0) {
				yyerror("second argument to '%s' must not be zero", $2);
				/* NOTREACHED */
			}
			res = l % r;
			break;
		case '*':
			if (r == 0) {
				res = 0;
				break;
			}
				
			/* check if the result would over- or underflow */
			if ((l > 0 && l > (LLONG_MAX / r))
				|| (l < 0 && l < (LLONG_MIN / r))) {
				yyerror("operation '%s %s %s' would cause over-\
 or underflow",
					$1, $2, $3);
				/* NOTREACHED */
			}

			res = l * r;
			break;
		}

		(void) asprintf(&val, "%lld", (long long int) res);
		$$ = val;

		}
	| expr COMPARE expr = {
		/*
		 * Returns the results of integer comparison if both arguments
		 * are integers; otherwise, returns the results of string
		 * comparison using the locale-specific collation sequence.
		 * The result of each comparison is 1 if the specified relation
		 * is true, or 0 if the relation is false.
		 */

		int64_t l, r;
		int res;

		/*
		 * Slight hack to avoid differences in the compare code
		 * between string and numeric compare.
		 */
		if (is_integer($1) && is_integer($3)) {
			/* numeric comparison */
			l = strtoll($1, NULL, 10);
			r = strtoll($3, NULL, 10);
		} else {
			/* string comparison */
			l = strcoll($1, $3);
			r = 0;
		}

		switch($2[0]) {	
		case '=': /* equal */
			res = (l == r);
			break;
		case '>': /* greater or greater-equal */
			if ($2[1] == '=')
				res = (l >= r);
			else
				res = (l > r);
			break;
		case '<': /* lower or lower-equal */
			if ($2[1] == '=')
				res = (l <= r);
			else
				res = (l < r);
			break;
		case '!': /* not equal */
			/* the check if this is != was done in yylex() */
			res = (l != r);
		}

		$$ = (res) ? "1" : "0";

		}
	| LEFT_PARENT expr RIGHT_PARENT { $$ = $2; }
	;

item:	STRING
	| ARITH_OPERATOR
	| COMPARE
	| SPEC_OR
	| SPEC_AND
	| SPEC_REG
	;
%%

/*
 * Returns 1 if the string is empty or contains only numeric zero.
 */
static int
is_zero_or_null(const char *str)
{
	char *endptr;

	return str[0] == '\0'
		|| ( strtoll(str, &endptr, 10) == 0LL
			&& endptr[0] == '\0');
}

/*
 * Returns 1 if the string is an integer.
 */
static int
is_integer(const char *str)
{
	char *endptr;

	(void) strtoll(str, &endptr, 10);
	/* note we treat empty string as valid number */
	return (endptr[0] == '\0');
}


const char *x = "|&=<>+-*/%:()";
const int x_token[] = {
	SPEC_OR, SPEC_AND, COMPARE, COMPARE, COMPARE, ARITH_OPERATOR,
	ARITH_OPERATOR, ARITH_OPERATOR, ARITH_OPERATOR, ARITH_OPERATOR,
	SPEC_REG, LEFT_PARENT, RIGHT_PARENT
};

int
yylex(void)
{
	const char *p = *av++;
	int retval = 0;

	if (!p) {
		return 0;
	}
	
	if (p[1] == '\0') {
		const char *w = strchr(x, p[0]);
		if (w) {
			retval = x_token[w-x];
		} else {
			retval = STRING;
		}
	} else if (p[1] == '=' && p[2] == '\0'
			&& (p[0] == '>' || p[0] == '<' || p[0] == '!'))
		retval = COMPARE;
	else
		retval = STRING;

	yylval = p;

	return retval;
}

/*
 * Print error message and exit with error 2 (syntax error).
 */
static void
yyerror(const char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	verrx(2, fmt, arg);
	va_end(arg);
}

int
main(int argc, const char **argv)
{
	(void) setlocale(LC_ALL, "");

	if (argc == 1) {
		(void) fprintf(stderr, "usage: expr expression\n");
		exit(2);
	}

	av = argv + 1;

	exit(yyparse());
	/* NOTREACHED */
}
