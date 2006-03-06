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

#include "config.h"

#include <sys/types.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <unistd.h>

#include "compat.h"
#include "util.h"



/*
 * Memory Allocation
 */

void           *
iscsi_malloc_atomic(unsigned n)
{
	void           *ptr;

	ptr = malloc(n);
	TRACE(TRACE_MEM, "iscsi_malloc_atomic(%i) = 0x%p\n", n, ptr);
	return ptr;
}

void           *
iscsi_malloc(unsigned n)
{
	void           *ptr;

	ptr = malloc(n);
	TRACE(TRACE_MEM, "iscsi_malloc_atomic(%i) = 0x%p\n", n, ptr);
	return ptr;
}

void 
iscsi_free_atomic(void *ptr)
{
	(void) free(ptr);
	TRACE(TRACE_MEM, "iscsi_free_atomic(0x%p)\n", ptr);
}

void 
iscsi_free(void *ptr)
{
	(void) free(ptr);
	TRACE(TRACE_MEM, "iscsi_free(0x%p)\n", ptr);
}

/* debugging levels */
void
set_debug(const char *level)
{
	if (strcmp(level, "net") == 0) {
		iscsi_debug_level |= TRACE_NET_ALL;
	} else if (strcmp(level, "iscsi") == 0) {
		iscsi_debug_level |= TRACE_ISCSI_ALL;
	} else if (strcmp(level, "scsi") == 0) {
		iscsi_debug_level |= TRACE_SCSI_ALL;
	} else if (strcmp(level, "osd") == 0) {
		iscsi_debug_level |= TRACE_OSD;
	} else if (strcmp(level, "all") == 0) {
		iscsi_debug_level |= TRACE_ALL;
	}
}

/*
 * Threading Routines
 */
int
iscsi_thread_create(iscsi_thread_t * thread, void *(*proc) (void *), void *arg)
{
	if (pthread_create(&thread->pthread, NULL, proc, arg) != 0) {
		TRACE_ERROR("pthread_create() failed\n");
		return -1;
	}
	if (pthread_detach(thread->pthread) != 0) {
		TRACE_ERROR("pthread_detach() failed\n");
		return -1;
	}
	return 0;
}

/*
 * Queuing Functions
 */
int 
iscsi_queue_init(iscsi_queue_t * q, int depth)
{
	q->head = q->tail = q->count = 0;
	q->depth = depth;
	if ((q->elem = iscsi_malloc_atomic(depth * sizeof(void *))) == NULL) {
		TRACE_ERROR("iscsi_malloc_atomic() failed\n");
		return -1;
	}
	iscsi_spin_init(&q->lock);
	return 0;
}

void 
iscsi_queue_destroy(iscsi_queue_t * q)
{
	iscsi_free_atomic(q->elem);
}

int 
iscsi_queue_full(iscsi_queue_t * q)
{
	return (q->count == q->depth);
}

int 
iscsi_queue_depth(iscsi_queue_t * q)
{
	return q->count;
}

int 
iscsi_queue_insert(iscsi_queue_t * q, void *ptr)
{
	uint32_t   flags;

	iscsi_spin_lock_irqsave(&q->lock, &flags);
	if (iscsi_queue_full(q)) {
		TRACE_ERROR("QUEUE FULL\n");
		iscsi_spin_unlock_irqrestore(&q->lock, &flags);
		return -1;
	}
	q->elem[q->tail] = ptr;
	q->tail++;
	if (q->tail == q->depth) {
		q->tail = 0;
	}
	q->count++;
	iscsi_spin_unlock_irqrestore(&q->lock, &flags);
	return 0;
}

void           *
iscsi_queue_remove(iscsi_queue_t * q)
{
	void           *ptr;
	uint32_t   flags = 0;
	iscsi_spin_lock_irqsave(&q->lock, &flags);
	if (!iscsi_queue_depth(q)) {
		TRACE(TRACE_QUEUE, "QUEUE EMPTY\n");
		iscsi_spin_unlock_irqrestore(&q->lock, &flags);
		return NULL;
	}
	q->count--;
	ptr = q->elem[q->head];
	q->head++;
	if (q->head == q->depth) {
		q->head = 0;
	}
	iscsi_spin_unlock_irqrestore(&q->lock, &flags);
	return ptr;
}

