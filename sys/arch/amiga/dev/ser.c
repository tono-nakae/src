/*
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ser.c	7.12 (Berkeley) 6/27/91
 */

#include "ser.h"

#if NSER > 0
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/ioctl.h"
#include "sys/tty.h"
#include "sys/proc.h"
#include "sys/conf.h"
#include "sys/file.h"
#include "sys/malloc.h"
#include "sys/uio.h"
#include "sys/kernel.h"
#include "sys/syslog.h"

#include "device.h"
#include "serreg.h"
#include "machine/cpu.h"

#include "../amiga/custom.h"
#include "../amiga/cia.h"
#include "../amiga/cc.h"

int	serprobe();
struct	driver serdriver = {
	serprobe, "ser",
};

int	serstart(), serparam(), serintr();
int	ser_active;
int	ser_hasfifo;
int	nser = NSER;
#ifdef SERCONSOLE
int	serconsole = SERCONSOLE;
#else
int	serconsole = -1;
#endif
int	serconsinit;
int	serdefaultrate = TTYDEF_SPEED;
int	sermajor;
struct	serdevice *ser_addr[NSER];
struct	vbl_node ser_vbl_node[NSER];
struct	tty ser_cons;
struct	tty *ser_tty[NSER];

struct speedtab serspeedtab[] = {
	0,	0,
	50,	SERBRD(50),
	75,	SERBRD(75),
	110,	SERBRD(110),
	134,	SERBRD(134),
	150,	SERBRD(150),
	200,	SERBRD(200),
	300,	SERBRD(300),
	600,	SERBRD(600),
	1200,	SERBRD(1200),
	1800,	SERBRD(1800),
	2400,	SERBRD(2400),
	4800,	SERBRD(4800),
	9600,	SERBRD(9600),
	19200,	SERBRD(19200),
	38400,	SERBRD(38400),
	-1,	-1
};


/* since this UART is not particularly bright (nice put), we'll have to do
   parity stuff on our own. this table contains the 8th bit in 7bit character
   mode, for even parity. If you want odd parity, flip the bit. (for
   generation of the table, see genpar.c) */

u_char even_parity[] = {
   0,  1,  1,  0,  1,  0,  0,  1,  1,  0,  0,  1,  0,  1,  1,  0, 
   1,  0,  0,  1,  0,  1,  1,  0,  0,  1,  1,  0,  1,  0,  0,  1, 
   1,  0,  0,  1,  0,  1,  1,  0,  0,  1,  1,  0,  1,  0,  0,  1, 
   0,  1,  1,  0,  1,  0,  0,  1,  1,  0,  0,  1,  0,  1,  1,  0, 
   1,  0,  0,  1,  0,  1,  1,  0,  0,  1,  1,  0,  1,  0,  0,  1, 
   0,  1,  1,  0,  1,  0,  0,  1,  1,  0,  0,  1,  0,  1,  1,  0, 
   0,  1,  1,  0,  1,  0,  0,  1,  1,  0,  0,  1,  0,  1,  1,  0, 
   1,  0,  0,  1,  0,  1,  1,  0,  0,  1,  1,  0,  1,  0,  0,  1, 
};


/* since we don't get interrupts for changes on the modem control line,
   well have to fake them by comparing current settings to the settings
   we remembered on last invocation. */
u_char last_ciab_pra;

extern	struct tty *constty;
#ifdef KGDB
#include "machine/remote-sl.h"

extern dev_t kgdb_dev;
extern int kgdb_rate;
extern int kgdb_debug_init;
#endif

#ifdef DEBUG
long	fifoin[17];
long	fifoout[17];
long	serintrcount[16];
long	sermintcount[16];
#endif

void sermint (register int unit);

