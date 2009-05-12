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

/** \file
 */
#include "config.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "keyring.h"
#include "packet-parse.h"
#include "signature.h"
#include "netpgpsdk.h"
#include "readerwriter.h"
#include "netpgpdefs.h"
#include "keyring_local.h"
#include "parse_local.h"
#include "validate.h"



/**
   \ingroup HighLevel_Keyring

   \brief Creates a new __ops_keydata_t struct

   \return A new __ops_keydata_t struct, initialised to zero.

   \note The returned __ops_keydata_t struct must be freed after use with __ops_keydata_free.
*/

__ops_keydata_t  *
__ops_keydata_new(void)
{
	return calloc(1, sizeof(__ops_keydata_t));
}


/**
 \ingroup HighLevel_Keyring

 \brief Frees keydata and its memory

 \param keydata Key to be freed.

 \note This frees the keydata itself, as well as any other memory alloc-ed by it.
*/
void 
__ops_keydata_free(__ops_keydata_t *keydata)
{
	unsigned        n;

	for (n = 0; n < keydata->nuids; ++n) {
		__ops_user_id_free(&keydata->uids[n]);
	}
	(void) free(keydata->uids);
	keydata->uids = NULL;
	keydata->nuids = 0;

	for (n = 0; n < keydata->npackets; ++n) {
		__ops_subpacket_free(&keydata->packets[n]);
	}
	(void) free(keydata->packets);
	keydata->packets = NULL;
	keydata->npackets = 0;

	if (keydata->type == OPS_PTAG_CT_PUBLIC_KEY) {
		__ops_pubkey_free(&keydata->key.pubkey);
	} else {
		__ops_seckey_free(&keydata->key.seckey);
	}

	(void) free(keydata);
}

/**
 \ingroup HighLevel_KeyGeneral

 \brief Returns the public key in the given keydata.
 \param keydata

  \return Pointer to public key

  \note This is not a copy, do not free it after use.
*/

const __ops_pubkey_t *
__ops_get_pubkey(const __ops_keydata_t * keydata)
{
	return (keydata->type == OPS_PTAG_CT_PUBLIC_KEY) ? &keydata->key.pubkey :
		&keydata->key.seckey.pubkey;
}

/**
\ingroup HighLevel_KeyGeneral

\brief Check whether this is a secret key or not.
*/

bool 
__ops_is_key_secret(const __ops_keydata_t * data)
{
	return data->type != OPS_PTAG_CT_PUBLIC_KEY;
}

/**
 \ingroup HighLevel_KeyGeneral

 \brief Returns the secret key in the given keydata.

 \note This is not a copy, do not free it after use.

 \note This returns a const. If you need to be able to write to this pointer, use __ops_get_writable_seckey
*/

const __ops_seckey_t *
__ops_get_seckey(const __ops_keydata_t * data)
{
	return (data->type == OPS_PTAG_CT_SECRET_KEY) ? &data->key.seckey : NULL;
}

/**
 \ingroup HighLevel_KeyGeneral

  \brief Returns the secret key in the given keydata.

  \note This is not a copy, do not free it after use.

  \note If you do not need to be able to modify this key, there is an equivalent read-only function __ops_get_seckey.
*/

__ops_seckey_t *
__ops_get_writable_seckey(__ops_keydata_t * data)
{
	return (data->type == OPS_PTAG_CT_SECRET_KEY) ? &data->key.seckey : NULL;
}

typedef struct {
	const __ops_keydata_t *key;
	char           *pphrase;
	__ops_seckey_t *seckey;
}               decrypt_t;