/*
 * Hashing Functions
 */
#include "initiator.h"

int 
hash_init(hash_t * h, int n)
{
	int	i;

	iscsi_spin_init(&h->lock);
	h->n = n;
	h->insertions = 0;
	h->collisions = 0;
	if ((h->bucket = iscsi_malloc_atomic(n * sizeof(initiator_cmd_t *))) == NULL) {
		TRACE_ERROR("iscsi_malloc_atomic() failed\n");
		return -1;
	}
	for (i = 0; i < n; i++)
		h->bucket[i] = NULL;
	return 0;
}

int 
hash_insert(hash_t * h, initiator_cmd_t * cmd, unsigned key)
{
	int	i;

	iscsi_spin_lock(&h->lock);
	cmd->hash_next = NULL;
	cmd->key = key;

	i = key % (h->n);
	if (h->bucket[i] == NULL) {
		TRACE(TRACE_HASH, "inserting key %u (val 0x%p) into bucket[%i]\n", key, cmd, i);
		h->bucket[i] = cmd;
	} else {
		cmd->hash_next = h->bucket[i];
		h->bucket[i] = cmd;
		h->collisions++;
		TRACE(TRACE_HASH, "inserting key %u (val 0x%p) into bucket[%i] (collision)\n", key, cmd, i);
	}
	h->insertions++;
	iscsi_spin_unlock(&h->lock);
	return 0;
}

struct initiator_cmd_t *
hash_remove(hash_t * h, unsigned key)
{
	initiator_cmd_t	*prev;
	initiator_cmd_t	*curr;
	int		 i;

	iscsi_spin_lock(&h->lock);
	i = key % (h->n);
	if (h->bucket[i] == NULL) {
		TRACE_ERROR("bucket emtpy\n");
		curr = NULL;
	} else {
		prev = NULL;
		curr = h->bucket[i];
		while ((curr->key != key) && (curr->hash_next != NULL)) {
			prev = curr;
			curr = curr->hash_next;
		}
		if (curr->key != key) {
			TRACE_ERROR("key %u (0x%x) not found in bucket[%i]\n", key, key, i);
			curr = NULL;
		} else {
			if (prev == NULL) {
				h->bucket[i] = h->bucket[i]->hash_next;
				TRACE(TRACE_HASH, "removed key %u (val 0x%p) from head of bucket\n", key, curr);
			} else {
				prev->hash_next = curr->hash_next;
				if (prev->hash_next == NULL) {
					TRACE(TRACE_HASH, "removed key %u (val 0x%p) from end of bucket\n", key, curr);
				} else {
					TRACE(TRACE_HASH, "removed key %u (val 0x%p) from middle of bucket\n", key, curr);
				}
			}
		}
	}
	iscsi_spin_unlock(&h->lock);
	return curr;
}

int 
hash_destroy(hash_t * h)
{
	iscsi_free_atomic(h->bucket);
	return 0;
}

/*
 * Socket Functions
 */

