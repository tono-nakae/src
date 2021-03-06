#	$NetBSD: Makefile.inc,v 1.10 2013/06/01 11:02:42 pooka Exp $
#

.PATH:	${.CURDIR}/../../../../netinet ${.CURDIR}/../../../../netinet6

# INET
SRCS+=	in_proto.c igmp.c in.c in_offload.c in_pcb.c ip_carp.c ip_icmp.c\
	ip_flow.c ip_id.c ip_input.c ip_reass.c ip_output.c raw_ip.c	\
	in_cksum.c cpu_in_cksum.c in4_cksum.c ip_encap.c portalgo.c

# INET6
SRCS+=	dest6.c frag6.c icmp6.c in6.c in6_cksum.c in6_ifattach.c	\
	in6_offload.c in6_pcb.c in6_proto.c in6_src.c ip6_flow.c	\
	ip6_forward.c ip6_id.c ip6_input.c ip6_mroute.c ip6_output.c	\
	mld6.c nd6.c nd6_nbr.c nd6_rtr.c raw_ip6.c route6.c scope6.c	\
	udp6_output.c udp6_usrreq.c

# ARP
SRCS+=	if_arp.c

# TCP
SRCS+=	tcp_congctl.c tcp_input.c tcp_output.c tcp_sack.c tcp_subr.c \
	tcp_timer.c tcp_usrreq.c tcp_vtw.c

# UDP
SRCS+=	udp_usrreq.c

CPPFLAGS+=	-I${.CURDIR}/opt -I${.CURDIR}/../libnet/opt

CPPFLAGS+=	-DPORTALGO_INET4_DEFAULT=PORTALGO_RANDOM_START
CPPFLAGS+=	-DPORTALGO_INET6_DEFAULT=PORTALGO_RANDOM_START

# TCP debugging
#SRCS+=		tcp_debug.c
#CPPFLAGS+=	-DTCP_DEBUG
