/*
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: auth1.c,v 1.17 2001/02/13 22:49:40 markus Exp $");

#include "xmalloc.h"
#include "rsa.h"
#include "ssh1.h"
#include "packet.h"
#include "buffer.h"
#include "mpaux.h"
#include "log.h"
#include "servconf.h"
#include "compat.h"
#include "auth.h"
#include "session.h"

#ifdef KRB5
extern krb5_context ssh_context;
krb5_principal tkt_client = NULL;    /* Principal from the received ticket. 
Also is used as an indication of succesful krb5 authentization. */
#endif

/* import */
extern ServerOptions options;
extern char *forced_command;

/*
 * convert ssh auth msg type into description
 */
static char *
get_authname(int type)
{
	static char buf[1024];
	switch (type) {
	case SSH_CMSG_AUTH_PASSWORD:
		return "password";
	case SSH_CMSG_AUTH_RSA:
		return "rsa";
	case SSH_CMSG_AUTH_RHOSTS_RSA:
		return "rhosts-rsa";
	case SSH_CMSG_AUTH_RHOSTS:
		return "rhosts";
	case SSH_CMSG_AUTH_TIS:
	case SSH_CMSG_AUTH_TIS_RESPONSE:
		return "challenge-response";
#if defined(KRB4) || defined(KRB5)
	case SSH_CMSG_AUTH_KERBEROS:
		return "kerberos";
#endif
	}
	snprintf(buf, sizeof buf, "bad-auth-msg-%d", type);
	return buf;
}

/*
 * read packets, try to authenticate the user and
 * return only if authentication is successful
 */
