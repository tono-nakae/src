/*	$NetBSD: reloc.c,v 1.55 2002/09/05 16:33:57 junyoung Exp $	 */

/*
 * Copyright 1996 John D. Polstra.
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
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
 *      This product includes software developed by John Polstra.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Dynamic linker for ELF.
 *
 * John Polstra <jdp@polstra.com>.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <dirent.h>

#include "debug.h"
#include "rtld.h"

#ifndef RTLD_INHIBIT_COPY_RELOCS
static int _rtld_do_copy_relocation __P((const Obj_Entry *, const Elf_Rela *,
    bool));

/*
 * XXX: These don't work for the alpha and i386; don't know about powerpc
 *	The alpha and the i386 avoid the problem by compiling everything PIC.
 *	These relocation are supposed to be writing the address of the
 *	function to be called on the bss.rel or bss.rela segment, but:
 *		- st_size == 0
 *		- on the i386 at least the call instruction is a direct call
 *		  not an indirect call.
 */
static int
_rtld_do_copy_relocation(dstobj, rela, dodebug)
	const Obj_Entry *dstobj;
	const Elf_Rela *rela;
	bool dodebug;
{
	void           *dstaddr = (void *)(dstobj->relocbase + rela->r_offset);
	const Elf_Sym  *dstsym = dstobj->symtab + ELF_R_SYM(rela->r_info);
	const char     *name = dstobj->strtab + dstsym->st_name;
	unsigned long   hash = _rtld_elf_hash(name);
	size_t          size = dstsym->st_size;
	const void     *srcaddr;
	const Elf_Sym  *srcsym = NULL;
	Obj_Entry      *srcobj;

	for (srcobj = dstobj->next; srcobj != NULL; srcobj = srcobj->next)
		if ((srcsym = _rtld_symlook_obj(name, hash, srcobj,
		    false)) != NULL)
			break;

	if (srcobj == NULL) {
		_rtld_error("Undefined symbol \"%s\" referenced from COPY"
		    " relocation in %s", name, dstobj->path);
		return (-1);
	}
	srcaddr = (const void *)(srcobj->relocbase + srcsym->st_value);
	(void)memcpy(dstaddr, srcaddr, size);
	rdbg(dodebug, ("COPY %s %s %s --> src=%p dst=%p *dst= %p size %ld",
	    dstobj->path, srcobj->path, name, (void *)srcaddr,
	    (void *)dstaddr, (void *)*(long *)dstaddr, (long)size));
	return (0);
}
#endif /* RTLD_INHIBIT_COPY_RELOCS */


/*
 * Process the special R_xxx_COPY relocations in the main program.  These
 * copy data from a shared object into a region in the main program's BSS
 * segment.
 *
 * Returns 0 on success, -1 on failure.
 */
int
_rtld_do_copy_relocations(dstobj, dodebug)
	const Obj_Entry *dstobj;
	bool dodebug;
{
#ifndef RTLD_INHIBIT_COPY_RELOCS

	/* COPY relocations are invalid elsewhere */
	assert(dstobj->mainprog);

	if (dstobj->rel != NULL) {
		const Elf_Rel  *rel;
		for (rel = dstobj->rel; rel < dstobj->rellim; ++rel) {
			if (ELF_R_TYPE(rel->r_info) == R_TYPE(COPY)) {
				Elf_Rela        ourrela;
				ourrela.r_info = rel->r_info;
				ourrela.r_offset = rel->r_offset;
				ourrela.r_addend = 0;
				if (_rtld_do_copy_relocation(dstobj,
				    &ourrela, dodebug) < 0)
					return (-1);
			}
		}
	}
	if (dstobj->rela != NULL) {
		const Elf_Rela *rela;
		for (rela = dstobj->rela; rela < dstobj->relalim; ++rela) {
			if (ELF_R_TYPE(rela->r_info) == R_TYPE(COPY)) {
				if (_rtld_do_copy_relocation(dstobj, rela,
				    dodebug) < 0)
					return (-1);
			}
		}
	}
#endif /* RTLD_INHIBIT_COPY_RELOCS */

	return (0);
}


#if !defined(__sparc__) && !defined(__x86_64__)

