# $NetBSD: _mipseb.mk,v 1.1 1998/07/27 01:03:48 tv Exp $

BFD_MACHINES =	cpu-mips.c
BFD_BACKENDS =	elf32-mips.c elf32.c elf.c elflink.c ecofflink.c \
		 coff-mips.c ecoff.c

ARCH_DEFS = -DARCH_mips \
 -DSELECT_ARCHITECTURES='&bfd_mips_arch'

TDEFAULTS = \
 -DDEFAULT_VECTOR=bfd_elf32_bigmips_vec \
 -DSELECT_VECS=' &bfd_elf32_littlemips_vec, &bfd_elf32_bigmips_vec, &ecoff_little_vec, &ecoff_big_vec ' \
 -DHAVE_bfd_elf32_littlemips_vec \
 -DHAVE_bfd_elf32_bigmips_vec \
 -DHAVE_ecoff_little_vec \
 -DHAVE_ecoff_big_vec \
 -DNETBSD_CORE

# XXX cannot support 64-bit BFD targets with gdb 4.16.
# They assume that  BFD_ARCH_SIZE is 64, but that causes bfd_vma_addr 
# to be a 64-bit int. GDB uses bfd_vma_addr for CORE_ADDR, but also
# casts CORE_ADDRS to ints, which loses on 32-bit mips hosts.
# TDEFAULTS += \
# -DHAVE_bfd_elf64_bigmips_vec \
# -DHAVE_bfd_elf64_littlemips_vec \
# _DSELECT VECS =  &bfd_elf64_bigmips_vec, &bfd_elf64_littlemips_vec, 
#
# BFD_BACKENDS += \
#	elf64-mips.c elf64.c

OPCODE_MACHINES =  mips-dis.c mips-opc.c mips16-opc.c