static void
do_authloop(Authctxt *authctxt)
{
	int authenticated = 0;
	u_int bits;
	RSA *client_host_key;
	BIGNUM *n;
	char *client_user, *password;
	char info[1024];
	u_int dlen;
	int plen, nlen, elen;
	u_int ulen;
	int type = 0;
	struct passwd *pw = authctxt->pw;

	debug("Attempting authentication for %s%.100s.",
	     authctxt->valid ? "" : "illegal user ", authctxt->user);

	/* If the user has no password, accept authentication immediately. */
	if (options.password_authentication &&
#ifdef KRB5
	    !options.kerberos_authentication &&
#endif /* KRB5 */
#ifdef KRB4
	    (!options.kerberos_authentication || options.krb4_or_local_passwd) &&
#endif /* KRB4 */
	    auth_password(pw, "")) {
		auth_log(authctxt, 1, "without authentication", "");
		return;
	}

	/* Indicate that authentication is needed. */
	packet_start(SSH_SMSG_FAILURE);
	packet_send();
	packet_write_wait();

	for (;;) {
		/* default to fail */
		authenticated = 0;

		info[0] = '\0';

		/* Get a packet from the client. */
		type = packet_read(&plen);

		/* Process the packet. */
		switch (type) {
#ifdef AFS
#ifndef KRB5
		case SSH_CMSG_HAVE_KERBEROS_TGT:
			if (!options.krb4_tgt_passing) {
				verbose("Kerberos v4 tgt passing disabled.");
				break;
			} else {
				/* Accept Kerberos v4 tgt. */
				char *tgt = packet_get_string(&dlen);
				packet_integrity_check(plen, 4 + dlen, type);
				if (!auth_krb4_tgt(pw, tgt))
					verbose("Kerberos v4 tgt REFUSED for %.100s", authctxt->user);
				xfree(tgt);
			}
			continue;
#endif /* !KRB5 */
		case SSH_CMSG_HAVE_AFS_TOKEN:
			if (!options.afs_token_passing || !k_hasafs()) {
				verbose("AFS token passing disabled.");
				break;
			} else {
				/* Accept AFS token. */
				char *token_string = packet_get_string(&dlen);
				packet_integrity_check(plen, 4 + dlen, type);
				if (!auth_afs_token(pw, token_string))
					verbose("AFS token REFUSED for %.100s", authctxt->user);
				xfree(token_string);
			}
			continue;
#endif /* AFS */
#if defined(KRB4) || defined(KRB5)
		case SSH_CMSG_AUTH_KERBEROS:
			if (!options.kerberos_authentication) {
				verbose("Kerberos authentication disabled.");
				break;
			} else {
				unsigned int length;
				char *kdata = packet_get_string(&length);
				packet_integrity_check(plen, 4 + length, type);

				/* 4 == KRB_PROT_VERSION */
				if (kdata[0] == 4) {
#ifndef KRB4
					verbose("Kerberos v4 authentication disabled.");
#else
					/* Try Kerberos v4 authentication. */
					KTEXT_ST auth;
					char *tkt_user = NULL;
					auth.length = length;

					if (authctxt->valid) {
					    if (auth.length < MAX_KTXT_LEN)
						memcpy(auth.dat, kdata, auth.length);
					    authenticated = auth_krb4(pw->pw_name, &auth, &tkt_user);
					    if (authenticated) {
						snprintf(info, sizeof info,
							 " tktuser %.100s", tkt_user);
						xfree(tkt_user);
					    }
					}
#endif /* KRB4 */
				} else {
#ifndef KRB5
					verbose("Kerberos v5 authentication disabled.");
#else
				  	krb5_data k5data; 
					k5data.length = length;
					k5data.data = kdata;
  #if 0	
					if (krb5_init_context(&ssh_context)) {
						verbose("Error while initializing Kerberos V5.");
						break;
					}
					krb5_init_ets(ssh_context);
  #endif
					/* pw->name is passed just for logging purposes */
					if (authctxt->valid
					    && auth_krb5(pw->pw_name, &k5data, &tkt_client)) {
					  	/* authorize client against .k5login */
					  	if (krb5_kuserok(ssh_context,
						      tkt_client,
						      pw->pw_name))
						  	authenticated = 1;
					}
#endif /* KRB5 */
				}
				xfree(kdata);
			}
			break;
#endif /* KRB4 || KRB5 */

		case SSH_CMSG_AUTH_RHOSTS:
			if (!options.rhosts_authentication) {
				verbose("Rhosts authentication disabled.");
				break;
			}
			/*
			 * Get client user name.  Note that we just have to
			 * trust the client; this is one reason why rhosts
			 * authentication is insecure. (Another is
			 * IP-spoofing on a local network.)
			 */
			client_user = packet_get_string(&ulen);
			packet_integrity_check(plen, 4 + ulen, type);

			/* Try to authenticate using /etc/hosts.equiv and .rhosts. */
			authenticated = auth_rhosts(pw, client_user);

			snprintf(info, sizeof info, " ruser %.100s", client_user);
			xfree(client_user);
			break;

		case SSH_CMSG_AUTH_RHOSTS_RSA:
			if (!options.rhosts_rsa_authentication) {
				verbose("Rhosts with RSA authentication disabled.");
				break;
			}
			/*
			 * Get client user name.  Note that we just have to
			 * trust the client; root on the client machine can
			 * claim to be any user.
			 */
			client_user = packet_get_string(&ulen);

			/* Get the client host key. */
			client_host_key = RSA_new();
			if (client_host_key == NULL)
				fatal("RSA_new failed");
			client_host_key->e = BN_new();
			client_host_key->n = BN_new();
			if (client_host_key->e == NULL || client_host_key->n == NULL)
				fatal("BN_new failed");
			bits = packet_get_int();
			packet_get_bignum(client_host_key->e, &elen);
			packet_get_bignum(client_host_key->n, &nlen);

			if (bits != BN_num_bits(client_host_key->n))
				verbose("Warning: keysize mismatch for client_host_key: "
				    "actual %d, announced %d", BN_num_bits(client_host_key->n), bits);
			packet_integrity_check(plen, (4 + ulen) + 4 + elen + nlen, type);

			authenticated = auth_rhosts_rsa(pw, client_user, client_host_key);
			RSA_free(client_host_key);

			snprintf(info, sizeof info, " ruser %.100s", client_user);
			xfree(client_user);
			break;

		case SSH_CMSG_AUTH_RSA:
			if (!options.rsa_authentication) {
				verbose("RSA authentication disabled.");
				break;
			}
			/* RSA authentication requested. */
			n = BN_new();
			packet_get_bignum(n, &nlen);
			packet_integrity_check(plen, nlen, type);
			authenticated = auth_rsa(pw, n);
			BN_clear_free(n);
			break;

		case SSH_CMSG_AUTH_PASSWORD:
			if (!options.password_authentication) {
				verbose("Password authentication disabled.");
				break;
			}
			/*
			 * Read user password.  It is in plain text, but was
			 * transmitted over the encrypted channel so it is
			 * not visible to an outside observer.
			 */
			password = packet_get_string(&dlen);
			packet_integrity_check(plen, 4 + dlen, type);

			/* Try authentication with the password. */
			authenticated = auth_password(pw, password);

			memset(password, 0, strlen(password));
			xfree(password);
			break;

		case SSH_CMSG_AUTH_TIS:
			debug("rcvd SSH_CMSG_AUTH_TIS");
			if (options.challenge_reponse_authentication == 1) {
				char *challenge = get_challenge(authctxt, authctxt->style);
				if (challenge != NULL) {
					debug("sending challenge '%s'", challenge);
					packet_start(SSH_SMSG_AUTH_TIS_CHALLENGE);
					packet_put_cstring(challenge);
					packet_send();
					packet_write_wait();
					continue;
				}
			}
			break;
		case SSH_CMSG_AUTH_TIS_RESPONSE:
			debug("rcvd SSH_CMSG_AUTH_TIS_RESPONSE");
			if (options.challenge_reponse_authentication == 1) {
				char *response = packet_get_string(&dlen);
				debug("got response '%s'", response);
				packet_integrity_check(plen, 4 + dlen, type);
				authenticated = verify_response(authctxt, response);
				memset(response, 'r', dlen);
				xfree(response);
			}
			break;

#ifdef KRB5
		case SSH_CMSG_HAVE_KERBEROS_TGT:
			/* Passing krb5 ticket */
			if (!options.krb5_tgt_passing 
                            /*|| !options.krb5_authentication */) {

			}
			
			if (tkt_client == NULL) {
			  /* passing tgt without krb5 authentication */
			}
			
			{
			  krb5_data tgt;
			  u_int tgtlen;
			  tgt.data = packet_get_string(&tgtlen);
			  tgt.length = tgtlen;
			  
			  if (!auth_krb5_tgt(authctxt->user, &tgt, tkt_client))
			    verbose ("Kerberos V5 TGT refused for %.100s", authctxt->user);
			  xfree(tgt.data);
			      
			  break;
			}
#endif /* KRB5 */
		default:
			/*
			 * Any unknown messages will be ignored (and failure
			 * returned) during authentication.
			 */
			log("Unknown message during authentication: type %d", type);
			break;
		}
		if (!authctxt->valid && authenticated)
			fatal("INTERNAL ERROR: authenticated invalid user %s",
			    authctxt->user);

		/* Special handling for root */
		if (authenticated && authctxt->pw->pw_uid == 0 &&
		    !auth_root_allowed(get_authname(type)))
			authenticated = 0;

		/* Log before sending the reply */
		auth_log(authctxt, authenticated, get_authname(type), info);

		if (authenticated)
			return;

		if (authctxt->failures++ > AUTH_FAIL_MAX)
			packet_disconnect(AUTH_FAIL_MSG, authctxt->user);

		packet_start(SSH_SMSG_FAILURE);
		packet_send();
		packet_write_wait();
	}
}

