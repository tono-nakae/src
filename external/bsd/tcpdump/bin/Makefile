#	$NetBSD: Makefile,v 1.8 2013/04/06 19:33:07 christos Exp $	

WARNS?=	1	# XXX: need to cleanup later

CWARNFLAGS.clang+=	-Wno-constant-logical-operand

.include <bsd.own.mk>

USE_FORT?= yes	# network client

BINDIR=/usr/sbin
SRCDIR=	${.CURDIR}/../dist
.PATH:	${SRCDIR}

PROG=	tcpdump
MAN=	tcpdump.8 

SRCS=	addrtoname.c cpack.c gmpls.c gmt2local.c machdep.c oui.c parsenfsfh.c \
	setsignal.c smbutil.c tcpdump.c util.c checksum.c signature.c \
	l2vpn.c nlpid.c ipproto.c af.c in_cksum.c pf_print_state.c

SRCS+=	bpf_dump.c
SRCS+=	version.c

SRCS+= \
print-802_11.c \
print-802_15_4.c \
print-ah.c \
print-aodv.c \
print-ap1394.c \
print-arcnet.c \
print-arp.c \
print-ascii.c \
print-atalk.c \
print-atm.c \
print-babel.c \
print-beep.c \
print-bfd.c \
print-bgp.c \
print-bootp.c \
print-bt.c \
print-carp.c \
print-cdp.c \
print-cfm.c \
print-chdlc.c \
print-cip.c \
print-cnfp.c \
print-dccp.c \
print-decnet.c \
print-domain.c \
print-dtp.c \
print-dvmrp.c \
print-eap.c \
print-egp.c \
print-eigrp.c \
print-enc.c \
print-esp.c \
print-ether.c \
print-fddi.c \
print-forces.c \
print-fr.c \
print-frag6.c \
print-gre.c \
print-hsrp.c \
print-icmp.c \
print-igmp.c \
print-igrp.c \
print-ip.c \
print-ipcomp.c \
print-ipfc.c \
print-ipnet.c \
print-ipx.c \
print-isakmp.c \
print-isoclns.c \
print-juniper.c \
print-krb.c \
print-l2tp.c \
print-lane.c \
print-ldp.c \
print-llc.c \
print-lldp.c \
print-lmp.c \
print-lspping.c \
print-lwapp.c \
print-lwres.c \
print-mobile.c \
print-mobility.c \
print-mpcp.c \
print-mpls.c \
print-msdp.c \
print-nfs.c \
print-ntp.c \
print-null.c \
print-olsr.c \
print-ospf.c \
print-pflog.c \
print-pfsync.c \
print-pgm.c \
print-pim.c \
print-ppi.c \
print-ppp.c \
print-pppoe.c \
print-pptp.c \
print-radius.c \
print-raw.c \
print-rip.c \
print-ripng.c \
print-rpki-rtr.c \
print-rrcp.c \
print-rsvp.c \
print-rx.c \
print-sctp.c \
print-sflow.c \
print-sip.c \
print-sl.c \
print-sll.c \
print-slow.c \
print-smb.c \
print-snmp.c \
print-stp.c \
print-sunatm.c \
print-sunrpc.c \
print-symantec.c \
print-syslog.c \
print-tcp.c \
print-telnet.c \
print-tftp.c \
print-timed.c \
print-tipc.c \
print-token.c \
print-udld.c \
print-udp.c \
print-usb.c \
print-vjc.c \
print-vqp.c \
print-vrrp.c \
print-vtp.c \
print-wb.c \
print-zephyr.c 

version.c: VERSION
	${_MKTARGET_CREATE}
	${TOOL_SED} -e 's/.*/char version[] = "&";/' ${.ALLSRC} > ${.TARGET}

LDADD+=	-lpcap -ll
DPADD+=	${LIBPCAP} ${LIBL}

CPPFLAGS+=	-I${.CURDIR}/../include -I${.CURDIR} -I${SRCDIR} -DHAVE_CONFIG_H
CPPFLAGS+=  -I${NETBSDSRCDIR}/dist/pf/sbin/pfctl \
	-I${NETBSDSRCDIR}/sys/dist/pf

AWKS= atime.awk packetdat.awk stime.awk send-ack.awk   

CPPFLAGS+=-DHAVE_CONFIG_H -D_U_="__attribute__((__unused__))"

CPPFLAGS+=-DWITH_USER=\"_tcpdump\"
CPPFLAGS+=-DWITH_CHROOT=\"/var/chroot/tcpdump\"

.if (${USE_INET6} != "no")
SRCS+=	print-ip6.c print-ip6opts.c print-icmp6.c \
	print-rt6.c print-dhcp6.c print-ospf6.c 

CPPFLAGS+=-DINET6=1 -DHAVE_RES_USE_INET6=1 
.endif

.if (${MKCRYPTO} != "no")
CPPFLAGS+=-DHAVE_LIBCRYPTO=1 
LDADD+=	-lcrypto -lcrypt
DPADD+=	${LIBCRYPTO} ${LIBCRYPT}
.endif

CLEANFILES+=	version.c tcpdump.8

tcpdump.8: tcpdump.1.in
	@rm -f ${.TARGET}
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
