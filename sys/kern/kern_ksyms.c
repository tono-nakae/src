/*	$NetBSD: kern_ksyms.c,v 1.2 2003/04/25 07:35:21 ragge Exp $	*/
/*
 * Copyright (c) 2001, 2003 Anders Magnusson (ragge@ludd.luth.se).
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Code to deal with in-kernel symbol table management + /dev/ksyms.
 *
 * For each loaded module the symbol table info is kept track of by a
 * struct, placed in a circular list. The first entry is the kernel
 * symbol table.
 */

/*
 * TODO:
 *	Fix quick-search of symbols. (comes with linker)
 *	Change the ugly way of adding new symbols (comes with linker)
 *	Add kernel locking stuff.
 *	(Ev) add support for poll.
 *	(Ev) fix support for mmap.
 *
 *	Export ksyms internal logic for use in post-mortem debuggers?
 *	  Need to move struct symtab to ksyms.h for that.
 */

#ifdef _KERNEL
#include "opt_ddb.h"
#endif

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/exec.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <machine/elf_machdep.h> /* XXX */
#define ELFSIZE ARCH_ELFSIZE

#include <sys/exec_elf.h>
#include <sys/ksyms.h>

#include <lib/libkern/libkern.h>

#ifdef DDB
#include <ddb/db_output.h>
#endif

#include "ksyms.h"

static int ksymsinited = 0;

#if NKSYMS
static void ksyms_hdr_init(caddr_t hdraddr);
static void ksyms_sizes_calc(void);
static int ksyms_isopen;
#endif

#ifdef KSYMS_DEBUG
#define	FOLLOW_CALLS		1
#define	FOLLOW_MORE_CALLS	2
#define	FOLLOW_DEVKSYMS		4
static int ksyms_debug;
#endif

#if NKSYMS
dev_type_open(ksymsopen);
dev_type_close(ksymsclose);
dev_type_read(ksymsread);
dev_type_write(ksymswrite);
dev_type_ioctl(ksymsioctl);

const struct cdevsw ksyms_cdevsw = {
	ksymsopen, ksymsclose, ksymsread, ksymswrite, ksymsioctl,
	nullstop, notty, nopoll, nommap, nullkqfilter, DV_DULL
};
#endif


/*
 * Store the different symbol tables in a double-linked list.
 */
struct symtab {
	CIRCLEQ_ENTRY(symtab) sd_queue;
	char *sd_name;		/* Name of this table */
	Elf_Sym *sd_symstart;	/* Address of symbol table */
	caddr_t sd_strstart;	/* Adderss of corresponding string table */
	int sd_symsize;		/* Size in bytes of symbol table */
	int sd_strsize;		/* Size of string table */
	int *sd_symnmoff;	/* Used when calculating the name offset */
};

static CIRCLEQ_HEAD(, symtab) symtab_queue =
    CIRCLEQ_HEAD_INITIALIZER(symtab_queue);

static struct symtab kernel_symtab;

/*
 * Finds a certain symbol name in a certain symbol table.
 * XXX - symbol hashing must be rewritten (missing)
 */
static Elf_Sym *
findsym(char *name, struct symtab *table)
{
	Elf_Sym *start = table->sd_symstart;
	int i, sz = table->sd_symsize/sizeof(Elf_Sym);
	char *np;

	for (i = 0; i < sz; i++) {
		np = table->sd_strstart + start[i].st_name;
		if (name[0] == np[0] && name[1] == np[1] &&
		    strcmp(name, np) == 0)
			return &start[i];
	}
	return NULL;
}

/*
 * The "attach" is in reality done in ksyms_init().
 */
void ksymsattach(int);
void
ksymsattach(int arg)
{
}

/*
 * Add a symbol table named name.
 * This is intended for use when the kernel loader enters the table.
 */
