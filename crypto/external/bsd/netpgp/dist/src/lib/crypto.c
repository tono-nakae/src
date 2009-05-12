/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@NetBSD.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
/*
 * Copyright (c) 2005-2008 Nominet UK (www.nic.uk)
 * All rights reserved.
 * Contributors: Ben Laurie, Rachel Willmer. The Contributors have asserted
 * their moral rights under the UK Copyright Design and Patents Act 1988 to
 * be recorded as the authors of this copyright work.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <string.h>

#include "crypto.h"
#include "readerwriter.h"
#include "memory.h"
#include "parse_local.h"
#include "netpgpdefs.h"
#include "signature.h"

/**
\ingroup Core_MPI
\brief Decrypt and unencode MPI
\param buf Buffer in which to write decrypted unencoded MPI
\param buflen Length of buffer
\param encmpi
\param seckey
\return length of MPI
\note only RSA at present
*/
int 
__ops_decrypt_and_unencode_mpi(unsigned char *buf,
				unsigned buflen,
				const BIGNUM * encmpi,
				const __ops_seckey_t *seckey)
{
	unsigned char   encmpibuf[NETPGP_BUFSIZ];
	unsigned char   mpibuf[NETPGP_BUFSIZ];
	unsigned        mpisize;
	int             n;
	int             i;

	mpisize = BN_num_bytes(encmpi);
	/* MPI can't be more than 65,536 */
	if (mpisize > sizeof(encmpibuf)) {
		(void) fprintf(stderr, "mpisize too big %u\n", mpisize);
		return -1;
	}
	BN_bn2bin(encmpi, encmpibuf);

	if (seckey->pubkey.alg != OPS_PKA_RSA) {
		(void) fprintf(stderr, "pubkey algorithm wrong\n");
		return -1;
	}

	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "\nDECRYPTING\n");
		(void) fprintf(stderr, "encrypted data     : ");
		for (i = 0; i < 16; i++) {
			(void) fprintf(stderr, "%2x ", encmpibuf[i]);
		}
		(void) fprintf(stderr, "\n");
	}
	n = __ops_rsa_private_decrypt(mpibuf, encmpibuf,
				(unsigned)(BN_num_bits(encmpi) + 7) / 8,
				&seckey->key.rsa, &seckey->pubkey.key.rsa);
	if (n == -1) {
		(void) fprintf(stderr, "ops_rsa_private_decrypt failure\n");
		return -1;
	}

	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "decrypted encoded m buf     : ");
		for (i = 0; i < 16; i++) {
			(void) fprintf(stderr, "%2x ", mpibuf[i]);
		}
		(void) fprintf(stderr, "\n");
	}
	if (n <= 0) {
		return -1;
	}

	if (__ops_get_debug_level(__FILE__)) {
		printf(" decrypted=%d ", n);
		hexdump(mpibuf, (unsigned)n, "");
		printf("\n");
	}
	/* Decode EME-PKCS1_V1_5 (RFC 2437). */

	if (mpibuf[0] != 0 || mpibuf[1] != 2) {
		return -1;
	}

	/* Skip the random bytes. */
	for (i = 2; i < n && mpibuf[i]; ++i) {
	}

	if (i == n || i < 10) {
		return -1;
	}

	/* Skip the zero */
	++i;

	/* this is the unencoded m buf */
	if ((unsigned) (n - i) <= buflen) {
		(void) memcpy(buf, mpibuf + i, (unsigned)(n - i));
	}

	if (__ops_get_debug_level(__FILE__)) {
		int             j;

		printf("decoded m buf:\n");
		for (j = 0; j < n - i; j++)
			printf("%2x ", buf[j]);
		printf("\n");
	}
	return n - i;
}

