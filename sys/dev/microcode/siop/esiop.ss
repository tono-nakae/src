;	$NetBSD: esiop.ss,v 1.1 2002/04/21 22:52:06 bouyer Exp $

;
; Copyright (c) 2002 Manuel Bouyer.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
; 1. Redistributions of source code must retain the above copyright
;    notice, this list of conditions and the following disclaimer.
; 2. Redistributions in binary form must reproduce the above copyright
;    notice, this list of conditions and the following disclaimer in the
;    documentation and/or other materials provided with the distribution.
; 3. All advertising materials mentioning features or use of this software
;    must display the following acknowledgement:
;	This product includes software developed by the University of
;	California, Berkeley and its contributors.
; 4. Neither the name of the University nor the names of its contributors
;    may be used to endorse or promote products derived from this software
;    without specific prior written permission.
;
; THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
; ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
; FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
; OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
; HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
; LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
; OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
; SUCH DAMAGE.
;

ARCH 825

; offsets in sym_xfer
ABSOLUTE t_id = 24;
ABSOLUTE t_msg_in = 32;
ABSOLUTE t_ext_msg_in = 40;
ABSOLUTE t_ext_msg_data = 48;
ABSOLUTE t_msg_out = 56;
ABSOLUTE t_cmd = 64;
ABSOLUTE t_status = 72;
ABSOLUTE t_data = 80;

; offsets in the per-target lun table
ABSOLUTE target_id = 0x0;
ABSOLUTE target_luntbl = 0x8;

;; interrupt codes
; interrupts that needs a valid target/lun/tag
ABSOLUTE int_done	= 0xff00;
ABSOLUTE int_msgin	= 0xff01;
ABSOLUTE int_extmsgin	= 0xff02;
ABSOLUTE int_extmsgdata	= 0xff03;
ABSOLUTE int_disc	= 0xff04;
; interrupts that don't have a valid I/T/Q
ABSOLUTE int_resfail	= 0xff80;     
ABSOLUTE int_err	= 0xffff;     

; We use the various scratch[a-j] registers to keep internal status:

; scratchA1: offset in data DSA (for save data pointer)
; scratchB: save/restore DSA in data loop
; scratchC: current target/lun/tag
; scratchC0: flags
ABSOLUTE f_c_target	= 0x01 ; target valid
ABSOLUTE f_c_lun	= 0x02 ; lun valud
ABSOLUTE f_c_tag	= 0x04 ; tag valid
ABSOLUTE f_c_data	= 0x08 ; data I/O in progress
ABSOLUTE f_c_data_mask	= 0xf7 ; ~f_c_data
ABSOLUTE f_c_sdp	= 0x10 ; got save data pointer message
; scratchC[1-3]: target/lun/tag

; scratchD: current DSA in start cmd ring
; scratchE0: index in start cmd ring
ABSOLUTE ncmd_slots	= 64 ; number of slots in CMD ring
; flags in a cmd slot
ABSOLUTE f_cmd_free	= 0x01 ; this slot is free
; offsets in a cmd slot
ABSOLUTE o_cmd_dsa	= 0; also holds f_cmd_free
ABSOLUTE o_cmd_id	= 4;

; SCRATCHE1: last status

ENTRY reselect;
ENTRY ring_reset;
ENTRY cmdr0;
ENTRY cmdr1;
ENTRY cmdr2;
ENTRY cmdr3;
ENTRY led_on1;
ENTRY led_on2;
ENTRY led_off;
ENTRY status;
ENTRY msgin;
ENTRY msgin_ack;
ENTRY get_extmsgdata;
ENTRY send_msgout;
ENTRY script_sched;
ENTRY load_targtable;

EXTERN tlq_offset;
EXTERN abs_msgin2;

PROC  esiop_script:

no_cmd:
	MOVE ISTAT & 0x10 TO SFBR;  pending done command ?
	INTFLY 0, IF NOT 0x00; 
reselect:
	MOVE 0x00 TO SCRATCHA1;
	MOVE 0x00 TO SCRATCHC0;
	MOVE 0xff TO SCRATCHE1;
; a NOP by default; patched with MOVE GPREG | 0x01 to GPREG on compile-time
; option "SIOP_SYMLED"
led_off:
	NOP;
	WAIT RESELECT REL(reselect_fail);
; a NOP by default; patched with MOVE GPREG & 0xfe to GPREG on compile-time
; option "SIOP_SYMLED"
led_on2:
        NOP;
	MOVE SSID & 0x0f to SFBR;
	MOVE SFBR to SCRATCHC1;
	MOVE SCRATCHC0 | f_c_target to SCRATCHC0; save target
	CLEAR CARRY;
	MOVE SCRATCHC1 SHL SFBR;
	MOVE SFBR SHL DSA0; target * 4
	MOVE 0x0 to DSA1;
	MOVE 0x0 to DSA2;
	MOVE 0x0 to DSA3;
