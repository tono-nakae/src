/*	$NetBSD: ip_fil_irix.c,v 1.1.1.1 2004/03/28 08:55:33 martti Exp $	*/

/*
 * Copyright (C) 1993-2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if !defined(lint)
static const char sccsid[] = "@(#)ip_fil.c	2.41 6/5/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)Id: ip_fil_irix.c,v 2.42.2.2 2004/03/22 12:18:08 darrenr Exp";
#endif

#if defined(KERNEL) || defined(_KERNEL)
# undef KERNEL
# undef _KERNEL
# define        KERNEL	1
# define        _KERNEL	1
#endif
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/dir.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if.h>
#include <sys/debug.h>
#ifdef IFF_DRVRLOCK /* IRIX6 */
# include <sys/hashing.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#if !defined(IFF_DRVRLOCK) /* IRIX < 6 */
# include <netinet/in_var.h>
#endif
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#ifdef USE_INET6
# include <netinet/icmp6.h>
#endif
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_auth.h"
#ifdef	IPFILTER_SYNC
#include "netinet/ip_sync.h"
#endif
#ifdef	IPFILTER_SCAN
#include "netinet/ip_scan.h"
#endif

#include "md5.h"

extern	int	tcp_mtudisc;
extern	int	ipforwarding;
extern	struct	protosw	inetsw[];
extern	int	tcp_ttl;
#if IRIX >= 60500
extern	toid_t	fr_timer_id;
#endif

static	int	(*fr_savep) __P((ip_t *, int, void *, int, struct mbuf **));
static	int	fr_send_ip __P((fr_info_t *, struct mbuf *));
extern  ipfmutex_t	ipf_rw;
extern	ipfrwlock_t	ipf_mutex;


int ipl_attach()
{
	int error = 0, s;

	SPL_NET(s);
	if (fr_running > 0) {
		SPL_X(s);
		return EBUSY;
	}

	if (fr_initialise() < 0)
		return -1;

	error = ipl_ipfilter_attach();
	if (error) {
		fr_deinitialise();
		SPL_X(s);
		return error;
	}

	bzero((char *)frcache, sizeof(frcache));
	if (fr_checkp != fr_check) {
		fr_savep = fr_checkp;
		fr_checkp = fr_check;
	}

	if (fr_control_forwarding & 1)
		ipforwarding = 1;

	SPL_X(s);

#if IRIX >= 60500
	fr_timer_id = timeout(fr_slowtimer, NULL, hz/2);
#else
	timeout(fr_slowtimer, NULL, hz/2);
#endif
	return 0;
}


/*
 * Disable the filter by removing the hooks from the IP input/output
 * stream.
 */
int ipl_detach()
{
	int s, error = 0;

	SPL_NET(s);

#if IRIX >= 60500
	if (fr_timer_id != 0) {
		/* error = untimeout(fr_timer_id); XXX - does not work with */
		/* timeout() return value, only itimeout() and dtimeout().  */
		fr_timer_id = 0;
	}
#else
	untimeout(fr_slowtimer);
#endif

	if (fr_control_forwarding & 2)
		ipforwarding = 0;

	fr_deinitialise();

	if (fr_savep != NULL)
		fr_checkp = fr_savep;
	fr_savep = NULL;
	(void) frflush(IPL_LOGIPF, FR_INQUE|FR_OUTQUE|FR_INACTIVE);
	(void) frflush(IPL_LOGIPF, FR_INQUE|FR_OUTQUE);

	ipl_ipfilter_detach();

	SPL_X(s);
	return 0;
}


/*
 * Filter ioctl interface.
 */
