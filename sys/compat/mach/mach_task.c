/*	$NetBSD: mach_task.c,v 1.32 2003/11/13 13:40:39 manu Exp $ */

/*-
 * Copyright (c) 2002-2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include "opt_compat_darwin.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mach_task.c,v 1.32 2003/11/13 13:40:39 manu Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/exec.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_param.h>

#include <compat/mach/mach_types.h>
#include <compat/mach/mach_message.h>
#include <compat/mach/mach_clock.h>
#include <compat/mach/mach_errno.h>
#include <compat/mach/mach_exec.h>
#include <compat/mach/mach_port.h>
#include <compat/mach/mach_task.h>
#include <compat/mach/mach_services.h>
#include <compat/mach/mach_syscallargs.h>

#ifdef COMPAT_DARWIN
#include <compat/darwin/darwin_exec.h>
#endif

int 
mach_task_get_special_port(args)
	struct mach_trap_args *args;
{
	mach_task_get_special_port_request_t *req = args->smsg;
	mach_task_get_special_port_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct lwp *l = args->l;
	struct mach_emuldata *med;
	struct mach_right *mr;

	med = (struct mach_emuldata *)l->l_proc->p_emuldata;

	switch (req->req_which_port) {
	case MACH_TASK_KERNEL_PORT:
		mr = mach_right_get(med->med_kernel, l, MACH_PORT_TYPE_SEND, 0);
		break;

	case MACH_TASK_HOST_PORT:
		mr = mach_right_get(med->med_host, l, MACH_PORT_TYPE_SEND, 0);
		break;

	case MACH_TASK_BOOTSTRAP_PORT:
		mr = mach_right_get(med->med_bootstrap, 
		    l, MACH_PORT_TYPE_SEND, 0);
#ifdef DEBUG_MACH
		printf("*** get bootstrap right %p, port %p, recv %p [%p]\n",
		    mr, mr->mr_port, mr->mr_port->mp_recv,
		    mr->mr_port->mp_recv->mr_sethead);
#endif
		break;

	case MACH_TASK_WIRED_LEDGER_PORT:
	case MACH_TASK_PAGED_LEDGER_PORT:
	default:
		uprintf("mach_task_get_special_port(): unimpl. port %d\n",
		    req->req_which_port);
		return mach_msg_error(args, EINVAL);
		break;
	}

	rep->rep_msgh.msgh_bits = 
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE) |
	    MACH_MSGH_BITS_COMPLEX;
	rep->rep_msgh.msgh_size = sizeof(*rep) - sizeof(rep->rep_trailer);
	rep->rep_msgh.msgh_local_port = req->req_msgh.msgh_local_port;
	rep->rep_msgh.msgh_id = req->req_msgh.msgh_id + 100;
	rep->rep_msgh_body.msgh_descriptor_count = 1;
	rep->rep_special_port.name = (mach_port_t)mr->mr_name;
	rep->rep_special_port.disposition = 0x11; /* XXX why? */
	rep->rep_trailer.msgh_trailer_size = 8;

	*msglen = sizeof(*rep);
	return 0;
}

int 
mach_ports_lookup(args)
	struct mach_trap_args *args;
{
	mach_ports_lookup_request_t *req = args->smsg;
	mach_ports_lookup_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct lwp *l = args->l;
	struct mach_emuldata *med;
	struct mach_right *mr;
	mach_port_name_t mnp[7];
	vaddr_t va;
	int error;

	/* 
	 * This is some out of band data sent with the reply. In the 
	 * encountered situation the out of band data has always been null
	 * filled. We have to see more of this in order to fully understand
	 * how this trap works.
	 */
	va = vm_map_min(&l->l_proc->p_vmspace->vm_map);
	if ((error = uvm_map(&l->l_proc->p_vmspace->vm_map, &va, PAGE_SIZE,
	    NULL, UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_ALL,
	    UVM_INH_COPY, UVM_ADV_NORMAL, UVM_FLAG_COPYONW))) != 0)
		return mach_msg_error(args, error);

	med = (struct mach_emuldata *)l->l_proc->p_emuldata;
	mnp[0] = (mach_port_name_t)MACH_PORT_DEAD;
	mnp[3] = (mach_port_name_t)MACH_PORT_DEAD;
	mnp[5] = (mach_port_name_t)MACH_PORT_DEAD;
	mnp[6] = (mach_port_name_t)MACH_PORT_DEAD;

	mr = mach_right_get(med->med_kernel, l, MACH_PORT_TYPE_SEND, 0);
	mnp[MACH_TASK_KERNEL_PORT] = mr->mr_name;
	mr = mach_right_get(med->med_host, l, MACH_PORT_TYPE_SEND, 0);
	mnp[MACH_TASK_HOST_PORT] = mr->mr_name;
	mr = mach_right_get(med->med_bootstrap, l, MACH_PORT_TYPE_SEND, 0);
	mnp[MACH_TASK_BOOTSTRAP_PORT] = mr->mr_name;