static void
addsymtab(char *name, Elf_Ehdr *ehdr, struct symtab *tab)
{
	caddr_t start = (caddr_t)ehdr;
	Elf_Shdr *shdr;
	Elf_Sym *sym;
	int i, j;

	/* Find the symbol table and the corresponding string table. */
	shdr = (Elf_Shdr *)(start + ehdr->e_shoff);
	for (i = 1; i < ehdr->e_shnum; i++) {
		if (shdr[i].sh_type != SHT_SYMTAB)
			continue;
		if (shdr[i].sh_offset == 0)
			continue;
		tab->sd_symstart = (Elf_Sym *)(start + shdr[i].sh_offset);
		tab->sd_symsize = shdr[i].sh_size;
		j = shdr[i].sh_link;
		if (shdr[j].sh_offset == 0)
			continue; /* Can this happen? */
		tab->sd_strstart = start + shdr[j].sh_offset;
		tab->sd_strsize = shdr[j].sh_size;
		break;
	}
	tab->sd_name = name;

	/* Change all symbols to be absolute references */
	sym = (Elf_Sym *)tab->sd_symstart;
	for (i = 0; i < tab->sd_symsize/sizeof(Elf_Sym); i++)
		sym[i].st_shndx = SHN_ABS;

	CIRCLEQ_INSERT_HEAD(&symtab_queue, tab, sd_queue);
}

/*
 * Setup the kernel symbol table stuff.
 */
void
ksyms_init(caddr_t start, caddr_t end)
{
	Elf_Ehdr *ehdr = (Elf_Ehdr *)start;

	/* check if this is a valid ELF header */
	/* No reason to verify arch type, the kernel is actually running! */
	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) ||
	    ehdr->e_ident[EI_CLASS] != ELFCLASS ||
	    ehdr->e_version > 1) {
		printf("Kernel symbol table invalid!\n");
		return; /* nothing to do */
	}

	addsymtab("netbsd", ehdr, &kernel_symtab);
#if NKSYMS
	ksyms_sizes_calc();
#endif
	ksymsinited = 1;
#ifdef DEBUG
	printf("Loaded initial symtab at %p, strtab at %p, # entries %ld\n",
	    kernel_symtab.sd_symstart, kernel_symtab.sd_strstart,
	    (long)kernel_symtab.sd_symsize/sizeof(Elf_Sym));
#endif

#if NKSYMS
	ksyms_hdr_init(start);
#endif
}

/*
 * Get the value associated with a symbol.
 * "mod" is the module name, or null if any module. 
 * "sym" is the symbol name.
 * "val" is a pointer to the corresponding value, if call succeeded.
 * Returns 0 if success or ENOENT if no such entry.
 */
int
ksyms_getval(char *mod, char *sym, unsigned long *val, int type)
{
	struct symtab *st;
	Elf_Sym *es;

	if (ksymsinited == 0)
		return ENOENT;

#ifdef KSYMS_DEBUG
	if (ksyms_debug & FOLLOW_CALLS)
		printf("ksyms_getval: mod %s sym %s valp %p\n", mod, sym, val);
#endif

	/* XXX search order XXX */
	CIRCLEQ_FOREACH(st, &symtab_queue, sd_queue) {
		if (mod && strcmp(st->sd_name, mod))
			continue;
		if ((es = findsym(sym, st)) == NULL)
			continue;

		/* Skip if bad binding */
		if (type == KSYMS_EXTERN &&
		    ELF_ST_BIND(es->st_info) != STB_GLOBAL)
			continue;

		if (val)
			*val = es->st_value;
		return 0;
	}
	return ENOENT;
}

/*
 * Get "mod" and "symbol" associated with an address.
 * Returns 0 if success or ENOENT if no such entry.
 */
int
ksyms_getname(char **mod, char **sym, vaddr_t v, int f)
{
	struct symtab *st;
	Elf_Sym *les, *es = NULL;
	vaddr_t laddr = 0;
	char *lmod, *stable;
	int type, i, sz;

	if (ksymsinited == 0)
		return ENOENT;

	CIRCLEQ_FOREACH(st, &symtab_queue, sd_queue) {
		sz = st->sd_symsize/sizeof(Elf_Sym);
		for (i = 0; i < sz; i++) {
			les = st->sd_symstart + i;
			type = ELF_ST_TYPE(les->st_info);

			if ((f & KSYMS_PROC) && (type != STT_FUNC))
				continue;

			if (type == STT_NOTYPE)
				continue;

			if (((f & KSYMS_ANY) == 0) &&
			    (type != STT_FUNC) && (type != STT_OBJECT))
				continue;

			if ((les->st_value <= v) && (les->st_value > laddr)) {
				laddr = les->st_value;
				es = les;
				lmod = st->sd_name;
				stable = st->sd_strstart;
			}
		}
	}
	if (es == NULL)
		return ENOENT;
	if ((f & KSYMS_EXACT) && (v != es->st_value))
		return ENOENT;
	if (mod)
		*mod = lmod;
	if (sym)
		*sym = stable + es->st_name;
	return 0;
}

