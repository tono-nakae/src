/*	$NetBSD: rtc.c,v 1.1 2000/05/09 21:56:02 bjh21 Exp $	*/

/*
 * Copyright (c) 2000 Ben Harris
 * Copyright (c) 1994-1996 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * rtc.c
 *
 * Routines to read and write the RTC and CMOS RAM
 *
 * Created      : 13/10/94
 */

#include <sys/param.h>

__RCSID("$NetBSD: rtc.c,v 1.1 2000/05/09 21:56:02 bjh21 Exp $");

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <dev/clock_subr.h>
#include <arm26/ioc/iic.h>
#include <arm26/ioc/pcf8583reg.h>

struct rtc_softc {
	struct device	sc_dev;
	int		sc_flags;
#define RTC_BROKEN	1
#define RTC_OPEN	2
	int		sc_addr;
};

static int rtcmatch(struct device *parent, struct cfdata *cf, void *aux);
static void rtcattach(struct device *parent, struct device *self, void *aux);
static int rtc_gettime(struct device *, struct timeval *);
static int rtc_settime(struct device *, struct timeval *);

#define RTC_ADDR_YEAR     	0xc0
#define RTC_ADDR_CENT     	0xc1

extern struct cfdriver rtc_cd;

/* device and attach structures */

struct cfattach rtc_ca = {
	sizeof(struct rtc_softc), rtcmatch, rtcattach
};

/*
 * rtcmatch()
 *
 * Validate the IIC address to make sure its an RTC we understand
 */

int
rtcmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct iicbus_attach_args *ib = aux;

	if ((ib->ib_addr & PCF8583_MASK) == PCF8583_ADDR &&
	    iic_control(parent, ib->ib_addr | IIC_READ, NULL, 0) == 0)
		return 1;
	return 0;
}

/*
 * rtcattach()
 *
 * Attach the rtc device
 */

void
rtcattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct rtc_softc *sc = (struct rtc_softc *)self;
	struct iicbus_attach_args *ib = aux;
	u_char buff[1];

	sc->sc_flags |= RTC_BROKEN;
	sc->sc_addr = ib->ib_addr;
	if ((ib->ib_addr & PCF8583_MASK) == PCF8583_ADDR) {
		printf(": PCF8583");

		/* Read RTC register 0 and report info found */

		buff[0] = PCF8583_REG_CSR;

		if (iic_control(self->dv_parent, sc->sc_addr | IIC_WRITE,
				buff, 1))
			return;

		if (iic_control(self->dv_parent, sc->sc_addr | IIC_READ,
				buff, 1))
			return;

		switch (buff[0] & PCF8583_CSR_FN_MASK) {
		case PCF8583_CSR_FN_32768HZ:
			printf(", 32.768 kHz clock");
			break;
		case PCF8583_CSR_FN_50HZ:
			printf(", 50 Hz clock");
			break;
		case PCF8583_CSR_FN_EVENT:
			printf(", event counter");
			break;
		case PCF8583_CSR_FN_TEST:
			printf(", test mode");
			break;
		}

		if (buff[0] & PCF8583_CSR_STOP)
			printf(", stopped");
		if (buff[0] & PCF8583_CSR_ALARMENABLE)
			printf(", alarm enabled");
		sc->sc_flags &= ~RTC_BROKEN;
	}

	printf("\n");
}

/* Read a byte from CMOS RAM */

int
cmos_read(struct device *self, int location)
{
	u_char buff;
	struct rtc_softc *sc = (struct rtc_softc *)self;

	buff = location;

	if (iic_control(self->dv_parent, sc->sc_addr | IIC_WRITE, &buff, 1))
		return(-1);
	if (iic_control(self->dv_parent, sc->sc_addr | IIC_READ, &buff, 1))
		return(-1);

	return(buff);
}


/* Write a byte to CMOS RAM */

int
cmos_write(struct device *self, int location, int value)
{
	u_char buff[2];
	struct rtc_softc *sc = (struct rtc_softc *)self;

	buff[0] = location;
	buff[1] = value;

	if (iic_control(self->dv_parent, sc->sc_addr | IIC_WRITE, buff, 2))
		return(-1);

	return(0);
}

