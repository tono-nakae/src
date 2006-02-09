/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By downloading, copying, installing or
 * using the software you agree to this license. If you do not agree to this license, do not download, install,
 * copy or use the software.
 *
 * Intel License Agreement
 *
 * Copyright (c) 2000, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *
 * -Redistributions of source code must retain the above copyright notice, this list of conditions and the
 *  following disclaimer.
 *
 * -Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 *  following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * -The name of Intel Corporation may not be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif


/*
 *
 */

#define ISCSI_HTONLL6(x) (uint64_t) \
      ( ((uint64_t)( ((uint64_t)(x) & (uint64_t)0x0000ff0000000000uLL) >> 40))     \
      | ((uint64_t)( ((uint64_t)(x) & (uint64_t)0x000000ff00000000uLL) >> 24))     \
      | ((uint64_t)( ((uint64_t)(x) & (uint64_t)0x00000000ff000000uLL) >> 8))      \
      | ((uint64_t)( ((uint64_t)(x) & (uint64_t)0x0000000000ff0000uLL) << 8))      \
      | ((uint64_t)( ((uint64_t)(x) & (uint64_t)0x000000000000ff00uLL) << 24))     \
      | ((uint64_t)( ((uint64_t)(x) & (uint64_t)0x00000000000000ffuLL) << 40)))

#define ISCSI_NTOHLL6(x) (uint64_t) \
      ( ((uint64_t)( ((uint64_t)(x) & (uint64_t)0x0000ff0000000000uLL) >> 40))     \
      | ((uint64_t)( ((uint64_t)(x) & (uint64_t)0x000000ff00000000uLL) >> 24))     \
      | ((uint64_t)( ((uint64_t)(x) & (uint64_t)0x00000000ff000000uLL) >> 8))      \
      | ((uint64_t)( ((uint64_t)(x) & (uint64_t)0x0000000000ff0000uLL) << 8))      \
      | ((uint64_t)( ((uint64_t)(x) & (uint64_t)0x000000000000ff00uLL) << 24))     \
      | ((uint64_t)( ((uint64_t)(x) & (uint64_t)0x00000000000000ffuLL) << 40)))

/*
 * Debugging Levels
 */

#define TRACE_NET_DEBUG      0x00000001
#define TRACE_NET_BUFF       0x00000002
#define TRACE_NET_IOV        0x00000004
#define TRACE_NET_ALL        (TRACE_NET_DEBUG|TRACE_NET_BUFF|TRACE_NET_IOV)

#define TRACE_ISCSI_DEBUG    0x00000010
#define TRACE_ISCSI_CMD      0x00000020
#define TRACE_ISCSI_ARGS     0x00000040
#define TRACE_ISCSI_PARAM    0x00000080
#define TRACE_ISCSI_ALL      (TRACE_ISCSI_DEBUG|TRACE_ISCSI_ARGS|TRACE_ISCSI_PARAM|TRACE_ISCSI_CMD)

#define TRACE_SCSI_DEBUG     0x00000100
#define TRACE_SCSI_CMD       0x00000200
#define TRACE_SCSI_DATA      0x00000400
#define TRACE_SCSI_ARGS      0x00000800
#define TRACE_SCSI_ALL       (TRACE_SCSI_DEBUG|TRACE_SCSI_CMD|TRACE_SCSI_DATA|TRACE_SCSI_ARGS)

#define TRACE_DEBUG          0x00001000
#define TRACE_HASH           0x00002000
#define TRACE_SYNC           0x00004000
#define TRACE_QUEUE          0x00008000
#define TRACE_WARN           0x00010000
#define TRACE_MEM            0x00020000

#define TRACE_OSD            0x00040000
#define TRACE_OSDFS          0x00080000
#define TRACE_OSDSO          0x00100000
#define TRACE_ALL            0xffffffff

/*
 * Set debugging level here. Turn on debugging in Makefile.
  */
#ifndef EXTERN
#define EXTERN  extern
#endif

EXTERN uint32_t iscsi_debug_level;

void	set_debug(const char *);

/*
 * Debugging Functions
 */

#ifdef CONFIG_ISCSI_DEBUG