int
serprobe(ad)
     register struct amiga_device *ad;
{
  register struct serdevice *ser;
  register int unit;
  unsigned short ir = custom.intenar;
	
  ser = (struct serdevice *) ad->amiga_addr;
  unit = ad->amiga_unit;
  if (unit == serconsole)
    DELAY(100000);

  ad->amiga_ipl = 2;
  ser_addr[unit] = ser;
  ser_active |= 1 << unit;
  ser_vbl_node[unit].function = (void (*)(void *))sermint;
  add_vbl_function (&ser_vbl_node[unit], SER_VBL_PRIORITY, (void *)unit);
#ifdef KGDB
  if (kgdb_dev == makedev(sermajor, unit)) {
    if (serconsole == unit)
      kgdb_dev = NODEV;		/* can't debug over console port */
    else {
      (void) serinit(unit, kgdb_rate);
      serconsinit = 1;		/* don't re-init in serputc */
      if (kgdb_debug_init) {
	/*
	 * Print prefix of device name,
	 * let kgdb_connect print the rest.
	 */
	printf("ser%d: ", unit);
	kgdb_connect(1);
      } else
	printf("ser%d: kgdb enabled\n", unit);
    }
  }
#endif
  /*
   * Need to reset baud rate, etc. of next print so reset serconsinit.
   */
  if (unit == serconsole)
    serconsinit = 0;

  return (1);
}

/* ARGSUSED */
int
#ifdef __STDC__
seropen(dev_t dev, int flag, int mode, struct proc *p)
#else
seropen(dev, flag, mode, p)
     dev_t dev;
     int flag, mode;
     struct proc *p;
#endif
{
  register struct tty *tp;
  register int unit;
  int error = 0;
  int s;
  
  unit = SERUNIT(dev);
	
  if (unit >= NSER || (ser_active & (1 << unit)) == 0)
    return (ENXIO);
  if(!ser_tty[unit]) 
    {
      tp = ser_tty[unit] = ttymalloc();
      /* default values are not optimal for this device, increase
	 buffers */
      clfree(&tp->t_rawq);
      clfree(&tp->t_canq);
      clfree(&tp->t_outq);
      clalloc(&tp->t_rawq, 8192, 1);
      clalloc(&tp->t_canq, 8192, 1);
      clalloc(&tp->t_outq, 8192, 0);
    } 
  else
    tp = ser_tty[unit];

  tp->t_oproc = (void (*)(struct tty *)) serstart;
  tp->t_param = serparam;
  tp->t_dev = dev;
 
  if ((tp->t_state & TS_ISOPEN) == 0) 
    {
      tp->t_state |= TS_WOPEN;
      ttychars(tp);
      if (tp->t_ispeed == 0) 
	{
	  tp->t_iflag = TTYDEF_IFLAG | IXOFF;	/* XXXXX */
	  tp->t_oflag = TTYDEF_OFLAG;
#if 0
	  tp->t_cflag = TTYDEF_CFLAG;
#else
	  tp->t_cflag = (CREAD | CS8 | CLOCAL); /* XXXXX */
#endif
	  tp->t_lflag = TTYDEF_LFLAG;
	  tp->t_ispeed = tp->t_ospeed = serdefaultrate;
	}
      serparam(tp, &tp->t_termios);
      ttsetwater(tp);
    } 
  else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0)
    return (EBUSY);

  (void) sermctl (dev, TIOCM_DTR | TIOCM_RTS, DMSET);

  if (DIALOUT(dev) || (sermctl (dev, 0, DMGET) & TIOCM_CD))
    tp->t_state |= TS_CARR_ON;

  s = spltty();
  while ((flag & O_NONBLOCK) == 0 
	 && (tp->t_cflag & CLOCAL) == 0 
	 && (tp->t_state & TS_CARR_ON) == 0) 
    {
      tp->t_state |= TS_WOPEN;
      if (error = ttysleep(tp, (caddr_t)&tp->t_rawq, TTIPRI | PCATCH,
			   ttopen, 0))
	break;
    }
  splx (s);
  if (error == 0)
    {
      /* reset the tty pointer, as there could have been a dialout
	 use of the tty with a dialin open waiting. */
      tp->t_dev = dev;
      error = (*linesw[tp->t_line].l_open)(dev, tp);
    }
  return (error);
}
 
/*ARGSUSED*/
int
serclose(dev, flag, mode, p)
     dev_t dev;
     int flag, mode;
     struct proc *p;
{
  register struct tty *tp;
  register struct serdevice *ser;
  register int unit;
  
  unit = SERUNIT(dev);
  