int 
modify_iov(struct iovec ** iov_ptr, int *iovc, uint32_t offset, uint32_t length)
{
	int             len;
	int             disp = offset;
	int             i;
	struct iovec   *iov = *iov_ptr;
	char		*basep;

	/* Given <offset>, find beginning iovec and modify its base and length */
	len = 0;
	for (i = 0; i < *iovc; i++) {
		len += iov[i].iov_len;
		if (len > offset) {
			TRACE(TRACE_NET_IOV, "found offset %u in iov[%i]\n", offset, i);
			break;
		}
		disp -= iov[i].iov_len;
	}
	if (i == *iovc) {
		TRACE_ERROR("sum of iov lens (%u) < offset (%u)\n", len, offset);
		return -1;
	}
	iov[i].iov_len -= disp;
	basep = iov[i].iov_base;
	basep += disp;
	iov[i].iov_base = basep;
	*iovc -= i;
	*iov_ptr = &(iov[i]);
	iov = *iov_ptr;

	/*
	 * Given <length>, find ending iovec and modify its length (base does
	 * not change)
	 */

	len = 0;		/* we should re-use len and i here... */
	for (i = 0; i < *iovc; i++) {
		len += iov[i].iov_len;
		if (len >= length) {
			TRACE(TRACE_NET_IOV, "length %u ends in iovec[%i]\n", length, i);
			break;
		}
	}
	if (i == *iovc) {
		TRACE_ERROR("sum of iovec lens (%u) < length (%u)\n", len, length);
		for (i = 0; i < *iovc; i++) {
			TRACE_ERROR("iov[%i].iov_base = %p (len %u)\n", i, iov[i].iov_base, (unsigned)iov[i].iov_len);
		}
		return -1;
	}
	iov[i].iov_len -= (len - length);
	*iovc = i + 1;

#ifdef CONFIG_ISCSI_DEBUG
	TRACE(TRACE_NET_IOV, "new iov:\n");
	len = 0;
	for (i = 0; i < *iovc; i++) {
		TRACE(TRACE_NET_IOV, "iov[%i].iov_base = %p (len %u)\n", i, iov[i].iov_base, (unsigned)iov[i].iov_len);
		len += iov[i].iov_len;
	}
	TRACE(TRACE_NET_IOV, "new iov length: %u bytes\n", len);
#endif

	return 0;
}

int 
iscsi_sock_setsockopt(iscsi_socket_t * sock, int level, int optname, void *optval, unsigned  optlen)
{
	int             rc;

	if ((rc = setsockopt(*sock, level, optname, optval, optlen)) != 0) {
		TRACE_ERROR("sock->ops->setsockopt() failed: rc %i errno %i\n", rc, errno);
		return -1;
	}
	return 0;
}

int 
iscsi_sock_getsockopt(iscsi_socket_t * sock, int level, int optname, void *optval, unsigned *optlen)
{
	int             rc;

	if ((rc = getsockopt(*sock, level, optname, optval, optlen)) != 0) {
		TRACE_ERROR("sock->ops->getsockopt() failed: rc %i errno %i\n", rc, errno);
		return -1;
	}
	return 0;
}