#if NKSYMS
static int symsz, strsz;

static void
ksyms_sizes_calc(void)
{               
        struct symtab *st; 
	int i;

        symsz = strsz = 0;
        CIRCLEQ_FOREACH(st, &symtab_queue, sd_queue) {
		if (st != &kernel_symtab) {
			for (i = 0; i < st->sd_symsize/sizeof(Elf_Sym); i++)
				st->sd_symstart[i].st_name =
				    strsz + st->sd_symnmoff[i];
		}
                symsz += st->sd_symsize;
                strsz += st->sd_strsize;
        }                               
}
#endif

/*
 * Temporary work buffers for dynamic loaded symbol tables.
 * Will go away when in-kernel linker is in place.
 */
#define	NSAVEDSYMS 512
#define	SZSYMNAMES NSAVEDSYMS*8		/* Just an approximation */
static Elf_Sym savedsyms[NSAVEDSYMS];
static int symnmoff[NSAVEDSYMS];
static char symnames[SZSYMNAMES];
static int cursyms, curnamep;

/*
 * Add a symbol to the temporary save area for symbols.
 * This routine will go away when the in-kernel linker is in place.
 */
static void
addsym(Elf_Sym *sym, char *name)
{
	int len;

#ifdef KSYMS_DEBUG
	if (ksyms_debug & FOLLOW_MORE_CALLS)
		printf("addsym: name %s val %lx\n", name, (long)sym->st_value);
#endif
	if (cursyms == NSAVEDSYMS || 
	    ((len = strlen(name)) + curnamep + 1) > SZSYMNAMES) {
		printf("addsym: too many sumbols, skipping '%s'\n", name);
		return;
	}
	strcpy(&symnames[curnamep], name);
	savedsyms[cursyms] = *sym;
	symnmoff[cursyms] = savedsyms[cursyms].st_name = curnamep;
	curnamep += (len + 1);
	cursyms++;
}
/*
 * Adds a symbol table.
 * "name" is the module name, "start" and "size" is where the symbol table
 * is located, and "type" is in which binary format the symbol table is.
 * New memory for keeping the symbol table is allocated in this function.
 * Returns 0 if success and EEXIST if the module name is in use.
 */
int
ksyms_addsymtab(char *mod, void *symstart, vsize_t symsize,
    char *strstart, vsize_t strsize)
{
	Elf_Sym *sym = symstart;
	struct symtab *st;
	long rval;
	int i;
	char *str;

#ifdef KSYMS_DEBUG
	if (ksyms_debug & FOLLOW_CALLS)
		printf("ksyms_addsymtab: mod %s symsize %lx strsize %lx\n",
		    mod, symsize, strsize);
#endif

#if NKSYMS
	/*
	 * Do not try to add a symbol table while someone is reading
	 * from /dev/ksyms.
	 */
	while (ksyms_isopen != 0)
		tsleep(&ksyms_isopen, PWAIT, "ksyms", 0);
#endif

	/* Check if this symtab already loaded */
	CIRCLEQ_FOREACH(st, &symtab_queue, sd_queue) {
		if (strcmp(mod, st->sd_name) == 0)
			return EEXIST;
	}

	/*
	 * XXX - Only add a symbol if it do not exist already.
	 * This is because of a flaw in the current LKM implementation,
	 * the loop will be removed once the in-kernel linker is in place.
	 */
	cursyms = curnamep = 0;
	for (i = 0; i < symsize/sizeof(Elf_Sym); i++) {
		if (sym[i].st_name == 0)
			continue; /* Just ignore */

		/* check validity of the symbol */
		/* XXX - save local symbols if DDB */
		if (ELF_ST_BIND(sym[i].st_info) != STB_GLOBAL)
			continue;
			
		/* Check if the symbol exists */
		if (ksyms_getval(NULL, strstart + sym[i].st_name,
		    &rval, KSYMS_EXTERN) == 0) {
			/* Check (and complain) about differing values */
			if (sym[i].st_value != rval) {
				printf("%s: symbol '%s' redeclared with "
				    "different value (%lx != %lx)\n",
				    mod, strstart + sym[i].st_name,
				    rval, (long)sym[i].st_value);
			}
		} else
			/* Ok, save this symbol */
			addsym(&sym[i], strstart + sym[i].st_name);
	}
	sym = malloc(sizeof(Elf_Sym)*cursyms, M_DEVBUF, M_WAITOK);
	str = malloc(curnamep, M_DEVBUF, M_WAITOK);
	memcpy(sym, savedsyms, sizeof(Elf_Sym)*cursyms);
	memcpy(str, symnames, curnamep);

	st = malloc(sizeof(struct symtab), M_DEVBUF, M_WAITOK);
	st->sd_name = malloc(strlen(mod)+1, M_DEVBUF, M_WAITOK);
	strcpy(st->sd_name, mod);
	st->sd_symnmoff = malloc(sizeof(int)*cursyms, M_DEVBUF, M_WAITOK);
	memcpy(st->sd_symnmoff, symnmoff, sizeof(int)*cursyms);
	st->sd_symstart = sym;
	st->sd_symsize = sizeof(Elf_Sym)*cursyms;
	st->sd_strstart = str;
	st->sd_strsize = curnamep;

	/* Make them absolute references */
	sym = st->sd_symstart;
	for (i = 0; i < st->sd_symsize/sizeof(Elf_Sym); i++)
		sym[i].st_shndx = SHN_ABS;

	CIRCLEQ_INSERT_TAIL(&symtab_queue, st, sd_queue);
#if NKSYMS
	ksyms_sizes_calc();
#endif
	return 0;
}