#ifdef DEBUG_MACH
	printf("mach_ports_lookup: kernel %08x, host %08x, boostrap %08x\n",
	    mnp[MACH_TASK_KERNEL_PORT],
	    mnp[MACH_TASK_HOST_PORT],
	    mnp[MACH_TASK_BOOTSTRAP_PORT]);
#endif
	/*
	 * On Darwin, the data seems always null...
	 */
	if ((error = copyout(mnp, (void *)va, sizeof(mnp))) != 0)
		return mach_msg_error(args, error);

	rep->rep_msgh.msgh_bits =
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE) |
	    MACH_MSGH_BITS_COMPLEX;
	rep->rep_msgh.msgh_size = sizeof(*rep) - sizeof(rep->rep_trailer);
	rep->rep_msgh.msgh_local_port = req->req_msgh.msgh_local_port;
	rep->rep_msgh.msgh_id = req->req_msgh.msgh_id + 100;
	rep->rep_msgh_body.msgh_descriptor_count = 1;	/* XXX why ? */
	rep->rep_init_port_set.address = (void *)va;
	rep->rep_init_port_set.count = 3; /* XXX why ? */
	rep->rep_init_port_set.copy = 2; /* XXX why ? */
	rep->rep_init_port_set.disposition = 0x11; /* XXX why? */
	rep->rep_init_port_set.type = 2; /* XXX why? */
	rep->rep_init_port_set_count = 3; /* XXX why? */
	rep->rep_trailer.msgh_trailer_size = 8;

	*msglen = sizeof(*rep);
	return 0;
}

int 
mach_task_set_special_port(args)
	struct mach_trap_args *args;
{
	mach_task_set_special_port_request_t *req = args->smsg;
	mach_task_set_special_port_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct lwp *l = args->l;
	mach_port_t mn;
	struct mach_right *mr;
	struct mach_port *mp;
	struct mach_emuldata *med;

	mn = req->req_special_port.name;

	/* Null port ? */
	if (mn == 0)
		return mach_msg_error(args, 0);

	/* Does the inserted port exists? */
	if ((mr = mach_right_check(mn, l, MACH_PORT_TYPE_ALL_RIGHTS)) == 0)
		return mach_msg_error(args, EPERM);

	if (mr->mr_type == MACH_PORT_TYPE_DEAD_NAME)
		return mach_msg_error(args, EINVAL);

	med = (struct mach_emuldata *)l->l_proc->p_emuldata;

	switch (req->req_which_port) {
	case MACH_TASK_KERNEL_PORT:
		mp = med->med_kernel;
		med->med_kernel = mr->mr_port;
		mp->mp_refcount--;
		if (mp->mp_refcount == 0)
			mach_port_put(mp);
		break;

	case MACH_TASK_HOST_PORT:
		mp = med->med_host;
		med->med_host = mr->mr_port;
		mp->mp_refcount--;
		if (mp->mp_refcount == 0)
			mach_port_put(mp);
		break;

	case MACH_TASK_BOOTSTRAP_PORT:
		mp = med->med_bootstrap;
		med->med_bootstrap = mr->mr_port;
		mp->mp_refcount--;
		if (mp->mp_refcount == 0)
			mach_port_put(mp);
#ifdef COMPAT_DARWIN
		/*
		 * mach_init sets the bootstrap port for any new process
		 */
		{
			struct darwin_emuldata *ded;

			ded = l->l_proc->p_emuldata;
			if (ded->ded_fakepid == 1) {
				mach_bootstrap_port = med->med_bootstrap;
#ifdef DEBUG_DARWIN
				printf("*** New bootstrap port %p, "
				    "recv %p [%p]\n",
				    mach_bootstrap_port, 
				    mach_bootstrap_port->mp_recv,
				    mach_bootstrap_port->mp_recv->mr_sethead);
#endif /* DEBUG_DARWIN */
			}
		}
#endif /* COMPAT_DARWIN */
		break;

	default:
		uprintf("mach_task_set_special_port: unimplemented port %d\n",
		    req->req_which_port);
	}