static __ops_parse_cb_return_t 
decrypt_cb(const __ops_packet_t *pkt, __ops_callback_data_t *cbinfo)
{
	const __ops_parser_content_union_t *content = &pkt->u;
	decrypt_t  *decrypt = __ops_parse_cb_get_arg(cbinfo);

	__OPS_USED(cbinfo);

	switch (pkt->tag) {
	case OPS_PARSER_PTAG:
	case OPS_PTAG_CT_USER_ID:
	case OPS_PTAG_CT_SIGNATURE:
	case OPS_PTAG_CT_SIGNATURE_HEADER:
	case OPS_PTAG_CT_SIGNATURE_FOOTER:
	case OPS_PTAG_CT_TRUST:
		break;

	case OPS_PARSER_CMD_GET_SK_PASSPHRASE:
		*content->skey_passphrase.passphrase = decrypt->pphrase;
		return OPS_KEEP_MEMORY;

	case OPS_PARSER_ERRCODE:
		switch (content->errcode.errcode) {
		case OPS_E_P_MPI_FORMAT_ERROR:
			/* Generally this means a bad passphrase */
			fprintf(stderr, "Bad passphrase!\n");
			return OPS_RELEASE_MEMORY;

		case OPS_E_P_PACKET_CONSUMED:
			/* And this is because of an error we've accepted */
			return OPS_RELEASE_MEMORY;
		default:
			break;
		}
		(void) fprintf(stderr, "parse error: %s\n",
				__ops_errcode(content->errcode.errcode));
		return OPS_FINISHED;

	case OPS_PARSER_ERROR:
		fprintf(stderr, "parse error: %s\n", content->error.error);
		return OPS_FINISHED;

	case OPS_PTAG_CT_SECRET_KEY:
		decrypt->seckey = calloc(1, sizeof(*decrypt->seckey));
		*decrypt->seckey = content->seckey;
		return OPS_KEEP_MEMORY;

	case OPS_PARSER_PACKET_END:
		/* nothing to do */
		break;

	default:
		fprintf(stderr, "Unexpected tag %d (0x%x)\n", pkt->tag,
			pkt->tag);
		return OPS_FINISHED;
	}

	return OPS_RELEASE_MEMORY;
}

/**
\ingroup Core_Keys
\brief Decrypts secret key from given keydata with given passphrase
\param key Key from which to get secret key
\param pphrase Passphrase to use to decrypt secret key
\return secret key
*/
__ops_seckey_t *
__ops_decrypt_seckey(const __ops_keydata_t * key,
				 const char *pphrase)
{
	__ops_parseinfo_t *pinfo;
	decrypt_t   decrypt;

	(void) memset(&decrypt, 0x0, sizeof(decrypt));
	decrypt.key = key;
	decrypt.pphrase = strdup(pphrase);

	pinfo = __ops_parseinfo_new();

	__ops_keydata_reader_set(pinfo, key);
	__ops_parse_cb_set(pinfo, decrypt_cb, &decrypt);
	pinfo->readinfo.accumulate = true;

	__ops_parse(pinfo, 0);

	return decrypt.seckey;
}

/**
\ingroup Core_Keys
\brief Set secret key in content
\param content Content to be set
\param key Keydata to get secret key from
*/
void 
__ops_set_seckey(__ops_parser_content_union_t * content, const __ops_keydata_t * key)
{
	*content->get_seckey.seckey = &key->key.seckey;
}

/**
\ingroup Core_Keys
\brief Get Key ID from keydata
\param key Keydata to get Key ID from
\return Pointer to Key ID inside keydata
*/
const unsigned char *
__ops_get_key_id(const __ops_keydata_t * key)
{
	return key->key_id;
}

/**
\ingroup Core_Keys
\brief How many User IDs in this key?
\param key Keydata to check
\return Num of user ids
*/
unsigned 
__ops_get_user_id_count(const __ops_keydata_t * key)
{
	return key->nuids;
}

/**
\ingroup Core_Keys
\brief Get indexed user id from key
\param key Key to get user id from
\param index Which key to get
\return Pointer to requested user id
*/
const unsigned char *
__ops_get_user_id(const __ops_keydata_t * key, unsigned subscript)
{
	return key->uids[subscript].user_id;
}

/**
   \ingroup HighLevel_Supported
   \brief Checks whether key's algorithm and type are supported by OpenPGP::SDK
   \param keydata Key to be checked
   \return true if key algorithm and type are supported by OpenPGP::SDK; false if not
*/

bool 
__ops_is_key_supported(const __ops_keydata_t *keydata)
{
	if (keydata->type == OPS_PTAG_CT_PUBLIC_KEY) {
		if (keydata->key.pubkey.alg == OPS_PKA_RSA) {
			return true;
		}
	} else if (keydata->type == OPS_PTAG_CT_PUBLIC_KEY) {
		if (keydata->key.pubkey.alg == OPS_PKA_DSA) {
			return true;
		}
	}
	return false;
}


