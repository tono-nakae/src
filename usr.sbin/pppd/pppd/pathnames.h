/*
 * define path names
 *
 * $Id: pathnames.h,v 1.4 1993/11/10 01:34:26 paulus Exp $
 */

#ifdef STREAMS
#define _PATH_PIDFILE 	"/etc/ppp"
#else
#define _PATH_PIDFILE 	"/var/run"
#endif

#define _PATH_UPAPFILE 	"/etc/ppp/pap-secrets"
#define _PATH_CHAPFILE 	"/etc/ppp/chap-secrets"
#define _PATH_SYSOPTIONS "/etc/ppp/options"
