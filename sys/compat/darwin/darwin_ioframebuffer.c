/*	$NetBSD: darwin_ioframebuffer.c,v 1.5 2003/05/13 20:48:16 manu Exp $ */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: darwin_ioframebuffer.c,v 1.5 2003/05/13 20:48:16 manu Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/signal.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/device.h>

#include <compat/mach/mach_types.h>
#include <compat/mach/mach_message.h>
#include <compat/mach/mach_port.h>
#include <compat/mach/mach_errno.h>
#include <compat/mach/mach_iokit.h>

#include <compat/darwin/darwin_ioframebuffer.h>
#include <compat/darwin/darwin_iokit.h>

/* This is ugly, but we hope to see it going away quickly */

static char darwin_iofbconfig[] = "<dict ID=\"0\"><key>IOFBModes</key><array ID=\"1\"><dict ID=\"2\"><key>AID</key><integer size=\"32\" ID=\"3\">0xee</integer><key>DM</key><data ID=\"4\">AAACAAAAAYAAMgAAAAAAAgAQAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"5\">0x3e</integer></dict><dict ID=\"6\"><key>AID</key><integer size=\"32\" ID=\"7\">0x82</integer><key>DM</key><data ID=\"8\">AAACAAAAAYAAPAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"9\">0x3</integer></dict><dict ID=\"10\"><key>AID</key><integer size=\"32\" ID=\"11\">0xe8</integer><key>DM</key><data ID=\"12\">AAACAAAAAYAAPAAAAAAAAgAQAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"13\">0x37</integer></dict><dict ID=\"14\"><key>DM</key><data ID=\"15\">AAACAAAAAYAARgAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"16\">0x5</integer></dict><dict ID=\"17\"><key>AID</key><reference IDREF=\"11\"/><key>DM</key><data ID=\"18\">AAACgAAAAeAAPAAAAAAAAgAQAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"19\">0x38</integer></dict><dict ID=\"20\"><key>AID</key><reference IDREF=\"3\"/><key>DM</key><data ID=\"21\">AAACgAAAAeAAMgAAAAAAAgAQAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"22\">0x3f</integer></dict><dict ID=\"23\"><key>AID</key><integer size=\"32\" ID=\"24\">0x96</integer><key>DM</key><data ID=\"25\">AAACgAAAAeAAPAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"26\">0x6</integer></dict><dict ID=\"27\"><key>AID</key><integer size=\"32\" ID=\"28\">0x8c</integer><key>DM</key><data ID=\"29\">AAACgAAAAeAAQwAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"30\">0x7</integer></dict><dict ID=\"31\"><key>AID</key><integer size=\"32\" ID=\"32\">0x98</integer><key>DM</key><data ID=\"33\">AAACgAAAAeAASAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"34\">0x8</integer></dict><dict ID=\"35\"><key>AID</key><integer size=\"32\" ID=\"36\">0x9a</integer><key>DM</key><data ID=\"37\">AAACgAAAAeAASwAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"38\">0x9</integer></dict><dict ID=\"39\"><key>AID</key><integer size=\"32\" ID=\"40\">0x9e</integer><key>DM</key><data ID=\"41\">AAACgAAAAeAAVQAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"42\">0xa</integer></dict><dict ID=\"43\"><key>AID</key><integer size=\"32\" ID=\"44\">0xa0</integer><key>DM</key><data ID=\"45\">AAACgAAAA2YASwAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"46\">0xe</integer></dict><dict ID=\"47\"><key>AID</key><reference IDREF=\"3\"/><key>DM</key><data ID=\"48\">AAAC0AAAAeAAMgAAAAAAAgAQAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"49\">0x4b</integer></dict><dict ID=\"50\"><key>AID</key><reference IDREF=\"11\"/><key>DM</key><data ID=\"51\">AAAC0AAAAeAAPAAAAAAAAgAQAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"52\">0x4d</integer></dict><dict ID=\"53\"><key>AID</key><reference IDREF=\"3\"/><key>DM</key><data ID=\"54\">AAAC0AAAAkAAMgAAAAAAAgAQAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"55\">0x4c</integer></dict><dict ID=\"56\"><key>AID</key><reference IDREF=\"11\"/><key>DM</key><data ID=\"57\">AAAC0AAAAkAAPAAAAAAAAgAQAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"58\">0x4e</integer></dict><dict ID=\"59\"><key>AID</key><reference IDREF=\"3\"/><key>DM</key><data ID=\"60\">AAADIAAAAlgAMgAAAAAAAgAQAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"61\">0x40</integer></dict><dict ID=\"62\"><key>AID</key><integer size=\"32\" ID=\"63\">0xb4</integer><key>DM</key><data ID=\"64\">AAADIAAAAlgAOAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"65\">0xf</integer></dict><dict ID=\"66\"><key>AID</key><integer size=\"32\" ID=\"67\">0xb6</integer><key>DM</key><data ID=\"68\">AAADIAAAAlgAPAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"69\">0x10</integer></dict><dict ID=\"70\"><key>AID</key><reference IDREF=\"11\"/><key>DM</key><data ID=\"71\">AAADIAAAAlgAPAAAAAAAAgAQAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"72\">0x39</integer></dict><dict ID=\"73\"><key>AID</key><integer size=\"32\" ID=\"74\">0x2a</integer><key>ID</key><integer size=\"32\" ID=\"75\">0x2d</integer><key>DF</key><integer size=\"32\" ID=\"76\">0x400</integer><key>DM</key><data ID=\"77\">AAADIAAAAlgAAAAAAAAAAgAABAAAAAAAAAAAAAAAAAAAAAAA</data></dict><dict ID=\"78\"><key>DF</key><integer size=\"32\" ID=\"79\">0x100</integer><key>DM</key><data ID=\"80\">AAADIAAAAlgAPAAAAAAAAgAAAQAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"81\">0x32</integer></dict><dict ID=\"82\"><key>AID</key><integer size=\"32\" ID=\"83\">0xb8</integer><key>DM</key><data ID=\"84\">AAADIAAAAlgASAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"85\">0x11</integer></dict><dict ID=\"86\"><key>AID</key><integer size=\"32\" ID=\"87\">0xba</integer><key>DM</key><data ID=\"88\">AAADIAAAAlgASwAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"89\">0x12</integer></dict><dict ID=\"90\"><key>AID</key><reference IDREF=\"74\"/><key>ID</key><integer size=\"32\" ID=\"91\">0x46</integer><key>DF</key><reference IDREF=\"76\"/><key>DM</key><reference IDREF=\"77\"/></dict><dict ID=\"92\"><key>AID</key><integer size=\"32\" ID=\"93\">0xbc</integer><key>DM</key><data ID=\"94\">AAADIAAAAlgAVQAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"95\">0x13</integer></dict><dict ID=\"96\"><key>AID</key><reference IDREF=\"3\"/><key>DM</key><data ID=\"97\">AAADQAAAAnAAMgAAAAAAAgAQAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"98\">0x41</integer></dict><dict ID=\"99\"><key>AID</key><reference IDREF=\"11\"/><key>DM</key><data ID=\"100\">AAADQAAAAnAAPAAAAAAAAgAQAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"101\">0x3a</integer></dict><dict ID=\"102\"><key>AID</key><integer size=\"32\" ID=\"103\">0xaa</integer><key>DM</key><data ID=\"104\">AAADQAAAAnAASwAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"105\">0x17</integer></dict><dict ID=\"106\"><key>DM</key><data ID=\"107\">AAADVAAAAoAAPAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"108\">0x18</integer></dict><dict ID=\"109\"><key>AID</key><integer size=\"32\" ID=\"110\">0xbe</integer><key>DM</key><data ID=\"111\">AAAEAAAAAwAAPAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"112\">0x19</integer></dict><dict ID=\"113\"><key>AID</key><reference IDREF=\"74\"/><key>ID</key><integer size=\"32\" ID=\"114\">0x2e</integer><key>DF</key><integer size=\"32\" ID=\"115\">0x407</integer><key>DM</key><data ID=\"116\">AAAEAAAAAwAAAAAAAAAAAgAABAcAAAAAAAAAAAAAAAAAAAAA</data></dict><dict ID=\"117\"><key>AID</key><reference IDREF=\"110\"/><key>ID</key><integer size=\"32\" ID=\"118\">0x33</integer><key>DF</key><reference IDREF=\"79\"/><key>DM</key><data ID=\"119\">AAAEAAAAAwAAPAAAAAAAAgAAAQAAAAAAAAAAAAAAAAAAAAAA</data></dict><dict ID=\"120\"><key>AID</key><integer size=\"32\" ID=\"121\">0xc8</integer><key>DM</key><data ID=\"122\">AAAEAAAAAwAARgAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"123\">0x1a</integer></dict><dict ID=\"124\"><key>AID</key><integer size=\"32\" ID=\"125\">0xcc</integer><key>DM</key><data ID=\"126\">AAAEAAAAAwAASwAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"127\">0x1b</integer></dict><dict ID=\"128\"><key>AID</key><integer size=\"32\" ID=\"129\">0xd2</integer><key>DM</key><reference IDREF=\"126\"/><key>ID</key><integer size=\"32\" ID=\"130\">0x1c</integer></dict><dict ID=\"131\"><key>AID</key><integer size=\"32\" ID=\"132\">0xd0</integer><key>DM</key><data ID=\"133\">AAAEAAAAAwAAVQAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"134\">0x1d</integer></dict><dict ID=\"135\"><key>AID</key><integer size=\"32\" ID=\"136\">0xdc</integer><key>DM</key><data ID=\"137\">AAAEgAAAA2YASwAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"138\">0x21</integer></dict><dict ID=\"139\"><key>AID</key><integer size=\"32\" ID=\"140\">0xfa</integer><key>DM</key><data ID=\"141\">AAAFAAAAA8AASwAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"142\">0x22</integer></dict><dict ID=\"143\"><key>AID</key><integer size=\"32\" ID=\"144\">0x104</integer><key>DM</key><data ID=\"145\">AAAFAAAABAAAPAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"146\">0x23</integer></dict><dict ID=\"147\"><key>AID</key><integer size=\"32\" ID=\"148\">0x106</integer><key>DM</key><data ID=\"149\">AAAFAAAABAAASwAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAA</data><key>ID</key><integer size=\"32\" ID=\"150\">0x24</integer></dict></array><key>IODisplayConnectFlags</key><integer size=\"32\" ID=\"151\">0x0</integer></dict>";