/**
    \ingroup HighLevel_KeyringFind

    \brief Returns key inside a keyring, chosen by index

    \param keyring Pointer to existing keyring
    \param index Index of required key

    \note Index starts at 0

    \note This returns a pointer to the original key, not a copy. You do not need to free the key after use.

    \return Pointer to the required key; or NULL if index too large.

    Example code:
    \code
    void example(const __ops_keyring_t* keyring)
    {
    __ops_keydata_t* keydata=NULL;
    keydata=__ops_keyring_get_key_by_index(keyring, 0);
    ...
    }
    \endcode
*/

const __ops_keydata_t *
__ops_keyring_get_key_by_index(const __ops_keyring_t * keyring, int subscript)
{
	if (subscript >= keyring->nkeys)
		return NULL;
	return &keyring->keys[subscript];
}

/* \todo check where userid pointers are copied */
/**
\ingroup Core_Keys
\brief Copy user id, including contents
\param dst Destination User ID
\param src Source User ID
\note If dst already has a user_id, it will be freed.
*/
void 
__ops_copy_userid(__ops_user_id_t * dst, const __ops_user_id_t * src)
{
	size_t          len = strlen((char *) src->user_id);
	if (dst->user_id)
		free(dst->user_id);
	dst->user_id = calloc(1, len + 1);

	(void) memcpy(dst->user_id, src->user_id, len);
}

/* \todo check where pkt pointers are copied */
/**
\ingroup Core_Keys
\brief Copy packet, including contents
\param dst Destination packet
\param src Source packet
\note If dst already has a packet, it will be freed.
*/
void 
__ops_copy_packet(__ops_subpacket_t * dst, const __ops_subpacket_t * src)
{
	if (dst->raw) {
		(void) free(dst->raw);
	}
	dst->raw = calloc(1, src->length);
	dst->length = src->length;
	(void) memcpy(dst->raw, src->raw, src->length);
}

/**
\ingroup Core_Keys
\brief Add User ID to keydata
\param keydata Key to which to add User ID
\param userid User ID to add
\return Pointer to new User ID
*/
__ops_user_id_t  *
__ops_add_userid_to_keydata(__ops_keydata_t * keydata, const __ops_user_id_t * userid)
{
	__ops_user_id_t  *new_uid = NULL;

	EXPAND_ARRAY(keydata, uids);

	/* initialise new entry in array */
	new_uid = &keydata->uids[keydata->nuids];

	new_uid->user_id = NULL;

	/* now copy it */
	__ops_copy_userid(new_uid, userid);
	keydata->nuids++;

	return new_uid;
}

/**
\ingroup Core_Keys
\brief Add packet to key
\param keydata Key to which to add packet
\param packet Packet to add
\return Pointer to new packet
*/
__ops_subpacket_t   *
__ops_add_packet_to_keydata(__ops_keydata_t * keydata, const __ops_subpacket_t * packet)
{
	__ops_subpacket_t   *new_pkt = NULL;

	EXPAND_ARRAY(keydata, packets);

	/* initialise new entry in array */
	new_pkt = &keydata->packets[keydata->npackets];
	new_pkt->length = 0;
	new_pkt->raw = NULL;

	/* now copy it */
	__ops_copy_packet(new_pkt, packet);
	keydata->npackets++;

	return new_pkt;
}

/**
\ingroup Core_Keys
\brief Add signed User ID to key
\param keydata Key to which to add signed User ID
\param user_id User ID to add
\param sigpacket Packet to add
*/
void 
__ops_add_signed_userid_to_keydata(__ops_keydata_t * keydata, const __ops_user_id_t * user_id, const __ops_subpacket_t * sigpacket)
{
	__ops_subpacket_t	*pkt = NULL;
	__ops_user_id_t		*uid = NULL;

	uid = __ops_add_userid_to_keydata(keydata, user_id);
	pkt = __ops_add_packet_to_keydata(keydata, sigpacket);

	/*
         * add entry in sigs array to link the userid and sigpacket
	 * and add ptr to it from the sigs array */
	EXPAND_ARRAY(keydata, sigs);

	/**setup new entry in array */
	keydata->sigs[keydata->nsigs].userid = uid;
	keydata->sigs[keydata->nsigs].packet = pkt;

	keydata->nsigs++;
}

