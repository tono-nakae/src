/* $NetBSD: ieee80211_rssadapt.c,v 1.1 2003/10/26 07:56:41 dyoung Exp $ */
/*-
 * Copyright (c) 2003, 2004 David Young.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of David Young may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY David Young ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL David
 * Young BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>		/* for hz */

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_compat.h>
#include <net80211/ieee80211_rssadapt.h>

#ifdef IEEE80211_DEBUG
static	struct timeval lastrateadapt;	/* time of last rate adaptation msg */
static	int currssadaptps = 0;		/* rate-adaptation msgs this second */
static	int ieee80211_adaptrate = 4;	/* rate-adaptation max msgs/sec */

#define RSSADAPT_DO_PRINT() \
	((ieee80211_debug > 0) && \
	 ppsratecheck(&lastrateadapt, &currssadaptps, ieee80211_adaptrate))
#define	RSSADAPT_PRINTF(X) \
	if (RSSADAPT_DO_PRINT()) \
		printf X

#else
#define	RSSADAPT_DO_PRINT() (0)
#define	RSSADAPT_PRINTF(X)
#endif

/* RSS threshold decay. */
u_int ieee80211_rssadapt_decay_denom = 16;
u_int ieee80211_rssadapt_decay_old = 15;
/* RSS threshold update. */
u_int ieee80211_rssadapt_thresh_denom = 8;
u_int ieee80211_rssadapt_thresh_old = 4;
/* RSS average update. */
u_int ieee80211_rssadapt_avgrssi_denom = 8;
u_int ieee80211_rssadapt_avgrssi_old = 4;

void
ieee80211_rssadapt_updatestats(struct ieee80211_rssadapt *ra)
{
	long interval; 

	ra->ra_pktrate =
	    (ra->ra_pktrate + 10 * (ra->ra_nfail + ra->ra_nok)) / 2;
	ra->ra_nfail = ra->ra_nok = 0;

	/* a node is eligible for its rate to be raised every 1/10 to 10
	 * seconds, more eligible in proportion to recent packet rates.
	 */
	interval = MAX(100000, 10000000 / MAX(1, 10 * ra->ra_pktrate));
	ra->ra_raise_interval.tv_sec = interval / (1000 * 1000);
	ra->ra_raise_interval.tv_usec = interval % (1000 * 1000);
}

void
ieee80211_rssadapt_input(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_rssadapt *ra, int rssi)
{
	int last_avg_rssi = ra->ra_avg_rssi;

	ra->ra_avg_rssi =
	    (ieee80211_rssadapt_avgrssi_old * ra->ra_avg_rssi +
	     ieee80211_rssadapt_avgrssi_new * (rssi << 8)) /
	    ieee80211_rssadapt_avgrssi_denom;

	RSSADAPT_PRINTF(("%s: src %s rssi %d avg %d -> %d\n",
	    ic->ic_if.if_xname, ether_sprintf(ni->ni_macaddr),
	    rssi, last_avg_rssi, ra->ra_avg_rssi));
}

/*
 * Adapt the data rate to suit the conditions.  When a transmitted
 * packet is dropped after IEEE80211_RSSADAPT_RETRY_LIMIT retransmissions,
 * raise the RSS threshold for transmitting packets of similar length at
 * the same data rate.
 */
void
ieee80211_rssadapt_lower_rate(struct ieee80211com *ic,
    struct ieee80211_node *ni, struct ieee80211_rssadapt *ra,
    struct ieee80211_rssdesc *id)
{
	struct ieee80211_rateset *rs = &ni->ni_rates;
	u_int16_t last_thr;
	u_int i, thridx, top;

	ra->ra_nok++;

	if (id->id_rateidx >= rs->rs_nrates) {
		RSSADAPT_PRINTF(("ieee80211_rssadapt_lower_rate: "
		    "%s rate #%d > #%d out of bounds\n",
		    ether_sprintf(ni->ni_macaddr), id->id_rateidx,
		        rs->rs_nrates - 1));
		return;
	}