/*
 * Performs authentication of an incoming connection.  Session key has already
 * been exchanged and encryption is enabled.
 */
void
do_authentication()
{
	Authctxt *authctxt;
	struct passwd *pw;
	int plen;
	u_int ulen;
	char *user, *style = NULL;

	/* Get the name of the user that we wish to log in as. */
	packet_read_expect(&plen, SSH_CMSG_USER);

	/* Get the user name. */
	user = packet_get_string(&ulen);
	packet_integrity_check(plen, (4 + ulen), SSH_CMSG_USER);

	if ((style = strchr(user, ':')) != NULL)
		*style++ = 0;

	authctxt = authctxt_new();
	authctxt->user = user;
	authctxt->style = style;

	/* Verify that the user is a valid user. */
	pw = getpwnam(user);
	if (pw && allowed_user(pw)) {
		authctxt->valid = 1;
		pw = pwcopy(pw);
	} else {
		debug("do_authentication: illegal user %s", user);
		pw = NULL;
	}
	authctxt->pw = pw;

	setproctitle("%s", pw ? user : "unknown");

	/*
	 * If we are not running as root, the user must have the same uid as
	 * the server.
	 */
	if (getuid() != 0 && pw && pw->pw_uid != getuid())
		packet_disconnect("Cannot change user when server not running as root.");

	/*
	 * Loop until the user has been authenticated or the connection is
	 * closed, do_authloop() returns only if authentication is successful
	 */
	do_authloop(authctxt);

	/* The user has been authenticated and accepted. */
	packet_start(SSH_SMSG_SUCCESS);
	packet_send();
	packet_write_wait();

	xfree(authctxt->user);
	xfree(authctxt);

	/* Perform session preparation. */
	do_authenticated(pw);
}
