/*	$NetBSD: pkcs11-list.c,v 1.1.1.1 2009/10/25 00:01:34 christos Exp $	*/

/* pkcs11-list [-P] [-m module] [-s slot] [-i $id | -l $label] [-p $pin] */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include "cryptoki.h"

#ifdef WIN32
#include "win32.c"
#else
#ifndef FORCE_STATIC_PROVIDER
#include "unix.c"
#endif
#endif

#if !(defined(HAVE_GETPASSPHRASE) || (defined (__SVR4) && defined (__sun)))
#define getpassphrase(x)	getpass(x)
#endif

int
main(int argc, char *argv[])
{
    CK_RV rv;
    CK_SLOT_ID slot = 0;
    CK_SESSION_HANDLE hSession;
    CK_UTF8CHAR *pin = NULL;
    CK_BYTE attr_id[2];
    CK_OBJECT_HANDLE akey[50];
    char *label = NULL;
    int error = 0, public = 0, all = 0;
    unsigned int i = 0, id = 0;
    int c, errflg = 0;
    CK_ULONG ulObjectCount;
    CK_ATTRIBUTE search_template[] = {
	{CKA_ID, &attr_id, sizeof(attr_id)}
    };
    extern char *optarg;
    extern int optopt;
    char *pk11_provider;

    pk11_provider = getenv("PKCS11_PROVIDER");
    if (pk11_provider != NULL)
        pk11_libname = pk11_provider;

    while ((c = getopt(argc, argv, ":m:s:i:l:p:P")) != -1) {
	switch (c) {
	case 'P':
	    public = 1;
	    break;
	case 'm':
	    pk11_libname = optarg;
	    break;
	case 's':
	    slot = atoi(optarg);
	    break;
	case 'i':
	    id = atoi(optarg);
	    id &= 0xffff;
	    break;
	case 'l':
	    label = optarg;
	    break;
	case 'p':
	    pin = (CK_UTF8CHAR *)optarg;
	    break;
	case ':':
	    fprintf(stderr, "Option -%c requires an operand\n", optopt);
	    errflg++;
	    break;
	case '?':
	default:
	    fprintf(stderr, "Unrecognised option: -%c\n", optopt);
	    errflg++;
	}
    }
    if (errflg) {
	fprintf(stderr,
		"usage: pkcs11-list [-P] [-m module] [-s slot] "
		"[-i id | -l label] [-p pin]\n");
	exit(1);
    }
    if ((!id) && (!label))
	all = 1;
    if (slot)
	printf("slot %lu\n", slot);
    if (id) {
	printf("id %i\n", id);
	attr_id[0] = (id >> 8) & 0xff;
	attr_id[1] = id & 0xff;
    } else if (label) {
	printf("label %s\n", label);
	search_template[0].type = CKA_LABEL;
	search_template[0].pValue = label;
	search_template[0].ulValueLen = strlen(label);
    }

    /* Initialize the CRYPTOKI library */
    rv = C_Initialize(NULL_PTR);
    if (rv != CKR_OK) {
	if (rv == 0xfe)
	    fprintf(stderr,
		    "Can't load or link module \"%s\"\n",
		    pk11_libname);
	else
	    fprintf(stderr, "C_Initialize: Error = 0x%.8lX\n", rv);
	exit(1);
    }

    /* Open a session on the slot found */
    rv = C_OpenSession(slot, CKF_SERIAL_SESSION,
		       NULL_PTR, NULL_PTR, &hSession);
    if (rv != CKR_OK) {
	fprintf(stderr, "C_OpenSession: Error = 0x%.8lX\n", rv);
	error = 1;
	goto exit_program;
    }

    /* Login to the Token (Keystore) */
    if (!public) {
	if (!pin)
	    pin = (CK_UTF8CHAR *)getpassphrase("Enter Pin: ");
	rv = C_Login(hSession, CKU_USER, pin, strlen((char *)pin));
	memset(pin, 0, strlen((char *)pin));
	if (rv != CKR_OK) {
	    fprintf(stderr, "C_Login: Error = 0x%.8lX\n", rv);
	    error = 1;
	    goto exit_session;
	}
    }

    rv = C_FindObjectsInit(hSession, search_template, all ? 0 : 1); 
    if (rv != CKR_OK) {
	fprintf(stderr, "C_FindObjectsInit: Error = 0x%.8lX\n", rv);
	error = 1;
	goto exit_session;
    }
    
    ulObjectCount = 1;
    while (ulObjectCount) {
	rv = C_FindObjects(hSession, akey, 50, &ulObjectCount);
	if (rv != CKR_OK) {
	    fprintf(stderr, "C_FindObjects: Error = 0x%.8lX\n", rv);
	    error = 1;
	    goto exit_search;
	}

	for (i = 0; i < ulObjectCount; i++) {
	    CK_OBJECT_CLASS oclass = 0;
	    CK_BYTE labelbuf[64 + 1];
	    CK_BYTE idbuf[64];
	    CK_ATTRIBUTE attr_template[] = {
		{CKA_CLASS, &oclass, sizeof(oclass)},
		{CKA_LABEL, labelbuf, sizeof(labelbuf) - 1},
		{CKA_ID, idbuf, sizeof(idbuf)}
	    };
	    unsigned int j, len;

	    memset(labelbuf, 0, sizeof(labelbuf));
	    memset(idbuf, 0, sizeof(idbuf));

	    rv = C_GetAttributeValue(hSession, akey[i], attr_template, 3);
	    if (rv != CKR_OK) {
		fprintf(stderr,
			"C_GetAttributeValue[%u]: rv = 0x%.8lX\n", i, rv);
		if (rv == CKR_BUFFER_TOO_SMALL)
		    fprintf(stderr, "%u too small: %lu %lu %lu\n", i,
			    attr_template[0].ulValueLen,
			    attr_template[1].ulValueLen,
			    attr_template[2].ulValueLen);
		error = 1;
		continue;
	    }

	    len = attr_template[2].ulValueLen;
	    printf("object[%u]: handle %lu class %lu "
		   "label[%lu] '%s' id[%lu] ",
		   i, akey[i], oclass,
		   attr_template[1].ulValueLen, labelbuf,
		   attr_template[2].ulValueLen);
	    if (len == 2) {
		id = (idbuf[0] << 8) & 0xff00;
		id |= idbuf[1] & 0xff;
		printf("%u\n", id);
	    } else {
		if (len > 8)
		    len = 8;
		if (len > 0)
		    printf("0x");
		for (j = 0; j < len; j++)
		    printf("%02x", idbuf[j]);
		if (attr_template[2].ulValueLen > len)
		    printf("...\n");
		else
		    printf("\n");
	    }
	}
    }

 exit_search:
    rv = C_FindObjectsFinal(hSession);
    if (rv != CKR_OK) {
	fprintf(stderr, "C_FindObjectsFinal: Error = 0x%.8lX\n", rv);
	error = 1;
    }

 exit_session:
    (void) C_CloseSession(hSession);

 exit_program:
    (void) C_Finalize(NULL_PTR);

    exit(error);
}
