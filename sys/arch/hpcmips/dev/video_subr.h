/*	$NetBSD: video_subr.h,v 1.2 2000/05/13 03:12:56 uch Exp $	*/

/*-
 * Copyright (c) 2000 UCHIYAMA Yasushi.  All rights reserved.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LEGAL_CLUT_INDEX(x)	((x) >= 0 && (x) <= 255)
#define RGB24(r, g, b)		((((r) << 16) & 0x00ff0000) |		\
				 (((g) << 8) & 0x0000ff00) |		\
				 (((b)) & 0x000000ff))

int	cmap_work_alloc __P((u_int8_t **, u_int8_t **, u_int8_t **,
			     u_int32_t **, int));
void	cmap_work_free __P((u_int8_t *, u_int8_t *, u_int8_t *,
			    u_int32_t *));
void	rgb24_compose __P((u_int32_t *, u_int8_t *, u_int8_t *, u_int8_t *,
			   int)); 
void	rgb24_decompose __P((u_int32_t *, u_int8_t *, u_int8_t *,
			     u_int8_t *, int));