int 
iscsi_sock_create(iscsi_socket_t * sock)
{
	int             rc;

	if ((rc = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		TRACE_ERROR("socket() failed: rc %i errno %i\n", rc, errno);
	}
	*sock = rc;
	if (rc < 0) {
		TRACE_ERROR("error creating socket (rc %i)\n", rc);
		return -1;
	}
	return 0;
}

int 
iscsi_sock_bind(iscsi_socket_t sock, int port)
{
	struct sockaddr_in	laddr;
	int			rc;

	(void) memset(&laddr, 0x0, sizeof(laddr));
	laddr.sin_family = AF_INET;
	laddr.sin_addr.s_addr = INADDR_ANY;
	laddr.sin_port = ISCSI_HTON16(port);
	if ((rc = bind(sock, (struct sockaddr *) (void *) &laddr, sizeof(laddr))) < 0) {
		TRACE_ERROR("bind() failed: rc %i errno %i\n", rc, errno);
		return -1;
	}
	return 0;
}

int 
iscsi_sock_listen(iscsi_socket_t sock)
{
	int             rc;

	if ((rc = listen(sock, 32)) < 0) {
		TRACE_ERROR("listen() failed: rc %i errno %i\n", rc, errno);
		return -1;
	}
	return 0;
}

int 
iscsi_sock_accept(iscsi_socket_t sock, iscsi_socket_t * newsock)
{
	struct sockaddr_in remoteAddr;
	socklen_t	remoteAddrLen;

	remoteAddrLen = sizeof(remoteAddr);
	(void) memset(&remoteAddr, 0, sizeof(remoteAddr));
	if ((*newsock = accept(sock, (struct sockaddr *) (void *)& remoteAddr, &remoteAddrLen)) < 0) {
		TRACE(TRACE_NET_DEBUG, "accept() failed: rc %i errno %i\n", *newsock, errno);
		return -1;
	}

	return 0;
}

int 
iscsi_sock_getsockname(iscsi_socket_t sock, struct sockaddr * name, unsigned *namelen)
{
	if (getsockname(sock, name, namelen) != 0) {
		TRACE_ERROR("getsockame() failed (errno %i)\n", errno);
		return -1;
	}
	return 0;
}

int 
iscsi_sock_getpeername(iscsi_socket_t sock, struct sockaddr * name, unsigned *namelen)
{
	if (getpeername(sock, name, namelen) != 0) {
		TRACE_ERROR("getpeername() failed (errno %i)\n", errno);
		return -1;
	}
	return 0;
}

int 
iscsi_sock_shutdown(iscsi_socket_t sock, int how)
{
	int             rc;

	if ((rc = shutdown(sock, how)) != 0) {
		TRACE(TRACE_NET_DEBUG, "shutdown() failed: rc %i, errno %i\n", rc, errno);
	}
	return 0;
}

int 
iscsi_sock_close(iscsi_socket_t sock)
{
	int             rc;

	if ((rc = close(sock)) != 0) {
		TRACE_ERROR("close() failed: rc %i errno %i\n", rc, errno);
		return -1;
	}
	return 0;
}

int 
iscsi_sock_connect(iscsi_socket_t sock, char *hostname, int port)
{
	struct sockaddr_in addr;
	int             rc = 0;
	int             i;

	(void) memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = ISCSI_HTONS(port);
	addr.sin_family = AF_INET;

	for (i = 0; i < ISCSI_SOCK_CONNECT_TIMEOUT; i++) {

		/* Attempt connection */

		addr.sin_addr.s_addr = inet_addr(hostname);
#if ISCSI_SOCK_CONNECT_NONBLOCK == 1
		if (fcntl(sock, F_SETFL, O_NONBLOCK) != 0) {
			TRACE_ERROR("fcntl() failed");
			return -1;
		}
#endif
		rc = connect(sock, (struct sockaddr *) (void *) &addr, sizeof(addr));
#if ISCSI_SOCK_CONNECT_NONBLOCK == 1
		if (fcntl(sock, F_SETFL, O_SYNC) != 0) {
			TRACE_ERROR("fcntl() failed\n");
			return -1;
		}
#endif

		/* Check errno */

		if (errno == EISCONN) {
			rc = 0;
			break;
		}
		if (errno == EAGAIN || errno == EINPROGRESS || errno == EALREADY) {
			if (i != ISCSI_SOCK_CONNECT_TIMEOUT - 1) {
				PRINT("***SLEEPING***\n");
				ISCSI_SLEEP(1);
			}
		} else {
			break;
		}
	}
	if (rc < 0) {
		TRACE_ERROR("connect() to %s:%i failed (errno %i)\n", hostname, port, errno);
	}
	return rc;
}

/*
 * NOTE: iscsi_sock_msg() alters *sg when socket sends and recvs return having only
 * transfered a portion of the iovec.  When this happens, the iovec is modified
 * and resent with the appropriate offsets.
 */

int 
iscsi_sock_msg(iscsi_socket_t sock, int xmit, unsigned len, void *data, int iovc)
{
	int             i, n = 0;
	int		rc;
	struct iovec   *iov;
	struct iovec    singleton;
	uint8_t   padding[ISCSI_SOCK_MSG_BYTE_ALIGN];
	struct iovec   *iov_padding = NULL;
	uint32_t        remainder;
	uint32_t        padding_len = 0;
	int             total_len = 0;

	TRACE(TRACE_NET_DEBUG, "%s %i bytes on sock\n", xmit ? "sending" : "receiving", len);
	if (iovc == 0) {
		TRACE(TRACE_NET_DEBUG, "building singleton iovec (data %p, len %u)\n", data, len);
		singleton.iov_base = data;
		singleton.iov_len = len;
		iov = &singleton;
		iovc = 1;
	} else {
		iov = (struct iovec *) data;
	}

	/* Add padding */

	if ((remainder = len % ISCSI_SOCK_MSG_BYTE_ALIGN) != 0) {
		if ((iov_padding = iscsi_malloc_atomic((iovc + 1) * sizeof(struct iovec))) == NULL) {
			TRACE_ERROR("iscsi_malloc_atomic() failed\n");
			return -1;
		}
		memcpy(iov_padding, iov, iovc * sizeof(struct iovec));
		iov_padding[iovc].iov_base = padding;
		padding_len = ISCSI_SOCK_MSG_BYTE_ALIGN - remainder;
		iov_padding[iovc].iov_len = padding_len;
		iov = iov_padding;
		iovc++;
		memset(padding, 0, padding_len);
		len += padding_len;
		TRACE(TRACE_NET_DEBUG, "Added iovec for padding (len %u)\n", padding_len);
	}
	/*
	 * We make copy of iovec if we're in debugging mode,  as we'll print
	 * out
	 */
	/*
	 * the iovec and the buffer contents at the end of this subroutine
	 * and
	 */

	do {
		/* Check iovec */

		total_len = 0;
		TRACE(TRACE_NET_DEBUG, "%s %i buffers\n", xmit ? "gathering from" : "scattering into", iovc);
		for (i = 0; i < iovc; i++) {
			TRACE(TRACE_NET_IOV, "iov[%i].iov_base = %p, len %u\n", i, iov[i].iov_base, (unsigned)iov[i].iov_len);
			total_len += iov[i].iov_len;
		}
		if (total_len != len - n) {
			TRACE_ERROR("iovcs sum to %i != total len of %i\n", total_len, len - n);
			TRACE_ERROR("iov = %p\n", iov);
			for (i = 0; i < iovc; i++) {
				TRACE_ERROR("iov[%i].iov_base = %p, len %u\n",
					i, iov[i].iov_base, (unsigned)iov[i].iov_len);
			}
			return -1;
		}
		if ((rc = (xmit) ? writev(sock, iov, iovc) : readv(sock, iov, iovc)) == 0) {
			TRACE(TRACE_NET_DEBUG, "%s() failed: rc %i errno %i\n", (xmit) ? "writev" : "readv", rc, errno);
			break;
		} else if (rc < 0) {
			/* Temp FIXME */
			TRACE_ERROR("%s() failed: rc %i errno %i\n", (xmit)?"writev":"readv", rc, errno);
			break;
		}
		n += rc;
		if (n < len) {
			TRACE(TRACE_NET_DEBUG, "Got partial %s: %i bytes of %i\n", (xmit) ? "send" : "recv", rc, len - n + rc);

			total_len = 0;
			for (i = 0; i < iovc; i++) {
				total_len += iov[i].iov_len;
			}
			TRACE(TRACE_NET_IOV, "before modify_iov: %s %i buffers, total_len = %u, n = %u, rc = %u\n",
			      xmit ? "gathering from" : "scattering into", iovc, total_len, n, rc);
			if (modify_iov(&iov, &iovc, (unsigned) rc, len - n) != 0) {
				TRACE_ERROR("modify_iov() failed\n");
				break;
			}
			total_len = 0;
			for (i = 0; i < iovc; i++) {
				total_len += iov[i].iov_len;
			}
			TRACE(TRACE_NET_IOV, "after modify_iov: %s %i buffers, total_len = %u, n = %u, rc = %u\n\n",
			      xmit ? "gathering from" : "scattering into", iovc, total_len, n, rc);
		}
	} while (n < len);

	if (remainder) {
		iscsi_free_atomic(iov_padding);
	}
	TRACE(TRACE_NET_DEBUG, "successfully %s %i bytes on sock (%i bytes padding)\n", xmit ? "sent" : "received", n, padding_len);
	return n - padding_len;
}

/*
 * Temporary Hack:
 *
 * TCP's Nagle algorithm and delayed-ack lead to poor performance when we send
 * two small messages back to back (i.e., header+data). The TCP_NODELAY option
 * is supposed to turn off Nagle, but it doesn't seem to work on Linux 2.4.
 * Because of this, if our data payload is small, we'll combine the header and
 * data, else send as two separate messages.
 */

int 
iscsi_sock_send_header_and_data(iscsi_socket_t sock,
				void *header, unsigned header_len,
				const void *data, unsigned data_len, int iovc)
{
	struct iovec	iov[ISCSI_MAX_IOVECS];

	if (data_len && data_len <= ISCSI_SOCK_HACK_CROSSOVER) {
		/* combine header and data into one iovec */
		if (iovc >= ISCSI_MAX_IOVECS) {
			TRACE_ERROR("iscsi_sock_msg() failed\n");
			return -1;
		}
		if (iovc == 0) {
			iov[0].iov_base = header;
			iov[0].iov_len = header_len;
			iov[1].iov_base = __UNCONST((const char *)data);
			iov[1].iov_len = data_len;
			iovc = 2;
		} else {
			iov[0].iov_base = header;
			iov[0].iov_len = header_len;
			(void) memcpy(&iov[1], data, sizeof(struct iovec) * iovc);
			iovc += 1;
		}
		if (iscsi_sock_msg(sock, Transmit, header_len + data_len, iov, iovc) != header_len + data_len) {
			TRACE_ERROR("iscsi_sock_msg() failed\n");
			return -1;
		}
	} else {
		if (iscsi_sock_msg(sock, Transmit, header_len, header, 0) != header_len) {
			TRACE_ERROR("iscsi_sock_msg() failed\n");
			return -1;
		}
		if (data_len != 0 && iscsi_sock_msg(sock, Transmit, data_len, __UNCONST((const char *) data), iovc) != data_len) {
			TRACE_ERROR("iscsi_sock_msg() failed\n");
			return -1;
		}
	}
	return header_len + data_len;
}


/* spin lock functions */
int 
iscsi_spin_init(iscsi_spin_t * lock)
{
	pthread_mutexattr_t mattr;

	pthread_mutexattr_init(&mattr);
#ifdef PTHREAD_MUTEX_ADAPTIVE_NP
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ADAPTIVE_NP);
#endif
	if (pthread_mutex_init(lock, &mattr) != 0)
		return -1;
	return 0;
}