#define TRACE(trace, args...) do {					\
	if (iscsi_debug_level & trace) {				\
		char     _iscsi_trace_buf[8192];			\
		(void) snprintf(_iscsi_trace_buf, sizeof(_iscsi_trace_buf), args); \
		PRINT("pid %i:%s:%d: %s",				\
			ISCSI_GETPID, __FILE__, __LINE__,		\
			_iscsi_trace_buf);				\
	}								\
} while (/* CONSTCOND */ 0)

#define PRINT_BUFF(buf, len) do {					\
	if (iscsi_debug_level & TRACE_NET_BUFF) {			\
		int	_i;						\
		for (_i=0 ; _i < len; _i++) {				\
			if (_i % 4 == 0) {				\
				if (_i) {				\
					PRINT("\n");			\
				}					\
				PRINT("%4i:", _i);			\
			}						\
			PRINT("%2x ", (uint8_t) (buf)[_i]);		\
		}							\
		if ((len + 1) % 32) {					\
			PRINT("\n");					\
		}							\
	}								\
} while (/* CONSTCOND */ 0)

#define TRACE_WARNING(args...) do {					\
	if (iscsi_debug_level & TRACE_WARN) {				\
		char     _iscsi_trace_buf[8192];			\
		(void) snprintf(_iscsi_trace_buf, sizeof(_iscsi_trace_buf), args); \
		PRINT("pid %i:%s:%i: ***WARNING*** %s",			\
		       ISCSI_GETPID, __FILE__, __LINE__,		\
		       _iscsi_trace_buf);				\
	}								\
} while (/* CONSTCOND */ 0)
#else
#define TRACE(trace, args...)
#define TRACE_WARNING(args...)
#define PRINT_BUFF(buf, len)
#endif

#define TRACE_ERROR(args...) do {					\
	char     _iscsi_trace_buf[8192];				\
	(void) snprintf(_iscsi_trace_buf, sizeof(_iscsi_trace_buf), args); \
	PRINT("pid %i:%s:%i: ***ERROR*** %s",				\
		       ISCSI_GETPID, __FILE__, __LINE__,		\
		       _iscsi_trace_buf);				\
	syslog(LOG_ERR, "pid %d:%s:%d: ***ERROR*** %s",			\
		       ISCSI_GETPID, __FILE__, __LINE__,		\
		       _iscsi_trace_buf);				\
} while (/* CONSTCOND */ 0)

/*
 * Byte Order
  */

#ifdef HAVE_ASM_BYTEORDER_H
#include <asm/byteorder.h>
#endif

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

#ifdef HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h>
#endif

#define __BYTE_ORDER    _BYTE_ORDER
#define __BIG_ENDIAN    _BIG_ENDIAN
#define __LITTLE_ENDIAN _LITTLE_ENDIAN

/*
#if __BYTE_ORDER == __BIG_ENDIAN
#    define ISCSI_NTOHLL(x) (x)
#    define ISCSI_HTONLL(x) (x)
#    define ISCSI_NTOHL(x)  (x)
#    define ISCSI_HTONL(x)  (x)
#    define ISCSI_NTOHS(x)  (x)
#    define ISCSI_HTONS(x)  (x)
#else
#    define ISCSI_NTOHLL(x) bswap_64(x)
#    define ISCSI_HTONLL(x) bswap_64(x)
#    define ISCSI_NTOHL(x)  bswap_32(x)
#    define ISCSI_HTONL(x)  bswap_32(x)
#    define ISCSI_NTOHS(x)  bswap_16(x)
#    define ISCSI_HTONS(x)  bswap_16(x)
#endif
#else
*/
#  define ISCSI_NTOHLL(x) 	BE64TOH(x)
#  define ISCSI_HTONLL(x) 	HTOBE64(x)
#  define ISCSI_NTOHL(a)	ntohl(a)
#  define ISCSI_HTONL(a)	htonl(a)
#  define ISCSI_NTOHS(a)	ntohs(a)
#  define ISCSI_HTONS(a)	htons(a)
/*
#endif .* !linux */

/*
 * printf and printk
 */