int iplioctl(dev, cmd, data, mode, cp, rp)
dev_t dev;
int cmd;
caddr_t data;
int mode;
cred_t *cp;
int *rp;
{
	int error = 0, unit = 0, tmp;
	friostat_t fio;
	u_int enable;
	int s;

	unit = GET_MINOR(dev);
	if ((IPL_LOGMAX < unit) || (unit < 0))
		return ENXIO;

	if (fr_running <= 0) {
		if (unit != IPL_LOGIPF)
			return EIO;
		if (cmd != SIOCIPFGETNEXT && cmd != SIOCIPFGET &&
		    cmd != SIOCIPFSET && cmd != SIOCFRENB &&
		    cmd != SIOCGETFS && cmd != SIOCGETFF)
			return EIO;
	}

	SPL_NET(s);

	error = fr_ioctlswitch(unit, data, cmd, mode);
	if (error != -1) {
		SPL_X(s);
		return error;
	}
	error = 0;


	switch (cmd)
	{
	case FIONREAD :
#ifdef IPFILTER_LOG
		error = COPYOUT(&iplused[IPL_LOGIPF], (caddr_t)data,
			       sizeof(iplused[IPL_LOGIPF]));
#endif
		break;
	case SIOCFRENB :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = COPYIN(data, &enable, sizeof(enable));
			if (error)
				break;
			if (enable) {
				if (fr_running > 0)
					error = 0;
				else
					error = ipl_attach();
				if (error == 0)
					fr_running = 1;
				else
					(void) ipl_detach();
			} else {
				error = ipl_detach();
				if (error == 0)
					fr_running = -1;
			}
		}
		break;
	case SIOCIPFSET :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
	case SIOCIPFGETNEXT :
	case SIOCIPFGET :
		error = fr_ipftune(cmd, data);
		break;
	case SIOCSETFF :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = COPYIN(data, &fr_flags, sizeof(fr_flags));
		break;
	case SIOCGETFF :
		error = COPYOUT(&fr_flags, data, sizeof(fr_flags));
		break;
	case SIOCFUNCL :
		error = fr_resolvefunc(data);
		break;
	case SIOCINAFR :
	case SIOCRMAFR :
	case SIOCADAFR :
	case SIOCZRLST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, data, fr_active, 1);
		break;
	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, data, 1 - fr_active, 1);
		break;
	case SIOCSWAPA :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			bzero((char *)frcache, sizeof(frcache[0]) * 2);
			*(u_int *)data = fr_active;
			fr_active = 1 - fr_active;
		}
		break;
	case SIOCGETFS :
		fr_getstat(&fio);
		error = fr_outobj(data, &fio, IPFOBJ_IPFSTAT);
		break;
	case	SIOCFRZST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = fr_zerostats(data);
		break;
	case	SIOCIPFFL :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = COPYIN(data, &tmp, sizeof(tmp));
			if (!error) {
				tmp = frflush(unit, tmp);
				error = COPYOUT(&tmp, data, sizeof(tmp));
			}
		}
		break;
	case SIOCSTLCK :
		error = COPYIN(data, &tmp, sizeof(tmp));
		if (error == 0) {
			fr_state_lock = tmp;
			fr_nat_lock = tmp;
			fr_frag_lock = tmp;
			fr_auth_lock = tmp;
		} else
			error = EFAULT;
		break;
#ifdef	IPFILTER_LOG
	case	SIOCIPFFB :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			*(int *)data = ipflog_clear(unit);
		break;
#endif /* IPFILTER_LOG */
	case SIOCGFRST :
		error = fr_outobj(data, fr_fragstats(), IPFOBJ_FRAGSTAT);
		break;
	case SIOCFRSYN :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			frsync();
		}
		break;
	default :
		error = EINVAL;
		break;
	}
	SPL_X(s);
	return error;
}


