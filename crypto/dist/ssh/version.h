/*	$NetBSD: version.h,v 1.32 2003/09/17 23:19:04 christos Exp $	*/
/* $OpenBSD: version.h,v 1.37 2003/04/01 10:56:46 markus Exp $ */

#define __OPENSSH_VERSION	"OpenSSH_3.6.1"
#define __NETBSDSSH_VERSION	"NetBSD_Secure_Shell-20030917"

/*
 * it is important to retain OpenSSH version identification part, it is
 * used for bug compatibility operation.  present NetBSD SSH version as comment
 */
#define SSH_VERSION	(__OPENSSH_VERSION " " __NETBSDSSH_VERSION)