/*
 * Remove a symbol table specified by name.
 * Returns 0 if success, EBUSY if device open and ENOENT if no such name.
 */
int
ksyms_delsymtab(char *mod)
{
	struct symtab *st;
	int found = 0;

#if NKSYMS
	/*
	 * Do not try to delete a symbol table while someone is reading
	 * from /dev/ksyms.
	 */
	while (ksyms_isopen != 0)
		tsleep(&ksyms_isopen, PWAIT, "ksyms", 0);
#endif

	CIRCLEQ_FOREACH(st, &symtab_queue, sd_queue) {
		if (strcmp(mod, st->sd_name) == 0) {
			found = 1;
			break;
		}
	}
	if (found == 0)
		return ENOENT;
	CIRCLEQ_REMOVE(&symtab_queue, st, sd_queue);
	free(st->sd_symstart, M_DEVBUF);
	free(st->sd_strstart, M_DEVBUF);
	free(st->sd_symnmoff, M_DEVBUF);
	free(st->sd_name, M_DEVBUF);
	free(st, M_DEVBUF);
#if NKSYMS
	ksyms_sizes_calc();
#endif
	return 0;
}

#ifdef DDB

/*
 * Keep sifting stuff here, to avoid export of ksyms internals.
 */
int
ksyms_sift(char *mod, char *sym, int mode)
{
	struct symtab *st;
	char *sb;
	int i, sz;

	if (ksymsinited == 0)
		return ENOENT;

	CIRCLEQ_FOREACH(st, &symtab_queue, sd_queue) {
		if (mod && strcmp(mod, st->sd_name))
			continue;
		sb = st->sd_strstart;

		sz = st->sd_symsize/sizeof(Elf_Sym);
		for (i = 0; i < sz; i++) {
			Elf_Sym *les = st->sd_symstart + i;
			char c;

			if (strstr(sb + les->st_name, sym) == NULL)
				continue;

			if (mode == 'F') {
				switch (ELF_ST_TYPE(les->st_info)) {
				case STT_OBJECT:
					c = '+';
					break;
				case STT_FUNC:
					c = '*';
					break;
				case STT_SECTION:
					c = '&';
					break;
				case STT_FILE:
					c = '/';
					break;
				default:
					c = ' ';
					break;
				}
				db_printf("%s%c ", sb + les->st_name, c);
			} else
				db_printf("%s ", sb + les->st_name);
		}
	}
	return ENOENT;
}
#endif

#if NKSYMS

/*
 * Static allocated ELF header.
 * Basic info is filled in at attach, sizes at open.
 */
#define	SYMTAB		1
#define	STRTAB		2
#define	SHSTRTAB	3
#define NSECHDR		4

#define	NPRGHDR		2
#define	SHSTRSIZ	28

static struct ksyms_hdr {
	Elf_Ehdr	kh_ehdr;
	Elf_Phdr	kh_phdr[NPRGHDR];
	Elf_Shdr	kh_shdr[NSECHDR];
	char 		kh_strtab[SHSTRSIZ];
} ksyms_hdr;


