/*	$NetBSD: pmap.c,v 1.9 2002/12/06 03:05:04 thorpej Exp $ */

/*
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Brown.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
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
#ifndef lint
__RCSID("$NetBSD: pmap.c,v 1.9 2002/12/06 03:05:04 thorpej Exp $");
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/sysctl.h>

#include <uvm/uvm.h>
#include <uvm/uvm_device.h>

#include <ufs/ufs/inode.h>
#undef doff_t
#undef IN_ACCESS
#include <isofs/cd9660/iso.h>
#include <isofs/cd9660/cd9660_node.h>

#include <kvm.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#ifndef __NetBSD_Version__
#error go away, you fool
#elif (__NetBSD_Version__ < 105000000)
#error only works with uvm
#endif

/*
 * stolen (and munged) from #include <uvm/uvm_object.h>
 */
#define UVM_OBJ_IS_VNODE(uobj)    ((uobj)->pgops == uvm_vnodeops)
#define UVM_OBJ_IS_AOBJ(uobj)     ((uobj)->pgops == aobj_pager)
#define UVM_OBJ_IS_DEVICE(uobj)   ((uobj)->pgops == uvm_deviceops)
#define UVM_OBJ_IS_UBCPAGER(uobj) ((uobj)->pgops == ubc_pager)

#define PRINT_VMSPACE		0x00000001
#define PRINT_VM_MAP		0x00000002
#define PRINT_VM_MAP_HEADER	0x00000004
#define PRINT_VM_MAP_ENTRY	0x00000008
#define DUMP_NAMEI_CACHE	0x00000010

struct cache_entry {
	LIST_ENTRY(cache_entry) ce_next;
	struct vnode *ce_vp, *ce_pvp;
	u_long ce_cid, ce_pcid;
	int ce_nlen;
	char ce_name[256];
};

LIST_HEAD(cache_head, cache_entry) lcache;
LIST_HEAD(nchashhead, namecache) *nchashtbl = NULL;
void *uvm_vnodeops, *uvm_deviceops, *aobj_pager, *ubc_pager;
void *kernel_floor;
struct vm_map *kmem_map, *mb_map, *phys_map, *exec_map, *pager_map;
u_long nchash_addr, nchashtbl_addr, kernel_map_addr;
int debug, verbose, recurse;
int print_all, print_map, print_maps, print_solaris, print_ddb;
int rwx = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE, heapfound;
rlim_t maxssiz;

struct kbit {
	/*
	 * size of data chunk
	 */
	size_t k_size;

	/*
	 * something for printf() and something for kvm_read()
	 */
	union {
		void *k_addr_p;
		u_long k_addr_ul;
	} k_addr;

	/*
	 * where we actually put the "stuff"
	 */
	union {
		char data[1];
		struct vmspace vmspace;
		struct vm_map vm_map;
		struct vm_map_entry vm_map_entry;
		struct vnode vnode;
		struct uvm_object uvm_object;
		struct mount mount;
		struct namecache namecache;
		struct inode inode;
		struct iso_node iso_node;
		struct uvm_device uvm_device;
	} k_data;
};

/* the size of the object in the kernel */
#define S(x)	((x)->k_size)
/* the address of the object in kernel, two forms */
#define A(x)	((x)->k_addr.k_addr_ul)
#define P(x)	((x)->k_addr.k_addr_p)
/* the data from the kernel */
#define D(x,d)	(&((x)->k_data.d))

/* suck the data from the kernel */
#define _KDEREF(kd, addr, dst, sz) do { \
	ssize_t len; \
	len = kvm_read((kd), (addr), (dst), (sz)); \
	if (len != (sz)) \
		errx(1, "trying to read %lu bytes from %lx: %s", \
		    (unsigned long)(sz), (addr), kvm_geterr(kd)); \
} while (0/*CONSTCOND*/)

/* suck the data using the structure */
#define KDEREF(kd, item) _KDEREF((kd), A(item), D(item, data), S(item))

/* when recursing, output is indented */
#define indent(n) ((n) * (recurse > 1 ? recurse - 1 : 0))

struct nlist ksyms[] = {
	{ "_maxsmap" },
#define NL_MAXSSIZ		0
	{ "_uvm_vnodeops" },
#define NL_UVM_VNODEOPS		1
	{ "_uvm_deviceops" },
#define NL_UVM_DEVICEOPS	2
	{ "_aobj_pager" },
#define NL_AOBJ_PAGER		3
	{ "_ubc_pager" },
#define NL_UBC_PAGER		4
	{ "_kernel_map" },
#define NL_KERNEL_MAP		5
	{ "_nchashtbl" },
#define NL_NCHASHTBL		6
	{ "_nchash" },
#define NL_NCHASH		7
	{ "_kernel_text" },
#define NL_KENTER		8
	{ NULL }
};