/**
\ingroup Core_MPI
\brief RSA-encrypt an MPI
*/
bool 
__ops_rsa_encrypt_mpi(const unsigned char *encoded_m_buf,
		    const size_t sz_encoded_m_buf,
		    const __ops_pubkey_t * pubkey,
		    __ops_pk_sesskey_parameters_t * skp)
{

	unsigned char   encmpibuf[NETPGP_BUFSIZ];
	int             n = 0;

	if (sz_encoded_m_buf != (size_t) BN_num_bytes(pubkey->key.rsa.n)) {
		(void) fprintf(stderr, "sz_encoded_m_buf wrong\n");
		return false;
	}

	n = __ops_rsa_public_encrypt(encmpibuf, encoded_m_buf,
				sz_encoded_m_buf, &pubkey->key.rsa);
	if (n == -1) {
		(void) fprintf(stderr, "__ops_rsa_public_encrypt failure\n");
		return false;
	}

	if (n <= 0)
		return false;

	skp->rsa.encrypted_m = BN_bin2bn(encmpibuf, n, NULL);

	if (__ops_get_debug_level(__FILE__)) {
		int             i;
		fprintf(stderr, "encrypted mpi buf     : ");
		for (i = 0; i < 16; i++) {
			fprintf(stderr, "%2x ", encmpibuf[i]);
		}
		fprintf(stderr, "\n");
	}
	return true;
}

static          __ops_parse_cb_return_t
callback_write_parsed(const __ops_packet_t *, __ops_callback_data_t *);

/**
\ingroup HighLevel_Crypto
Encrypt a file
\param infile Name of file to be encrypted
\param outfile Name of file to write to. If NULL, name is constructed from infile
\param pub_key Public Key to encrypt file for
\param use_armour Write armoured text, if set
\param allow_overwrite Allow output file to be overwrwritten if it exists
\return true if OK; else false
*/
bool 
__ops_encrypt_file(const char *infile,
			const char *outfile,
			const __ops_keydata_t * pub_key,
			const bool use_armour,
			const bool allow_overwrite)
{
	__ops_createinfo_t *create;
	unsigned char  *buf;
	size_t          bufsz;
	size_t		done;
	int             fd_in = 0;
	int             fd_out = 0;

#ifdef O_BINARY
	fd_in = open(infile, O_RDONLY | O_BINARY);
#else
	fd_in = open(infile, O_RDONLY);
#endif
	if (fd_in < 0) {
		perror(infile);
		return false;
	}
	fd_out = __ops_setup_file_write(&create, outfile, allow_overwrite);
	if (fd_out < 0) {
		return false;
	}

	/* set armoured/not armoured here */
	if (use_armour) {
		__ops_writer_push_armoured_message(create);
	}

	/* Push the encrypted writer */
	__ops_writer_push_encrypt_se_ip(create, pub_key);

	/* Do the writing */

	buf = NULL;
	bufsz = 16;
	done = 0;
	for (;;) {
		int             n = 0;

		buf = realloc(buf, done + bufsz);

		if ((n = read(fd_in, buf + done, bufsz)) == 0) {
			break;
		}
		if (n < 0) {
			(void) fprintf(stderr, "Problem in read\n");
			return false;
		}
		done += n;
	}

	/* This does the writing */
	__ops_write(buf, done, create);

	/* tidy up */
	close(fd_in);
	free(buf);
	__ops_teardown_file_write(create, fd_out);

	return true;
}

/**
   \ingroup HighLevel_Crypto
   \brief Decrypt a file.
   \param infile Name of file to be decrypted
   \param outfile Name of file to write to. If NULL, the filename is constructed from the input filename, following GPG conventions.
   \param keyring Keyring to use
   \param use_armour Expect armoured text, if set
   \param allow_overwrite Allow output file to overwritten, if set.
   \param cb_get_passphrase Callback to use to get passphrase
*/