/**
\ingroup Core_Keys
\brief Add selfsigned User ID to key
\param keydata Key to which to add user ID
\param userid Self-signed User ID to add
\return true if OK; else false
*/
bool 
__ops_add_selfsigned_userid_to_keydata(__ops_keydata_t * keydata, __ops_user_id_t * userid)
{
	__ops_subpacket_t    sigpacket;

	__ops_memory_t   *mem_userid = NULL;
	__ops_createinfo_t *cinfo_userid = NULL;

	__ops_memory_t   *mem_sig = NULL;
	__ops_createinfo_t *cinfo_sig = NULL;

	__ops_create_sig_t *sig = NULL;

	/*
         * create signature packet for this userid
         */

	/* create userid pkt */
	__ops_setup_memory_write(&cinfo_userid, &mem_userid, 128);
	__ops_write_struct_user_id(userid, cinfo_userid);

	/* create sig for this pkt */

	sig = __ops_create_sig_new();
	__ops_sig_start_key_sig(sig, &keydata->key.seckey.pubkey, userid, OPS_CERT_POSITIVE);
	__ops_sig_add_birthtime(sig, time(NULL));
	__ops_sig_add_issuer_key_id(sig, keydata->key_id);
	__ops_sig_add_primary_user_id(sig, true);
	__ops_sig_hashed_subpackets_end(sig);

	__ops_setup_memory_write(&cinfo_sig, &mem_sig, 128);
	__ops_write_sig(sig, &keydata->key.seckey.pubkey, &keydata->key.seckey, cinfo_sig);

	/* add this packet to keydata */

	sigpacket.length = __ops_memory_get_length(mem_sig);
	sigpacket.raw = __ops_memory_get_data(mem_sig);

	/* add userid to keydata */
	__ops_add_signed_userid_to_keydata(keydata, userid, &sigpacket);

	/* cleanup */
	__ops_create_sig_delete(sig);
	__ops_createinfo_delete(cinfo_userid);
	__ops_createinfo_delete(cinfo_sig);
	__ops_memory_free(mem_userid);
	__ops_memory_free(mem_sig);

	return true;
}

/**
\ingroup Core_Keys
\brief Initialise __ops_keydata_t
\param keydata Keydata to initialise
\param type OPS_PTAG_CT_PUBLIC_KEY or OPS_PTAG_CT_SECRET_KEY
*/
void 
__ops_keydata_init(__ops_keydata_t * keydata, const __ops_content_tag_t type)
{
	if (keydata->type != OPS_PTAG_CT_RESERVED) {
		(void) fprintf(stderr,
			"__ops_keydata_init: wrong keydata type\n");
	} else if (type != OPS_PTAG_CT_PUBLIC_KEY &&
		   type != OPS_PTAG_CT_SECRET_KEY) {
		(void) fprintf(stderr, "__ops_keydata_init: wrong type\n");
	} else {
		keydata->type = type;
	}
}

/**
    Example Usage:
    \code

    // definition of variables
    __ops_keyring_t keyring;
    char* filename="~/.gnupg/pubring.gpg";

    // Read keyring from file
    __ops_keyring_fileread(&keyring,filename);

    // do actions using keyring
    ...

    // Free memory alloc-ed in __ops_keyring_fileread()
    __ops_keyring_free(keyring);
    \endcode
*/


static          __ops_parse_cb_return_t
cb_keyring_read(const __ops_packet_t * pkt,
		__ops_callback_data_t * cbinfo)
{
	__OPS_USED(cbinfo);

	switch (pkt->tag) {
	case OPS_PARSER_PTAG:
	case OPS_PTAG_CT_ENCRYPTED_SECRET_KEY:	/* we get these because we
						 * didn't prompt */
	case OPS_PTAG_CT_SIGNATURE_HEADER:
	case OPS_PTAG_CT_SIGNATURE_FOOTER:
	case OPS_PTAG_CT_SIGNATURE:
	case OPS_PTAG_CT_TRUST:
	case OPS_PARSER_ERRCODE:
		break;

	default:
		;
	}

	return OPS_RELEASE_MEMORY;
}

/**
   \ingroup HighLevel_KeyringRead

   \brief Reads a keyring from a file

   \param keyring Pointer to an existing __ops_keyring_t struct
   \param armour true if file is armoured; else false
   \param filename Filename of keyring to be read

   \return __ops true if OK; false on error

   \note Keyring struct must already exist.

   \note Can be used with either a public or secret keyring.

   \note You must call __ops_keyring_free() after usage to free alloc-ed memory.

   \note If you call this twice on the same keyring struct, without calling
   __ops_keyring_free() between these calls, you will introduce a memory leak.

   \sa __ops_keyring_read_from_mem()
   \sa __ops_keyring_free()

   Example code:
   \code
   __ops_keyring_t* keyring=calloc(1, sizeof(*keyring));
   bool armoured=false;
   __ops_keyring_fileread(keyring, armoured, "~/.gnupg/pubring.gpg");
   ...
   __ops_keyring_free(keyring);
   free (keyring);

   \endcode
*/