struct nlist kmaps[] = {
	{ "_kmem_map" },
#define NL_KMEM_MAP		0
	{ "_mb_map" },
#define NL_MB_MAP		1
	{ "_phys_map" },
#define NL_PHYS_MAP		2
	{ "_exec_map" },
#define NL_EXEC_MAP		3
	{ "_pager_map" },
#define NL_PAGER_MAP		4
	{ NULL }
};

void check(int);
void load_symbols(kvm_t *);
void process_map(kvm_t *, pid_t, struct kinfo_proc2 *);
void dump_vm_map(kvm_t *, struct kbit *, struct kbit *, char *);
size_t dump_vm_map_entry(kvm_t *, struct kbit *, struct kbit *, int);
char *findname(kvm_t *, struct kbit *, struct kbit *, struct kbit *,
	       struct kbit *, struct kbit *);
int search_cache(kvm_t *, struct kbit *, char **, char *, size_t);
void load_name_cache(kvm_t *);
void cache_enter(int, struct namecache *);

int
main(int argc, char *argv[])
{
	kvm_t *kd;
	pid_t pid;
	int many, ch, rc;
	char errbuf[_POSIX2_LINE_MAX + 1];
	struct kinfo_proc2 *kproc;
	char *kmem, *kernel;

	check(STDIN_FILENO);
	check(STDOUT_FILENO);
	check(STDERR_FILENO);

	pid = -1;
	verbose = debug = 0;
	print_all = print_map = print_maps = print_solaris = print_ddb = 0;
	recurse = 0;
	kmem = kernel = NULL;

	while ((ch = getopt(argc, argv, "aD:dlmM:N:p:PRrsvx")) != -1) {
		switch (ch) {
		case 'a':
			print_all = 1;
			break;
		case 'd':
			print_ddb = 1;
			break;
		case 'D':
			debug = atoi(optarg);
			break;
		case 'l':
			print_maps = 1;
			break;
		case 'm':
			print_map = 1;
			break;
		case 'M':
			kmem = optarg;
			break;
		case 'N':
			kernel = optarg;
			break;
		case 'p':
			pid = atoi(optarg);
			break;
		case 'P':
			pid = getpid();
			break;
		case 'R':
			recurse = 1;
			break;
		case 's':
			print_solaris = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'r':
		case 'x':
			errx(1, "-%c option not implemented, sorry", optopt);
			/*NOTREACHED*/
		case '?':
		default:
			fprintf(stderr, "usage: %s [-adlmPsv] [-D number] "
				"[-M core] [-N system] [-p pid] [pid ...]\n",
				getprogname());
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	/* more than one "process" to dump? */
	many = (argc > 1 - (pid == -1 ? 0 : 1)) ? 1 : 0;

	/* apply default */
	if (print_all + print_map + print_maps + print_solaris +
	    print_ddb == 0)
		print_solaris = 1;

	/* start by opening libkvm */
	kd = kvm_openfiles(kernel, kmem, NULL, O_RDONLY, errbuf);
	errbuf[_POSIX2_LINE_MAX] = '\0';
	if (kd == NULL)
		errx(1, "%s", errbuf);

	/* get "bootstrap" addresses from kernel */
	load_symbols(kd);

	do {
		if (pid == -1) {
			if (argc == 0)
				pid = getppid();
			else {
				pid = atoi(argv[0]);
				argv++;
				argc--;
			}
		}

		/* find the process id */
		if (pid == 0)
			kproc = NULL;
		else {
			kproc = kvm_getproc2(kd, KERN_PROC_PID, pid,
					     sizeof(struct kinfo_proc2), &rc);
			if (kproc == NULL || rc == 0) {
				errno = ESRCH;
				warn("%d", pid);
				pid = -1;
				continue;
			}
		}

		/* dump it */
		if (many) {
			if (kproc)
				printf("process %d:\n", kproc->p_pid);
			else
				printf("kernel:\n");
		}

		process_map(kd, pid, kproc);
		pid = -1;
	} while (argc > 0);

	/* done.  go away. */
	rc = kvm_close(kd);
	if (rc == -1)
		err(1, "kvm_close");

	return (0);
}

void
check(int fd)
{
	struct stat st;
	int n;

	if (fstat(fd, &st) == -1) {
		(void)close(fd);
		n = open("/dev/null", O_RDWR);
		if (n == fd || n == -1)
			/* we're either done or we can do no more */
			return;
		/* if either of these fail, there's not much we can do */
		(void)dup2(n, fd);
		(void)close(n);
		/* XXX should we exit if it fails? */
	}
}

void
process_map(kvm_t *kd, pid_t pid, struct kinfo_proc2 *proc)
{
	struct kbit kbit[2], *vmspace, *vm_map;
	char *thing;

	vmspace = &kbit[0];
	vm_map = &kbit[1];

	A(vmspace) = 0;
	A(vm_map) = 0;

	if (pid > 0) {
		heapfound = 0;
		A(vmspace) = (u_long)proc->p_vmspace;
		S(vmspace) = sizeof(struct vmspace);
		KDEREF(kd, vmspace);
		thing = "proc->p_vmspace.vm_map";
	} else {
		heapfound = 1; /* but really, do kernels have a heap? */
		A(vmspace) = 0;
		S(vmspace) = 0;
		thing = "kernel_map";
	}

	if (pid > 0 && (debug & PRINT_VMSPACE)) {
		printf("proc->p_vmspace %p = {", P(vmspace));
		printf(" vm_refcnt = %d,", D(vmspace, vmspace)->vm_refcnt);
		printf(" vm_shm = %p,\n", D(vmspace, vmspace)->vm_shm);
		printf("    vm_rssize = %d,", D(vmspace, vmspace)->vm_rssize);
		printf(" vm_swrss = %d,", D(vmspace, vmspace)->vm_swrss);
		printf(" vm_tsize = %d,", D(vmspace, vmspace)->vm_tsize);
		printf(" vm_dsize = %d,\n", D(vmspace, vmspace)->vm_dsize);
		printf("    vm_ssize = %d,", D(vmspace, vmspace)->vm_ssize);
		printf(" vm_taddr = %p,", D(vmspace, vmspace)->vm_taddr);
		printf(" vm_daddr = %p,\n", D(vmspace, vmspace)->vm_daddr);
		printf("    vm_maxsaddr = %p,",
		       D(vmspace, vmspace)->vm_maxsaddr);
		printf(" vm_minsaddr = %p }\n",
		       D(vmspace, vmspace)->vm_minsaddr);
	}

	S(vm_map) = sizeof(struct vm_map);
	if (pid > 0) {
		A(vm_map) = A(vmspace);
		memcpy(D(vm_map, vm_map), &D(vmspace, vmspace)->vm_map,
		       S(vm_map));
	} else {
		A(vm_map) = kernel_map_addr;
		KDEREF(kd, vm_map);
	}

	dump_vm_map(kd, vmspace, vm_map, thing);
}

void
load_symbols(kvm_t *kd)
{
	int rc, i;

	rc = kvm_nlist(kd, &ksyms[0]);
	if (rc != 0) {
		for (i = 0; ksyms[i].n_name != NULL; i++)
			if (ksyms[i].n_value == 0)
				warnx("symbol %s: not found", ksyms[i].n_name);
		exit(1);
	}

	uvm_vnodeops =	(void*)ksyms[NL_UVM_VNODEOPS].n_value;
	uvm_deviceops =	(void*)ksyms[NL_UVM_DEVICEOPS].n_value;
	aobj_pager =	(void*)ksyms[NL_AOBJ_PAGER].n_value;
	ubc_pager =	(void*)ksyms[NL_UBC_PAGER].n_value;

	kernel_floor =	(void*)ksyms[NL_KENTER].n_value;
	nchash_addr =	ksyms[NL_NCHASH].n_value;

	_KDEREF(kd, ksyms[NL_MAXSSIZ].n_value, &maxssiz,
		sizeof(maxssiz));
	_KDEREF(kd, ksyms[NL_NCHASHTBL].n_value, &nchashtbl_addr,
	       sizeof(nchashtbl_addr));
	_KDEREF(kd, ksyms[NL_KERNEL_MAP].n_value, &kernel_map_addr,
		sizeof(kernel_map_addr));

	/*
	 * Some of these may be missing from some platforms, for
	 * example sparc, sh3, and most powerpc platforms don't
	 * have a "phys_map".
	 */
	(void)kvm_nlist(kd, &kmaps[0]);
	if (kmaps[NL_KMEM_MAP].n_value != 0)
		_KDEREF(kd, kmaps[NL_KMEM_MAP].n_value, &kmem_map,
			sizeof(kmem_map));
	if (kmaps[NL_MB_MAP].n_value != 0)
		_KDEREF(kd, kmaps[NL_MB_MAP].n_value, &mb_map,
			sizeof(mb_map));
	if (kmaps[NL_PHYS_MAP].n_value != 0)
		_KDEREF(kd, kmaps[NL_PHYS_MAP].n_value, &phys_map,
			sizeof(phys_map));
	if (kmaps[NL_EXEC_MAP].n_value != 0)
		_KDEREF(kd, kmaps[NL_EXEC_MAP].n_value, &exec_map,
			sizeof(exec_map));
	if (kmaps[NL_PAGER_MAP].n_value != 0)
		_KDEREF(kd, kmaps[NL_PAGER_MAP].n_value, &pager_map,
			sizeof(pager_map));
}

void
dump_vm_map(kvm_t *kd, struct kbit *vmspace, struct kbit *vm_map,
	    char *mname)
{
	struct kbit kbit[2], *header, *vm_map_entry;
	struct vm_map_entry *last, *next;
	size_t total;
	u_long addr;

	header = &kbit[0];
	vm_map_entry = &kbit[1];
	A(header) = 0;
	A(vm_map_entry) = 0;

	if (debug & PRINT_VM_MAP) {
		printf("%*s%s %p = {", indent(2), "", mname, P(vm_map));
		printf(" pmap = %p,\n", D(vm_map, vm_map)->pmap);
		printf("%*s    lock = <struct lock>,", indent(2), "");
		printf(" header = <struct vm_map_entry>,");
		printf(" nentries = %d,\n", D(vm_map, vm_map)->nentries);
		printf("%*s    size = %lx,", indent(2), "",
		       D(vm_map, vm_map)->size);
		printf(" ref_count = %d,", D(vm_map, vm_map)->ref_count);
		printf(" ref_lock = <struct simplelock>,\n");
		printf("%*s    hint = %p,", indent(2), "",
		       D(vm_map, vm_map)->hint);
		printf(" hint_lock = <struct simplelock>,\n");
		printf("%*s    first_free = %p,", indent(2), "",
		       D(vm_map, vm_map)->first_free);
		printf(" flags = %x <%s%s%s%s%s%s >,\n", D(vm_map, vm_map)->flags,
		       D(vm_map, vm_map)->flags & VM_MAP_PAGEABLE ? " PAGEABLE" : "",
		       D(vm_map, vm_map)->flags & VM_MAP_INTRSAFE ? " INTRSAFE" : "",
		       D(vm_map, vm_map)->flags & VM_MAP_WIREFUTURE ? " WIREFUTURE" : "",
		       D(vm_map, vm_map)->flags & VM_MAP_BUSY ? " BUSY" : "",
		       D(vm_map, vm_map)->flags & VM_MAP_WANTLOCK ? " WANTLOCK" : ""
#ifdef VM_MAP_DYING
		       , D(vm_map, vm_map)->flags & VM_MAP_DYING ? " DYING" : ""
#endif
#ifdef VM_MAP_TOPDOWN
		       , D(vm_map, vm_map)->flags & VM_MAP_TOPDOWN ? " TOPDOWN" : ""
#endif
		       );
		printf("%*s    flags_lock = <struct simplelock>,", indent(2), "");
		printf(" timestamp = %u }\n", D(vm_map, vm_map)->timestamp);
	}
	if (print_ddb) {
		char *name;

		if (A(vm_map) == kernel_map_addr)
			name = "kernel_map";
		else if (P(vm_map) == kmem_map)
			name = "kmem_map";
		else if (P(vm_map) == mb_map)
			name = "mb_map";
		else if (P(vm_map) == phys_map)
			name = "phys_map";
		else if (P(vm_map) == exec_map)
			name = "exec_map";
		else if (P(vm_map) == pager_map)
			name = "pager_map";
		else
			name = NULL;

		printf("%*s%s %p: [0x%lx->0x%lx]\n", indent(2), "",
		       recurse < 2 ? "MAP" : "SUBMAP", P(vm_map),
		       D(vm_map, vm_map)->min_offset,
		       D(vm_map, vm_map)->max_offset);
		printf("\t%*s#ent=%d, sz=%ld, ref=%d, version=%d, flags=0x%x\n",
		       indent(2), "", D(vm_map, vm_map)->nentries,
		       D(vm_map, vm_map)->size, D(vm_map, vm_map)->ref_count,
		       D(vm_map, vm_map)->timestamp, D(vm_map, vm_map)->flags);
		printf("\t%*spmap=%p(resident=<unknown>)\n", indent(2), "",
		       D(vm_map, vm_map)->pmap);
		if (verbose && name != NULL)
			printf("\t%*s([ %s ])\n", indent(2), "", name);
	}

	A(header) = A(vm_map) + offsetof(struct vm_map, header);
	S(header) = sizeof(struct vm_map_entry);
	memcpy(D(header, vm_map_entry), &D(vm_map, vm_map)->header, S(header));
	dump_vm_map_entry(kd, vmspace, header, 1);

	/*
	 * we're not recursing into a submap, so print headers
	 */
	if (recurse < 2) {
		/* headers */
#ifdef DISABLED_HEADERS
		if (print_map)
			printf("%-*s %-*s rwx RWX CPY NCP I W A\n",
			       (int)sizeof(long) * 2 + 2, "Start",
			       (int)sizeof(long) * 2 + 2, "End");
		if (print_maps)
			printf("%-*s %-*s rwxp %-*s Dev   Inode      File\n",
			       (int)sizeof(long) * 2 + 0, "Start",
			       (int)sizeof(long) * 2 + 0, "End",
			       (int)sizeof(long) * 2 + 0, "Offset");
		if (print_solaris)
			printf("%-*s %*s Protection        File\n",
			       (int)sizeof(long) * 2 + 0, "Start",
			       (int)sizeof(int) * 2 - 1,  "Size ");
#endif
		if (print_all)
			printf("%-*s %-*s %*s %-*s rwxpc  RWX  I/W/A Dev  %*s"
			       " - File\n",
			       (int)sizeof(long) * 2, "Start",
			       (int)sizeof(long) * 2, "End",
			       (int)sizeof(int)  * 2, "Size ",
			       (int)sizeof(long) * 2, "Offset",
			       (int)sizeof(int)  * 2, "Inode");
	}

	/* these are the "sub entries" */
	total = 0;
	next = D(header, vm_map_entry)->next;
	last = P(header);

	while (next != 0 && next != last) {
		addr = (u_long)next;
		A(vm_map_entry) = addr;
		S(vm_map_entry) = sizeof(struct vm_map_entry);
		KDEREF(kd, vm_map_entry);
		next = D(vm_map_entry, vm_map_entry)->next;
		total += dump_vm_map_entry(kd, vmspace, vm_map_entry, 0);
	}

	/*
	 * we're not recursing into a submap, so print totals
	 */
	if (recurse < 2) {
		if (print_solaris)
			printf("%-*s %8luK\n",
			       (int)sizeof(void *) * 2 - 2, " total",
			       (unsigned long)total);
		if (print_all)
			printf("%-*s %9luk\n",
			       (int)sizeof(void *) * 4 - 1, " total",
			       (unsigned long)total);
	}
}

size_t
dump_vm_map_entry(kvm_t *kd, struct kbit *vmspace,
		  struct kbit *vm_map_entry,
		  int ishead)
{
	struct kbit kbit[3];
	struct kbit *uvm_obj, *vp, *vfs;
	struct vm_map_entry *vme;
	size_t sz;
	char *name;
	dev_t dev;
	ino_t inode;

	uvm_obj = &kbit[0];
	vp = &kbit[1];
	vfs = &kbit[2];

	A(uvm_obj) = 0;
	A(vp) = 0;
	A(vfs) = 0;

	vme = D(vm_map_entry, vm_map_entry);

	if ((ishead && (debug & PRINT_VM_MAP_HEADER)) ||
	    (!ishead && (debug & PRINT_VM_MAP_ENTRY))) {
		printf("%*s%s %p = {", indent(2), "",
		       ishead ? "vm_map.header" : "vm_map_entry",
		       P(vm_map_entry));
		printf(" prev = %p,", vme->prev);
		printf(" next = %p,\n", vme->next);
		printf("%*s    start = %lx,", indent(2), "", vme->start);
		printf(" end = %lx,", vme->end);
		printf(" object.uvm_obj/sub_map = %p,\n", vme->object.uvm_obj);
		printf("%*s    offset = %" PRIx64 ",", indent(2), "",
		       vme->offset);
		printf(" etype = %x <%s%s%s%s >,", vme->etype,
		       vme->etype & UVM_ET_OBJ ? " OBJ" : "",
		       vme->etype & UVM_ET_SUBMAP ? " SUBMAP" : "",
		       vme->etype & UVM_ET_COPYONWRITE ? " COW" : "",
		       vme->etype & UVM_ET_NEEDSCOPY ? " NEEDSCOPY" : "");
		printf(" protection = %x,\n", vme->protection);
		printf("%*s    max_protection = %x,", indent(2), "",
		       vme->max_protection);
		printf(" inheritance = %d,", vme->inheritance);
		printf(" wired_count = %d,\n", vme->wired_count);
		printf("%*s    aref = { ar_pageoff = %x, ar_amap = %p },",
		       indent(2), "", vme->aref.ar_pageoff, vme->aref.ar_amap);
		printf(" advice = %d,\n", vme->advice);
		printf("%*s    flags = %x <%s%s > }\n", indent(2), "",
		       vme->flags,
		       vme->flags & UVM_MAP_STATIC ? " STATIC" : "",
		       vme->flags & UVM_MAP_KMEM ? " KMEM" : "");
	}

	if (ishead)
		return (0);

	A(vp) = 0;
	A(uvm_obj) = 0;

	if (vme->object.uvm_obj != NULL) {
		P(uvm_obj) = vme->object.uvm_obj;
		S(uvm_obj) = sizeof(struct uvm_object);
		KDEREF(kd, uvm_obj);
		if (UVM_ET_ISOBJ(vme) &&
		    UVM_OBJ_IS_VNODE(D(uvm_obj, uvm_object))) {
			P(vp) = P(uvm_obj);
			S(vp) = sizeof(struct vnode);
			KDEREF(kd, vp);
		}
	}

	A(vfs) = NULL;

	if (P(vp) != NULL && D(vp, vnode)->v_mount != NULL) {
		P(vfs) = D(vp, vnode)->v_mount;
		S(vfs) = sizeof(struct mount);
		KDEREF(kd, vfs);
		D(vp, vnode)->v_mount = D(vfs, mount);
	}

	/*
	 * dig out the device number and inode number from certain
	 * file system types.
	 */
#define V_DATA_IS(vp, type, d, i) do { \
	struct kbit data; \
	P(&data) = D(vp, vnode)->v_data; \
	S(&data) = sizeof(*D(&data, type)); \
	KDEREF(kd, &data); \
	dev = D(&data, type)->d; \
	inode = D(&data, type)->i; \
} while (0/*CONSTCOND*/)

	dev = 0;
	inode = 0;

	if (A(vp) &&
	    D(vp, vnode)->v_type == VREG &&
	    D(vp, vnode)->v_data != NULL) {
		switch (D(vp, vnode)->v_tag) {
		case VT_UFS:
		case VT_LFS:
		case VT_EXT2FS:
			V_DATA_IS(vp, inode, i_dev, i_number);
			break;
		case VT_ISOFS:
			V_DATA_IS(vp, iso_node, i_dev, i_number);
			break;
		case VT_NON:
		case VT_NFS:
		case VT_MFS:
		case VT_MSDOSFS:
		case VT_LOFS:
		case VT_FDESC:
		case VT_PORTAL:
		case VT_NULL:
		case VT_UMAP:
		case VT_KERNFS:
		case VT_PROCFS:
		case VT_AFS:
		case VT_UNION:
		case VT_ADOSFS:
		case VT_CODA:
		case VT_FILECORE:
		case VT_NTFS:
		case VT_VFS:
		case VT_OVERLAY:
		case VT_SMBFS:
			break;
		}
	}

	name = findname(kd, vmspace, vm_map_entry, vp, vfs, uvm_obj);

	if (print_map) {
		printf("%*s0x%lx 0x%lx %c%c%c %c%c%c %s %s %d %d %d",
		       indent(2), "",
		       vme->start, vme->end,
		       (vme->protection & VM_PROT_READ) ? 'r' : '-',
		       (vme->protection & VM_PROT_WRITE) ? 'w' : '-',
		       (vme->protection & VM_PROT_EXECUTE) ? 'x' : '-',
		       (vme->max_protection & VM_PROT_READ) ? 'r' : '-',
		       (vme->max_protection & VM_PROT_WRITE) ? 'w' : '-',
		       (vme->max_protection & VM_PROT_EXECUTE) ? 'x' : '-',
		       (vme->etype & UVM_ET_COPYONWRITE) ? "COW" : "NCOW",
		       (vme->etype & UVM_ET_NEEDSCOPY) ? "NC" : "NNC",
		       vme->inheritance, vme->wired_count,
		       vme->advice);
		if (verbose) {
			if (inode)
				printf(" %d,%d %d",
				       major(dev), minor(dev), inode);
			if (name[0])
				printf(" %s", name);
		}
		printf("\n");
	}

	if (print_maps) {
		printf("%*s%0*lx-%0*lx %c%c%c%c %0*" PRIx64 " %02x:%02x %d     %s\n",
		       indent(2), "",
		       (int)sizeof(void *) * 2, vme->start,
		       (int)sizeof(void *) * 2, vme->end,
		       (vme->protection & VM_PROT_READ) ? 'r' : '-',
		       (vme->protection & VM_PROT_WRITE) ? 'w' : '-',
		       (vme->protection & VM_PROT_EXECUTE) ? 'x' : '-',
		       (vme->etype & UVM_ET_COPYONWRITE) ? 'p' : 's',
		       (int)sizeof(void *) * 2,
		       vme->offset,
		       major(dev), minor(dev), inode,
		       (name[0] != ' ') || verbose ? name : "");
	}

	if (print_ddb) {
		printf("%*s - %p: 0x%lx->0x%lx: obj=%p/0x%" PRIx64 ", amap=%p/%d\n",
		       indent(2), "",
		       P(vm_map_entry), vme->start, vme->end,
		       vme->object.uvm_obj, vme->offset,
		       vme->aref.ar_amap, vme->aref.ar_pageoff);
		printf("\t%*ssubmap=%c, cow=%c, nc=%c, prot(max)=%d/%d, inh=%d, "
		       "wc=%d, adv=%d\n",
		       indent(2), "",
		       (vme->etype & UVM_ET_SUBMAP) ? 'T' : 'F',
		       (vme->etype & UVM_ET_COPYONWRITE) ? 'T' : 'F',
		       (vme->etype & UVM_ET_NEEDSCOPY) ? 'T' : 'F',
		       vme->protection, vme->max_protection,
		       vme->inheritance, vme->wired_count, vme->advice);
		if (verbose) {
			printf("\t%*s", indent(2), "");
			if (inode)
				printf("(dev=%d,%d ino=%d [%s] [%p])\n",
				       major(dev), minor(dev), inode,
				       name, P(vp));
			else if (name[0] == ' ')
				printf("(%s)\n", &name[2]);
			else
				printf("(%s)\n", name);
		}
	}

	sz = 0;
	if (print_solaris) {
		char prot[30];

		prot[0] = '\0';
		prot[1] = '\0';
		if (vme->protection & VM_PROT_READ)
			strcat(prot, "/read");
		if (vme->protection & VM_PROT_WRITE)
			strcat(prot, "/write");
		if (vme->protection & VM_PROT_EXECUTE)
			strcat(prot, "/exec");

		sz = (size_t)((vme->end - vme->start) / 1024);
		printf("%*s%0*lX %6luK %-15s   %s\n",
		       indent(2), "",
		       (int)sizeof(void *) * 2,
		       (unsigned long)vme->start,
		       (unsigned long)sz,
		       &prot[1],
		       name);
	}

	if (print_all) {
		sz = (size_t)((vme->end - vme->start) / 1024);
		printf(A(vp) ?
		       "%*s%0*lx-%0*lx %7luk %0*" PRIx64 " %c%c%c%c%c (%c%c%c) %d/%d/%d %02d:%02d %7d - %s [%p]\n" :
		       "%*s%0*lx-%0*lx %7luk %0*" PRIx64 " %c%c%c%c%c (%c%c%c) %d/%d/%d %02d:%02d %7d - %s\n",
		       indent(2), "",
		       (int)sizeof(void *) * 2,
		       vme->start,
		       (int)sizeof(void *) * 2,
		       vme->end - (vme->start != vme->end ? 1 : 0),
		       (unsigned long)sz,
		       (int)sizeof(void *) * 2,
		       vme->offset,
		       (vme->protection & VM_PROT_READ) ? 'r' : '-',
		       (vme->protection & VM_PROT_WRITE) ? 'w' : '-',
		       (vme->protection & VM_PROT_EXECUTE) ? 'x' : '-',
		       (vme->etype & UVM_ET_COPYONWRITE) ? 'p' : 's',
		       (vme->etype & UVM_ET_NEEDSCOPY) ? '+' : '-',
		       (vme->max_protection & VM_PROT_READ) ? 'r' : '-',
		       (vme->max_protection & VM_PROT_WRITE) ? 'w' : '-',
		       (vme->max_protection & VM_PROT_EXECUTE) ? 'x' : '-',
		       vme->inheritance,
		       vme->wired_count,
		       vme->advice,
		       major(dev), minor(dev), inode,
		       name, P(vp));
	}

	/* no access allowed, don't count space */
	if ((vme->protection & rwx) == 0)
		sz = 0;

	if (recurse && (vme->etype & UVM_ET_SUBMAP)) {
		struct kbit mkbit, *submap;

		recurse++;
		submap = &mkbit;
		P(submap) = vme->object.sub_map;
		S(submap) = sizeof(*vme->object.sub_map);
		KDEREF(kd, submap);
		dump_vm_map(kd, vmspace, submap, "submap");
		recurse--;
	}

	return (sz);
}

char*
findname(kvm_t *kd, struct kbit *vmspace,
	 struct kbit *vm_map_entry, struct kbit *vp,
	 struct kbit *vfs, struct kbit *uvm_obj)
{
	static char buf[1024], *name;
	struct vm_map_entry *vme;
	size_t l;

	vme = D(vm_map_entry, vm_map_entry);

	if (UVM_ET_ISOBJ(vme)) {
		if (A(vfs)) {
			l = (unsigned)strlen(D(vfs, mount)->mnt_stat.f_mntonname);
			switch (search_cache(kd, vp, &name, buf, sizeof(buf))) { 
			    case 0: /* found something */
                                name--;
                                *name = '/';
				/*FALLTHROUGH*/
			    case 2: /* found nothing */
				name -= 5;
				memcpy(name, " -?- ", (size_t)5);
				name -= l;
				memcpy(name,
				       D(vfs, mount)->mnt_stat.f_mntonname, l);
				break;
			    case 1: /* all is well */
				name--;
				*name = '/';
				if (l != 1) {
					name -= l;
					memcpy(name,
					       D(vfs, mount)->mnt_stat.f_mntonname, l);
				}
				break;
			}
		}
		else if (UVM_OBJ_IS_DEVICE(D(uvm_obj, uvm_object))) {
			struct kbit kdev;
			dev_t dev;

			P(&kdev) = P(uvm_obj);
			S(&kdev) = sizeof(struct uvm_device);
			KDEREF(kd, &kdev);
			dev = D(&kdev, uvm_device)->u_device;
			name = devname(dev, S_IFCHR);
			if (name != NULL)
				snprintf(buf, sizeof(buf), "/dev/%s", name);
			else
				snprintf(buf, sizeof(buf), "  [ device %d,%d ]",
					 major(dev), minor(dev));
			name = buf;
		}
		else if (UVM_OBJ_IS_AOBJ(D(uvm_obj, uvm_object))) 
			name = "  [ uvm_aobj ]";
		else if (UVM_OBJ_IS_UBCPAGER(D(uvm_obj, uvm_object)))
			name = "  [ ubc_pager ]";
		else if (UVM_OBJ_IS_VNODE(D(uvm_obj, uvm_object)))
			name = "  [ ?VNODE? ]";
		else {
			snprintf(buf, sizeof(buf), "  [ ?? %p ?? ]",
				 D(uvm_obj, uvm_object)->pgops);
			name = buf;
		}
	}

	else if (D(vmspace, vmspace)->vm_maxsaddr <=
		 (caddr_t)vme->start &&
		 (D(vmspace, vmspace)->vm_maxsaddr + (size_t)maxssiz) >=
		 (caddr_t)vme->end)
		name = "  [ stack ]";

	else if ((vme->protection & rwx) == rwx && !heapfound) {
		/* XXX this could probably be done better */
		heapfound = 1;
		name = "  [ heap ]";
	}

	else
		name = "  [ anon ]";

	return (name);
}

int
search_cache(kvm_t *kd, struct kbit *vp, char **name, char *buf, size_t blen)
{
	char *o, *e;
	struct cache_entry *ce;
	struct kbit svp;
	u_long cid;

	if (nchashtbl == NULL)
		load_name_cache(kd);

	P(&svp) = P(vp);
	S(&svp) = sizeof(struct vnode);
	cid = D(vp, vnode)->v_id;

	e = &buf[blen - 1];
	o = e;
	do {
		LIST_FOREACH(ce, &lcache, ce_next)
			if (ce->ce_vp == P(&svp) && ce->ce_cid == cid)
				break;
		if (ce && ce->ce_vp == P(&svp) && ce->ce_cid == cid) {
			if (o != e)
				*(--o) = '/';
			o -= ce->ce_nlen;
			memcpy(o, ce->ce_name, (unsigned)ce->ce_nlen);
			P(&svp) = ce->ce_pvp;
			cid = ce->ce_pcid;
		}
		else
			break;
	} while (1/*CONSTCOND*/);
	*e = '\0';
	*name = o;

	if (e == o)
		return (2);

	KDEREF(kd, &svp);
	return (D(&svp, vnode)->v_flag & VROOT);
}

void
load_name_cache(kvm_t *kd)
{
	struct namecache _ncp, *ncp, *oncp;
	struct nchashhead _ncpp, *ncpp; 
	u_long nchash;
	int i;

	LIST_INIT(&lcache);

	_KDEREF(kd, nchash_addr, &nchash, sizeof(nchash));
	nchashtbl = malloc(sizeof(nchashtbl) * (int)nchash);
	_KDEREF(kd, nchashtbl_addr, nchashtbl,
		sizeof(nchashtbl) * (int)nchash);

	ncpp = &_ncpp;

	for (i = 0; i <= nchash; i++) {
		ncpp = &nchashtbl[i];
		oncp = NULL;
		LIST_FOREACH(ncp, ncpp, nc_hash) {
			if (ncp == oncp ||
			    (void*)ncp < kernel_floor ||
			    ncp == (void*)0xdeadbeef)
				break;
			oncp = ncp;
			_KDEREF(kd, (u_long)ncp, &_ncp, sizeof(*ncp));
			ncp = &_ncp;
			if ((void*)ncp->nc_vp > kernel_floor &&
			    ncp->nc_nlen > 0) {
				if (ncp->nc_nlen > 2 ||
				    ncp->nc_name[0] != '.' ||
				    (ncp->nc_name[1] != '.' &&
				     ncp->nc_nlen != 1))
					cache_enter(i, ncp);
			}
		}
	}
}

void
cache_enter(int i, struct namecache *ncp)
{
	struct cache_entry *ce;

	if (debug & DUMP_NAMEI_CACHE)
		printf("[%d] ncp->nc_vp %10p, ncp->nc_dvp %10p, "
		       "ncp->nc_nlen %3d [%.*s] (nc_dvpid=%lu, nc_vpid=%lu)\n",
		       i, ncp->nc_vp, ncp->nc_dvp,
		       ncp->nc_nlen, ncp->nc_nlen, ncp->nc_name,
		       ncp->nc_dvpid, ncp->nc_vpid);

	ce = malloc(sizeof(struct cache_entry));
	
	ce->ce_vp = ncp->nc_vp;
	ce->ce_pvp = ncp->nc_dvp;
	ce->ce_cid = ncp->nc_vpid;
	ce->ce_pcid = ncp->nc_dvpid;
	ce->ce_nlen = ncp->nc_nlen;
	strncpy(ce->ce_name, ncp->nc_name, sizeof(ce->ce_name));
	ce->ce_name[MIN(ce->ce_nlen, sizeof(ce->ce_name) - 1)] = '\0';

	LIST_INSERT_HEAD(&lcache, ce, ce_next);
}