static int
rtc_settime(struct device *self, struct timeval *tv)
{
	struct rtc_softc *sc = (struct rtc_softc *)self;
	u_char buff[8];
	struct clock_ymdhms ymdhms;

	clock_secs_to_ymdhms(tv->tv_sec, &ymdhms);

	buff[0] = PCF8583_REG_CENTI;

	buff[PCF8583_REG_CENTI] = TOBCD(tv->tv_usec / 10000);
	buff[PCF8583_REG_SEC]   = TOBCD(ymdhms.dt_sec);
	buff[PCF8583_REG_MIN]   = TOBCD(ymdhms.dt_min);
	buff[PCF8583_REG_HOUR]  = TOBCD(ymdhms.dt_hour);
	buff[PCF8583_REG_YEARDATE] = TOBCD(ymdhms.dt_day) |
	    ((ymdhms.dt_year % 4) << PCF8583_YEAR_SHIFT);
	buff[PCF8583_REG_WKDYMON] = TOBCD(ymdhms.dt_mon) |
	    ((ymdhms.dt_wday % 4) << PCF8583_WKDY_SHIFT);

	if (iic_control(self->dv_parent, sc->sc_addr | IIC_WRITE, buff, 7))
		return -1;

	if (cmos_write(self, RTC_ADDR_YEAR, ymdhms.dt_year % 100))
		return -1;
	if (cmos_write(self, RTC_ADDR_CENT, ymdhms.dt_year / 100))
		return -1;
	return(0);
}

void
inittodr(time_t base)
{
	int check;

	check = 0;
	if (rtc_cd.cd_ndevs == 0 || rtc_cd.cd_devs[0] == NULL) {
		printf("inittodr: rtc0 not present");
		time.tv_sec = base;
		time.tv_usec = 0;
		check = 1;
	} else {
		rtc_gettime(rtc_cd.cd_devs[0], &time);
		if (time.tv_sec > base + 3 * SECDAY) {
			printf("inittodr: Clock has gained %ld days",
			       (time.tv_sec - base) / SECDAY);
			check = 1;
		} else if (time.tv_sec + SECDAY < base) {
			printf("inittodr: Clock has lost %ld day(s)",
			       (base - time.tv_sec) / SECDAY);
			check = 1;
		}
	}
	if (check)
		printf(" - CHECK AND RESET THE DATE.\n");
}


static int
rtc_gettime(struct device *self, struct timeval *tv)
{
	u_char buff[8];
	int byte, centi;
	struct rtc_softc *sc = (struct rtc_softc *)self;
	struct clock_ymdhms ymdhms;
    
	buff[0] = 0;

	if (iic_control(self->dv_parent, sc->sc_addr | IIC_WRITE, buff, 1))
		return -1;

	if (iic_control(self->dv_parent, sc->sc_addr | IIC_READ, buff, 8))
		return -1;

	centi          = FROMBCD(buff[PCF8583_REG_CENTI]);
	ymdhms.dt_sec  = FROMBCD(buff[PCF8583_REG_SEC]);
	ymdhms.dt_min  = FROMBCD(buff[PCF8583_REG_MIN]);
	ymdhms.dt_hour = FROMBCD(buff[PCF8583_REG_HOUR] & PCF8583_HOUR_MASK);

	/* If in 12 hour mode need to look at the AM/PM flag */
	
	if (buff[PCF8583_REG_HOUR] & PCF8583_HOUR_12H) {
		ymdhms.dt_hour %= 12; /* 12AM -> 0, 12PM -> 12 */
		if (buff[PCF8583_REG_HOUR] & PCF8583_HOUR_PM)
			ymdhms.dt_hour += 12;
	}

	ymdhms.dt_day = FROMBCD(buff[PCF8583_REG_YEARDATE] &
				PCF8583_DATE_MASK);
	ymdhms.dt_mon = FROMBCD(buff[PCF8583_REG_WKDYMON] &
				PCF8583_MON_MASK);

	byte = cmos_read(self, RTC_ADDR_YEAR);
	if (byte == -1)
		return -1;
	ymdhms.dt_year = byte;
	byte = cmos_read(self, RTC_ADDR_CENT);
	if (byte == -1)
		return -1;
	ymdhms.dt_year += 100 * byte;

	/* Try to notice if the year's rolled over. */
	if (buff[PCF8583_REG_CSR] & PCF8583_CSR_MASK)
		printf("%s: cannot check year in mask mode\n", self->dv_xname);
	else
		while (ymdhms.dt_year % 4 !=
		       (buff[PCF8583_REG_YEARDATE] &
			PCF8583_YEAR_MASK) >> PCF8583_YEAR_SHIFT)
			ymdhms.dt_year++;
	
	tv->tv_sec = clock_ymdhms_to_secs(&ymdhms);
	tv->tv_usec = centi * 10000;
	return 0;
}