bool 
__ops_decrypt_file(const char *infile,
			const char *outfile,
			__ops_keyring_t *keyring,
			const bool use_armour,
			const bool allow_overwrite,
			__ops_parse_cb_t *cb_get_passphrase)
{
	__ops_parseinfo_t	*parse = NULL;
	char			*filename = NULL;
	int			 fd_in = 0;
	int			 fd_out = 0;

	/* setup for reading from given input file */
	fd_in = __ops_setup_file_read(&parse, infile,
				    NULL,
				    callback_write_parsed,
				    false);
	if (fd_in < 0) {
		perror(infile);
		return false;
	}

	/* setup output filename */
	if (outfile) {
		fd_out = __ops_setup_file_write(&parse->cbinfo.cinfo, outfile,
				allow_overwrite);
		if (fd_out < 0) {
			perror(outfile);
			__ops_teardown_file_read(parse, fd_in);
			return false;
		}
	} else {
		unsigned	filenamelen;
		int             suffixlen = 4;
		const char     *suffix = infile + strlen(infile) - suffixlen;

		if (strcmp(suffix, ".gpg") == 0 ||
		    strcmp(suffix, ".asc") == 0) {
			filenamelen = strlen(infile) - strlen(suffix);
			filename = calloc(1, filenamelen + 1);
			(void) strncpy(filename, infile, filenamelen);
			filename[filenamelen] = 0x0;
		}

		fd_out = __ops_setup_file_write(&parse->cbinfo.cinfo,
					filename, allow_overwrite);
		if (fd_out < 0) {
			perror(filename);
			(void) free(filename);
			__ops_teardown_file_read(parse, fd_in);
			return false;
		}
		if (filename) {
			(void) free(filename);
		}
	}

	/* \todo check for suffix matching armour param */

	/* setup for writing decrypted contents to given output file */

	/* setup keyring and passphrase callback */
	parse->cbinfo.cryptinfo.keyring = keyring;
	parse->cbinfo.cryptinfo.cb_get_passphrase = cb_get_passphrase;

	/* Set up armour/passphrase options */
	if (use_armour) {
		__ops_reader_push_dearmour(parse);
	}

	/* Do it */
	__ops_parse(parse, 1);

	/* Unsetup */
	if (use_armour) {
		__ops_reader_pop_dearmour(parse);
	}

	if (filename) {
		__ops_teardown_file_write(parse->cbinfo.cinfo, fd_out);
	}
	__ops_teardown_file_read(parse, fd_in);
	/* \todo cleardown crypt */

	return true;
}

static          __ops_parse_cb_return_t
callback_write_parsed(const __ops_packet_t *pkt, __ops_callback_data_t *cbinfo)
{
	const __ops_parser_content_union_t	*content = &pkt->u;
	static bool				 skipping;

	if (__ops_get_debug_level(__FILE__)) {
		printf("callback_write_parsed: ");
		__ops_print_packet(pkt);
	}
	if (pkt->tag != OPS_PTAG_CT_UNARMOURED_TEXT && skipping) {
		puts("...end of skip");
		skipping = false;
	}
	switch (pkt->tag) {
	case OPS_PTAG_CT_UNARMOURED_TEXT:
		printf("OPS_PTAG_CT_UNARMOURED_TEXT\n");
		if (!skipping) {
			puts("Skipping...");
			skipping = true;
		}
		fwrite(content->unarmoured_text.data, 1,
		       content->unarmoured_text.length, stdout);
		break;

	case OPS_PTAG_CT_PK_SESSION_KEY:
		return pk_sesskey_cb(pkt, cbinfo);

	case OPS_PARSER_CMD_GET_SECRET_KEY:
		return get_seckey_cb(pkt, cbinfo);

	case OPS_PARSER_CMD_GET_SK_PASSPHRASE:
		return cbinfo->cryptinfo.cb_get_passphrase(pkt, cbinfo);

	case OPS_PTAG_CT_LITERAL_DATA_BODY:
		return litdata_cb(pkt, cbinfo);

	case OPS_PTAG_CT_ARMOUR_HEADER:
	case OPS_PTAG_CT_ARMOUR_TRAILER:
	case OPS_PTAG_CT_ENCRYPTED_PK_SESSION_KEY:
	case OPS_PTAG_CT_COMPRESSED:
	case OPS_PTAG_CT_LITERAL_DATA_HEADER:
	case OPS_PTAG_CT_SE_IP_DATA_BODY:
	case OPS_PTAG_CT_SE_IP_DATA_HEADER:
	case OPS_PTAG_CT_SE_DATA_BODY:
	case OPS_PTAG_CT_SE_DATA_HEADER:
		/* Ignore these packets  */
		/* They're handled in __ops_parse_packet() */
		/* and nothing else needs to be done */
		break;

	default:
		if (__ops_get_debug_level(__FILE__)) {
			fprintf(stderr, "Unexpected packet tag=%d (0x%x)\n",
				pkt->tag,
				pkt->tag);
		}
		break;
	}

	return OPS_RELEASE_MEMORY;
}
