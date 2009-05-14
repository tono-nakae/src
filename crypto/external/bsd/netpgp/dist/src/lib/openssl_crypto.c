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

#ifdef HAVE_OPENSSL_MD5_H
#include <openssl/md5.h>
#endif

#ifdef HAVE_OPENSSL_SHA_H
#include <openssl/sha.h>
#endif

#ifdef HAVE_OPENSSL_DSA_H
#include <openssl/dsa.h>
#endif

#ifdef HAVE_OPENSSL_RSA_H
#include <openssl/rsa.h>
#endif

#ifdef HAVE_OPENSSL_ERR_H
#include <openssl/err.h>
#endif


#include <stdlib.h>

/* Apple */
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
#undef MD5_DIGEST_LENGTH
#undef SHA_DIGEST_LENGTH
#define COMMON_DIGEST_FOR_OPENSSL	1
#include <CommonCrypto/CommonDigest.h>
#endif

#include "crypto.h"
#include "keyring.h"
#include "readerwriter.h"
#include "netpgpdefs.h"
#include "packet.h"


static void 
test_seckey(const __ops_seckey_t * seckey)
{
	RSA            *test = RSA_new();

	test->n = BN_dup(seckey->pubkey.key.rsa.n);
	test->e = BN_dup(seckey->pubkey.key.rsa.e);

	test->d = BN_dup(seckey->key.rsa.d);
	test->p = BN_dup(seckey->key.rsa.p);
	test->q = BN_dup(seckey->key.rsa.q);

	if (RSA_check_key(test) != 1) {
		(void) fprintf(stderr,
			"test_seckey: RSA_check_key failed\n");
	}
	RSA_free(test);
}

static void 
md5_init(__ops_hash_t * hash)
{
	if (hash->data) {
		(void) fprintf(stderr, "md5_init: hash data non-null\n");
	}
	hash->data = calloc(1, sizeof(MD5_CTX));
	MD5_Init(hash->data);
}

static void 
md5_add(__ops_hash_t * hash, const unsigned char *data, unsigned length)
{
	MD5_Update(hash->data, data, length);
}

static unsigned 
md5_finish(__ops_hash_t * hash, unsigned char *out)
{
	MD5_Final(out, hash->data);
	free(hash->data);
	hash->data = NULL;
	return 16;
}

static __ops_hash_t md5 = {
	OPS_HASH_MD5,
	MD5_DIGEST_LENGTH,
	"MD5",
	md5_init,
	md5_add,
	md5_finish,
	NULL
};

/**
   \ingroup Core_Crypto
   \brief Initialise to MD5
   \param hash Hash to initialise
*/
void 
__ops_hash_md5(__ops_hash_t * hash)
{
	*hash = md5;
}

static void 
sha1_init(__ops_hash_t * hash)
{
	if (__ops_get_debug_level(__FILE__)) {
		fprintf(stderr, "***\n***\nsha1_init\n***\n");
	}
	if (hash->data) {
		(void) fprintf(stderr, "sha1_init: hash data non-null\n");
	}
	hash->data = calloc(1, sizeof(SHA_CTX));
	SHA1_Init(hash->data);
}

static void 
sha1_add(__ops_hash_t * hash, const unsigned char *data, unsigned length)
{
	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i;

		(void) fprintf(stderr, "adding %d to hash:\n ", length);
		for (i = 0; i < length; i++) {
			(void) fprintf(stderr, "0x%02x ", data[i]);
			if (!((i + 1) % 16)) {
				(void) fprintf(stderr, "\n");
			} else if (!((i + 1) % 8)) {
				(void) fprintf(stderr, "  ");
			}
		}
		(void) fprintf(stderr, "\n");
	}
	SHA1_Update(hash->data, data, length);
}

static unsigned 
sha1_finish(__ops_hash_t * hash, unsigned char *out)
{
	SHA1_Final(out, hash->data);
	if (__ops_get_debug_level(__FILE__)) {
		unsigned        i;

		(void) fprintf(stderr, "***\n***\nsha1_finish\n***\n");
		for (i = 0; i < OPS_SHA1_HASH_SIZE; i++)
			(void) fprintf(stderr, "0x%02x ", out[i]);
		(void) fprintf(stderr, "\n");
	}
	(void) free(hash->data);
	hash->data = NULL;
	return OPS_SHA1_HASH_SIZE;
}