  ser = ser_addr[unit];
  tp = ser_tty[unit];
  (*linesw[tp->t_line].l_close)(tp, flag);
  custom.adkcon = ADKCONF_UARTBRK; /* clear break */
#ifdef KGDB
  /* do not disable interrupts if debugging */
  if (dev != kgdb_dev)
#endif
    custom.intena = INTF_RBF|INTF_TBE; /* clear interrupt enable */
  custom.intreq = INTF_RBF|INTF_TBE; /* and   interrupt request */
#if 0
  /* if the device is closed, it's close, no matter whether we deal with modem
     control signals nor not. */
  if (tp->t_cflag&HUPCL || tp->t_state&TS_WOPEN ||
      (tp->t_state&TS_ISOPEN) == 0)
#endif
    (void) sermctl(dev, 0, DMSET);
  ttyclose(tp);
#if 0
  if (tp != &ser_cons)
    {
      remove_vbl_function (&ser_vbl_node[unit]);
      ttyfree (tp);
      ser_tty[unit] = (struct tty *)NULL;
    }
#endif
  return (0);
}
 
int
serread(dev, uio, flag)
     dev_t dev;
     struct uio *uio;
{
  register struct tty *tp = ser_tty[SERUNIT(dev)];
  int error;

  if (! tp)
    return ENXIO;

  error = (*linesw[tp->t_line].l_read)(tp, uio, flag);
  
  return error;
}
 
int
serwrite(dev, uio, flag)
     dev_t dev;
     struct uio *uio;
{
  int unit = SERUNIT(dev);
  register struct tty *tp = ser_tty[unit];
 
  if (! tp)
    return ENXIO;

  /*
   * (XXX) We disallow virtual consoles if the physical console is
   * a serial port.  This is in case there is a display attached that
   * is not the console.  In that situation we don't need/want the X
   * server taking over the console.
   */
  if (constty && unit == serconsole)
    constty = NULL;
  return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}


/* don't do any processing of data here, so we store the raw code
   obtained from the uart register. In theory, 110kBaud gives you
   11kcps, so 16k buffer should be more than enough, interrupt
   latency of 1s should never happen, or something is seriously
   wrong.. */
#define SERIBUF_SIZE 16384
static u_short serbuf[SERIBUF_SIZE];
static u_short *sbrpt = serbuf;
static u_short *sbwpt = serbuf;


/* this is a replacement for the lack of a hardware fifo. 32k should be
   enough (there's only one unit anyway, so this is not going to 
   accumulate). */
void
ser_fastint ()
{
  /* we're at RBE-level, which is higher than VBL-level which is used
     to periodically transmit contents of this buffer up one layer,
     so no spl-raising is necessary. */

  register u_short ints, code;

  ints = custom.intreqr & INTF_RBF;
  if (! ints)
    return;

  /* clear interrupt */
  custom.intreq = ints;
  /* this register contains both data and status bits! */
  code = custom.serdatr;

  /* should really not happen, but you never know.. buffer
     overflow. */
  if (sbwpt + 1 == sbrpt 
      || (sbwpt == serbuf + SERIBUF_SIZE - 1 && sbrpt == serbuf))
    {
      log (LOG_WARNING, "ser_fastint: buffer overflow!");
      return;
    }

  *sbwpt++ = code;
  if (sbwpt == serbuf + SERIBUF_SIZE)
    sbwpt = serbuf;
}


int
serintr (unit)
     register int unit;
{
  register struct serdevice *ser;
  int s1, s2;

  ser = ser_addr[unit];

  /* make sure we're not interrupted by another
     vbl, but allow level5 ints */
  s1 = spltty();

  /* ok, pass along any acumulated information .. */
  while (sbrpt != sbwpt)
    {
      /* no collision with ser_fastint() */
      sereint (unit, *sbrpt, ser);
      /* lock against ser_fastint() */
      s2 = spl5();
      {
	sbrpt++;
	if (sbrpt == serbuf + SERIBUF_SIZE)
	  sbrpt = serbuf;
      }
      splx (s2);
    }
  
  splx (s1);

#if 0
/* add the code below if you really need it */
	  {
/*
 * Process a received byte.  Inline for speed...
 */
#ifdef KGDB
#define	RCVBYTE() \
	    ch = code & 0xff; \
	    if ((tp->t_state & TS_ISOPEN) == 0) { \
		if (ch == FRAME_END && \
		    kgdb_dev == makedev(sermajor, unit)) \
			kgdb_connect(0); /* trap into kgdb */ \
	    }
#else
#define	RCVBYTE()
#endif
	    RCVBYTE();
	    /* sereint does the receive-processing */
	    sereint (unit, code, ser);
	  }
#endif
}

