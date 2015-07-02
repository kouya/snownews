/* Snownews - A lightweight console RSS newsreader
 * $Id: digcalc.c 1146 2005-09-10 10:05:21Z kiza $
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 * http://kiza.kcore.de/software/snownews/
 *
 * digcalc.c
 * 
 * This is the sample implementation from RFC 2617.
 * The code has been modified to work with Colin Plumb's
 * MD5 implementation rather than using RSA's.
 */

#include <string.h>
#include "digcalc.h"
#include <openssl/evp.h>


/* calculate H(A1) as per spec */
void DigestCalcHA1(
    IN char * pszAlg,
    IN char * pszUserName,
    IN char * pszRealm,
    IN char * pszPassword,
    IN char * pszNonce,
    IN char * pszCNonce,
    OUT HASHHEX SessionKey
    )
{
	EVP_MD_CTX mdctx;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;
	int i;
	HASH HA1;
	
	EVP_DigestInit(&mdctx, EVP_md5());
	EVP_DigestUpdate(&mdctx, pszUserName, strlen(pszUserName));
	EVP_DigestUpdate(&mdctx, ":", 1);
	EVP_DigestUpdate(&mdctx, pszRealm, strlen(pszRealm));
	EVP_DigestUpdate(&mdctx, ":", 1);
	EVP_DigestUpdate(&mdctx, pszPassword, strlen(pszPassword));
	EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
	EVP_MD_CTX_cleanup(&mdctx);
	
	if (strcmp(pszAlg, "md5-sess") == 0) {
		EVP_DigestInit(&mdctx, EVP_md5());
		EVP_DigestUpdate(&mdctx, HA1, HASHLEN);
		EVP_DigestUpdate(&mdctx, ":", 1);
		EVP_DigestUpdate(&mdctx, pszNonce, strlen(pszNonce));
		EVP_DigestUpdate(&mdctx, ":", 1);
		EVP_DigestUpdate(&mdctx, pszCNonce, strlen(pszCNonce));
		EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
		EVP_MD_CTX_cleanup(&mdctx);
	};
	
	for (i = 0; i < md_len; i++) {
		sprintf(&SessionKey[2*i], "%02x", md_value[i]);
	}
};

/* calculate request-digest/response-digest as per HTTP Digest spec */
void DigestCalcResponse(
    IN HASHHEX HA1,           /* H(A1) */
    IN char * pszNonce,       /* nonce from server */
    IN char * pszNonceCount,  /* 8 hex digits */
    IN char * pszCNonce,      /* client nonce */
    IN char * pszQop,         /* qop-value: "", "auth", "auth-int" */
    IN char * pszMethod,      /* method from the request */
    IN char * pszDigestUri,   /* requested URL */
    IN HASHHEX HEntity,       /* H(entity body) if qop="auth-int" */
    OUT HASHHEX Response      /* request-digest or response-digest */
    )
{
	EVP_MD_CTX mdctx;
	HASHHEX HA2Hex;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;
	int i;
	
	/* calculate H(A2) */
	EVP_DigestInit(&mdctx, EVP_md5());
	EVP_DigestUpdate(&mdctx, pszMethod, strlen(pszMethod));
	EVP_DigestUpdate(&mdctx, ":", 1);
	EVP_DigestUpdate(&mdctx, pszDigestUri, strlen(pszDigestUri));
	if (strcmp(pszQop, "auth-int") == 0) {
		EVP_DigestUpdate(&mdctx, ":", 1);
		EVP_DigestUpdate(&mdctx, HEntity, HASHHEXLEN);
	};
	EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
	EVP_MD_CTX_cleanup(&mdctx);
	
	for (i = 0; i < md_len; i++) {
		sprintf(&HA2Hex[2*i], "%02x", md_value[i]);
	}
	
	/* calculate response */
	EVP_DigestInit(&mdctx, EVP_md5());
	EVP_DigestUpdate(&mdctx, HA1, HASHHEXLEN);
	EVP_DigestUpdate(&mdctx, ":", 1);
	EVP_DigestUpdate(&mdctx, pszNonce, strlen(pszNonce));
	EVP_DigestUpdate(&mdctx, ":", 1);
	if (*pszQop) {
		EVP_DigestUpdate(&mdctx, pszNonceCount, strlen(pszNonceCount));
		EVP_DigestUpdate(&mdctx, ":", 1);
		EVP_DigestUpdate(&mdctx, pszCNonce, strlen(pszCNonce));
		EVP_DigestUpdate(&mdctx, ":", 1);
		EVP_DigestUpdate(&mdctx, pszQop, strlen(pszQop));
		EVP_DigestUpdate(&mdctx, ":", 1);
	};
	EVP_DigestUpdate(&mdctx, HA2Hex, HASHHEXLEN);
	EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
	EVP_MD_CTX_cleanup(&mdctx);
	
	for (i = 0; i < md_len; i++) {
		sprintf(&Response[2*i], "%02x", md_value[i]);
	}
	
#if 0
	/* This is the old MD5 digest calculator for a reference until I am sure
	 * the new code works for sure */
	/* *************************** */
      struct MD5Context Md5Ctx;
      HASH HA2;
      HASH RespHash;
      HASHHEX HA2Hex;

      /* calculate H(A2) */
      MD5Init(&Md5Ctx);
      MD5Update(&Md5Ctx, pszMethod, strlen(pszMethod));
      MD5Update(&Md5Ctx, ":", 1);
      MD5Update(&Md5Ctx, pszDigestUri, strlen(pszDigestUri));
      if (strcmp(pszQop, "auth-int") == 0) {
            MD5Update(&Md5Ctx, ":", 1);
            MD5Update(&Md5Ctx, HEntity, HASHHEXLEN);
      };
      MD5Final(HA2, &Md5Ctx);
       CvtHex(HA2, HA2Hex);

      /* calculate response */
      MD5Init(&Md5Ctx);
      MD5Update(&Md5Ctx, HA1, HASHHEXLEN);
      MD5Update(&Md5Ctx, ":", 1);
      MD5Update(&Md5Ctx, pszNonce, strlen(pszNonce));
      MD5Update(&Md5Ctx, ":", 1);
      if (*pszQop) {

          MD5Update(&Md5Ctx, pszNonceCount, strlen(pszNonceCount));
          MD5Update(&Md5Ctx, ":", 1);
          MD5Update(&Md5Ctx, pszCNonce, strlen(pszCNonce));
          MD5Update(&Md5Ctx, ":", 1);
          MD5Update(&Md5Ctx, pszQop, strlen(pszQop));
          MD5Update(&Md5Ctx, ":", 1);
      };
      MD5Update(&Md5Ctx, HA2Hex, HASHHEXLEN);
      MD5Final(RespHash, &Md5Ctx);
      CvtHex(RespHash, Response);
#endif
};