static __ops_hash_t sha1 = {
	OPS_HASH_SHA1,
	OPS_SHA1_HASH_SIZE,
	"SHA1",
	sha1_init,
	sha1_add,
	sha1_finish,
	NULL
};

/**
   \ingroup Core_Crypto
   \brief Initialise to SHA1
   \param hash Hash to initialise
*/
void 
__ops_hash_sha1(__ops_hash_t * hash)
{
	*hash = sha1;
}

static void 
sha256_init(__ops_hash_t * hash)
{
	if (__ops_get_debug_level(__FILE__)) {
		fprintf(stderr, "***\n***\nsha256_init\n***\n");
	}
	if (hash->data) {
		(void) fprintf(stderr, "sha256_init: hash data non-null\n");
	}
	hash->data = calloc(1, sizeof(SHA256_CTX));
	SHA256_Init(hash->data);
}

static void 
sha256_add(__ops_hash_t * hash, const unsigned char *data, unsigned length)
{
	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i;

		(void) fprintf(stderr, "adding %d to hash:\n ", length);
		for (i = 0; i < length; i++) {
			(void) fprintf(stderr, "0x%02x ", data[i]);
			if (!((i + 1) % 16))
				(void) fprintf(stderr, "\n");
			else if (!((i + 1) % 8))
				(void) fprintf(stderr, "  ");
		}
		(void) fprintf(stderr, "\n");
	}
	SHA256_Update(hash->data, data, length);
}

static unsigned 
sha256_finish(__ops_hash_t * hash, unsigned char *out)
{
	SHA256_Final(out, hash->data);
	if (__ops_get_debug_level(__FILE__)) {
		unsigned        i;

		(void) fprintf(stderr, "***\n***\nsha1_finish\n***\n");
		for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
			(void) fprintf(stderr, "0x%02x ", out[i]);
		(void) fprintf(stderr, "\n");
	}
	(void) free(hash->data);
	hash->data = NULL;
	return SHA256_DIGEST_LENGTH;
}

static __ops_hash_t sha256 = {
	OPS_HASH_SHA256,
	SHA256_DIGEST_LENGTH,
	"SHA256",
	sha256_init,
	sha256_add,
	sha256_finish,
	NULL
};

void 
__ops_hash_sha256(__ops_hash_t *hash)
{
	*hash = sha256;
}

/*
 * SHA384
 */
static void 
sha384_init(__ops_hash_t *hash)
{
	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "***\n***\nsha384_init\n***\n");
	}
	if (hash->data) {
		(void) fprintf(stderr, "sha384_init: hash data non-null\n");
	}
	hash->data = calloc(1, sizeof(SHA512_CTX));
	SHA384_Init(hash->data);
}

static void 
sha384_add(__ops_hash_t * hash, const unsigned char *data, unsigned length)
{
	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i;

		(void) fprintf(stderr, "adding %d to hash:\n ", length);
		for (i = 0; i < length; i++) {
			(void) fprintf(stderr, "0x%02x ", data[i]);
			if (!((i + 1) % 16))
				(void) fprintf(stderr, "\n");
			else if (!((i + 1) % 8))
				(void) fprintf(stderr, "  ");
		}
		(void) fprintf(stderr, "\n");
	}
	SHA384_Update(hash->data, data, length);
}

static unsigned 
sha384_finish(__ops_hash_t * hash, unsigned char *out)
{
	SHA384_Final(out, hash->data);
	if (__ops_get_debug_level(__FILE__)) {
		unsigned        i;

		(void) fprintf(stderr, "***\n***\nsha1_finish\n***\n");
		for (i = 0; i < SHA384_DIGEST_LENGTH; i++)
			(void) fprintf(stderr, "0x%02x ", out[i]);
		(void) fprintf(stderr, "\n");
	}
	(void) free(hash->data);
	hash->data = NULL;
	return SHA384_DIGEST_LENGTH;
}

static __ops_hash_t sha384 = {
	OPS_HASH_SHA384,
	SHA384_DIGEST_LENGTH,
	"SHA384",
	sha384_init,
	sha384_add,
	sha384_finish,
	NULL
};

void 
__ops_hash_sha384(__ops_hash_t *hash)
{
	*hash = sha384;
}