void
ksyms_hdr_init(caddr_t hdraddr)
{

	/* Copy the loaded elf exec header */
	memcpy(&ksyms_hdr.kh_ehdr, hdraddr, sizeof(Elf_Ehdr));

	/* Set correct program/section header sizes, offsets and numbers */
	ksyms_hdr.kh_ehdr.e_phoff = offsetof(struct ksyms_hdr, kh_phdr[0]);
	ksyms_hdr.kh_ehdr.e_phentsize = sizeof(Elf_Phdr);
	ksyms_hdr.kh_ehdr.e_phnum = NPRGHDR;
	ksyms_hdr.kh_ehdr.e_shoff = offsetof(struct ksyms_hdr, kh_shdr[0]);
	ksyms_hdr.kh_ehdr.e_shentsize = sizeof(Elf_Shdr);
	ksyms_hdr.kh_ehdr.e_shnum = NSECHDR;
	ksyms_hdr.kh_ehdr.e_shstrndx = NSECHDR - 1; /* Last section */

	/*
	 * Keep program headers zeroed (unused).
	 * The section headers are hand-crafted.
	 * First section is section zero.
	 */

	/* Second section header; ".symtab" */
	ksyms_hdr.kh_shdr[SYMTAB].sh_name = 1; /* Section 3 offset */
	ksyms_hdr.kh_shdr[SYMTAB].sh_type = SHT_SYMTAB;
	ksyms_hdr.kh_shdr[SYMTAB].sh_offset = sizeof(struct ksyms_hdr);
/*	ksyms_hdr.kh_shdr[SYMTAB].sh_size = filled in at open */
	ksyms_hdr.kh_shdr[SYMTAB].sh_link = 2; /* Corresponding strtab */
	ksyms_hdr.kh_shdr[SYMTAB].sh_info = 0; /* XXX */
	ksyms_hdr.kh_shdr[SYMTAB].sh_addralign = sizeof(long);
	ksyms_hdr.kh_shdr[SYMTAB].sh_entsize = sizeof(Elf_Sym);

	/* Third section header; ".strtab" */
	ksyms_hdr.kh_shdr[STRTAB].sh_name = 9; /* Section 3 offset */
	ksyms_hdr.kh_shdr[STRTAB].sh_type = SHT_STRTAB;
/*	ksyms_hdr.kh_shdr[STRTAB].sh_offset = filled in at open */
/*	ksyms_hdr.kh_shdr[STRTAB].sh_size = filled in at open */
/*	ksyms_hdr.kh_shdr[STRTAB].sh_link = kept zero */
	ksyms_hdr.kh_shdr[STRTAB].sh_info = 0;
	ksyms_hdr.kh_shdr[STRTAB].sh_addralign = sizeof(char);
	ksyms_hdr.kh_shdr[STRTAB].sh_entsize = 0;

	/* Fourth section, ".shstrtab" */
	ksyms_hdr.kh_shdr[SHSTRTAB].sh_name = 17; /* This section name offset */
	ksyms_hdr.kh_shdr[SHSTRTAB].sh_type = SHT_STRTAB;
	ksyms_hdr.kh_shdr[SHSTRTAB].sh_offset =
	    offsetof(struct ksyms_hdr, kh_strtab);
	ksyms_hdr.kh_shdr[SHSTRTAB].sh_size = SHSTRSIZ;
	ksyms_hdr.kh_shdr[SHSTRTAB].sh_addralign = sizeof(char);

	/* Set section names */
	strcpy(&ksyms_hdr.kh_strtab[1], ".symtab");
	strcpy(&ksyms_hdr.kh_strtab[9], ".strtab");
	strcpy(&ksyms_hdr.kh_strtab[17], ".shstrtab");
};

int
ksymsopen(dev_t dev, int oflags, int devtype, struct proc *p)
{

	if (minor(dev))
		return ENXIO;

	ksyms_hdr.kh_shdr[SYMTAB].sh_size = symsz;
	ksyms_hdr.kh_shdr[STRTAB].sh_offset = symsz +
	    ksyms_hdr.kh_shdr[SYMTAB].sh_offset;
	ksyms_hdr.kh_shdr[STRTAB].sh_size = strsz;
	ksyms_isopen = 1;

#ifdef KSYMS_DEBUG
	if (ksyms_debug & FOLLOW_DEVKSYMS)
		printf("ksymsopen: symsz 0x%x strsz 0x%x\n", symsz, strsz);
#endif

	return 0;
}

