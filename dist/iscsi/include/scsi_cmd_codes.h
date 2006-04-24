/* $NetBSD: scsi_cmd_codes.h,v 1.5 2006/04/24 21:59:03 agc Exp $ */

/*
 * Copyright � 2006 Alistair Crooks.  All rights reserved.
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
 *	This product includes software developed by Alistair Crooks
 *	for the NetBSD project.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef SCSI_CMD_CODES_H_
#define SCSI_CMD_CODES_H_

/* information taken from SPC3, T10/1416-D Revision 23, from www.t10.org */

enum {
	TEST_UNIT_READY = 0x00,
	WRITE_6 = 0x06,
	READ_6 = 0x08,
	INQUIRY = 0x12,
	MODE_SENSE_6 = 0x1a,
	STOP_START_UNIT = 0x1b,
	READ_CAPACITY = 0x25,
	READ_10 = 0x28,
	WRITE_10 = 0x2a,
	VERIFY = 0x2f,
	SYNC_CACHE = 0x35,
	LOG_SENSE = 0x4d,
	MODE_SENSE_10 = 0x5a,
	PERSISTENT_RESERVE_IN = 0x5e,
	REPORT_LUNS = 0xa0
};

#define SIX_BYTE_COMMAND(op)	((op) <= 0x1f)
#define TEN_BYTE_COMMAND(op)	((op) > 0x1f && (op) <= 0x5f)

#define ISCSI_MODE_SENSE_LEN	11

/* miscellaneous definitions */
enum {
	DISK_PERIPHERAL_DEVICE = 0x0,

	INQUIRY_EVPD_BIT = 0x01,

	INQUIRY_DEVICE_IDENTIFICATION_VPD = 0x83,
	INQUIRY_SUPPORTED_VPD_PAGES = 0x0,
		INQUIRY_DEVICE_PIV = 0x1,
		INQUIRY_DEVICE_ASSOCIATION_TARGET_PORT = 0x1,
		INQUIRY_DEVICE_ASSOCIATION_TARGET_DEVICE = 0x2,
		INQUIRY_DEVICE_CODESET_UTF8 = 0x3,
		INQUIRY_DEVICE_ISCSI_PROTOCOL = 0x5,
		INQUIRY_DEVICE_T10_VENDOR = 0x1,
		INQUIRY_DEVICE_IDENTIFIER_SCSI_NAME = 0x8,

	WIDE_BUS_16 = 0x20,
	WIDE_BUS_32 = 0x40,

	SCSI_VERSION_SPC = 0x03,
	SCSI_VERSION_SPC2 = 0x04,
	SCSI_VERSION_SPC3 = 0x05
};

#endif /* !SCSI_CMD_CODES_H_ */