void fr_forgetifp(ifp)
void *ifp;
{
	register frentry_t *f;

	WRITE_ENTER(&ipf_mutex);
	for (f = ipacct[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipacct[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
#ifdef	USE_INET6
	for (f = ipacct6[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipacct6[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter6[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter6[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
#endif
	RWLOCK_EXIT(&ipf_mutex);
	fr_natsync(ifp);
}


/*
 * routines below for saving IP headers to buffer
 */
int iplopen(pdev, flags, devtype, cp)
dev_t *pdev;
int flags, devtype;
cred_t *cp;
{
	u_int min = geteminor(*pdev);

	if (IPL_LOGMAX < min)
		min = ENXIO;
	else
		min = 0;
	return min;
}


int iplclose(dev, flags, devtype, cp)
dev_t dev;
int flags, devtype;
cred_t *cp;
{
	u_int	min = GET_MINOR(dev);

	if (IPL_LOGMAX < min)
		min = ENXIO;
	else
		min = 0;
	return min;
}

/*
 * iplread/ipllog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
int iplread(dev, uio, crp)
dev_t dev;
register struct uio *uio;
cred_t *crp;
{
#ifdef IPFILTER_LOG
	return ipflog_read(GET_MINOR(dev), uio);
#else
	return ENXIO;
#endif
}


/*
 * fr_send_reset - this could conceivably be a call to tcp_respond(), but that
 * requires a large amount of setting up and isn't any more efficient.
 */
int fr_send_reset(fin)
fr_info_t *fin;
{
	struct tcphdr *tcp, *tcp2;
	int tlen = 0, hlen;
	struct mbuf *m;
#ifdef	USE_INET6
	ip6_t *ip6;
#endif
	ip_t *ip;

	tcp = fin->fin_dp;
	if (tcp->th_flags & TH_RST)
		return -1;		/* feedback loop */

#ifndef	IPFILTER_CKSUM
	if (fr_checkl4sum(fin) == -1)
		return -1;
#endif

	m = m_get(M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		return ENOBUFS;
	if (m == NULL)
		return -1;

	tlen = fin->fin_dlen - (TCP_OFF(tcp) << 2) +
			((tcp->th_flags & TH_SYN) ? 1 : 0) +
			((tcp->th_flags & TH_FIN) ? 1 : 0);

#ifdef	USE_INET6
	hlen = (fin->fin_v == 6) ? sizeof(ip6_t) : sizeof(ip_t);
#else
	hlen = sizeof(ip_t);
#endif
	m->m_len = sizeof(*tcp2) + hlen;
# if (BSD >= 199103)
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
# endif
	ip = mtod(m, struct ip *);
# ifdef	USE_INET6
	ip6 = (ip6_t *)ip;
# endif
	bzero((char *)ip, sizeof(*tcp2) + hlen);
	tcp2 = (struct tcphdr *)((char *)ip + hlen);

	tcp2->th_sport = tcp->th_dport;
	tcp2->th_dport = tcp->th_sport;
	if (tcp->th_flags & TH_ACK) {
		tcp2->th_seq = tcp->th_ack;
		tcp2->th_flags = TH_RST;
	} else {
		tcp2->th_ack = ntohl(tcp->th_seq);
		tcp2->th_ack += tlen;
		tcp2->th_ack = htonl(tcp2->th_ack);
		tcp2->th_flags = TH_RST|TH_ACK;
	}
	TCP_OFF_A(tcp2, sizeof(*tcp2) >> 2);
# ifdef	USE_INET6
	if (fin->fin_v == 6) {
		ip6->ip6_plen = htons(sizeof(struct tcphdr));
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_src = fin->fin_dst6;
		ip6->ip6_dst = fin->fin_src6;
		tcp2->th_sum = in6_cksum(m, IPPROTO_TCP,
					 sizeof(*ip6), sizeof(*tcp2));
		return fr_send_ip(fin, m);
	}
# endif
	ip->ip_p = IPPROTO_TCP;
	ip->ip_len = htons(sizeof(struct tcphdr));
	ip->ip_src.s_addr = fin->fin_daddr;
	ip->ip_dst.s_addr = fin->fin_saddr;
	tcp2->th_sum = in_cksum(m, hlen + sizeof(*tcp2));
	ip->ip_len = hlen + sizeof(*tcp2);
	return fr_send_ip(fin, m);
}


static int fr_send_ip(fin, m)
fr_info_t *fin;
struct mbuf *m;
{
	ip_t *ip;

	ip = mtod(m, ip_t *);

	IP_V_A(ip, fin->fin_v);
	switch (fin->fin_v)
	{
	case 4 :
		IP_HL_A(ip, sizeof(ip_t) >> 2);
		ip->ip_tos = fin->fin_ip->ip_tos;
		ip->ip_id = fin->fin_ip->ip_id;
		if (ip->ip_p == IPPROTO_TCP && tcp_mtudisc != 0)
			ip->ip_off = IP_DF;
		ip->ip_ttl = tcp_ttl;
		ip->ip_sum = 0;
		break;
#ifdef	USE_INET6
	case 6 :
	{
		ip6_t *ip6 = (ip6_t *)ip;

		ip6->ip6_hlim = 127;

		return ip6_output(m, NULL, NULL, 0, NULL, NULL);
	}
#endif
	default :
		return EINVAL;
	}
#ifdef	IPSEC
	m->m_pkthdr.rcvif = NULL;
#endif
	return fr_fastroute(m, &m, fin, NULL);
}


int fr_send_icmp_err(type, fin, dst)
int type;
fr_info_t *fin;
int dst;
{
	int err, hlen = 0, xtra = 0, iclen, ohlen = 0, avail;
	struct in_addr dst4;
	struct icmp *icmp;
	struct mbuf *m;
	void *ifp;
#ifdef USE_INET6
	ip6_t *ip6, *ip62;
	struct in6_addr dst6;
	int code;
#endif
	ip_t *ip, *ip2;

	if ((type < 0) || (type > ICMP_MAXTYPE))
		return -1;

#ifdef USE_INET6
	code = fin->fin_icode;
	if ((code < 0) || (code > sizeof(icmptoicmp6unreach)/sizeof(int)))
		return -1;
#endif

#ifndef	IPFILTER_CKSUM
	if (fr_checkl4sum(fin) == -1)
		return -1;
#endif

	avail = 0;
	m = NULL;
	ifp = fin->fin_ifp;
	if (fin->fin_v == 4) {
		if ((fin->fin_p == IPPROTO_ICMP) &&
		    !(fin->fin_flx & FI_SHORT))
			switch (ntohs(fin->fin_data[0]) >> 8)
			{
			case ICMP_ECHO :
			case ICMP_TSTAMP :
			case ICMP_IREQ :
			case ICMP_MASKREQ :
				break;
			default :
				return 0;
			}

		avail = MLEN;
		m = m_get(M_DONTWAIT, MT_HEADER);
		if (m == NULL)
			return ENOBUFS;

		if (dst == 0) {
			if (fr_ifpaddr(4, FRI_NORMAL, ifp,
				       &dst4, NULL) == -1) {
				FREE_MB_T(m);
				return -1;
			}
		} else
			dst4.s_addr = fin->fin_daddr;

		hlen = sizeof(ip_t);
		if (fin->fin_hlen < fin->fin_plen)
			xtra = MIN(fin->fin_dlen, 8);
		else
			xtra = 0;
	}

#ifdef	USE_INET6
	else if (fin->fin_v == 6) {
		hlen = sizeof(ip6_t);
		ohlen = sizeof(ip6_t);
		type = icmptoicmp6types[type];
		if (type == ICMP6_DST_UNREACH)
			code = icmptoicmp6unreach[code];

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL)
			return ENOBUFS;

		MCLGET(m, M_DONTWAIT);
		if (m == NULL)
			return ENOBUFS;
		avail = (m->m_flags & M_EXT) ? MCLBYTES : MHLEN;
		xtra = MIN(fin->fin_plen + sizeof(ip6_t),
			   avail - hlen - sizeof(*icmp) - max_linkhdr);
		if (dst == 0) {
			if (fr_ifpaddr(6, FRI_NORMAL, ifp,
				       (struct in_addr *)&dst6, NULL) == -1) {
				FREE_MB_T(m);
				return -1;
			}
		} else
			dst6 = fin->fin_dst6;
	}
#endif

	iclen = hlen + sizeof(*icmp) + xtra;
# if (BSD >= 199103)
	avail -= (max_linkhdr + iclen);
	m->m_data += max_linkhdr;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
	m->m_pkthdr.len = iclen;
#else
	avail -= (m->m_off + iclen);
#endif
	if (avail < 0) {
		m_freem(m);
		return ENOBUFS;
	}
	m->m_len = iclen;
	ip = mtod(m, ip_t *);
	icmp = (struct icmp *)((char *)ip + hlen);
	ip2 = (ip_t *)&icmp->icmp_ip;
	bzero((char *)ip, iclen);

	icmp->icmp_type = type;
	icmp->icmp_code = fin->fin_icode;
	icmp->icmp_cksum = 0;
#ifdef	icmp_nextmtu
	if (type == ICMP_UNREACH &&
	    fin->fin_icode == ICMP_UNREACH_NEEDFRAG && ifp)
		icmp->icmp_nextmtu = htons(((struct ifnet *)ifp)->if_mtu);
#endif

	bcopy((char *)fin->fin_ip, (char *)ip2, ohlen);

#ifdef	USE_INET6
	ip6 = (ip6_t *)ip;
	if (fin->fin_v == 6) {
		ip62 = (ip6_t *)ip2;
		ip62->ip6_plen = htons(ip62->ip6_plen);

		ip6->ip6_flow = 0;
		ip6->ip6_plen = htons(iclen - hlen);
		ip6->ip6_nxt = IPPROTO_ICMPV6;
		ip6->ip6_hlim = 0;
		ip6->ip6_src = dst6;
		ip6->ip6_dst = fin->fin_src6;
		if (xtra > 0)
			bcopy((char *)fin->fin_ip + fin->fin_hlen,
			      (char *)&icmp->icmp_ip + fin->fin_hlen, xtra);
		icmp->icmp_cksum = in6_cksum(m, IPPROTO_ICMPV6,
					     sizeof(*ip6), iclen - hlen);
	} else
#endif
	{
		ip2->ip_len = htons(ip2->ip_len);

		ip->ip_p = IPPROTO_ICMP;
		ip->ip_src.s_addr = dst4.s_addr;
		ip->ip_dst.s_addr = fin->fin_saddr;

		if (xtra > 0)
			bcopy((char *)fin->fin_ip + fin->fin_hlen,
			      (char *)&icmp->icmp_ip + fin->fin_hlen, xtra);
		icmp->icmp_cksum = ipf_cksum((u_short *)icmp,
					     sizeof(*icmp) + 8);
		ip->ip_len = iclen;
		ip->ip_p = IPPROTO_ICMP;
	}
	err = fr_send_ip(fin, m);
	return err;
}


void iplinit(void)
{
int i;

for (i = 0; i < 256; i++)
if (cdevsw[i].d_open == iplopen){printf("iplinit:ipfilter @%d\n", i); break;}
if (i==256)printf("iplinit:ipfilter not found\n");
	if (ipl_attach() != 0)
		printf("IP Filter failed to attach\n");
	else
		ip_init();
}
int iplattach(void)
{
int i;

for (i = 0; i < 256; i++)
if (cdevsw[i].d_open == iplopen){printf("iplattach:ipfilter @%d\n", i); break;}
if (i==256)printf("iplattach:ipfilter not found\n");
return 0;
}

void iplstart(void)
{
int i;

for (i = 0; i < 256; i++)
if (cdevsw[i].d_open == iplopen){printf("iplstart:ipfilter @%d\n", i); break;}
if (i==256)printf("iplstart:ipfilter not found\n");
}


size_t mbufchainlen(m0)
register struct mbuf *m0;
{
	register size_t len = 0;

	for (; m0; m0 = m0->m_next)
		len += m0->m_len;
	return len;
}


int fr_fastroute(m0, mpp, fin, fdp)
struct mbuf *m0, **mpp;
fr_info_t *fin;
frdest_t *fdp;
{
	register struct ip *ip, *mhip;
	register struct mbuf *m = m0;
	register struct route *ro;
	int len, off, error = 0, hlen, code;
	struct ifnet *ifp, *sifp;
	struct sockaddr_in *dst;
	struct route iproute;
	frentry_t *fr;

	hlen = fin->fin_hlen;
	ip = mtod(m0, struct ip *);

#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		/*
		 * currently "to <if>" and "to <if>:ip#" are not supported
		 * for IPv6
		 */
		return ip6_output(m0, NULL, NULL, 0, NULL, NULL);
	}
#endif
	/*
	 * Route packet.
	 */
	ROUTE_RDLOCK();

	ro = &iproute;
	bzero((caddr_t)ro, sizeof (*ro));
	dst = (struct sockaddr_in *)&ro->ro_dst;
	dst->sin_family = AF_INET;

	fr = fin->fin_fr;
	if (fdp)
		ifp = fdp->fd_ifp;
	else {
		ifp = fin->fin_ifp;
		dst->sin_addr = ip->ip_dst;
	}

	/*
	 * In case we're here due to "to <if>" being used with "keep state",
	 * check that we're going in the correct direction.
	 */
	if ((fr != NULL) && (fin->fin_rev != 0)) {
		if ((ifp != NULL) && (fdp == &fr->fr_tif))
			return -1;
		dst->sin_addr = ip->ip_dst;
	} else if (fdp)
		dst->sin_addr = fdp->fd_ip.s_addr ? fdp->fd_ip : ip->ip_dst;

	rtalloc(ro);

	ROUTE_UNLOCK();

	if (!ifp) {
		if (!fr || !(fr->fr_flags & FR_FASTROUTE)) {
			error = -2;
			goto bad;
		}
		if (ro->ro_rt == 0 || (ifp = ro->ro_rt->rt_ifp) == 0) {
			if (in_localaddr(ip->ip_dst))
				error = EHOSTUNREACH;
			else
				error = ENETUNREACH;
			goto bad;
		}
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = (struct sockaddr_in *)&ro->ro_rt->rt_gateway;
	}
	if (ro->ro_rt)
		ro->ro_rt->rt_use++;

	/*
	 * For input packets which are being "fastrouted", they won't
	 * go back through output filtering and miss their chance to get
	 * NAT'd and counted.
	 */
	fin->fin_ifp = ifp;
	if (fin->fin_out == 0) {
		u_32_t pass;

		fin->fin_out = 1;
		(void) fr_acctpkt(fin, &pass);

		fin->fin_fr = NULL;

		if (!fr || !(fr->fr_flags & FR_RETMASK))
			(void) fr_checkstate(fin, &pass);
		(void) fr_checknatout(fin, NULL);
	} else
		ip->ip_sum = 0;
	/*
	 * If small enough for interface, can just send directly.
	 */
	if (ip->ip_len <= ifp->if_mtu) {
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
		if (!ip->ip_sum)
			ip->ip_sum = in_cksum(m, hlen);
#if IRIX >= 60500
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst,
					  NULL);
#else
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst);
#endif
		goto done;
	}
	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & IP_DF) {
		error = EMSGSIZE;
		goto bad;
	}
	len = (ifp->if_mtu - hlen) &~ 7;
	if (len < 8) {
		error = EMSGSIZE;
		goto bad;
	}

    {
	int mhlen, firstlen = len;
	struct mbuf **mnext = &m->m_act;

	/*
	 * Loop through length of segment after first fragment,
	 * make new header and copy data of each part and link onto chain.
	 */
	m0 = m;
	mhlen = sizeof (struct ip);
	for (off = hlen + len; off < ip->ip_len; off += len) {
#ifdef	MGETHDR
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
#else
		MGET(m, M_DONTWAIT, MT_HEADER);
#endif
		if (m == 0) {
			error = ENOBUFS;
			goto bad;
		}
#if (BSD >= 199103)
		m->m_data += max_linkhdr;
#else
		m->m_off = MMAXOFF - hlen;
#endif
		mhip = mtod(m, struct ip *);
		bcopy((char *)ip, (char *)mhip, sizeof(*ip));
		if (hlen > sizeof (struct ip)) {
			mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			IP_HL_A(mhip, mhlen >> 2);
		}
		m->m_len = mhlen;
		mhip->ip_off = ((off - hlen) >> 3) + (ip->ip_off & ~IP_MF);
		if (ip->ip_off & IP_MF)
			mhip->ip_off |= IP_MF;
		if (off + len >= ip->ip_len)
			len = ip->ip_len - off;
		else
			mhip->ip_off |= IP_MF;
		mhip->ip_len = htons((u_short)(len + mhlen));
		m->m_next = m_copy(m0, off, len);
		if (m->m_next == 0) {
			error = ENOBUFS;	/* ??? */
			goto sendorfree;
		}
#if (BSD >= 199103)
		m->m_pkthdr.len = mhlen + len;
		m->m_pkthdr.rcvif = NULL;
#endif
		mhip->ip_off = htons((u_short)mhip->ip_off);
		mhip->ip_sum = 0;
		mhip->ip_sum = in_cksum(m, mhlen);
		*mnext = m;
		mnext = &m->m_act;
	}
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m_adj(m0, hlen + firstlen - ip->ip_len);
	ip->ip_len = htons((u_short)(hlen + firstlen));
	ip->ip_off = htons((u_short)(ip->ip_off | IP_MF));
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m0, hlen);
sendorfree:
	for (m = m0; m; m = m0) {
		m0 = m->m_act;
		m->m_act = 0;
		if (error == 0)
#if IRIX >= 60500
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst, ro->ro_rt);
#else
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst);
#endif
		else
			m_freem(m);
	}
    }	
done:
	if (!error)
		fr_frouteok[0]++;
	else
		fr_frouteok[1]++;

	if (ro->ro_rt)
		RTFREE(ro->ro_rt);
	*mpp = NULL;
	return 0;
bad:
	if (error == EMSGSIZE) {
		sifp = fin->fin_ifp;
		code = fin->fin_icode;
		fin->fin_icode = ICMP_UNREACH_NEEDFRAG;
		fin->fin_ifp = ifp;
		(void) fr_send_icmp_err(ICMP_UNREACH, fin, 1);
		fin->fin_ifp = sifp;
		fin->fin_icode = code;
	}
	m_freem(m);
	goto done;
}


int fr_verifysrc(fin)
fr_info_t *fin;
{
	struct sockaddr_in *dst;
	struct route iproute;

	bzero((char *)&iproute, sizeof(iproute));
	dst = (struct sockaddr_in *)&iproute.ro_dst;
	dst->sin_family = AF_INET;
	dst->sin_addr.s_addr = fin->fin_saddr;
	rtalloc(&iproute);
	if (iproute.ro_rt == NULL)
		return 0;
	return (fin->fin_ifp == iproute.ro_rt->rt_ifp);
}


/*
 * return the first IP Address associated with an interface
 */
int fr_ifpaddr(v, atype, ifptr, inp, inpmask)
int v, atype;
void *ifptr;
struct in_addr *inp, *inpmask;
{
#ifdef USE_INET6
	struct in6_addr *inp6 = NULL;
#endif
	struct sockaddr_in *sin, *mask;
	struct ifaddr *ifa;
	struct in_addr in;
	struct ifnet *ifp;

	mask = NULL;
	ifp = ifptr;

	if (v == 4)
		inp->s_addr = 0;
#ifdef      USE_INET6
	else if (v == 6)
		bzero((char *)inp, sizeof(struct in6_addr));
#endif

#if defined(IFF_DRVRLOCK) /* IRIX 6 */
	ifa = &((struct in_ifaddr *)ifp->in_ifaddr)->ia_ifa;
#else
	ifa = ifp->if_addrlist;
#endif

	sin = (struct sockaddr_in *)ifa->ifa_addr;
	while (sin && ifa) {
		if ((v == 4) && (sin->sin_family == AF_INET))
			break;
# ifdef USE_INET6
		if ((v == 6) && (sin->sin_family == AF_INET6)) {
			inp6 = &((struct sockaddr_in6 *)sin)->sin6_addr;
			if (!IN6_IS_ADDR_LINKLOCAL(inp6) &&
			    !IN6_IS_ADDR_LOOPBACK(inp6))
				break;
		}
# endif
		ifa = ifa->ifa_next;
		if (ifa != NULL)
			sin = (struct sockaddr_in *)ifa->ifa_addr;
	}
	if (ifa == NULL || sin == NULL)
		return -1;

	mask = (struct sockaddr_in *)ifa->ifa_netmask;
	if (atype == FRI_BROADCAST)
		sin = (struct sockaddr_in *)ifa->ifa_broadaddr;
	else if (atype == FRI_PEERADDR)
		sin = (struct sockaddr_in *)ifa->ifa_dstaddr;

#ifdef      USE_INET6
	if (v == 6)
		return fr_ifpfillv6addr(atype, (struct sockaddr_in6 *)sin,
					(struct sockaddr_in6 *)mask,
					inp, inpmask);
#endif
	return fr_ifpfillv4addr(atype, sin, mask, inp, inpmask);
}


#if IRIX >= 60500
void fr_slowtimer(void *arg)
#else
void fr_slowtimer()
#endif
{
	if (fr_running <= 0) {
#if IRIX >= 60500
		fr_timer_id = 0;
#endif
		return;
	}

	READ_ENTER(&ipf_global);
	if (fr_running <= 0) {
#if IRIX >= 60500
		fr_timer_id = 0;
#endif
		RWLOCK_EXIT(&ipf_global);
		return;
	}

	ipl_ipfilter_intfsync();

	fr_fragexpire();
	fr_timeoutstate();
	fr_natexpire();
	fr_authexpire();
	fr_ticks++;

#if IRIX >= 60500
	fr_timer_id = timeout(fr_slowtimer, NULL, hz/2);
#else
	timeout(fr_slowtimer, NULL, hz/2);
#endif
	RWLOCK_EXIT(&ipf_global);
}


u_32_t fr_newisn(fin)
fr_info_t *fin;
{
	static iss_seq_off = 0;
	u_char hash[16];
	u_32_t newiss;
	MD5_CTX ctx;

	/*
	 * Compute the base value of the ISS.  It is a hash
	 * of (saddr, sport, daddr, dport, secret).
	 */
	MD5Init(&ctx);

	MD5Update(&ctx, (u_char *) &fin->fin_fi.fi_src,
		  sizeof(fin->fin_fi.fi_src));
	MD5Update(&ctx, (u_char *) &fin->fin_fi.fi_dst,
		  sizeof(fin->fin_fi.fi_dst));
	MD5Update(&ctx, (u_char *) &fin->fin_dat, sizeof(fin->fin_dat));

	MD5Update(&ctx, ipf_iss_secret, sizeof(ipf_iss_secret));

	MD5Final(hash, &ctx);

	bcopy(hash, &newiss, sizeof(newiss));

	/*
	 * Now increment our "timer", and add it in to
	 * the computed value.
	 *
	 * XXX Use `addin'?
	 * XXX TCP_ISSINCR too large to use?
	 */
	iss_seq_off += 0x00010000;
	newiss += iss_seq_off;
	return newiss;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_nextipid                                                 */
/* Returns:     int - 0 == success, -1 == error (packet should be droppped) */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Returns the next IPv4 ID to use for this packet.                         */
/* ------------------------------------------------------------------------ */
u_short fr_nextipid(fin)
fr_info_t *fin;
{
	static u_short ipid = 0;
	u_short id;

	MUTEX_ENTER(&ipf_rw);
	id = ipid++;
	MUTEX_EXIT(&ipf_rw);

	return id;
}


void fr_checkv4sum(fin)
fr_info_t *fin;
{
#ifdef IPFILTER_CKSUM
	if (fr_checkl4sum(fin) == -1)
		fin->fin_flx |= FI_BAD;
#endif
}


#ifdef USE_INET6
INLINE void fr_checkv6sum(fin)
fr_info_t *fin;
{
#ifdef IPFILTER_CKSUM
	if (fr_checkl6sum(fin) == -1)
		fin->fin_flx |= FI_BAD;
#endif
}
#endif /* USE_INET6 */
