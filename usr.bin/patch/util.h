/*	$NetBSD: util.h,v 1.9 2003/05/30 18:14:13 kristerw Exp $	*/

int move_file(char *, char *);
void copy_file(char *, char *);
void *xmalloc(size_t);
void *xrealloc(void *, size_t);
char *xstrdup(const char *);
void say(const char *, ...)
     __attribute__((__format__(__printf__, 1, 2)));
void fatal(const char *, ...)
     __attribute__((__format__(__printf__, 1, 2)));
void pfatal(const char *, ...)
     __attribute__((__format__(__printf__, 1, 2)));
void ask(const char *, ...)
     __attribute__((__format__(__printf__, 1, 2)));
void set_signals(int);
void ignore_signals(void);
void makedirs(char *, bool);
char *fetchname(char *, int, int);