	rep->rep_msgh.msgh_bits =
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE);
	rep->rep_msgh.msgh_size = sizeof(*rep) - sizeof(rep->rep_trailer);
	rep->rep_msgh.msgh_local_port = req->req_msgh.msgh_local_port;
	rep->rep_msgh.msgh_id = req->req_msgh.msgh_id + 100;
	rep->rep_trailer.msgh_trailer_size = 8;

	*msglen = sizeof(*rep);
	return 0;
}

int
mach_task_threads(args)
	struct mach_trap_args *args;
{
	mach_task_threads_request_t *req = args->smsg;
	mach_task_threads_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct lwp *l = args->l;
	struct mach_emuldata *med;
	int error;
	vaddr_t va;
	size_t size;
	int i;
	struct mach_right *mr;
	mach_port_name_t *mnp;

	med = l->l_proc->p_emuldata;

	size = l->l_proc->p_nlwps * sizeof(*mnp);
	va = vm_map_min(&l->l_proc->p_vmspace->vm_map);

	if ((error = uvm_map(&l->l_proc->p_vmspace->vm_map, &va, 
	    round_page(size), NULL, UVM_UNKNOWN_OFFSET, 0, 
	    UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_ALL, UVM_INH_COPY, 
	    UVM_ADV_NORMAL, UVM_FLAG_COPYONW))) != 0)
		return mach_msg_error(args, error);

	mnp = malloc(size, M_TEMP, M_WAITOK);
	for (i = 0; i < l->l_proc->p_nlwps; i++) {
		/* XXX each thread should have a kernel port */
		mr = mach_right_get(med->med_kernel, l, MACH_PORT_TYPE_SEND, 0);
		mnp[i] = mr->mr_name;
	}
	if ((error = copyout(mnp, (void *)va, size)) != 0) {
		free(mnp, M_TEMP);
		return mach_msg_error(args, error);
	}
	free(mnp, M_TEMP);

	rep->rep_msgh.msgh_bits =
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE) |
	    MACH_MSGH_BITS_COMPLEX;
	rep->rep_msgh.msgh_size = sizeof(*rep) - sizeof(rep->rep_trailer);
	rep->rep_msgh.msgh_local_port = req->req_msgh.msgh_local_port;
	rep->rep_msgh.msgh_id = req->req_msgh.msgh_id + 100;
	rep->rep_body.msgh_descriptor_count = 1;
	rep->rep_list.address = (void *)va;
	rep->rep_list.count = l->l_proc->p_nlwps;
	rep->rep_list.copy = 0x02;
	rep->rep_list.disposition = 0x11;
	rep->rep_list.type = 0x02;
	rep->rep_count = l->l_proc->p_nlwps;
	rep->rep_trailer.msgh_trailer_size = 8;

	*msglen = sizeof(*rep);
	return 0;
}

int
mach_task_get_exception_ports(args)
	struct mach_trap_args *args;
{
	mach_task_get_exception_ports_request_t *req = args->smsg;
	mach_task_get_exception_ports_reply_t *rep = args->rmsg;
	struct lwp *l = args->l;
	size_t *msglen = args->rsize;
	struct mach_emuldata *med;

	med = l->l_proc->p_emuldata;

	uprintf("Unimplemented mach_task_get_exception_ports\n");

	rep->rep_msgh.msgh_bits =
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE);
	rep->rep_msgh.msgh_size = sizeof(*rep) - sizeof(rep->rep_trailer);
	rep->rep_msgh.msgh_local_port = req->req_msgh.msgh_local_port;
	rep->rep_msgh.msgh_id = req->req_msgh.msgh_id + 100;
	rep->rep_trailer.msgh_trailer_size = 8;

	*msglen = sizeof(*rep);

	return 0;
}

int
mach_task_set_exception_ports(args)
	struct mach_trap_args *args;
{
	mach_task_set_exception_ports_request_t *req = args->smsg;
	mach_task_set_exception_ports_reply_t *rep = args->rmsg;
	struct lwp *l = args->l;
	size_t *msglen = args->rsize;
	struct mach_emuldata *med;

	med = l->l_proc->p_emuldata;

	uprintf("Unimplemented mach_task_set_exception_ports\n");

	rep->rep_msgh.msgh_bits =
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE);
	rep->rep_msgh.msgh_size = sizeof(*rep) - sizeof(rep->rep_trailer);
	rep->rep_msgh.msgh_local_port = req->req_msgh.msgh_local_port;
	rep->rep_msgh.msgh_id = req->req_msgh.msgh_id + 100;
	rep->rep_trailer.msgh_trailer_size = 8;

	*msglen = sizeof(*rep);

	return 0;
}

