#	$NetBSD: bsd.endian.mk,v 1.5 2003/07/27 11:16:30 lukem Exp $

.ifndef TARGET_ENDIANNESS

.include <bsd.init.mk>

# find out endianness of target and set proper flag for pwd_mkdb and such,
# so that it creates database in same endianness.
#
.if exists(${DESTDIR}/usr/include/sys/endian.h) && exists(${CC:ts::C/:.*$//})
TARGET_ENDIANNESS!= \
	printf '\#include <sys/endian.h>\n_BYTE_ORDER\n' | \
	${CC} -nostdinc ${CPPFLAG_ISYSTEM} ${DESTDIR}/usr/include -E - | \
	tail -1 | awk '{print $$1}'
.else
TARGET_ENDIANNESS=
.endif

#.if ${TARGET_ENDIANNESS} == "1234"
#TARGET_ENDIANNESS=	little
#.elif ${TARGET_ENDIANNESS} == "4321"
#TARGET_ENDIANNESS=	big
#.else
#TARGET_ENDIANNESS=	unknown
#.endif

.endif	# TARGET_ENDIANNESS
