/*	$NetBSD: test_mutex1.c,v 1.3 2007/02/05 22:48:01 ad Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: test_mutex1.c,v 1.3 2007/02/05 22:48:01 ad Exp $");

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/kernel.h>

#define	NTHREADS	9
#define	SECONDS		60

int	testcall(struct lwp *, void *, register_t *);
void	thread1(void *);
void	thread2(void *);
void	thread3(void *);
void	thread_exit(int);
void	thread_enter(int *);

struct proc	*test_threads[NTHREADS];
kmutex_t	test_mutex;
kcondvar_t	test_cv;
int		test_count;
int		test_exit;

void
thread_enter(int *nlocks)
{
	struct lwp *l = curlwp;

	KERNEL_UNLOCK_ALL(l, nlocks);
	lwp_lock(l);
	l->l_usrpri = MAXPRI;
	lwp_changepri(l, MAXPRI);
	lwp_unlock(l);
}

void
thread_exit(int nlocks)
{

	mutex_enter(&test_mutex);
	if (--test_count == 0)
		cv_signal(&test_cv);
	mutex_exit(&test_mutex);

	KERNEL_LOCK(nlocks, curlwp);
	kthread_exit(0);
}

/*
 * test_mutex -> kernel_lock
 */
void
thread1(void *cookie)
{
	int count, nlocks;

	thread_enter(&nlocks);

	for (count = 0; !test_exit; count++) {
		mutex_enter(&test_mutex);
		KERNEL_LOCK(1, curlwp);
		if ((count % 11) == 0)
			yield();
		mutex_exit(&test_mutex);
		KERNEL_UNLOCK_ONE(curlwp);
		if ((count % 17) == 0)
			yield();
	}

	thread_exit(nlocks);
}

/*
 * kernel_lock -> test_mutex
 */
void
thread2(void *cookie)
{
	int count, nlocks;

	thread_enter(&nlocks);

	for (count = 0; !test_exit; count++) {
		KERNEL_LOCK(1, curlwp);
		mutex_enter(&test_mutex);
		if ((count % 401) == 0)
			yield();
		KERNEL_UNLOCK_ONE(curlwp);
		mutex_exit(&test_mutex);
		if ((count % 19) == 0)
			yield();
	}

	thread_exit(nlocks);
}

/*
 * test_mutex without kernel_lock, to provide pressure
 */
void
thread3(void *cookie)
{
	int count, nlocks;

	thread_enter(&nlocks);

	for (count = 0; !test_exit; count++) {
		mutex_enter(&test_mutex);
		DELAY(10);
		if ((count % 101) == 0)
			yield();
		mutex_exit(&test_mutex);
		if ((count % 23) == 0)
			yield();
	}

	thread_exit(nlocks);
}

int
testcall(struct lwp *l, void *uap, register_t *retval)
{
	void (*func)(void *);
	int i;

	mutex_init(&test_mutex, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&test_cv, "testcv");

	printf("test: creating threads\n");

	test_count = NTHREADS;
	test_exit = 0;

	for (i = 0; i < test_count; i++) {
		switch (i % 3) {
		case 0:
			func = thread1;
			break;
		case 1:
			func = thread2;
			break;
		case 2:
			func = thread3;
			break;
		}
		kthread_create1(func, NULL, &test_threads[i], "thread%d", i);
	}

	printf("test: sleeping\n");

	mutex_enter(&test_mutex);
	while (test_count != 0) {
		(void)cv_timedwait(&test_cv, &test_mutex, hz * SECONDS);
		test_exit = 1;
	}
	mutex_exit(&test_mutex);

	printf("test: finished\n");

	cv_destroy(&test_cv);
	mutex_destroy(&test_mutex);

	return 0;
}