int
mach_task_info(args)
	struct mach_trap_args *args;
{
	mach_task_info_request_t *req = args->smsg;
	mach_task_info_reply_t *rep = args->rmsg;
	struct lwp *l = args->l;
	size_t *msglen = args->rsize;
	int count;

	switch(req->req_flavor) {
	case MACH_TASK_BASIC_INFO: {	
		struct mach_task_basic_info *mtbi;
		struct rusage *ru;

		count = sizeof(*mtbi) / sizeof(rep->rep_info[0]);
		if (req->req_count < count)
			return mach_msg_error(args, ENOBUFS);

		ru = &l->l_proc->p_stats->p_ru;
		mtbi = (struct mach_task_basic_info *)&rep->rep_info[0];

		mtbi->mtbi_suspend_count = ru->ru_nvcsw + ru->ru_nivcsw;
		mtbi->mtbi_virtual_size = ru->ru_ixrss;
		mtbi->mtbi_resident_size = ru->ru_maxrss;
		mtbi->mtbi_user_time.seconds = ru->ru_utime.tv_sec;
		mtbi->mtbi_user_time.microseconds = ru->ru_utime.tv_usec;
		mtbi->mtbi_system_time.seconds = ru->ru_stime.tv_sec;
		mtbi->mtbi_system_time.microseconds = ru->ru_stime.tv_usec;
		mtbi->mtbi_policy = 0;

		*msglen = sizeof(*rep) - sizeof(rep->rep_info) + sizeof(*mtbi);
		break;
	}

	/* XXX this is supposed to be about threads, not processes... */
	case MACH_TASK_THREAD_TIMES_INFO: {
		struct mach_task_thread_times_info *mttti;
		struct rusage *ru;

		count = sizeof(*mttti) / sizeof(rep->rep_info[0]);
		if (req->req_count < count)
			return mach_msg_error(args, ENOBUFS);

		ru = &l->l_proc->p_stats->p_ru;
		mttti = (struct mach_task_thread_times_info *)&rep->rep_info[0];

		mttti->mttti_user_time.seconds = ru->ru_utime.tv_sec;
		mttti->mttti_user_time.microseconds = ru->ru_utime.tv_usec;
		mttti->mttti_system_time.seconds = ru->ru_stime.tv_sec;
		mttti->mttti_system_time.microseconds = ru->ru_stime.tv_usec;

		*msglen = sizeof(*rep) - sizeof(rep->rep_info) + sizeof(*mttti);
		break;
	}

	/* XXX a few statistics missing here */
	case MACH_TASK_EVENTS_INFO: {
		struct mach_task_events_info *mtei;
		struct rusage *ru;

		count = sizeof(*mtei) / sizeof(rep->rep_info[0]);
		if (req->req_count < count)
			return mach_msg_error(args, ENOBUFS);

		mtei = (struct mach_task_events_info *)&rep->rep_info[0];
		ru = &l->l_proc->p_stats->p_ru;

		mtei->mtei_faults = ru->ru_majflt;
		mtei->mtei_pageins = ru->ru_minflt;
		mtei->mtei_cow_faults = 0; /* XXX */
		mtei->mtei_message_sent = ru->ru_msgsnd;
		mtei->mtei_message_received = ru->ru_msgrcv;
		mtei->mtei_syscalls_mach = 0; /* XXX */
		mtei->mtei_syscalls_unix = 0; /* XXX */
		mtei->mtei_csw = 0; /* XXX */

		*msglen = sizeof(*rep) - sizeof(rep->rep_info) + sizeof(*mtei);
		break;
	}

	default:
		uprintf("mach_task_info: unsupported flavor %d\n", 
		    req->req_flavor);
		return mach_msg_error(args, EINVAL);
	};
	
	rep->rep_msgh.msgh_bits =
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE);
	rep->rep_msgh.msgh_size = *msglen - sizeof(rep->rep_trailer);
	rep->rep_msgh.msgh_local_port = req->req_msgh.msgh_local_port;
	rep->rep_msgh.msgh_id = req->req_msgh.msgh_id + 100;
	rep->rep_count = count;
	rep->rep_info[count + 1] = 8; /* Trailer */
 
	return 0;
}

int
mach_task_suspend(args)
	struct mach_trap_args *args;
{
	mach_task_suspend_request_t *req = args->smsg;
	mach_task_suspend_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct lwp *l = args->l;
	mach_port_t mn;
	struct mach_right *mr;
	struct proc *p;
	struct mach_emuldata *med;
	int s;

	/* XXX more permission checks nescessary here? */
	mn = req->req_msgh.msgh_remote_port;
	if ((mr = mach_right_check(mn, l, MACH_PORT_TYPE_ALL_RIGHTS)) == 0)
		return mach_msg_error(args, EINVAL);

