/*	$NetBSD: usbcdc.h,v 1.1 1999/01/01 07:43:13 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
 * Carlstedt Research & Technology.
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

#ifndef _USBCDC_H_
#define _USBCDC_H_

#define UDESCSUB_CDC_HEADER	0
#define UDESCSUB_CDC_CM		1 /* Call Management */
#define UDESCSUB_CDC_ACM	2 /* Abstract Control Model */
#define UDESCSUB_CDC_DLM	3 /* Direct Line Management */
#define UDESCSUB_CDC_TRF	4 /* Telephone Ringer */
#define UDESCSUB_CDC_TCLSR	5 /* Telephone Call ... */
#define UDESCSUB_CDC_UNION	6
#define UDESCSUB_CDC_CS		7 /* Country Selection */
#define UDESCSUB_CDC_TOM	8 /* Telephone Operational Modes */
#define UDESCSUB_CDC_USBT	9 /* USB Terminal */

struct usb_cdc_header_descriptor {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uWord		bcdCDC;
};

struct usb_cdc_cm_descriptor {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bmCapabilities;
#define USB_CDC_CM_DOES_CM		0x01
#define USB_CDC_CM_CM_OVER_DATA		0x02
	uByte		bDataInterface;
};

struct usb_cdc_acm_descriptor {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bmCapabilities;
#define USB_CDC_ACM_HAS_FEATURE		0x01
#define USB_CDC_ACM_HAS_LINE		0x02
#define USB_CDC_ACM_HAS_BREAK		0x04
#define USB_CDC_ACM_HAS_NETWORK_CONN	0x08
};

struct usb_cdc_union_descriptor {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bMasterInterface;
	uByte		bSlaveInterface[1];
};

#define UCDC_SEND_ENCAPSULATED_COMMAND	0x00
#define UCDC_GET_ENCAPSULATED_RESPONSE	0x01
#define UCDC_SET_COMM_FEATURE		0x02
#define UCDC_GET_COMM_FEATURE		0x03
#define  UCDC_ABSTRACT_STATE		0x01
#define  UCDC_COUNTRY_SETTING		0x02
#define UCDC_CLEAR_COMM_FEATURE		0x04
#define UCDC_SET_LINE_CODING		0x20
#define UCDC_GET_LINE_CODING		0x21

#endif /* _USBCDC_H_ */