#ifdef __KERNEL__
#define PRINT printk
#else
#define PRINT printf
#endif

/*
 * Process ID
 */

#ifdef __KERNEL__
#define ISCSI_GETPID current->pid
#else
#define ISCSI_GETPID getpid()
#endif

/*
 * Sleeping
 */

#ifdef __KERNEL__
#define ISCSI_SLEEP(N) {uint32_t future = jiffies+N*HZ; while (jiffies<future) ISCSI_SPIN;}
#else
#define ISCSI_SLEEP(N) sleep(N)
#endif

/*
 * Memory
 */

void           *iscsi_malloc(unsigned);
void            iscsi_free(void *);
void           *iscsi_malloc_atomic(unsigned);
void            iscsi_free_atomic(void *);

/*
 * Comparison
 */

#ifndef MIN
#define MIN(A,B) (((A)<(B))?(A):(B))
#endif

#define MIN_3(A,B,C) (((A)<(B))?(((A)<(C))?(A):(C)):(((B)<(C))?(B):(C)))

/* Spin locks */

#ifdef __KERNEL__
typedef         spinlock_t
                iscsi_spin_t;
#else
typedef         pthread_mutex_t
                iscsi_spin_t;
#endif

int             iscsi_spin_init(iscsi_spin_t * );
int             iscsi_spin_lock(iscsi_spin_t * );
int             iscsi_spin_unlock(iscsi_spin_t * );
int             iscsi_spin_lock_irqsave(iscsi_spin_t * , uint32_t *);
int             iscsi_spin_unlock_irqrestore(iscsi_spin_t * , uint32_t *);
int             iscsi_spin_destroy(iscsi_spin_t * );

/*
 * End of ISCSI spin routines
 */

/*
 * Tags
 */

#define ISCSI_SET_TAG(tag) do {						\
	iscsi_spin_lock(&g_tag_spin);					\
	*tag = g_tag++;							\
	iscsi_spin_unlock(&g_tag_spin);					\
} while (/* CONSTCOND */ 0)

#define ISCSI_SET_TAG_IN_INTR(tag) do {					\
	uint32_t flags;							\
	iscsi_spin_lock_irqsave(&g_tag_spin, &flags);			\
	*tag = g_tag++;							\
	iscsi_spin_unlock_irqrestore(&g_tag_spin, &flags);		\
} while (/* CONSTCOND */ 0)



/*
 * Hashing
 */


typedef struct hash_t {
	struct initiator_cmd_t **bucket;
	int             collisions;
	int             insertions;
	int             n;
	iscsi_spin_t    lock;
}               hash_t;

int             hash_init(hash_t * , int );
int             hash_insert(hash_t * , struct initiator_cmd_t * , uint32_t );
struct initiator_cmd_t *hash_remove(hash_t * , uint32_t );
int             hash_destroy(hash_t * );

/*
 * Queuing
 */

typedef struct iscsi_queue_t {
	int             head;
	int             tail;
	int             count;
	void          **elem;
	int             depth;
	iscsi_spin_t    lock;
}               iscsi_queue_t;

int             iscsi_queue_init(iscsi_queue_t * , int );
void            iscsi_queue_destroy(iscsi_queue_t * );
int             iscsi_queue_insert(iscsi_queue_t * , void *);
void           *iscsi_queue_remove(iscsi_queue_t * );
int             iscsi_queue_depth(iscsi_queue_t * );
int             iscsi_queue_full(iscsi_queue_t * );

/*
 * Socket Abstraction
 */

#ifdef __KERNEL__
typedef struct socket *iscsi_socket_t;
#else
typedef int     iscsi_socket_t;
#endif

/* Turning off Nagle's Algorithm doesn't always seem to work, */
/* so we combine two messages into one when the second's size */
/* is less than or equal to ISCSI_SOCK_HACK_CROSSOVER. */

#define ISCSI_SOCK_HACK_CROSSOVER    1024
#define ISCSI_SOCK_CONNECT_NONBLOCK  0
#define ISCSI_SOCK_CONNECT_TIMEOUT   1
#define ISCSI_SOCK_MSG_BYTE_ALIGN    4