#if 0
int
rtcopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct rtc_softc *sc;
	int unit = minor(dev);
    
	if (unit >= rtc_cd.cd_ndevs)
		return(ENXIO);

	sc = rtc_cd.cd_devs[unit];
    
	if (!sc) return(ENXIO);

	if (sc->sc_flags & RTC_BROKEN) return(ENXIO);
	if (sc->sc_flags & RTC_OPEN) return(EBUSY);

	sc->sc_flags |= RTC_OPEN;

	return(0);
}


int
rtcclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct rtc_softc *sc = rtc_cd.cd_devs[unit];
    
	sc->sc_flags &= ~RTC_OPEN;

	return(0);
}


int
rtcread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	rtc_t rtc;
	int s;
	char buffer[32];
	int length;

	s = splclock();
	if (rtc_read(NULL, &rtc) == 0) {
		(void)splx(s);
		return(ENXIO);
	}

	(void)splx(s);

	sprintf(buffer, "%02d:%02d:%02d.%02d%02d %02d/%02d/%02d%02d\n",
	    rtc.rtc_hour, rtc.rtc_min, rtc.rtc_sec, rtc.rtc_centi,
	    rtc.rtc_micro, rtc.rtc_day, rtc.rtc_mon, rtc.rtc_cen,
	    rtc.rtc_year);

	if (uio->uio_offset > strlen(buffer))
		return 0;

	length = strlen(buffer) - uio->uio_offset;
	if (length > uio->uio_resid)
		length = uio->uio_resid;

	return(uiomove((caddr_t)buffer, length, uio));
}


static int
twodigits(buffer, pos)
	char *buffer;
	int pos;
{
	int result = 0;

	if (buffer[pos] >= '0' && buffer[pos] <= '9')
		result = (buffer[pos] - '0') * 10;
	if (buffer[pos+1] >= '0' && buffer[pos+1] <= '9')
		result += (buffer[pos+1] - '0');
	return(result);
}

int
rtcwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	rtc_t rtc;
	int s;
	char buffer[25];
	int length;
	int error;

	/*
	 * We require atomic updates!
	 */
	length = uio->uio_resid;
	if (uio->uio_offset || (length != sizeof(buffer)
	  && length != sizeof(buffer - 1)))
		return(EINVAL);
	
	if ((error = uiomove((caddr_t)buffer, sizeof(buffer), uio)))
		return(error);

	if (length == sizeof(buffer) && buffer[sizeof(buffer) - 1] != '\n')
		return(EINVAL);

	printf("rtcwrite: %s\n", buffer);

	rtc.rtc_micro = 0;
	rtc.rtc_centi = twodigits(buffer, 9);
	rtc.rtc_sec   = twodigits(buffer, 6);
	rtc.rtc_min   = twodigits(buffer, 3);
	rtc.rtc_hour  = twodigits(buffer, 0);
	rtc.rtc_day   = twodigits(buffer, 14);
	rtc.rtc_mon   = twodigits(buffer, 17);
	rtc.rtc_year  = twodigits(buffer, 22); 
	rtc.rtc_cen   = twodigits(buffer, 20); 

	s = splclock();
	rtc_write(NULL, &rtc);
	(void)splx(s);

	return(0);
}


int
rtcioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
/*	struct rtc_softc *sc = rtc_cd.cd_devs[minor(dev)];*/

/*	switch (cmd) {
	case RTCIOC_READ:
		return(0);
	}*/

	return(EINVAL);
}
#endif

/* End of rtc.c */