int
sereint(unit, stat, ser)
     register int unit, stat;
     register struct serdevice *ser;
{
  register struct tty *tp;
  register int c;
  register u_char ch;

  tp = ser_tty[unit];
  if ((tp->t_state & TS_ISOPEN) == 0) 
    {
#ifdef KGDB
      /* we don't care about parity errors */
      if (kgdb_dev == makedev(sermajor, unit) && c == FRAME_END)
	kgdb_connect(0); /* trap into kgdb */
#endif
      return;
    }

  ch = stat & 0xff;
  c = ch;
  /* all databits 0 including stop indicate break condition */
  if (!(stat & 0x1ff))
    c |= TTY_FE;
  
  /* if parity checking enabled, check parity */
  else if ((tp->t_cflag & PARENB) &&
	   (((ch >> 7) + even_parity[ch & 0x7f] + !!(tp->t_cflag & PARODD)) & 1))
    c |= TTY_PE;
  
  if (stat & SERDATRF_OVRUN)
    log(LOG_WARNING, "ser%d: silo overflow\n", unit);
  
  (*linesw[tp->t_line].l_rint)(c, tp);
}

/* this interrupt is periodically invoked in the vertical blank 
   interrupt. It's used to keep track of the modem control lines
   and (new with the fast_int code) to move accumulated data
   up into the tty layer. */
void
sermint (register int unit)
{
  register struct tty *tp;
  register u_char stat, last, istat;
  register struct serdevice *ser;
  
  tp = ser_tty[unit];
  if (!tp)
    return;

  if ((tp->t_state & (TS_ISOPEN|TS_WOPEN)) == 0) 
    {
      sbrpt = sbwpt = serbuf;
      return;
    }
  
  /* first empty buffer */
  serintr (unit);
  
  stat = ciab.pra;
  last = last_ciab_pra;
  last_ciab_pra = stat;
	
  /* check whether any interesting signal changed state */
  istat = stat ^ last;

  if ((istat & CIAB_PRA_CD) && DIALIN(tp->t_dev))
    {
      if (ISDCD (stat))
	(*linesw[tp->t_line].l_modem)(tp, 1);
      else if ((*linesw[tp->t_line].l_modem)(tp, 0) == 0)
	{
	  CLRDTR (stat);
	  CLRRTS (stat);
	  ciab.pra = stat;
	  last_ciab_pra = stat;
	}
    } 
  if ((istat & CIAB_PRA_CTS) && (tp->t_state & TS_ISOPEN) &&
	   (tp->t_cflag & CRTSCTS)) 
    {
#if 0
      /* the line is up and we want to do rts/cts flow control */
      if (ISCTS (stat))
	{
	  tp->t_state &=~ TS_TTSTOP;
	  ttstart(tp);
	  /* cause tbe-int if we were stuck there */
	  custom.intreq = INTF_SETCLR | INTF_TBE;
	}
      else
	tp->t_state |= TS_TTSTOP;
#else
      /* do this on hardware level, not with tty driver */
      if (ISCTS (stat))
	{
	  tp->t_state &= ~TS_TTSTOP;
	  /* cause TBE interrupt */
	  custom.intreq = INTF_SETCLR | INTF_TBE;
	}
#endif
    }
}