; load DSA for the target table
load_targtable:
	MOVE DSA0 + 0x00 to DSA0;
	MOVE DSA1 + 0x00 to DSA1 with carry;
	MOVE DSA2 + 0x00 to DSA2 with carry;
	MOVE DSA3 + 0x00 to DSA3 with carry;
	LOAD DSA0, 4, FROM 0; now load DSA for this target
	SELECT FROM target_id, REL(nextisn);
nextisn:
	MOVE 1, abs_msgin2, WHEN MSG_IN;
	MOVE SFBR & 0x07 to SCRATCHC2;
	MOVE SCRATCHC0 | f_c_lun to SCRATCHC0; save LUN
	CLEAR ACK and CARRY;
	MOVE SCRATCHC2 SHL SFBR; 
	MOVE SFBR SHL SFBR; target * 4
	MOVE DSA0 + SFBR TO DSA0;
	MOVE DSA1 + 0x0 TO DSA1 with carry;
	MOVE DSA2 + 0x0 TO DSA2 with carry;
	MOVE DSA3 + 0x0 TO DSA3 with carry;
	LOAD DSA0, 4, from target_luntbl; load DSA for ths LUN
	JUMP REL(waitphase);

reselect_fail:
	; check that host asserted SIGP, this'll clear SIGP in ISTAT
	MOVE CTEST2 & 0x40 TO SFBR;
	INT int_resfail,  IF 0x00;
script_sched:
; Load ring DSA
	MOVE SCRATCHD0 to SFBR;
	MOVE SFBR to DSA0;
	MOVE SCRATCHD1 to SFBR;
	MOVE SFBR to DSA1;
	MOVE SCRATCHD2 to SFBR;
	MOVE SFBR to DSA2;
	MOVE SCRATCHD3 to SFBR;
	MOVE SFBR to DSA3;
	LOAD SCRATCHA0,4, from o_cmd_dsa; /* get flags */
	MOVE SCRATCHA0 & f_cmd_free to SFBR;
	JUMP REL(no_cmd), IF NOT 0x0;
; this slot is busy, attempt to exec command
	SELECT ATN FROM o_cmd_id, REL(reselect);
; select either succeeded or timed out. In either case update ring pointer.
; if timed out the STO interrupt will be posted at the first SCSI bus access
; waiting for a valid phase.
	MOVE SCRATCHE0 + 1 to SFBR;
	MOVE SFBR to SCRATCHE0;
	JUMP REL(ring_reset), IF ncmd_slots;
	MOVE SCRATCHD0 + 8 to SCRATCHD0; sizeof (esiop_cmd_slot)
	MOVE SCRATCHD1 + 0 to SCRATCHD1 WITH CARRY;
	MOVE SCRATCHD2 + 0 to SCRATCHD2 WITH CARRY;
	MOVE SCRATCHD3 + 0 to SCRATCHD3 WITH CARRY;
	JUMP REL(handle_cmd);
ring_reset:
cmdr0:
	MOVE 0xff to SCRATCHD0; correct value will be patched by driver
cmdr1:
	MOVE 0xff to SCRATCHD1;
cmdr2:
	MOVE 0xff to SCRATCHD2;
cmdr3:
	MOVE 0xff to SCRATCHD3;
	MOVE 0x00 to SCRATCHE0;
handle_cmd:
; a NOP by default; patched with MOVE GPREG & 0xfe to GPREG on compile-time
; option "SIOP_SYMLED"
led_on1:
	NOP;
	MOVE SCRATCHA0 | f_cmd_free to SCRATCHA0;
	STORE noflush SCRATCHA0, 4, FROM o_cmd_dsa;
	LOAD DSA0, 4, FROM o_cmd_dsa; /* load new DSA */
	MOVE DSA0 & 0xfc to DSA0; /* clear flags */
	MOVE 0x00 TO SCRATCHA1;
	MOVE 0xff TO SCRATCHE1;
	LOAD SCRATCHC0, 4, FROM tlq_offset;
msgin_ack:
	CLEAR ACK;
waitphase:
	JUMP REL(msgout), WHEN MSG_OUT;
	JUMP REL(msgin), WHEN MSG_IN;
	JUMP REL(dataout), WHEN DATA_OUT;
	JUMP REL(datain), WHEN DATA_IN;
	JUMP REL(cmdout), WHEN CMD;
	JUMP REL(status), WHEN STATUS;
	INT int_err;

; entry point for msgout after a msgin or status phase
send_msgout: 
	SET ATN;
	CLEAR ACK;