static char darwin_ioframebuffer_properties[] = "<dict ID=\"0\"><key>IOClass</key><string ID=\"1\">AppleBacklightDisplay</string><key>IOProbeScore</key><integer size=\"32\" ID=\"2\">0xbb8</integer><key>IOProviderClass</key><string ID=\"3\">IODisplayConnect</string><key>CFBundleIdentifier</key><string ID=\"4\">com.apple.iokit.IOGraphicsFamily</string><key>IOMatchCategory</key><string ID=\"5\">IODefaultMatchCategory</string><key>IODisplayConnectFlags</key><data ID=\"6\">AAAIxA==</data><key>AppleDisplayType</key><integer size=\"32\" ID=\"7\">0x2</integer><key>AppleSense</key><integer size=\"32\" ID=\"8\">0x400</integer><key>DisplayVendorID</key><integer size=\"32\" ID=\"9\">0x756e6b6e</integer><key>DisplayProductID</key><integer size=\"32\" ID=\"10\">0x20000</integer><key>IODisplayParameters</key><dict ID=\"11\"><key>brightness</key><dict ID=\"12\"><key>max</key><integer size=\"32\" ID=\"13\">0x73</integer><key>min</key><integer size=\"32\" ID=\"14\">0x3a</integer><key>value</key><integer size=\"32\" ID=\"15\">0x7f</integer></dict><key>commit</key><dict ID=\"16\"><key>reg</key><integer size=\"32\" ID=\"17\">0x0</integer></dict></dict><key>IODisplayGUID</key><integer size=\"64\" ID=\"18\">0x610000000000000</integer><key>Power Management protected data</key><string ID=\"19\">{ theNumberOfPowerStates = 4, version 1, power state 0 = { capabilityFlags 00000000, outputPowerCharacter 00000000, inputPowerRequirement 00000000, staticPower 0, unbudgetedPower 0, powerToAttain 0, timeToAttain 0, settleUpTime 0, timeToLower 0, settleDownTime 0, powerDomainBudget 0 }, power state 1 = { capabilityFlags 00000000, outputPowerCharacter 00000000, inputPowerRequirement 00000000, staticPower 0, unbudgetedPower 0, powerToAttain 0, timeToAttain 0, settleUpTime 0, timeToLower 0, settleDownTime 0, powerDomainBudget 0 }, power state 2 = { capabilityFlags 00008000, outputPowerCharacter 00000000, inputPowerRequirement 00000002, staticPower 0, unbudgetedPower 0, powerToAttain 0, timeToAttain 0, settleUpTime 0, timeToLower 0, settleDownTime 0, powerDomainBudget 0 }, power state 3 = { capabilityFlags 0000c000, outputPowerCharacter 00000000, inputPowerRequirement 00000002, staticPower 0, unbudgetedPower 0, powerToAttain 0, timeToAttain 0, settleUpTime 0, timeToLower 0, settleDownTime 0, powerDomainBudget 0 }, aggressiveness = 0, myCurrentState = 0, parentsCurrentPowerFlags = 00000002, maxCapability = 3 }</string><key>Power Management private data</key><string ID=\"20\">{ this object = 0114c800, interested driver = 0114c800, driverDesire = 0, deviceDesire = 0, ourDesiredPowerState = 0, previousReqest = 0 }</string></dict>";