int 
iscsi_spin_lock(iscsi_spin_t * lock)
{
	return pthread_mutex_lock(lock);
}

int 
iscsi_spin_unlock(iscsi_spin_t * lock)
{
	return pthread_mutex_unlock(lock);
}

/* ARGSUSED1 */
int 
iscsi_spin_lock_irqsave(iscsi_spin_t * lock, uint32_t *flags)
{
	return pthread_mutex_lock(lock);
}

/* ARGSUSED1 */
int 
iscsi_spin_unlock_irqrestore(iscsi_spin_t * lock, uint32_t *flags)
{
	return pthread_mutex_unlock(lock);
}

int 
iscsi_spin_destroy(iscsi_spin_t * lock)
{
	return pthread_mutex_destroy(lock);
}


/*
 * Mutex Functions, kernel module doesn't require mutex for locking.
 * For thread sync, 'down' & 'up' have been wrapped into condition
 * varibles, which is kernel semaphores in kernel module.
 */

int 
iscsi_mutex_init(iscsi_mutex_t * m)
{
	return (pthread_mutex_init(m, NULL) != 0) ? -1 : 0;
}

int 
iscsi_mutex_lock(iscsi_mutex_t * m)
{
	return pthread_mutex_lock(m);
}

int 
iscsi_mutex_unlock(iscsi_mutex_t * m)
{
	return pthread_mutex_unlock(m);
}