/*
 * SHA512
 */
static void 
sha512_init(__ops_hash_t * hash)
{
	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "***\n***\nsha512_init\n***\n");
	}
	if (hash->data) {
		(void) fprintf(stderr, "sha512_init: hash data non-null\n");
	}
	hash->data = calloc(1, sizeof(SHA512_CTX));
	SHA512_Init(hash->data);
}

static void 
sha512_add(__ops_hash_t * hash, const unsigned char *data, unsigned length)
{
	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i;

		(void) fprintf(stderr, "adding %d to hash:\n ", length);
		for (i = 0; i < length; i++) {
			(void) fprintf(stderr, "0x%02x ", data[i]);
			if (!((i + 1) % 16))
				(void) fprintf(stderr, "\n");
			else if (!((i + 1) % 8))
				(void) fprintf(stderr, "  ");
		}
		(void) fprintf(stderr, "\n");
	}
	SHA512_Update(hash->data, data, length);
}

static unsigned 
sha512_finish(__ops_hash_t * hash, unsigned char *out)
{
	SHA512_Final(out, hash->data);
	if (__ops_get_debug_level(__FILE__)) {
		unsigned        i;

		(void) fprintf(stderr, "***\n***\nsha1_finish\n***\n");
		for (i = 0; i < SHA512_DIGEST_LENGTH; i++)
			(void) fprintf(stderr, "0x%02x ", out[i]);
		(void) fprintf(stderr, "\n");
	}
	(void) free(hash->data);
	hash->data = NULL;
	return SHA512_DIGEST_LENGTH;
}

static __ops_hash_t sha512 = {
	OPS_HASH_SHA512,
	SHA512_DIGEST_LENGTH,
	"SHA512",
	sha512_init,
	sha512_add,
	sha512_finish,
	NULL
};

void 
__ops_hash_sha512(__ops_hash_t * hash)
{
	*hash = sha512;
}

/*
 * SHA224
 */

static void 
sha224_init(__ops_hash_t * hash)
{
	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "***\n***\nsha1_init\n***\n");
	}
	if (hash->data) {
		(void) fprintf(stderr, "sha224_init: hash data non-null\n");
	}
	hash->data = calloc(1, sizeof(SHA256_CTX));
	SHA224_Init(hash->data);
}

static void 
sha224_add(__ops_hash_t * hash, const unsigned char *data, unsigned length)
{
	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i;

		(void) fprintf(stderr, "adding %d to hash:\n ", length);
		for (i = 0; i < length; i++) {
			(void) fprintf(stderr, "0x%02x ", data[i]);
			if (!((i + 1) % 16))
				(void) fprintf(stderr, "\n");
			else if (!((i + 1) % 8))
				(void) fprintf(stderr, "  ");
		}
		(void) fprintf(stderr, "\n");
	}
	SHA224_Update(hash->data, data, length);
}

static unsigned 
sha224_finish(__ops_hash_t * hash, unsigned char *out)
{
	SHA224_Final(out, hash->data);
	if (__ops_get_debug_level(__FILE__)) {
		unsigned        i;

		(void) fprintf(stderr, "***\n***\nsha1_finish\n***\n");
		for (i = 0; i < SHA224_DIGEST_LENGTH; i++)
			(void) fprintf(stderr, "0x%02x ", out[i]);
		(void) fprintf(stderr, "\n");
	}
	(void) free(hash->data);
	hash->data = NULL;
	return SHA224_DIGEST_LENGTH;
}

static __ops_hash_t sha224 = {
	OPS_HASH_SHA224,
	SHA224_DIGEST_LENGTH,
	"SHA224",
	sha224_init,
	sha224_add,
	sha224_finish,
	NULL
};

void 
__ops_hash_sha224(__ops_hash_t * hash)
{
	*hash = sha224;
}