int
serioctl(dev, cmd, data, flag)
     dev_t dev;
     caddr_t data;
{
  register struct tty *tp;
  register int unit = SERUNIT(dev);
  register struct serdevice *ser;
  register int error;
  
  tp = ser_tty[unit];
  if (! tp)
    return ENXIO;
  
  error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag);
  if (error >= 0)
    return (error);

  error = ttioctl(tp, cmd, data, flag);
  if (error >= 0)
    return (error);
  
  ser = ser_addr[unit];
  switch (cmd) 
    {
    case TIOCSBRK:
      custom.adkcon = ADKCONF_SETCLR | ADKCONF_UARTBRK;
      break;

    case TIOCCBRK:
      custom.adkcon = ADKCONF_UARTBRK;
      break;

    case TIOCSDTR:
      (void) sermctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIS);
      break;
      
    case TIOCCDTR:
      (void) sermctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIC);
      break;
      
    case TIOCMSET:
      (void) sermctl(dev, *(int *)data, DMSET);
      break;
      
    case TIOCMBIS:
      (void) sermctl(dev, *(int *)data, DMBIS);
      break;
      
    case TIOCMBIC:
      (void) sermctl(dev, *(int *)data, DMBIC);
      break;
      
    case TIOCMGET:
      *(int *)data = sermctl(dev, 0, DMGET);
      break;
      
    default:
      return (ENOTTY);
    }

  return (0);
}

int
serparam(tp, t)
     register struct tty *tp;
     register struct termios *t;
{
  register struct serdevice *ser;
  register int cfcr, cflag = t->c_cflag;
  int unit = SERUNIT(tp->t_dev);
  int ospeed = ttspeedtab(t->c_ospeed, serspeedtab);
  
  /* check requested parameters */
  if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
    return (EINVAL);

  /* and copy to tty */
  tp->t_ispeed = t->c_ispeed;
  tp->t_ospeed = t->c_ospeed;
  tp->t_cflag = cflag;
  
  custom.intena = INTF_SETCLR | INTF_RBF | INTF_TBE;
  last_ciab_pra = ciab.pra;
  
  if (ospeed == 0) 
    {
      (void) sermctl(tp->t_dev, 0, DMSET);  /* hang up line */
      return (0);
    }
  else
    {
      /* make sure any previous hangup is undone, ie.
	 reenable DTR. */
      (void) sermctl (tp->t_dev, TIOCM_DTR | TIOCM_RTS, DMSET);
    }
  /* set the baud rate */
  custom.serper = (0<<15) | ospeed;  /* select 8 bit mode (instead of 9 bit) */
  
  return (0);
}


static void
ser_putchar (tp, c)
     struct tty *tp;
     unsigned short c;
{
  /* handle truncation of character if necessary */
  if ((tp->t_cflag & CSIZE) == CS7)
    c &= 0x7f;
  
  /* handle parity if necessary (forces CS7) */
  if (tp->t_cflag & PARENB)
    {
      c &= 0x7f;
      if (even_parity[c])
	c |= 0x80;
      if (tp->t_cflag & PARODD)
	c ^= 0x80;
    }
  
  /* add stop bit(s) */
  if (tp->t_cflag & CSTOPB)
    c |= 0x300;
  else
    c |= 0x100;
  
  custom.serdat = c;
}


#define SEROBUF_SIZE	32
static u_char ser_outbuf[SEROBUF_SIZE];
static u_char *sob_ptr=ser_outbuf, *sob_end=ser_outbuf;
void
ser_outintr ()
{
  struct tty *tp = ser_tty[0]; /* hmmmmm */
  unsigned short c;
  int s = spltty ();

  if (! tp)
    goto out;

  if (! (custom.intreqr & INTF_TBE))
    goto out;

  /* clear interrupt */
  custom.intreq = INTF_TBE;

  if (sob_ptr == sob_end)
    {
      tp->t_state &= ~(TS_BUSY|TS_FLUSH);
      if (tp->t_line)
	(*linesw[tp->t_line].l_start)(tp);
      else
	serstart (tp);

      goto out;
    }

  /* do hardware flow control here. if the CTS line goes down, don't
     transmit anything. That way, we'll be restarted by the periodic
     interrupt when CTS comes back up. */
  if (ISCTS (ciab.pra))
    ser_putchar (tp, *sob_ptr++);
out:
  splx (s);
}
 