bool 
__ops_keyring_fileread(__ops_keyring_t * keyring, const bool armour, const char *filename)
{
	__ops_parseinfo_t *pinfo;
	int             fd;
	bool   res = true;

	pinfo = __ops_parseinfo_new();

	/* add this for the moment, */
	/*
	 * \todo need to fix the problems with reading signature subpackets
	 * later
	 */

	/* __ops_parse_options(pinfo,OPS_PTAG_SS_ALL,OPS_PARSE_RAW); */
	__ops_parse_options(pinfo, OPS_PTAG_SS_ALL, OPS_PARSE_PARSED);

#ifdef O_BINARY
	fd = open(filename, O_RDONLY | O_BINARY);
#else
	fd = open(filename, O_RDONLY);
#endif
	if (fd < 0) {
		__ops_parseinfo_delete(pinfo);
		perror(filename);
		return false;
	}
#ifdef USE_MMAP_FOR_FILES
	__ops_reader_set_mmap(pinfo, fd);
#else
	__ops_reader_set_fd(pinfo, fd);
#endif

	__ops_parse_cb_set(pinfo, cb_keyring_read, NULL);

	if (armour) {
		__ops_reader_push_dearmour(pinfo);
	}
	if (__ops_parse_and_accumulate(keyring, pinfo) == 0) {
		res = false;
	} else {
		res = true;
	}
	__ops_print_errors(__ops_parseinfo_get_errors(pinfo));

	if (armour)
		__ops_reader_pop_dearmour(pinfo);

	close(fd);

	__ops_parseinfo_delete(pinfo);

	return res;
}

#if 0
/**
   \ingroup HighLevel_KeyringRead

   \brief Reads a keyring from memory

   \param keyring Pointer to existing __ops_keyring_t struct
   \param armour true if file is armoured; else false
   \param mem Pointer to a __ops_memory_t struct containing keyring to be read

   \return __ops true if OK; false on error

   \note Keyring struct must already exist.

   \note Can be used with either a public or secret keyring.

   \note You must call __ops_keyring_free() after usage to free alloc-ed memory.

   \note If you call this twice on the same keyring struct, without calling
   __ops_keyring_free() between these calls, you will introduce a memory leak.

   \sa __ops_keyring_fileread
   \sa __ops_keyring_free

   Example code:
   \code
   __ops_memory_t* mem; // Filled with keyring packets
   __ops_keyring_t* keyring=calloc(1, sizeof(*keyring));
   bool armoured=false;
   __ops_keyring_read_from_mem(keyring, armoured, mem);
   ...
   __ops_keyring_free(keyring);
   free (keyring);
   \endcode
*/
static bool 
__ops_keyring_read_from_mem(__ops_keyring_t * keyring, const bool armour, __ops_memory_t * mem)
{
	__ops_parseinfo_t *pinfo = NULL;
	bool   res = true;

	pinfo = __ops_parseinfo_new();
	__ops_parse_options(pinfo, OPS_PTAG_SS_ALL, OPS_PARSE_PARSED);

	__ops_setup_memory_read(&pinfo, mem, NULL, cb_keyring_read, false);

	if (armour) {
		__ops_reader_push_dearmour(pinfo);
	}
	res = (__ops_parse_and_accumulate(keyring, pinfo) != 0);
	__ops_print_errors(__ops_parseinfo_get_errors(pinfo));

	if (armour)
		__ops_reader_pop_dearmour(pinfo);

	/* don't call teardown_memory_read because memory was passed in */
	__ops_parseinfo_delete(pinfo);

	return res;
}
#endif

/**
   \ingroup HighLevel_KeyringRead

   \brief Frees keyring's contents (but not keyring itself)

   \param keyring Keyring whose data is to be freed

   \note This does not free keyring itself, just the memory alloc-ed in it.
 */