int
ksymsclose(dev_t dev, int oflags, int devtype, struct proc *p)
{

#ifdef KSYMS_DEBUG
	if (ksyms_debug & FOLLOW_DEVKSYMS)
		printf("ksymsclose\n");
#endif

	ksyms_isopen = 0;
	wakeup(&ksyms_isopen);
	return 0;
}

#define	HDRSIZ	sizeof(struct ksyms_hdr)

int
ksymsread(dev_t dev, struct uio *uio, int ioflag)
{
	struct symtab *st;
	size_t filepos, inpos, off;

#ifdef KSYMS_DEBUG
	if (ksyms_debug & FOLLOW_DEVKSYMS)
		printf("ksymsread: offset 0x%llx resid 0x%lx\n",
		    (long long)uio->uio_offset, uio->uio_resid);
#endif
	if (ksymsinited == 0)
		return ENXIO;

	off = uio->uio_offset;
	if (off >= (strsz + symsz + HDRSIZ))
		return 0; /* End of symtab */
	/*
	 * First: Copy out the ELF header.
	 */
	if (off < HDRSIZ)
		uiomove((char *)&ksyms_hdr + off, HDRSIZ - off, uio);

	/*
	 * Copy out the symbol table.
	 */
	filepos = HDRSIZ;
	CIRCLEQ_FOREACH(st, &symtab_queue, sd_queue) {
		if (uio->uio_resid == 0)
			return 0;
		if (uio->uio_offset <= st->sd_symsize + filepos) {
			inpos = uio->uio_offset - filepos;
			uiomove((char *)st->sd_symstart + inpos,
			   st->sd_symsize - inpos, uio);
		}
		filepos += st->sd_symsize;
	}

	if (filepos != HDRSIZ + symsz)
		panic("ksymsread: unsunc");

	/*
	 * Copy out the string table
	 */
	CIRCLEQ_FOREACH(st, &symtab_queue, sd_queue) {
		if (uio->uio_resid == 0)
			return 0;
		if (uio->uio_offset <= st->sd_strsize + filepos) {
			inpos = uio->uio_offset - filepos;
			uiomove((char *)st->sd_strstart + inpos,
			   st->sd_strsize - inpos, uio);
		}
		filepos += st->sd_strsize;
	}
	return 0;
}

int
ksymswrite(dev_t dev, struct uio *uio, int ioflag)
{
	return EROFS;
}

int
ksymsioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc *p)
{
#ifdef notyet
	struct ksyms_gsymbol *kg = (struct ksyms_gsymbol *)data;
	struct symtab *st;
	Elf_Sym *sym;
	unsigned long val;
	int error = 0;

	switch (cmd) {
	case KIOCGVALUE:
		/*
		 * Use the in-kernel symbol lookup code for fast
		 * retreival of a value.
		 */
		if (error = copyinstr(kg->kg_name, symnm, maxsymnmsz, NULL))
			break;
		if (error = ksyms_getval(NULL, symnm, &val, KSYMS_EXTERN))
			break;
		error = copyout(&val, kg->kg_value, sizeof(long));
		break;

	case KIOCGSYMBOL:
		/*
		 * Use the in-kernel symbol lookup code for fast
		 * retreival of a symbol.
		 */
		if (error = copyinstr(kg->kg_name, symnm, maxsymnmsz, NULL))
			break;
		CIRCLEQ_FOREACH(st, &symtab_queue, sd_queue) {
			if ((sym = findsym(symnm, st)) == NULL)
				continue;

			/* Skip if bad binding */
			if (ELF_ST_BIND(sym->st_info) != STB_GLOBAL) {
				sym = NULL;
				continue;
			}
			break;
		}
		if (sym != NULL)
			error = copyout(sym, kg->kg_sym, sizeof(Elf_Sym));
		else
			error = ENOENT;
		break;

	case KIOCGSIZE:
		/*
		 * Get total size of symbol table.
		 */
		*(int *)data = strsz + symsz + HDRSIZ;
		break;

	default:
		error = ENOTTY;
		break;
	}
#endif
	return ENOTTY;
}
#endif