unsigned 
__ops_dsa_verify(const unsigned char *hash, size_t hash_length,
	       const __ops_dsa_sig_t * sig,
	       const __ops_dsa_pubkey_t * dsa)
{
	DSA_SIG        *osig;
	DSA            *odsa;
	int             ret;
	unsigned int    qlen;

	osig = DSA_SIG_new();
	osig->r = sig->r;
	osig->s = sig->s;

	odsa = DSA_new();
	odsa->p = dsa->p;
	odsa->q = dsa->q;
	odsa->g = dsa->g;
	odsa->pub_key = dsa->y;

	if (__ops_get_debug_level(__FILE__)) {
		unsigned        i;

		(void) fprintf(stderr, "hash passed in:\n");
		for (i = 0; i < hash_length; i++) {
			(void) fprintf(stderr, "%02x ", hash[i]);
		}
		(void) fprintf(stderr, "\n");
		printf("hash_length=%" PRIsize "d\n", hash_length);
	}
	/* printf("Q=%d\n", BN_num_bytes(odsa->q)); */
	if ((qlen = BN_num_bytes(odsa->q)) < hash_length) {
		hash_length = qlen;
	}
	ret = DSA_do_verify(hash, (int)hash_length, osig, odsa);
	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "ret=%d\n", ret);
	}
	if (ret < 0) {
		(void) fprintf(stderr,
			"__ops_do_verify: DSA_do_verify failed\n");
		return 0;
	}

	odsa->p = odsa->q = odsa->g = odsa->pub_key = NULL;
	DSA_free(odsa);

	osig->r = osig->s = NULL;
	DSA_SIG_free(osig);

	return ret;
}

/**
   \ingroup Core_Crypto
   \brief Recovers message digest from the signature
   \param out Where to write decrypted data to
   \param in Encrypted data
   \param length Length of encrypted data
   \param rsa RSA public key
   \return size of recovered message digest
*/
int 
__ops_rsa_public_decrypt(unsigned char *out, const unsigned char *in,
		       size_t length, const __ops_rsa_pubkey_t * rsa)
{
	RSA            *orsa;
	int             n;

	orsa = RSA_new();
	orsa->n = rsa->n;
	orsa->e = rsa->e;

	n = RSA_public_decrypt((int)length, in, out, orsa, RSA_NO_PADDING);

	orsa->n = orsa->e = NULL;
	RSA_free(orsa);

	return n;
}

/**
   \ingroup Core_Crypto
   \brief Signs data with RSA
   \param out Where to write signature
   \param in Data to sign
   \param length Length of data
   \param srsa RSA secret key
   \param rsa RSA public key
   \return number of bytes decrypted
*/
int 
__ops_rsa_private_encrypt(unsigned char *out,
			const unsigned char *in,
			size_t length,
			const __ops_rsa_seckey_t *srsa,
			const __ops_rsa_pubkey_t *rsa)
{
	RSA            *orsa;
	int             n;

	orsa = RSA_new();
	orsa->n = rsa->n;	/* XXX: do we need n? */
	orsa->d = srsa->d;
	orsa->p = srsa->q;
	orsa->q = srsa->p;

	/* debug */
	orsa->e = rsa->e;
	/* If this isn't set, it's very likely that the programmer hasn't */
	/* decrypted the secret key. RSA_check_key segfaults in that case. */
	/* Use __ops_decrypt_seckey() to do that. */
	if (orsa->d == NULL) {
		(void) fprintf(stderr, "orsa is not set\n");
		return 0;
	}
	if (RSA_check_key(orsa) != 1) {
		(void) fprintf(stderr, "RSA_check_key is not set\n");
		return 0;
	}
	/* end debug */

	n = RSA_private_encrypt((int)length, in, out, orsa, RSA_NO_PADDING);

	orsa->n = orsa->d = orsa->p = orsa->q = NULL;
	RSA_free(orsa);

	return n;
}

/**
\ingroup Core_Crypto
\brief Decrypts RSA-encrypted data
\param out Where to write the plaintext
\param in Encrypted data
\param length Length of encrypted data
\param srsa RSA secret key
\param rsa RSA public key
\return size of recovered plaintext
*/
int 
__ops_rsa_private_decrypt(unsigned char *out, const unsigned char *in,
			size_t length, const __ops_rsa_seckey_t * srsa,
			const __ops_rsa_pubkey_t * rsa)
{
	RSA            *orsa;
	int             n;
	char            errbuf[1024];

	orsa = RSA_new();
	orsa->n = rsa->n;	/* XXX: do we need n? */
	orsa->d = srsa->d;
	orsa->p = srsa->q;
	orsa->q = srsa->p;

	/* debug */
	orsa->e = rsa->e;
	if (RSA_check_key(orsa) != 1) {
		(void) fprintf(stderr, "RSA_check_key is not set\n");
		return 0;
	}
	/* end debug */

	n = RSA_private_decrypt((int)length, in, out, orsa, RSA_NO_PADDING);

	if (__ops_get_debug_level(__FILE__)) {
		printf("__ops_rsa_private_decrypt: n=%d\n",n);
	}

	errbuf[0] = '\0';
	if (n == -1) {
		unsigned long   err = ERR_get_error();
		ERR_error_string(err, &errbuf[0]);
		fprintf(stderr, "openssl error : %s\n", errbuf);
	}
	orsa->n = orsa->d = orsa->p = orsa->q = NULL;
	RSA_free(orsa);

	return n;
}