int             iscsi_sock_create(iscsi_socket_t * );
int             iscsi_sock_setsockopt(iscsi_socket_t * , int , int , void *, unsigned  );
int             iscsi_sock_getsockopt(iscsi_socket_t * , int , int , void *, unsigned *);
int             iscsi_sock_bind(iscsi_socket_t , int );
int             iscsi_sock_listen(iscsi_socket_t );
int             iscsi_sock_connect(iscsi_socket_t , char *, int );
int             iscsi_sock_accept(iscsi_socket_t , iscsi_socket_t * );
int             iscsi_sock_shutdown(iscsi_socket_t , int );
int             iscsi_sock_close(iscsi_socket_t );
int             iscsi_sock_msg(iscsi_socket_t , int , unsigned , void *, int );
int		iscsi_sock_send_header_and_data(iscsi_socket_t ,
				void *, unsigned ,
				const void *, unsigned , int );
int             iscsi_sock_getsockname(iscsi_socket_t , struct sockaddr * , unsigned *);
int             iscsi_sock_getpeername(iscsi_socket_t , struct sockaddr * , unsigned *);
int             modify_iov(struct iovec ** , int *, uint32_t , uint32_t );

/*
 * Mutexes
 */

#ifdef __KERNEL__
typedef struct semaphore iscsi_mutex_t;
#else
typedef pthread_mutex_t iscsi_mutex_t;
#endif

int             iscsi_mutex_init(iscsi_mutex_t * );
int             iscsi_mutex_lock(iscsi_mutex_t * );
int             iscsi_mutex_unlock(iscsi_mutex_t * );
int             iscsi_mutex_destroy(iscsi_mutex_t * );

#define ISCSI_LOCK(M, ELSE)	do {					\
	if (iscsi_mutex_lock(M) != 0) {					\
		TRACE_ERROR("iscsi_mutex_lock() failed\n");		\
		ELSE;							\
	}								\
} while (/* CONSTCOND */ 0)

#define ISCSI_UNLOCK(M, ELSE)	do {					\
	if (iscsi_mutex_unlock(M) != 0) {				\
		TRACE_ERROR("iscsi_mutex_unlock() failed\n");		\
		ELSE;							\
	}								\
} while (/* CONSTCOND */ 0)

#define ISCSI_MUTEX_INIT(M, ELSE) do {					\
	if (iscsi_mutex_init(M) != 0) {					\
		TRACE_ERROR("iscsi_mutex_init() failed\n");		\
		ELSE;							\
	}								\
} while (/* CONSTCOND */ 0)

#define ISCSI_MUTEX_DESTROY(M, ELSE) do {				\
	if (iscsi_mutex_destroy(M) != 0) {				\
		TRACE_ERROR("iscsi_mutex_destroy() failed\n");		\
		ELSE;							\
	}								\
} while (/* CONSTCOND */ 0)

/*
 * Condition Variable
 */

#ifdef __KERNEL__
typedef struct semaphore iscsi_cond_t;
#else
typedef pthread_cond_t iscsi_cond_t;
#endif

int             iscsi_cond_init(iscsi_cond_t * );
int             iscsi_cond_wait(iscsi_cond_t * , iscsi_mutex_t * );
int             iscsi_cond_signal(iscsi_cond_t * );
int             iscsi_cond_destroy(iscsi_cond_t * );

#define ISCSI_COND_INIT(C, ELSE) do {					\
	if (iscsi_cond_init(C) != 0) {					\
		ELSE;							\
	}								\
} while (/* CONSTCOND */ 0)

#define ISCSI_WAIT(C, M, ELSE)	do {					\
	if (iscsi_cond_wait(C, M) != 0) {				\
		ELSE;							\
	}								\
} while (/* CONSTCOND */ 0)

#define ISCSI_SIGNAL(C, ELSE) 	do {					\
	if (iscsi_cond_signal(C) != 0) {				\
		ELSE;							\
	}								\
} while (/* CONSTCOND */ 0)

#define ISCSI_COND_DESTROY(C, ELSE)	do {				\
	if (iscsi_cond_destroy(C) != 0) {				\
		ELSE;							\
	}								\
} while (/* CONSTCOND */ 0)