	for (i = 0, top = IEEE80211_RSSADAPT_BKT0;
	     i < IEEE80211_RSSADAPT_BKTS;
	     i++, top <<= IEEE80211_RSSADAPT_BKTPOWER) {
		thridx = i;
		if (id->id_len <= top)
			break;
	}

	last_thr = ra->ra_rate_thresh[thridx][id->id_rateidx];
	ra->ra_rate_thresh[thridx][id->id_rateidx] =
	    (ieee80211_rssadapt_thresh_old * last_thr +
	     ieee80211_rssadapt_thresh_new * (id->id_rssi << 8)) /
	    ieee80211_rssadapt_thresh_denom;

	RSSADAPT_PRINTF(("%s: dst %s rssi %d threshold[%d, %d.%d] %d -> %d\n",
	    ic->ic_if.if_xname, ether_sprintf(ni->ni_macaddr),
	    id->id_rssi, id->id_len,
	    (rs->rs_rates[id->id_rateidx] & IEEE80211_RATE_VAL) / 2,
	    (rs->rs_rates[id->id_rateidx] & IEEE80211_RATE_VAL) * 5 % 10,
	    last_thr, ra->ra_rate_thresh[thridx][id->id_rateidx]));
}

void
ieee80211_rssadapt_raise_rate(struct ieee80211com *ic,
    struct ieee80211_rssadapt *ra, struct ieee80211_rssdesc *id)
{
	u_int16_t (*thrs)[IEEE80211_RATE_SIZE], newthr, oldthr;
	struct ieee80211_node *ni = id->id_node;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	int i, j, rate, top;

	ra->ra_nfail++;

	if (!ratecheck(&ra->ra_last_raise, &ra->ra_raise_interval))
		return;

	for (i = 0, top = IEEE80211_RSSADAPT_BKT0;
	     i < IEEE80211_RSSADAPT_BKTS;
	     i++, top <<= IEEE80211_RSSADAPT_BKTPOWER) {
		thrs = &ra->ra_rate_thresh[i];
		if (id->id_len <= top)
			break;
	}

	if (id->id_rateidx + 1 < rs->rs_nrates &&
	    (*thrs)[id->id_rateidx + 1] > (*thrs)[id->id_rateidx]) {
		rate = (rs->rs_rates[id->id_rateidx + 1] & IEEE80211_RATE_VAL);

		RSSADAPT_PRINTF(("%s: threshold[%d, %d.%d] decay %d ",
		    ic->ic_if.if_xname,
		    IEEE80211_RSSADAPT_BKT0 << (IEEE80211_RSSADAPT_BKTPOWER* i),
		    rate / 2, rate * 5 % 10, (*thrs)[id->id_rateidx + 1]));
		oldthr = (*thrs)[id->id_rateidx + 1];
		if ((*thrs)[id->id_rateidx] == 0)
			newthr = ra->ra_avg_rssi;
		else
			newthr = (*thrs)[id->id_rateidx];
		(*thrs)[id->id_rateidx + 1] =
		    (ieee80211_rssadapt_decay_old * oldthr +
		     ieee80211_rssadapt_decay_new * newthr) /
		    ieee80211_rssadapt_decay_denom;

		RSSADAPT_PRINTF(("-> %d\n", (*thrs)[id->id_rateidx + 1]));
	}

#ifdef IEEE80211_DEBUG
	if (RSSADAPT_DO_PRINT()) {
		printf("%s: dst %s thresholds\n", ic->ic_if.if_xname,
		    ether_sprintf(ni->ni_macaddr));
		for (i = 0; i < IEEE80211_RSSADAPT_BKTS; i++) {
			printf("%d-byte", IEEE80211_RSSADAPT_BKT0 << (IEEE80211_RSSADAPT_BKTPOWER * i));
			for (j = 0; j < rs->rs_nrates; j++) {
				rate = (rs->rs_rates[j] & IEEE80211_RATE_VAL);
				printf(", T[%d.%d] = %d", rate / 2,
				    rate * 5 % 10, ra->ra_rate_thresh[i][j]);
			}
			printf("\n");
		}
	}
#endif /* IEEE80211_DEBUG */
}