int
serstart(tp)
     register struct tty *tp;
{
  register int cc, s;
  int unit;
  register struct serdevice *ser;
  int hiwat = 0;

  if (! (tp->t_state & TS_ISOPEN))
    return;

  unit = SERUNIT(tp->t_dev);
  ser = ser_addr[unit];

  s = spltty();
  if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP)) 
    goto out;

  cc = tp->t_outq.c_cc;
  if (cc <= tp->t_lowat) 
    {
      if (tp->t_state & TS_ASLEEP) 
	{
	  tp->t_state &= ~TS_ASLEEP;
	  wakeup((caddr_t)&tp->t_outq);
	}
      selwakeup(&tp->t_wsel);
    }

  if (! cc || (tp->t_state & TS_BUSY))
    goto out;

  /* we only do bulk transfers if using CTSRTS flow control,
     not for (probably sloooow) ixon/ixoff devices. */
  if (! (tp->t_cflag & CRTSCTS))
    cc = 1;

  /*
   * Limit the amount of output we do in one burst
   * to prevent hogging the CPU. 
   */
  if (cc > SEROBUF_SIZE) 
    {
      hiwat++;
      cc = SEROBUF_SIZE;
    }
  cc = q_to_b (&tp->t_outq, ser_outbuf, cc);
  if (cc > 0)
    {
      tp->t_state |= TS_BUSY;

      sob_ptr = ser_outbuf;
      sob_end = ser_outbuf + cc;
      /* get first character out, then have tbe-interrupts blow out
	 further characters, until buffer is empty, and TS_BUSY
	 gets cleared. */
      ser_putchar (tp, *sob_ptr++);
    }

out:  
  splx(s);
}
 
/*
 * Stop output on a line.
 */
/*ARGSUSED*/
int
serstop(tp, flag)
     register struct tty *tp;
{
  register int s;

  s = spltty();
  if (tp->t_state & TS_BUSY) 
    {
      if ((tp->t_state & TS_TTSTOP) == 0)
	tp->t_state |= TS_FLUSH;
    }
  splx(s);
}
 
int
sermctl(dev, bits, how)
     dev_t dev;
     int bits, how;
{
  register struct serdevice *ser;
  register int unit;
  u_char ub;
  int s;

  unit = SERUNIT(dev);
  ser = ser_addr[unit];

  /* convert TIOCM* mask into CIA mask (which is really low-active!!) */
  if (how != DMGET)
    {
      ub = 0;
      if (bits & TIOCM_DTR) ub |= CIAB_PRA_DTR;
      if (bits & TIOCM_RTS) ub |= CIAB_PRA_RTS;
      if (bits & TIOCM_CTS) ub |= CIAB_PRA_CTS;
      if (bits & TIOCM_CD)  ub |= CIAB_PRA_CD;
      if (bits & TIOCM_RI)  ub |= CIAB_PRA_SEL;	/* collision with /dev/par ! */
      if (bits & TIOCM_DSR) ub |= CIAB_PRA_DSR;
    }


  s = spltty();
  switch (how) 
    {
    case DMSET:
      /* invert and set */
      ciab.pra = ~ub;
      break;
      
    case DMBIC:
      ciab.pra |= ub;
      ub = ~ciab.pra;
      break;
      
    case DMBIS:
      ciab.pra &= ~ub;
      ub = ~ciab.pra;
      break;
      
    case DMGET:
      ub = ~ciab.pra;
      break;
    }
  (void) splx(s);
  
  bits = 0;
  if (ub & CIAB_PRA_DTR) bits |= TIOCM_DTR;
  if (ub & CIAB_PRA_RTS) bits |= TIOCM_RTS;
  if (ub & CIAB_PRA_CTS) bits |= TIOCM_CTS;
  if (ub & CIAB_PRA_CD)  bits |= TIOCM_CD;
  if (ub & CIAB_PRA_SEL) bits |= TIOCM_RI;
  if (ub & CIAB_PRA_DSR) bits |= TIOCM_DSR;
  
  return bits;
}

/*
 * Following are all routines needed for SER to act as console
 */
#include "../amiga/cons.h"

sercnprobe(cp)
	struct consdev *cp;
{
  int unit = CONUNIT;
  /* locate the major number */
  for (sermajor = 0; sermajor < nchrdev; sermajor++)
    if (cdevsw[sermajor].d_open == seropen)
      break;

  /* XXX: ick */
  unit = CONUNIT;

