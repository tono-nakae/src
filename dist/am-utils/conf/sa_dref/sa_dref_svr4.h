/*	$NetBSD: sa_dref_svr4.h,v 1.1.1.1 2000/06/07 00:52:21 dogcow Exp $ */
/* $srcdir/conf/sa_dref/sa_dref_svr4.h */
#define	NFS_SA_DREF(dst, src) { \
		(dst)->addr->buf = (char *) (src); \
		(dst)->addr->len = sizeof(struct sockaddr_in); \
		(dst)->addr->maxlen = sizeof(struct sockaddr_in); \
	}