msgout: 
        MOVE FROM t_msg_out, WHEN MSG_OUT;
	CLEAR ATN;  
	JUMP REL(waitphase);

handle_sdp:
	CLEAR ACK;
	MOVE SCRATCHC0 | f_c_sdp TO SCRATCHC0;
	; should get a disconnect message now
msgin:
	CLEAR ATN
	MOVE FROM t_msg_in, WHEN MSG_IN;
handle_msgin:
	JUMP REL(handle_cmpl), IF 0x00	; command complete message
	JUMP REL(handle_sdp), IF 0x02	; save data pointer message
	JUMP REL(handle_extin), IF 0x01	; extended message
	INT int_msgin, IF NOT 0x04;
	CALL REL(disconnect)		; disconnect message
; if we didn't get sdp, or if offset is 0, no need to interrupt
	MOVE SCRATCHC0 & f_c_sdp TO SFBR;
	JUMP REL(script_sched), if 0x00;
	MOVE SCRATCHA1 TO SFBR;
	JUMP REL(script_sched), if 0x00;
; Ok, we need to save data pointers
	INT int_disc;

cmdout:
        MOVE FROM t_cmd, WHEN CMD; 
	JUMP REL(waitphase);
status: 
        MOVE FROM t_status, WHEN STATUS;
	MOVE SFBR TO SCRATCHE1;
	JUMP REL(waitphase);
datain:
        CALL REL(savedsa);
	MOVE SCRATCHC0 | f_c_data TO SCRATCHC0;
	datain_loop:
	MOVE FROM t_data, WHEN DATA_IN;
	MOVE SCRATCHA1 + 1 TO SCRATCHA1 ; adjust offset
	MOVE DSA0 + 8 to DSA0;
	MOVE DSA1 + 0 to DSA1 WITH CARRY;
	MOVE DSA2 + 0 to DSA2 WITH CARRY;
	MOVE DSA3 + 0 to DSA3 WITH CARRY;
	JUMP REL(datain_loop), WHEN DATA_IN;
	CALL REL(restoredsa);
	MOVE SCRATCHC0 & f_c_data_mask TO SCRATCHC0;
	JUMP REL(waitphase);

dataout:
        CALL REL(savedsa);
	MOVE SCRATCHC0 | f_c_data TO SCRATCHC0;
dataout_loop:
	MOVE FROM t_data, WHEN DATA_OUT;
	MOVE SCRATCHA1 + 1 TO SCRATCHA1 ; adjust offset
	MOVE DSA0 + 8 to DSA0;
	MOVE DSA1 + 0 to DSA1 WITH CARRY;
	MOVE DSA2 + 0 to DSA2 WITH CARRY;
	MOVE DSA3 + 0 to DSA3 WITH CARRY;
	JUMP REL(dataout_loop), WHEN DATA_OUT;
	CALL REL(restoredsa);
	MOVE SCRATCHC0 & f_c_data_mask TO SCRATCHC0;
	JUMP REL(waitphase);

savedsa:
        MOVE DSA0 to SFBR;
	MOVE SFBR to SCRATCHB0;       
	MOVE DSA1 to SFBR;
	MOVE SFBR to SCRATCHB1;       
	MOVE DSA2 to SFBR;
	MOVE SFBR to SCRATCHB2;
	MOVE DSA3 to SFBR;
	MOVE SFBR to SCRATCHB3;
	RETURN;

restoredsa:
	MOVE SCRATCHB0 TO SFBR;
	MOVE SFBR TO DSA0;
	MOVE SCRATCHB1 TO SFBR;
	MOVE SFBR TO DSA1;
	MOVE SCRATCHB2 TO SFBR;
	MOVE SFBR TO DSA2;
	MOVE SCRATCHB3 TO SFBR;       
	MOVE SFBR TO DSA3;
	RETURN;

disconnect:
        MOVE SCNTL2 & 0x7f TO SCNTL2;
	CLEAR ATN;
	CLEAR ACK;
	WAIT DISCONNECT;
	RETURN;

handle_cmpl:
	CALL REL(disconnect);
	MOVE SCRATCHE1 to SFBR;
	INT int_done, IF NOT 0x00; if status is not "done", let host handle it
	MOVE ISTAT | 0x10 TO ISTAT; else signal that cmd is done in ISTAT
	JUMP REL(script_sched); and attempt next command

handle_extin:
	CLEAR ACK;
	MOVE FROM t_ext_msg_in, WHEN MSG_IN;
	INT int_extmsgin; /* let host fill in t_ext_msg_data */
get_extmsgdata:
	CLEAR ACK;
	MOVE FROM t_ext_msg_data, WHEN MSG_IN;
	INT int_extmsgdata; 