/**
   \ingroup Core_Crypto
   \brief RSA-encrypts data
   \param out Where to write the encrypted data
   \param in Plaintext
   \param length Size of plaintext
   \param rsa RSA Public Key
*/
int 
__ops_rsa_public_encrypt(unsigned char *out,
			const unsigned char *in,
			size_t length,
			const __ops_rsa_pubkey_t *rsa)
{
	RSA            *orsa;
	int             n;

	/* printf("__ops_rsa_public_encrypt: length=%ld\n", length); */

	orsa = RSA_new();
	orsa->n = rsa->n;
	orsa->e = rsa->e;

	/* printf("len: %ld\n", length); */
	/* __ops_print_bn("n: ", orsa->n); */
	/* __ops_print_bn("e: ", orsa->e); */
	n = RSA_public_encrypt((int)length, in, out, orsa, RSA_NO_PADDING);

	if (n == -1) {
		BIO            *fd_out;

		fd_out = BIO_new_fd(fileno(stderr), BIO_NOCLOSE);
		ERR_print_errors(fd_out);
	}
	orsa->n = orsa->e = NULL;
	RSA_free(orsa);

	return n;
}

/**
   \ingroup Core_Crypto
   \brief initialises openssl
   \note Would usually call __ops_init() instead
   \sa __ops_init()
*/
void 
__ops_crypto_init()
{
#ifdef DMALLOC
	CRYPTO_malloc_debug_init();
	CRYPTO_dbg_set_options(V_CRYPTO_MDEBUG_ALL);
	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
#endif
}

/**
   \ingroup Core_Crypto
   \brief Finalise openssl
   \note Would usually call __ops_finish() instead
   \sa __ops_finish()
*/
void 
__ops_crypto_finish()
{
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_state((unsigned long)0);
#ifdef DMALLOC
	CRYPTO_mem_leaks_fp(stderr);
#endif
}

/**
   \ingroup Core_Hashes
   \brief Get Hash name
   \param hash Hash struct
   \return Hash name
*/
const char     *
__ops_text_from_hash(__ops_hash_t * hash)
{
	return hash->name;
}