int 
iscsi_mutex_destroy(iscsi_mutex_t * m)
{
	return pthread_mutex_destroy(m);
}

/*
 * Condition Functions
 */

int 
iscsi_cond_init(iscsi_cond_t * c)
{
	return pthread_cond_init(c, NULL);
}

int 
iscsi_cond_wait(iscsi_cond_t * c, iscsi_mutex_t * m)
{
	return pthread_cond_wait(c, m);
}

int 
iscsi_cond_signal(iscsi_cond_t * c)
{
	return pthread_cond_signal(c);
}

int 
iscsi_cond_destroy(iscsi_cond_t * c)
{
	return pthread_cond_destroy(c);
}

/*
 * Misc. Functions
 */

uint32_t 
iscsi_atoi(char *value)
{
	if (value == NULL) {
		TRACE_ERROR("iscsi_atoi() called with NULL value\n");
		return 0;
	}
	return atoi(value);
}

static const char HexString[] = "0123456789abcdef";

/* get the hex value (subscript) of the character */
static int 
HexStringIndex(const char *s, int c)
{
	const char	*cp;

	return (c == '0') ? 0 : ((cp = strchr(s, tolower(c))) == NULL) ? -1 : (int)(cp - s);
}

int 
HexDataToText(
	      uint8_t *data, uint32_t dataLength,
	      char *text, uint32_t textLength)
{
	uint32_t   n;

	if (!text || textLength == 0) {
		return -1;
	}
	if (!data || dataLength == 0) {
		*text = 0x0;
		return -1;
	}
	if (textLength < 3) {
		*text = 0x0;
		return -1;
	}
	*text++ = '0';
	*text++ = 'x';

	textLength -= 2;

	while (dataLength > 0) {

		if (textLength < 3) {
			*text = 0x0;
			return -1;
		}
		n = *data++;
		dataLength--;

		*text++ = HexString[(n >> 4) & 0xf];
		*text++ = HexString[n & 0xf];

		textLength -= 2;
	}

	*text = 0x0;

	return 0;
}