void 
__ops_keyring_free(__ops_keyring_t * keyring)
{
	free(keyring->keys);
	keyring->keys = NULL;
	keyring->nkeys = 0;
	keyring->nkeys_allocated = 0;
}

/**
   \ingroup HighLevel_KeyringFind

   \brief Finds key in keyring from its Key ID

   \param keyring Keyring to be searched
   \param keyid ID of required key

   \return Pointer to key, if found; NULL, if not found

   \note This returns a pointer to the key inside the given keyring, not a copy. Do not free it after use.

   Example code:
   \code
   void example(__ops_keyring_t* keyring)
   {
   __ops_keydata_t* keydata=NULL;
   unsigned char keyid[OPS_KEY_ID_SIZE]; // value set elsewhere
   keydata=__ops_keyring_find_key_by_id(keyring,keyid);
   ...
   }
   \endcode
*/
const __ops_keydata_t *
__ops_keyring_find_key_by_id(const __ops_keyring_t * keyring,
			   const unsigned char keyid[OPS_KEY_ID_SIZE])
{
	int	n;

	for (n = 0; keyring && n < keyring->nkeys; n++) {
		if (__ops_get_debug_level(__FILE__)) {
			int	i;

			printf("__ops_keyring_find_key_by_id: keyring keyid ");
			for (i = 0 ; i < OPS_KEY_ID_SIZE ; i++) {
				printf("%02x", keyring->keys[n].key_id[i]);
			}
			printf(", keyid ");
			for (i = 0 ; i < OPS_KEY_ID_SIZE ; i++) {
				printf("%02x", keyid[i]);
			}
			printf("\n");
		}
		if (memcmp(keyring->keys[n].key_id, keyid, OPS_KEY_ID_SIZE) == 0) {
			return &keyring->keys[n];
		}
		if (memcmp(&keyring->keys[n].key_id[OPS_KEY_ID_SIZE / 2],
				keyid, OPS_KEY_ID_SIZE / 2) == 0) {
			return &keyring->keys[n];
		}
	}
	return NULL;
}

/* convert a string keyid into a binary keyid */
static void
str2keyid(const char *userid, unsigned char *keyid, size_t len)
{
	static const char	*uppers = "0123456789ABCDEF";
	static const char	*lowers = "0123456789abcdef";
	unsigned char		 hichar;
	unsigned char		 lochar;
	size_t			 j;
	const char		*hi;
	const char		*lo;
	int			 i;

	for (i = j = 0 ; j < len && userid[i] && userid[i + 1] ; i += 2, j++) {
		if ((hi = strchr(uppers, userid[i])) == NULL) {
			if ((hi = strchr(lowers, userid[i])) == NULL) {
				break;
			}
			hichar = (hi - lowers);
		} else {
			hichar = (hi - uppers);
		}
		if ((lo = strchr(uppers, userid[i + 1])) == NULL) {
			if ((lo = strchr(lowers, userid[i + 1])) == NULL) {
				break;
			}
			lochar = (lo - lowers);
		} else {
			lochar = (lo - uppers);
		}
		keyid[j] = (hichar << 4) | (lochar);
	}
	keyid[j] = 0x0;
}