/*
 * Threading Routines
 */

typedef struct iscsi_thread_t {
#ifdef __KERNEL__
	struct task_struct *pthread;
#else
	pthread_t       pthread;
#endif
}               iscsi_thread_t;

int             iscsi_thread_create(iscsi_thread_t * , void *(*proc) (void *), void *);

#ifdef __KERNEL__
#define ISCSI_SET_THREAD(ME) me->thread.pthread = current;

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,4,0)
#define REPARENT_TO_INIT() reparent_to_init()
#define ISCSI_THREAD_START(NAME)      \
      lock_kernel();                      \
      daemonize();                        \
      REPARENT_TO_INIT(); 				  \
      unlock_kernel();                    \
      sprintf(current->comm, "%s", NAME);
#else
#define REPARENT_TO_INIT
#define ISCSI_THREAD_START(NAME)      \
      sprintf(current->comm, "%s", NAME);
#endif
#else
#define ISCSI_SET_THREAD(ME)	/* for user pthread id set by pthread_create
				 * in iscsi_thread_create */
#define ISCSI_THREAD_START(NAME)
#endif

/*
 * Worker Thread
 */

#define ISCSI_WORKER_STATE_STARTED   1
#define ISCSI_WORKER_STATE_ERROR     2
#define ISCSI_WORKER_STATE_EXITING   4

typedef struct {
	iscsi_thread_t  thread;
	iscsi_mutex_t   work_mutex;
	iscsi_cond_t    work_cond;
	iscsi_mutex_t   exit_mutex;
	iscsi_cond_t    exit_cond;
	int             id;
	int             pid;
	volatile uint32_t state;
}               iscsi_worker_t;

#define ISCSI_WORKER_EXIT(ME) do {					\
	TRACE(TRACE_ISCSI_DEBUG ,"exiting\n");				\
	(ME)->state |= ISCSI_WORKER_STATE_EXITING;			\
	return 0;							\
} while (/* CONSTCOND */ 0)

/*
 * Spin Lock
 */

#ifdef __KERNEL__
#define ISCSI_SPIN schedule()
#else
#define ISCSI_SPIN
#endif

/*
 * Pre/Post condition checking
 */

#define NO_CLEANUP {}
#define RETURN_GREATER(NAME, V1, V2, CU, RC)                         \
if ((V1)>(V2)) {                                                     \
  TRACE_ERROR("Bad \"%s\": %u > %u.\n", NAME, (unsigned)V1, (unsigned)V2); \
  CU;                                                                \
  return RC;                                                         \
}

#define RETURN_NOT_EQUAL(NAME, V1, V2, CU, RC)                       \
if ((V1)!=(V2)) {                                                    \
  TRACE_ERROR("Bad \"%s\": Got %u expected %u.\n", NAME, V1, V2);    \
  CU;                                                                \
  return RC;                                                         \
}

#define WARN_NOT_EQUAL(NAME, V1, V2)                                 \
if ((V1)!=(V2)) {                                                    \
  TRACE_WARNING("Bad \"%s\": Got %u expected %u.\n", NAME, V1, V2);  \
}

#define RETURN_EQUAL(NAME, V1, V2, CU, RC)                           \
if ((V1)==(V2)) {                                                    \
  TRACE_ERROR("Bad \"%s\": %u == %u.\n", NAME, V1, V2);              \
  CU;                                                                \
  return RC;                                                         \
}

/*
 * Misc. Functions
 */

uint32_t        iscsi_atoi(char *);
int HexTextToData(const char *, uint32_t , uint8_t *, uint32_t );
int HexDataToText(uint8_t *, uint32_t , char *, uint32_t );
void            GenRandomData(uint8_t *, uint32_t );

/* this is the maximum number of iovecs which we can use in iscsi_sock_send_header_and_data */
#ifndef ISCSI_MAX_IOVECS
#define ISCSI_MAX_IOVECS        32
#endif

enum {  
	/* used in iscsi_sock_msg() */
	Receive = 0,
	Transmit = 1
};


#endif				/* _UTIL_H_ */