struct mach_iokit_property darwin_ioframebuffer_properties_array[] = {
	{ "IOFBDependentID", NULL },
	{ "IOFBDependentIndex", NULL },
	{ "graphic-options", "<integer size=\"32\" ID=\"0\">0x0</integer>"},
	{ "IOFBConfig", darwin_iofbconfig },
	{ "IOFBMemorySize", 
	    "<integer size=\"32\" ID=\"0\">0x1000000</integer>"},
	{ NULL, 0}
};

struct mach_iokit_devclass darwin_ioframebuffer_devclass = {
	"<dict ID=\"0\"><key>IOProviderClass</key>"
	    "<string ID=\"1\">IOFramebuffer</string></dict>",
	darwin_ioframebuffer_properties,
	darwin_ioframebuffer_properties_array,
	darwin_ioframebuffer_connect_method_scalari_scalaro,
	NULL,
	"IOFramebuffer",
};


int
darwin_ioframebuffer_connect_method_scalari_scalaro(args)
	struct mach_trap_args *args;
{
	mach_io_connect_method_scalari_scalaro_request_t *req = args->smsg;
	mach_io_connect_method_scalari_scalaro_reply_t *rep = args->rmsg;
	size_t *msglen = args->rsize;

#ifdef DEBUG_DARWIN
	printf("darwin_ioframebuffer_connect_method_scalari_scalaro()\n");
#endif
	rep->rep_msgh.msgh_bits =
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE);
	rep->rep_msgh.msgh_size = sizeof(*rep) - sizeof(rep->rep_trailer);
	rep->rep_msgh.msgh_local_port = req->req_msgh.msgh_local_port;
	rep->rep_msgh.msgh_id = req->req_msgh.msgh_id + 100;
	rep->rep_outcount = 1;
	rep->rep_out[0] = 1;
	rep->rep_out[rep->rep_outcount + 1] = 8; /* XXX Trailer */

	/*
	 * XXX this is ugly, but we don't really know what we are doing here.
	 */
	rep->rep_outcount = 1;
	rep->rep_out[0] = 1;
	if (req->req_in[0] == 0x00000003) {
		rep->rep_outcount = 0;
	}
	if (req->req_in[0] == 0x6d726466) {
		rep->rep_outcount = 1;
		rep->rep_out[0] = 0;
	}
	rep->rep_out[rep->rep_outcount + 1] = 8; /* XXX Trailer */

	*msglen = sizeof(*rep) - ((4096 + rep->rep_outcount) * sizeof(int));
	return 0;
}