/**
   \ingroup HighLevel_KeyringFind

   \brief Finds key from its User ID

   \param keyring Keyring to be searched
   \param userid User ID of required key

   \return Pointer to Key, if found; NULL, if not found

   \note This returns a pointer to the key inside the keyring, not a copy. Do not free it.

   Example code:
   \code
   void example(__ops_keyring_t* keyring)
   {
   __ops_keydata_t* keydata=NULL;
   keydata=__ops_keyring_find_key_by_userid(keyring,"user@domain.com");
   ...
   }
   \endcode
*/
const __ops_keydata_t *
__ops_keyring_find_key_by_userid(const __ops_keyring_t *keyring,
			       const char *userid)
{
	const __ops_keydata_t	*kp;
	unsigned char		 keyid[OPS_KEY_ID_SIZE + 1];
	unsigned int    	 i = 0;
	size_t          	 len;
	char	                *cp;
	int             	 n = 0;

	if (!keyring)
		return NULL;

	len = strlen(userid);
	for (n = 0; n < keyring->nkeys; ++n) {
		for (i = 0; i < keyring->keys[n].nuids; i++) {
			if (__ops_get_debug_level(__FILE__)) {
				printf("[%d][%d] userid %s, last '%d'\n",
					n, i, keyring->keys[n].uids[i].user_id,
					keyring->keys[n].uids[i].user_id[len]);
			}
			if (strncmp((char *) keyring->keys[n].uids[i].user_id, userid, len) == 0 &&
			    keyring->keys[n].uids[i].user_id[len] == ' ') {
				return &keyring->keys[n];
			}
		}
	}

	if (strchr(userid, '@') == NULL) {
		/* no '@' sign */
		/* first try userid as a keyid */
		(void) memset(keyid, 0x0, sizeof(keyid));
		str2keyid(userid, keyid, sizeof(keyid));
		if (__ops_get_debug_level(__FILE__)) {
			printf("userid \"%s\", keyid %02x%02x%02x%02x\n",
				userid,
				keyid[0], keyid[1], keyid[2], keyid[3]);
		}
		if ((kp = __ops_keyring_find_key_by_id(keyring, keyid)) != NULL) {
			return kp;
		}
		/* match on full name */
		for (n = 0; n < keyring->nkeys; n++) {
			for (i = 0; i < keyring->keys[n].nuids; i++) {
				if (__ops_get_debug_level(__FILE__)) {
					printf("keyid \"%s\" len %" PRIsize "u, keyid[len] '%c'\n",
					       (char *) keyring->keys[n].uids[i].user_id,
					       len, keyring->keys[n].uids[i].user_id[len]);
				}
				if (strncasecmp((char *) keyring->keys[n].uids[i].user_id, userid, len) == 0 &&
				    keyring->keys[n].uids[i].user_id[len] == ' ') {
					return &keyring->keys[n];
				}
			}
		}
	}
	/* match on <email@address> */
	for (n = 0; n < keyring->nkeys; n++) {
		for (i = 0; i < keyring->keys[n].nuids; i++) {
			/*
			 * look for the rightmost '<', in case there is one
			 * in the comment field
			 */
			if ((cp = strrchr((char *) keyring->keys[n].uids[i].user_id, '<')) != NULL) {
				if (__ops_get_debug_level(__FILE__)) {
					printf("cp ,%s, userid ,%s, len %" PRIsize "u ,%c,\n",
					       cp + 1, userid, len, *(cp + len + 1));
				}
				if (strncasecmp(cp + 1, userid, len) == 0 &&
				    *(cp + len + 1) == '>') {
					return &keyring->keys[n];
				}
			}
		}
	}

	/* printf("end: n=%d,i=%d\n",n,i); */
	return NULL;
}

/**
   \ingroup HighLevel_KeyringList

   \brief Prints all keys in keyring to stdout.

   \param keyring Keyring to use

   \return none

   Example code:
   \code
   void example()
   {
   __ops_keyring_t* keyring=calloc(1, sizeof(*keyring));
   bool armoured=false;
   __ops_keyring_fileread(keyring, armoured, "~/.gnupg/pubring.gpg");

   __ops_keyring_list(keyring);

   __ops_keyring_free(keyring);
   free (keyring);
   }
   \endcode
*/

void
__ops_keyring_list(const __ops_keyring_t * keyring)
{
	int             n;
	__ops_keydata_t  *key;

	printf("%d keys\n", keyring->nkeys);
	for (n = 0, key = &keyring->keys[n]; n < keyring->nkeys; ++n, ++key) {
		if (__ops_is_key_secret(key)) {
			__ops_print_seckeydata(key);
		} else {
			__ops_print_pubkeydata(key);
		}
		(void) fputc('\n', stdout);
	}
}

unsigned
__ops_get_keydata_content_type(const __ops_keydata_t * keydata)
{
	return keydata->type;
}

/* this interface isn't right - hook into callback for getting passphrase */
int
__ops_export_key(const __ops_keydata_t *keydata, unsigned char *passphrase)
{
	__ops_createinfo_t	*cinfo;
	__ops_memory_t		*mem;

	__ops_setup_memory_write(&cinfo, &mem, 128);
	if (__ops_get_keydata_content_type(keydata) == OPS_PTAG_CT_PUBLIC_KEY) {
		__ops_write_transferable_pubkey(keydata, true, cinfo);
	} else {
		__ops_write_transferable_seckey(keydata,
				    passphrase,
			    strlen((char *)passphrase), true, cinfo);
	}
	printf("%s", (char *) __ops_memory_get_data(mem));
	__ops_teardown_memory_write(cinfo, mem);
	return 1;
}