  /* initialize required fields */
  cp->cn_dev = makedev(sermajor, unit);
#if 0
  /* on ser it really doesn't matter whether we're later
     using the tty interface or single-character io thru
     cnputc, so don't reach out to later on remember that
     our console is here (see ite.c) */
  cp->cn_tp = ser_tty[unit];
#endif
  cp->cn_pri = CN_NORMAL;

  /*
   * If serconsole is initialized, raise our priority.
   */
  if (serconsole == unit)
    cp->cn_pri = CN_REMOTE;
#ifdef KGDB
  if (major(kgdb_dev) == 1)			/* XXX */
    kgdb_dev = makedev(sermajor, minor(kgdb_dev));
#endif
}

sercninit(cp)
	struct consdev *cp;
{
  int unit = SERUNIT(cp->cn_dev);

  serinit(unit, serdefaultrate);
  serconsole = unit;
  serconsinit = 1;
}

serinit(unit, rate)
	int unit, rate;
{
  int s;

#ifdef lint
  stat = unit; if (stat) return;
#endif
  s = splhigh();
  /* might want to fiddle with the CIA later ??? */
  custom.serper = ttspeedtab(rate, serspeedtab);
  splx(s);
}

sercngetc(dev)
{
  u_short stat;
  int c, s;

#ifdef lint
  stat = dev; if (stat) return (0);
#endif
  s = splhigh();
  while (!((stat = custom.serdatr & 0xffff) & SERDATRF_RBF))
    ;
  c = stat & 0xff;
  /* clear interrupt */
  custom.intreq = INTF_RBF;
  splx(s);
  return (c);
}

/*
 * Console kernel output character routine.
 */
sercnputc(dev, c)
     dev_t dev;
     register int c;
{
  register int timo;
  short stat;
  int s = splhigh();

#ifdef lint
  stat = dev; if (stat) return;
#endif
  if (serconsinit == 0) 
    {
      (void) serinit(SERUNIT(dev), serdefaultrate);
      serconsinit = 1;
    }

  /* wait for any pending transmission to finish */
  timo = 50000;
  while (! (custom.serdatr & SERDATRF_TBE) && --timo)
    ;

  custom.serdat = (c&0xff) | 0x100;
  /* wait for this transmission to complete */
  timo = 1500000;
  while (! (custom.serdatr & SERDATRF_TBE) && --timo) 
    ;

  /* wait for the device (my vt100..) to process the data, since
     we don't do flow-control with cnputc */
  for (timo = 0; timo < 30000; timo++) ;
  
  /* clear any interrupts generated by this transmission */
  custom.intreq = INTF_TBE;
  splx(s);
}


serspit(c)
     int c;
{
  register struct Custom *cu asm("a2") = (struct Custom *)CUSTOMbase;
  register int timo asm("d2");
  extern int cold;
  int s;
  
  if (c == 10)
    serspit (13);
  
  s = splhigh();
  
  /* wait for any pending transmission to finish */
  timo = 500000;
  while (! (cu->serdatr & (SERDATRF_TBE|SERDATRF_TSRE)) && --timo)
    ;
  cu->serdat = (c&0xff) | 0x100;
  /* wait for this transmission to complete */
  timo = 15000000;
  while (! (cu->serdatr & SERDATRF_TBE) && --timo) 
    ;
  /* clear any interrupts generated by this transmission */
  cu->intreq = INTF_TBE;
  
  for (timo = 0; timo < 30000; timo++) ;
  
  splx (s);
}

serspits(cp)
     char *cp;
{
  while (*cp)
    serspit(*cp++);
}

int
serselect(dev, rw, p)
     dev_t dev;
     int rw;
     struct proc *p;
{
  register struct tty *tp = ser_tty[SERUNIT(dev)];
  int nread;
  int s = spltty();
  struct proc *selp;
  
  switch (rw) 
    {
    case FREAD:
      nread = ttnread(tp);
      if (nread > 0 || ((tp->t_cflag&CLOCAL) == 0 
			&& (tp->t_state&TS_CARR_ON) == 0))
	goto win;
      selrecord(p, &tp->t_rsel);
      break;
      
    case FWRITE:
      if (tp->t_outq.c_cc <= tp->t_lowat)
	goto win;
      selrecord(p, &tp->t_wsel);
      break;
    }
  splx(s);
  return (0);

win:
  splx(s);
  return (1);
}

#endif