int
_rtld_relocate_nonplt_object(obj, rela, dodebug)
	Obj_Entry *obj;
	const Elf_Rela *rela;
	bool dodebug;
{
	Elf_Addr        *where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
	const Elf_Sym   *def;
	const Obj_Entry *defobj;
#if defined(__alpha__) || defined(__arm__) || defined(__hppa__) || \
    defined(__i386__) || defined(__m68k__) || defined(__powerpc__) || \
    defined(__sh__) || defined(__vax__)
	Elf_Addr         tmp;
#endif

	switch (ELF_R_TYPE(rela->r_info)) {

	case R_TYPE(NONE):
		break;

#if defined(__i386__)
	case R_TYPE(GOT32):

		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;

		tmp = (Elf_Addr)(defobj->relocbase + def->st_value);
		if (*where != tmp)
			*where = tmp;
		rdbg(dodebug, ("GOT32 %s in %s --> %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, defobj->path));
		break;

	case R_TYPE(PC32):
		/*
		 * I don't think the dynamic linker should ever see this
		 * type of relocation.  But the binutils-2.6 tools sometimes
		 * generate it.
		 */

		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;

		*where += (Elf_Addr)(defobj->relocbase + def->st_value) -
		    (Elf_Addr)where;
		rdbg(dodebug, ("PC32 %s in %s --> %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, defobj->path));
		break;

	case R_TYPE(32):
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;

		*where += (Elf_Addr)(defobj->relocbase + def->st_value);
		rdbg(dodebug, ("32 %s in %s --> %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, defobj->path));
		break;
#endif /* __i386__ */

#if defined(__m68k__)
	case R_TYPE(GOT32):
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;

		tmp = (Elf_Addr)(defobj->relocbase + def->st_value +
		    rela->r_addend);
		if (*where != tmp)
			*where = tmp;
		rdbg(dodebug, ("GOT32 %s in %s --> %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, defobj->path));
		break;

	case R_TYPE(PC32):
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;

		tmp = (Elf_Addr)(defobj->relocbase + def->st_value +
		    rela->r_addend) - (Elf_Addr)where;
		if (*where != tmp)
			*where = tmp;
		rdbg(dodebug, ("PC32 %s in %s --> %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, defobj->path));
		break;

	case R_TYPE(32):
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;

		tmp = (Elf_Addr)(defobj->relocbase + def->st_value +
		    rela->r_addend);
		if (*where != tmp)
			*where = tmp;
		rdbg(dodebug, ("32 %s in %s --> %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, defobj->path));
		break;
#endif /* __m68k__ */

#if defined(__sh__)
	case R_TYPE(GOT32):
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;

		tmp = (Elf_Addr)(defobj->relocbase + def->st_value +
		    rela->r_addend);
		if (*where != tmp) 
			*where = tmp;
		rdbg(dodebug, ("GOT32 %s in %s --> %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, defobj->path));
		break;

	case R_TYPE(REL32):  
		/*
		 * I don't think the dynamic linker should ever see this
		 * type of relocation, but some versions of Binutils
		 * generate it.
		 */
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;

		*where += (Elf_Addr)(defobj->relocbase + def->st_value +
		    rela->r_addend) - (Elf_Addr)where;
		rdbg(dodebug, ("PC32 %s in %s --> %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, defobj->path));
		break;

	case R_TYPE(DIR32):
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;

		*where += (Elf_Addr)(defobj->relocbase + def->st_value +
		    rela->r_addend);
		rdbg(dodebug, ("32 %s in %s --> %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, defobj->path));
		break;
#endif /* __sh__ */

#if defined(__alpha__)
	case R_TYPE(REFQUAD):
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;

		tmp = (Elf_Addr)(defobj->relocbase + def->st_value) +
		    *where + rela->r_addend;
		if (*where != tmp)
			*where = tmp;
		rdbg(dodebug, ("REFQUAD %s in %s --> %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, defobj->path));
		break;

	case R_TYPE(RELATIVE):
	    {
		extern Elf_Addr	_GLOBAL_OFFSET_TABLE_[];
		extern Elf_Addr	_GOT_END_[];

		/* This is the ...iffy hueristic. */
		if (!dodebug ||
		    (caddr_t)where < (caddr_t)_GLOBAL_OFFSET_TABLE_ ||
		    (caddr_t)where >= (caddr_t)_GOT_END_) {
			*where += (Elf_Addr)obj->relocbase;
			rdbg(dodebug, ("RELATIVE in %s --> %p", obj->path,
			    (void *)*where));
		} else
			rdbg(dodebug, ("RELATIVE in %s stays at %p",
			    obj->path, (void *)*where));
		break;
	    }
#endif /* __alpha__ */

#if defined(__alpha__) || defined(__i386__) || defined(__m68k__) || \
    defined(__sh__)
	case R_TYPE(GLOB_DAT):
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;

		tmp = (Elf_Addr)(defobj->relocbase + def->st_value) +
		    rela->r_addend;
		if (*where != tmp)
			*where = tmp;
		rdbg(dodebug, ("GLOB_DAT %s in %s --> %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, defobj->path));
		break;
#if defined(__sh__)
	case R_TYPE(RELATIVE):
		if (rela->r_addend)
			*where = (Elf_Addr)obj->relocbase + rela->r_addend;
		else
			*where += (Elf_Addr)obj->relocbase;
		rdbg(dodebug, ("RELATIVE in %s --> %p", obj->path,
		    (void *)*where));
		break;
#elif !defined(__alpha__)
	case R_TYPE(RELATIVE):
		*where += (Elf_Addr)obj->relocbase;
		rdbg(dodebug, ("RELATIVE in %s --> %p", obj->path,
		    (void *)*where));
		break;
#endif /* ! __alpha__ */
#endif /* __alpha__ || __i386__ || __m68k__ || __sh__ */

#if defined(__alpha__) || defined(__hppa__) || defined(__i386__) || \
    defined(__m68k__) || defined(__sh__)
	case R_TYPE(COPY):
		/*
		 * These are deferred until all other relocations have
		 * been done.  All we do here is make sure that the COPY
		 * relocation is not in a shared library.  They are allowed
		 * only in executable files.
		 */
		if (!obj->mainprog) {
			_rtld_error(
			"%s: Unexpected R_COPY relocation in shared library",
			    obj->path);
			return -1;
		}
		rdbg(dodebug, ("COPY (avoid in main)"));
		break;
#endif /* __alpha__ || __hppa__ || __i386__ || __m68k__ || __sh__ */

#if defined(__mips__)
	case R_TYPE(REL32):
		/* 32-bit PC-relative reference */
		def = obj->symtab + ELF_R_SYM(rela->r_info);

		if (ELFDEFNNAME(ST_BIND)(def->st_info) == STB_LOCAL &&
		  (ELFDEFNNAME(ST_TYPE)(def->st_info) == STT_SECTION ||
		   ELFDEFNNAME(ST_TYPE)(def->st_info) == STT_NOTYPE)) {
			/*
			 * XXX: ABI DIFFERENCE!
			 * 
			 * Old NetBSD binutils would generate shared libs
			 * with section-relative relocations being already
			 * adjusted for the start address of the section.
			 * 
			 * New binutils, OTOH, generate shared libs with
			 * the same relocations being based at zero, so we
			 * need to add in the start address of the section.  
			 * 
			 * We assume that all section-relative relocs with
			 * contents less than the start of the section need 
			 * to be adjusted; this should work with both old
			 * and new shlibs.
			 * 
			 * --rkb, Oct 6, 2001
			 */
			if (def->st_info == STT_SECTION && 
				    (*where < def->st_value))
			    *where += (Elf_Addr) def->st_value;

			*where += (Elf_Addr)obj->relocbase;

			rdbg(dodebug, ("REL32 %s in %s --> %p in %s",
			    obj->strtab + def->st_name, obj->path,
			    (void *)*where, obj->path));
		} else {
			/* XXX maybe do something re: bootstrapping? */
			def = _rtld_find_symdef(rela->r_info, obj, &defobj,
			    false);
			if (def == NULL)
				return -1;
			*where += (Elf_Addr)(defobj->relocbase + def->st_value);
			rdbg(dodebug, ("REL32 %s in %s --> %p in %s",
			    defobj->strtab + def->st_name, obj->path,
			    (void *)*where, defobj->path));
		}
		break;

#endif /* __mips__ */

#if defined(__powerpc__) || defined(__vax__)
	case R_TYPE(32):	/* word32 S + A */
	case R_TYPE(GLOB_DAT):	/* word32 S + A */
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;

		tmp = (Elf_Addr)(defobj->relocbase + def->st_value +
		    rela->r_addend);

		if (*where != tmp)
			*where = tmp;
		rdbg(dodebug, ("32/GLOB_DAT %s in %s --> %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, defobj->path));
		break;

	case R_TYPE(COPY):
		rdbg(dodebug, ("COPY"));
		break;

	case R_TYPE(JMP_SLOT):
		rdbg(dodebug, ("JMP_SLOT"));
		break;

	case R_TYPE(RELATIVE):	/* word32 B + A */
		tmp = (Elf_Addr)(obj->relocbase + rela->r_addend);
		if (*where != tmp)
			*where = tmp;
		rdbg(dodebug, ("RELATIVE in %s --> %p", obj->path,
		    (void *)*where));
		break;
#endif /* __powerpc__ || __vax__ */

#if defined(__arm__)
	case R_TYPE(GLOB_DAT):	/* word32 B + S */
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;
		*where = (Elf_Addr)(defobj->relocbase + def->st_value);
		rdbg(dodebug, ("GLOB_DAT %s in %s --> %p @ %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, where, defobj->path));
		break;

	case R_TYPE(COPY):
		rdbg(dodebug, ("COPY"));
		break;

	case R_TYPE(JUMP_SLOT):
		rdbg(dodebug, ("JUMP_SLOT"));
		break;

	case R_TYPE(ABS32):	/* word32 B + S + A */
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;
		*where += (Elf_Addr)defobj->relocbase + def->st_value;
		rdbg(dodebug, ("ABS32 %s in %s --> %p @ %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, where, defobj->path));
		break;

	case R_TYPE(RELATIVE):	/* word32 B + A */
		*where += (Elf_Addr)obj->relocbase;
		rdbg(dodebug, ("RELATIVE in %s --> %p @ %p", obj->path,
		    (void *)*where, where));
		break;

	case R_TYPE(PC24): {	/* word32 S - P + A */
		Elf32_Sword addend;

		/*
		 * Extract addend and sign-extend if needed.
		 */
		addend = *where;
		if (addend & 0x00800000)
			addend |= 0xff000000;

		def = _rtld_find_symdef(rela->r_info, obj, &defobj, false);
		if (def == NULL)
			return -1;
		tmp = (Elf_Addr)obj->relocbase + def->st_value
		    - (Elf_Addr)where + (addend << 2);
		if ((tmp & 0xfe000000) != 0xfe000000 &&
		    (tmp & 0xfe000000) != 0) {
			_rtld_error(
			"%s: R_ARM_PC24 relocation @ %p to %s failed "
			"(displacement %ld (%#lx) out of range)",
			    obj->path, where, defobj->strtab + def->st_name,
			    (long) tmp, (long) tmp);
			return -1;
		}
		tmp >>= 2;
		*where = (*where & 0xff000000) | (tmp & 0x00ffffff);
		rdbg(dodebug, ("PC24 %s in %s --> %p @ %p in %s",
		    defobj->strtab + def->st_name, obj->path,
		    (void *)*where, where, defobj->path));
		break;
	}
#endif /* __arm__ */

#ifdef __hppa__
	case R_TYPE(DIR32):
		if (ELF_R_SYM(rela->r_info)) {
			/*
			 * This is either a DIR32 against a symbol
			 * (def->st_name != 0), or against a local 
			 * section (def->st_name == 0).
			 */
			def = obj->symtab + ELF_R_SYM(rela->r_info);
			defobj = obj;
			if (def->st_name != 0)
				/*
		 		 * While we're relocating self, _rtld_objlist
				 * is NULL, so we just pass in self.
				 */
				def = _rtld_find_symdef(rela->r_info, obj,
				    &defobj, false);
			if (def == NULL)
				return -1;

			tmp = (Elf_Addr)(defobj->relocbase + def->st_value +
			    rela->r_addend);

			if (*where != tmp)
				*where = tmp;
			rdbg(dodebug, ("DIR32 %s in %s --> %p in %s",
			    defobj->strtab + def->st_name, obj->path,
			    (void *)*where, defobj->path));
		} else {
			extern Elf_Addr	_GLOBAL_OFFSET_TABLE_[];
			extern Elf_Addr	_GOT_END_[];

			tmp = (Elf_Addr)(obj->relocbase + rela->r_addend);

			/* This is the ...iffy hueristic. */
			if (!dodebug ||
			    (caddr_t)where < (caddr_t)_GLOBAL_OFFSET_TABLE_ ||
			    (caddr_t)where >= (caddr_t)_GOT_END_) {
				if (*where != tmp)
					*where = tmp;
				rdbg(dodebug, ("DIR32 in %s --> %p", obj->path,
				    (void *)*where));
			} else
				rdbg(dodebug, ("DIR32 in %s stays at %p",
				    obj->path, (void *)*where));
		}
		break;

	case R_TYPE(PLABEL32):
		if (ELF_R_SYM(rela->r_info)) {
			/*
	 		 * While we're relocating self, _rtld_objlist
			 * is NULL, so we just pass in self.
			 */
			def = _rtld_find_symdef(rela->r_info, obj, &defobj,
			    false);
			if (def == NULL)
				return -1;

			tmp = _rtld_function_descriptor_alloc(defobj, def, 
			    rela->r_addend);
			if (tmp == (Elf_Addr)-1)
				return -1;

			if (*where != tmp)
				*where = tmp;
			rdbg(dodebug, ("PLABEL32 %s in %s --> %p in %s",
			    defobj->strtab + def->st_name, obj->path,
			    (void *)*where, defobj->path));
		} else {
			/*
			 * This is a PLABEL for a static function, and the
			 * dynamic linker has both allocated a PLT entry
			 * for this function and told us where it is.  We
			 * can safely use the PLT entry as the PLABEL
			 * because there should be no other PLABEL reloc
			 * referencing this function.  This object should
			 * also have an IPLT relocation to initialize the
			 * PLT entry.
			 *
			 * The dynamic linker should also have ensured
			 * that the addend has the next-least-significant
			 * bit set; the $$dyncall millicode uses this to
			 * distinguish a PLABEL pointer from a plain
			 * function pointer.
			 */
			tmp = (Elf_Addr)(obj->relocbase + rela->r_addend);

			if (*where != tmp)
				*where = tmp;
			rdbg(dodebug, ("PLABEL32 in %s --> %p",
			    obj->path, (void *)*where));
		}
		break;
#endif /* __hppa__ */

	default:
		def = _rtld_find_symdef(rela->r_info, obj, &defobj, true);
		rdbg(dodebug, ("sym = %lu, type = %lu, offset = %p, "
		    "addend = %p, contents = %p, symbol = %s",
		    (u_long)ELF_R_SYM(rela->r_info),
		    (u_long)ELF_R_TYPE(rela->r_info),
		    (void *)rela->r_offset, (void *)rela->r_addend,
		    (void *)*where,
		    def ? defobj->strtab + def->st_name : "??"));
		_rtld_error("%s: Unsupported relocation type %ld "
		    "in non-PLT relocations\n",
		    obj->path, (u_long) ELF_R_TYPE(rela->r_info));
		return -1;
	}
	return 0;
}


#if !defined(__powerpc__) && !defined(__hppa__)

int
_rtld_relocate_plt_object(obj, rela, addrp, bind_now, dodebug)
	Obj_Entry *obj;
	const Elf_Rela *rela;
	caddr_t *addrp;
	bool bind_now;
	bool dodebug;
{
	Elf_Addr *where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
	Elf_Addr new_value;

	/* Fully resolve procedure addresses now */

#if defined(__alpha__) || defined(__arm__) || defined(__i386__) || \
    defined(__m68k__) || defined(__sh__) || defined(__vax__)
	if (bind_now || obj->pltgot == NULL) {
		const Elf_Sym  *def;
		const Obj_Entry *defobj;

#if !defined(__arm__)
		assert(ELF_R_TYPE(rela->r_info) == R_TYPE(JMP_SLOT));
#else
		assert(ELF_R_TYPE(rela->r_info) == R_TYPE(JUMP_SLOT));
#endif

		def = _rtld_find_symdef(rela->r_info, obj, &defobj, true);
		if (def == NULL)
			return -1;

		new_value = (Elf_Addr)(defobj->relocbase + def->st_value);
#if defined(__sh__) || defined(__vax__)
		new_value += rela->r_addend;
#endif
		rdbg(dodebug, ("bind now %d/fixup in %s --> old=%p new=%p",
		    (int)bind_now,
		    defobj->strtab + def->st_name,
		    (void *)*where, (void *)new_value));
	} else
#endif /* __alpha__ || __i386__ || __m68k__ || __sh__ || __vax__ */
	if (!obj->mainprog) {
		/* Just relocate the GOT slots pointing into the PLT */
		new_value = *where + (Elf_Addr)(obj->relocbase);
		rdbg(dodebug, ("fixup !main in %s --> %p", obj->path,
		    (void *)*where));
	} else {
		return 0;
	}
	/*
         * Since this page is probably copy-on-write, let's not write
         * it unless we really really have to.
         */
	if (*where != new_value)
		*where = new_value;
	if (addrp != NULL) {
		*addrp = *(caddr_t *)(obj->relocbase + rela->r_offset);
#if defined(__vax__)
		*addrp -= rela->r_addend;
#endif
	}
	return 0;
}

#endif /* __powerpc__  || __hppa__ */

#endif /* __sparc__ || __x86_64__ */

caddr_t
_rtld_bind(obj, reloff)
	Obj_Entry *obj;
	Elf_Word reloff;
{
	const Elf_Rela *rela;
	Elf_Rela        ourrela;
	caddr_t		addr;

	if (obj->pltrel != NULL) {
		const Elf_Rel *rel;

		rel = (const Elf_Rel *)((caddr_t) obj->pltrel + reloff);
		ourrela.r_info = rel->r_info;
		ourrela.r_offset = rel->r_offset;
		ourrela.r_addend = 0;
		rela = &ourrela;
	} else {
		rela = (const Elf_Rela *)((caddr_t) obj->pltrela + reloff);
#ifdef __sparc64__
		if (ELF_R_TYPE(obj->pltrela->r_info) == R_TYPE(JMP_SLOT)) {
			/*
			 * XXXX
			 *
			 * The first four PLT entries are reserved.  There
			 * is some disagreement whether they should have
			 * associated relocation entries.  Both the SPARC
			 * 32-bit and 64-bit ELF specifications say that
			 * they should have relocation entries, but the 
			 * 32-bit SPARC binutils do not generate them,
			 * and now the 64-bit SPARC binutils have stopped
			 * generating them too.
			 * 
			 * So, to provide binary compatibility, we will
			 * check the first entry, if it is reserved it
			 * should not be of the type JMP_SLOT.  If it
			 * is JMP_SLOT, then the 4 reserved entries were
			 * not generated and our index is 4 entries too far.
			 */
			rela -= 4;
		}
#endif
	}

	if (_rtld_relocate_plt_object(obj, rela, &addr, true, true) < 0)
		_rtld_die();

	return addr;
}

/*
 * Relocate newly-loaded shared objects.  The argument is a pointer to
 * the Obj_Entry for the first such object.  All objects from the first
 * to the end of the list of objects are relocated.  Returns 0 on success,
 * or -1 on failure.
 */
int
_rtld_relocate_objects(first, bind_now, dodebug)
	Obj_Entry *first;
	bool bind_now;
	bool dodebug;
{
	Obj_Entry *obj;
	int ok = 1;

	for (obj = first; obj != NULL; obj = obj->next) {
		if (obj->nbuckets == 0 || obj->nchains == 0 ||
		    obj->buckets == NULL || obj->symtab == NULL ||
		    obj->strtab == NULL) {
			_rtld_error("%s: Shared object has no run-time"
			    " symbol table", obj->path);
			return -1;
		}
		rdbg(dodebug, (" relocating %s (%ld/%ld rel/rela, "
		    "%ld/%ld plt rel/rela)",
		    obj->path,
		    (long)(obj->rellim - obj->rel),
		    (long)(obj->relalim - obj->rela),
		    (long)(obj->pltrellim - obj->pltrel),
		    (long)(obj->pltrelalim - obj->pltrela)));

		if (obj->textrel) {
			/*
			 * There are relocations to the write-protected text
			 * segment.
			 */
			if (mprotect(obj->mapbase, obj->textsize,
				PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
				_rtld_error("%s: Cannot write-enable text "
				    "segment: %s", obj->path, xstrerror(errno));
				return -1;
			}
		}
		if (obj->rel != NULL) {
			/* Process the non-PLT relocations. */
			const Elf_Rel  *rel;
			for (rel = obj->rel; rel < obj->rellim; ++rel) {
				Elf_Rela        ourrela;
				ourrela.r_info = rel->r_info;
				ourrela.r_offset = rel->r_offset;
#if defined(__mips__)
				/* rel->r_offset is not valid on mips? */
				if (ELF_R_TYPE(ourrela.r_info) == R_TYPE(NONE))
					ourrela.r_addend = 0;
				else
#endif
					ourrela.r_addend =
					    *(Elf_Word *)(obj->relocbase +
					    rel->r_offset);

				if (_rtld_relocate_nonplt_object(obj, &ourrela,
				    dodebug) < 0)
					ok = 0;
			}
		}
		if (obj->rela != NULL) {
			/* Process the non-PLT relocations. */
			const Elf_Rela *rela;
			for (rela = obj->rela; rela < obj->relalim; ++rela) {
				if (_rtld_relocate_nonplt_object(obj, rela,
				    dodebug) < 0)
					ok = 0;
			}
		}
		if (obj->textrel) {	/* Re-protected the text segment. */
			if (mprotect(obj->mapbase, obj->textsize,
				     PROT_READ | PROT_EXEC) == -1) {
				_rtld_error("%s: Cannot write-protect text "
				    "segment: %s", obj->path, xstrerror(errno));
				return -1;
			}
		}
		/* Process the PLT relocations. */
		if (obj->pltrel != NULL) {
			const Elf_Rel  *rel;
			for (rel = obj->pltrel; rel < obj->pltrellim; ++rel) {
				Elf_Rela        ourrela;
				ourrela.r_info = rel->r_info;
				ourrela.r_offset = rel->r_offset;
				ourrela.r_addend =
				    *(Elf_Word *)(obj->relocbase +
				    rel->r_offset);
				if (_rtld_relocate_plt_object(obj, &ourrela,
				    NULL, bind_now, dodebug) < 0)
					ok = 0;
			}
		}
		if (obj->pltrela != NULL) {
			const Elf_Rela *rela;
			for (rela = obj->pltrela; rela < obj->pltrelalim;
			    ++rela) {
#ifdef __sparc64__
				if (ELF_R_TYPE(rela->r_info) !=
					R_TYPE(JMP_SLOT)) {
					/*
					 * XXXX
					 *
					 * The first four PLT entries are
					 * reserved.  There is some
					 * disagreement whether they should
					 * have associated relocation
					 * entries.  Both the SPARC 32-bit
					 * and 64-bit ELF specifications say
					 * that they should have relocation
					 * entries, but the 32-bit SPARC
					 * binutils do not generate them,
					 * and now the 64-bit SPARC binutils
					 * have stopped generating them too.
					 *
					 * To provide binary compatibility, we
					 * will skip any entries that are not
					 * of type JMP_SLOT.  
					 */
					continue;
				}
#endif
				if (_rtld_relocate_plt_object(obj, rela,
				    NULL, bind_now, dodebug) < 0)
					ok = 0;
			}
		}
		if (!ok)
			return -1;


		/* Set some sanity-checking numbers in the Obj_Entry. */
		obj->magic = RTLD_MAGIC;
		obj->version = RTLD_VERSION;

		/* Fill in the dynamic linker entry points. */
		obj->dlopen = _rtld_dlopen;
		obj->dlsym = _rtld_dlsym;
		obj->dlerror = _rtld_dlerror;
		obj->dlclose = _rtld_dlclose;
		obj->dladdr = _rtld_dladdr;

		/* Set the special PLTGOT entries. */
		if (obj->pltgot != NULL)
			_rtld_setup_pltgot(obj);
	}

	return 0;
}
