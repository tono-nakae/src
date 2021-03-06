#	$NetBSD: Makefile,v 1.9 2013/06/23 13:56:40 matt Exp $

S=		${.CURDIR}/../../../..

PROG=		bootmini2440
SRCS=		entry.S main.c devopen.c netif.c dev_net.c dm9000.c dev_sdmmc.c
SRCS+=		s3csdi.c vers.c

.include <bsd.own.mk>

CLEANFILES+=	vers.c ${PROG}.elf
CFLAGS+=	-Wall -Wno-main -ffreestanding -fno-unwind-tables
CPPFLAGS+=	-D_STANDALONE -DSUPPORT_DHCP -DLIBSA_PRINTF_WIDTH_SUPPORT
CPPFLAGS+=	-DDM9000MAC="0x08,0x08,0x11,0x18,0x12,0x27" 
CPPFLAGS+=	-DDEFAULT_BOOTFILE="ld0a:netbsd;net:"
CPPFLAGS+=	-nostdinc -I. -I${.OBJDIR} -I${S} -I${S}/arch
CPPFLAGS+=	-march=armv4 -mabi=apcs-gnu -mfloat-abi=soft
CPUFLAGS=
DBG=		

LIBCRT0=	# nothing
LIBCRTBEGIN=	# nothing
LIBCRTEND=	# nothing
LIBC=		# nothing

MAN=		# no manual page
NOMAN=		# defined
STRIPFLAG=
BINMODE=	444

RELOC=		0x30A00000
ENTRY=		_start

.if !make(obj) && !make(clean) && !make(cleandir)
.BEGIN:
	@[ -h machine ] || ln -s ${S}/arch/evbarm/include machine
	@[ -h arm ] || ln -s ${S}/arch/arm/include arm
.NOPATH: machine arm
.endif
CLEANFILES+= machine arm

### find out what to use for libkern
KERN_AS=	library
.include "${S}/lib/libkern/Makefile.inc"
LIBKERN=	${KERNLIB}

### find out what to use for libz
Z_AS=		library
.include "${S}/lib/libz/Makefile.inc"
LIBZ=		${ZLIB}

### find out what to use for libsa
SA_AS=		library
SAMISCMAKEFLAGS= SA_USE_CREAD=yes SA_USE_LOADFILE=yes
.include "${S}/lib/libsa/Makefile.inc"
LIBSA=		${SALIB}

.PHONY: vers.c
vers.c: version
	${HOST_SH} ${S}/conf/newvers_stand.sh -K \
	    ${${MKREPRO} == "yes" :?:-D} ${.CURDIR}/version "evbarm"

${PROG}: ${OBJS} ${LIBSA} ${LIBZ} ${LIBKERN}
	${_MKTARGET_LINK}
	${LD} -N -Ttext ${RELOC} -Bstatic -e ${ENTRY} -o ${.TARGET}.elf \
	    ${OBJS} ${LIBSA} ${LIBZ} ${LIBKERN}
	${OBJCOPY} -S -O binary ${.TARGET}.elf ${.TARGET}

.include <bsd.prog.mk>