int 
HexTextToData(
	      const char *text, uint32_t textLength,
	      uint8_t *data, uint32_t dataLength)
{
	int             i;
	uint32_t    n1;
	uint32_t    n2;
	uint32_t    len = 0;

	if ((text[0] == '0') && (text[1] != 'x' || text[1] != 'X')) {
		/* skip prefix */
		text += 2;
		textLength -= 2;
	}
	if ((textLength % 2) == 1) {

		i = HexStringIndex(HexString, *text++);
		if (i < 0)
			return -1;	/* error, bad character */

		n2 = i;

		if (dataLength < 1) {
			return -1;	/* error, too much data */
		}
		*data++ = n2;
		len++;
	}
	while (*text != 0x0) {

		if ((i = HexStringIndex(HexString, *text++)) < 0) {
			/* error, bad character */
			return -1;
		}

		n1 = i;

		if (*text == 0x0) {
			/* error, odd string length */
			return -1;
		}

		if ((i = HexStringIndex(HexString, *text++)) < 0) {
			/* error, bad character */
			return -1;
		}

		n2 = i;

		if (len >= dataLength) {
			/* error, too much data */
			return len;
		}
		*data++ = (n1 << 4) | n2;
		len++;
	}

	return (len == 0) ? -1 : 0;
}

void 
GenRandomData(uint8_t *data, uint32_t length)
{
	unsigned        n;
	uint32_t            r;

	for ( ; length > 0 ; length--) {

		r = rand();
		r = r ^ (r >> 8);
		r = r ^ (r >> 4);
		n = r & 0x7;

		r = rand();
		r = r ^ (r >> 8);
		r = r ^ (r >> 5);
		n = (n << 3) | (r & 0x7);

		r = rand();
		r = r ^ (r >> 8);
		r = r ^ (r >> 5);
		n = (n << 2) | (r & 0x3);

		*data++ = n;
	}
}
