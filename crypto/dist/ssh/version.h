/*	$NetBSD: version.h,v 1.14 2001/06/20 07:49:45 itojun Exp $	*/
/* $OpenBSD: version.h,v 1.23 2001/04/24 16:43:16 markus Exp $ */

#define __OPENSSH_VERSION	"OpenSSH_2.9"
#define __NETBSDSSH_VERSION	"NetBSD_Secure_Shell-20010614"

/*
 * it is important to retain OpenSSH version identification part, it is
 * used for bug compatibility operation.  present NetBSD SSH version as comment
 */
#define SSH_VERSION	(__OPENSSH_VERSION " " __NETBSDSSH_VERSION)