	if ((mr->mr_port == NULL) || 
	    (mr->mr_port->mp_datatype != MACH_MP_PROC))
		return mach_msg_error(args, EINVAL);

	p = (struct proc *)mr->mr_port->mp_data;
	med = p->p_emuldata;
	med->med_suspend++; /* XXX Mach also has a per thread semaphore */
		
	if (p->p_stat == SACTIVE) {
		sigminusset(&contsigmask, &p->p_sigctx.ps_siglist);
		SCHED_LOCK(s);
		p->p_stat = SSTOP;
		l->l_stat = LSSTOP;
		p->p_nrlwps--;
		mi_switch(l, NULL);
		SCHED_ASSERT_UNLOCKED();
		splx(s);
	}

	rep->rep_msgh.msgh_bits =
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE);
	rep->rep_msgh.msgh_size = sizeof(*rep) - sizeof(rep->rep_trailer);
	rep->rep_msgh.msgh_local_port = req->req_msgh.msgh_local_port;
	rep->rep_msgh.msgh_id = req->req_msgh.msgh_id + 100;
	rep->rep_trailer.msgh_trailer_size = 8;

	*msglen = sizeof(*rep);

	return 0;
}

int
mach_task_resume(args)
	struct mach_trap_args *args;
{
	mach_task_resume_request_t *req = args->smsg;
	mach_task_resume_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;
	struct lwp *l = args->l;
	mach_port_t mn;
	struct mach_right *mr;
	struct proc *p;
	struct mach_emuldata *med;
	int s;

	/* XXX more permission checks nescessary here? */
	mn = req->req_msgh.msgh_remote_port;
	if ((mr = mach_right_check(mn, l, MACH_PORT_TYPE_ALL_RIGHTS)) == 0)
		return mach_msg_error(args, EINVAL);

	if ((mr->mr_port == NULL) || 
	    (mr->mr_port->mp_datatype != MACH_MP_PROC))
		return mach_msg_error(args, EINVAL);

	p = (struct proc *)mr->mr_port->mp_data;
	med = p->p_emuldata;
	med->med_suspend--; /* XXX Mach also has a per thread semaphore */
	if (med->med_suspend > 0)
		return mach_msg_error(args, 0); /* XXX error code */
		
	/* XXX We should also wake up the stopped thread... */
	if (p->p_stat == SSTOP) {
		sigminusset(&stopsigmask, &p->p_sigctx.ps_siglist);
		SCHED_LOCK(s);
		p->p_stat = SACTIVE;
		SCHED_ASSERT_UNLOCKED();
		splx(s);
	}

	rep->rep_msgh.msgh_bits =
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE);
	rep->rep_msgh.msgh_size = sizeof(*rep) - sizeof(rep->rep_trailer);
	rep->rep_msgh.msgh_local_port = req->req_msgh.msgh_local_port;
	rep->rep_msgh.msgh_id = req->req_msgh.msgh_id + 100;
	rep->rep_retval = 0;
	rep->rep_trailer.msgh_trailer_size = 8;

	*msglen = sizeof(*rep);

	return 0;
}

int
mach_sys_task_for_pid(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct mach_sys_task_for_pid_args /* {
		syscallarg(mach_port_t) target_tport;
		syscallarg(int) pid;
		syscallarg(mach_port_t) *t;
	} */ *uap = v;
	struct mach_right *mr;
	struct mach_emuldata *med;
	struct lwp *tl;
	struct proc *tp;
	struct proc *p;
	int error;

	if ((mr = mach_right_check(SCARG(uap, target_tport), 
	    l, MACH_PORT_TYPE_ALL_RIGHTS)) == NULL)
		return EPERM;

	if (mr->mr_port->mp_datatype != MACH_MP_PROC)	
		return EINVAL;
	tp = (struct proc *)mr->mr_port->mp_data;
	tl = LIST_FIRST(&tp->p_lwps);

	if ((p = pfind(SCARG(uap, pid))) == NULL)
		return ESRCH;

	/* This will only work on a Mach process */
	if ((p->p_emul != &emul_mach) &&
#ifdef COMPAT_DARWIN
	    (p->p_emul != &emul_darwin) &&
#endif
	    1)
		return EINVAL;

	med = p->p_emuldata;

	if ((mr = mach_right_get(med->med_kernel, tl, 
	    MACH_PORT_TYPE_SEND, 0)) == NULL)
		return EINVAL;

	if ((error = copyout(&mr->mr_name, SCARG(uap, t), 
	    sizeof(mr->mr_name))) != 0)
		return error;

	return 0;
}