/**
 \ingroup HighLevel_KeyGenerate
 \brief Generates an RSA keypair
 \param numbits Modulus size
 \param e Public Exponent
 \param keydata Pointer to keydata struct to hold new key
 \return 1 if key generated successfully; otherwise 0
 \note It is the caller's responsibility to call __ops_keydata_free(keydata)
*/
unsigned 
__ops_rsa_generate_keypair(const int numbits,
			const unsigned long e,
			__ops_keydata_t *keydata)
{
	__ops_seckey_t *seckey = NULL;
	RSA            *rsa = NULL;
	BN_CTX         *ctx = BN_CTX_new();
	__ops_createinfo_t *cinfo;
	__ops_memory_t   *mem;

	__ops_keydata_init(keydata, OPS_PTAG_CT_SECRET_KEY);
	seckey = __ops_get_writable_seckey(keydata);

	/* generate the key pair */

	rsa = RSA_generate_key(numbits, e, NULL, NULL);

	/* populate __ops key from ssl key */

	seckey->pubkey.version = 4;
	seckey->pubkey.birthtime = time(NULL);
	seckey->pubkey.days_valid = 0;
	seckey->pubkey.alg = OPS_PKA_RSA;

	seckey->pubkey.key.rsa.n = BN_dup(rsa->n);
	seckey->pubkey.key.rsa.e = BN_dup(rsa->e);

	seckey->s2k_usage = OPS_S2KU_ENCRYPTED_AND_HASHED;
	seckey->s2k_specifier = OPS_S2KS_SALTED;
	/* seckey->s2k_specifier=OPS_S2KS_SIMPLE; */
	seckey->alg = OPS_SA_CAST5;	/* \todo make param */
	seckey->hash_alg = OPS_HASH_SHA1;	/* \todo make param */
	seckey->octetc = 0;
	seckey->checksum = 0;

	seckey->key.rsa.d = BN_dup(rsa->d);
	seckey->key.rsa.p = BN_dup(rsa->p);
	seckey->key.rsa.q = BN_dup(rsa->q);
	seckey->key.rsa.u = BN_mod_inverse(NULL, rsa->p, rsa->q, ctx);
	if (seckey->key.rsa.u == NULL) {
		(void) fprintf(stderr, "seckey->key.rsa.u is NULL\n");
		return 0;
	}
	BN_CTX_free(ctx);

	RSA_free(rsa);

	__ops_keyid(keydata->key_id, OPS_KEY_ID_SIZE, OPS_KEY_ID_SIZE,
			&keydata->key.seckey.pubkey);
	__ops_fingerprint(&keydata->fingerprint, &keydata->key.seckey.pubkey);

	/* Generate checksum */

	cinfo = NULL;
	mem = NULL;

	__ops_setup_memory_write(&cinfo, &mem, 128);

	__ops_push_skey_checksum_writer(cinfo, seckey);

	switch (seckey->pubkey.alg) {
		/* case OPS_PKA_DSA: */
		/* return __ops_write_mpi(key->key.dsa.x,info); */

	case OPS_PKA_RSA:
	case OPS_PKA_RSA_ENCRYPT_ONLY:
	case OPS_PKA_RSA_SIGN_ONLY:
		if (!__ops_write_mpi(seckey->key.rsa.d, cinfo) ||
		    !__ops_write_mpi(seckey->key.rsa.p, cinfo) ||
		    !__ops_write_mpi(seckey->key.rsa.q, cinfo) ||
		    !__ops_write_mpi(seckey->key.rsa.u, cinfo)) {
			return 0;
		}
		break;

		/* case OPS_PKA_ELGAMAL: */
		/* return __ops_write_mpi(key->key.elgamal.x,info); */

	default:
		(void) fprintf(stderr, "Bad seckey->pubkey.alg\n");
		return 0;
	}

	/* close rather than pop, since its the only one on the stack */
	__ops_writer_close(cinfo);
	__ops_teardown_memory_write(cinfo, mem);

	/* should now have checksum in seckey struct */

	/* test */
	if (__ops_get_debug_level(__FILE__)) {
		test_seckey(seckey);
	}

	return 1;
}

/**
 \ingroup HighLevel_KeyGenerate
 \brief Creates a self-signed RSA keypair
 \param numbits Modulus size
 \param e Public Exponent
 \param userid User ID
 \return The new keypair or NULL

 \note It is the caller's responsibility to call __ops_keydata_free(keydata)
 \sa __ops_rsa_generate_keypair()
 \sa __ops_keydata_free()
*/
__ops_keydata_t  *
__ops_rsa_create_selfsigned_keypair(const int numbits,
			const unsigned long e,
			__ops_user_id_t *userid)
{
	__ops_keydata_t  *keydata = NULL;

	keydata = __ops_keydata_new();
	if (__ops_rsa_generate_keypair(numbits, e, keydata) != 1 ||
	    __ops_add_selfsigned_userid_to_keydata(keydata, userid) != 1) {
		__ops_keydata_free(keydata);
		return NULL;
	}
	return keydata;
}

DSA_SIG        *
__ops_dsa_sign(unsigned char *hashbuf,
		unsigned hashsize,
		const __ops_dsa_seckey_t *sdsa,
		const __ops_dsa_pubkey_t *dsa)
{
	DSA_SIG        *dsasig;
	DSA            *odsa;

	odsa = DSA_new();
	odsa->p = dsa->p;
	odsa->q = dsa->q;
	odsa->g = dsa->g;
	odsa->pub_key = dsa->y;
	odsa->priv_key = sdsa->x;

	dsasig = DSA_do_sign(hashbuf, (int)hashsize, odsa);

	odsa->p = odsa->q = odsa->g = odsa->pub_key = odsa->priv_key = NULL;
	DSA_free(odsa);

	return dsasig;
}
