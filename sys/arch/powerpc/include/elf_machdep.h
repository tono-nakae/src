/*	$NetBSD: elf_machdep.h,v 1.4 2000/04/02 15:35:50 minoura Exp $	*/

#define	ELF32_MACHDEP_ENDIANNESS	ELFDATA2MSB
#define	ELF32_MACHDEP_ID_CASES						\
		case EM_PPC:						\
			break;

#define	ELF64_MACHDEP_ENDIANNESS	XXX	/* break compilation */
#define	ELF64_MACHDEP_ID_CASES						\
		/* no 64-bit ELF machine types supported */

#define ARCH_ELFSIZE		32	/* MD native binary size */

#include <machine/reloc.h>		/* XXX */

#define R_TYPE(name) __CONCAT(RELOC_,name)
