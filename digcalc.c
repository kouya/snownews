// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2004 Oliver Feiler <kiza@kcore.de>
//
// Snownews is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 3
// as published by the Free Software Foundation.
//
// Snownews is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Snownews. If not, see http://www.gnu.org/licenses/.

// This is the sample implementation from RFC 2617.
// The code has been modified to work with Colin Plumb's
// MD5 implementation rather than using RSA's.

#include <string.h>
#include "digcalc.h"
#include <openssl/evp.h>

/* calculate H(A1) as per spec */
void DigestCalcHA1 (const char* pszAlg, const char* pszUserName, const char* pszRealm,
	const char* pszPassword, const char* pszNonce, const char* pszCNonce,
	HASHHEX SessionKey)
{
	EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
	EVP_DigestInit (mdctx, EVP_md5());
	EVP_DigestUpdate (mdctx, pszUserName, strlen(pszUserName));
	EVP_DigestUpdate (mdctx, ":", 1);
	EVP_DigestUpdate (mdctx, pszRealm, strlen(pszRealm));
	EVP_DigestUpdate (mdctx, ":", 1);
	EVP_DigestUpdate (mdctx, pszPassword, strlen(pszPassword));
	unsigned char md_value [EVP_MAX_MD_SIZE];
	unsigned md_len;
	EVP_DigestFinal_ex (mdctx, md_value, &md_len);

	if (strcmp(pszAlg, "md5-sess") == 0) {
		EVP_DigestInit (mdctx, EVP_md5());
		HASH HA1;
		EVP_DigestUpdate (mdctx, HA1, HASHLEN);
		EVP_DigestUpdate (mdctx, ":", 1);
		EVP_DigestUpdate (mdctx, pszNonce, strlen(pszNonce));
		EVP_DigestUpdate (mdctx, ":", 1);
		EVP_DigestUpdate (mdctx, pszCNonce, strlen(pszCNonce));
		EVP_DigestFinal_ex (mdctx, md_value, &md_len);
	}
	EVP_MD_CTX_free (mdctx);

	for (unsigned i = 0; i < md_len; ++i)
		sprintf (&SessionKey[2*i], "%02x", md_value[i]);
};

/* calculate request-digest/response-digest as per HTTP Digest spec */
void DigestCalcResponse(
    const HASHHEX HA1,		/* H(A1) */
    const char* pszNonce,	/* nonce from server */
    const char* pszNonceCount,	/* 8 hex digits */
    const char* pszCNonce,	/* client nonce */
    const char* pszQop,		/* qop-value: "", "auth", "auth-int" */
    const char* pszMethod,	/* method from the request */
    const char* pszDigestUri,	/* requested URL */
    const HASHHEX HEntity,	/* H(entity body) if qop="auth-int" */
    HASHHEX Response		/* request-digest or response-digest */
    )
{
	/* calculate H(A2) */
	EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
	EVP_DigestInit (mdctx, EVP_md5());
	EVP_DigestUpdate (mdctx, pszMethod, strlen(pszMethod));
	EVP_DigestUpdate (mdctx, ":", 1);
	EVP_DigestUpdate (mdctx, pszDigestUri, strlen(pszDigestUri));
	if (strcmp(pszQop, "auth-int") == 0) {
		EVP_DigestUpdate (mdctx, ":", 1);
		EVP_DigestUpdate (mdctx, HEntity, HASHHEXLEN);
	}
	unsigned char md_value [EVP_MAX_MD_SIZE];
	unsigned md_len = 0;
	EVP_DigestFinal_ex (mdctx, md_value, &md_len);

	HASHHEX HA2Hex;
	for (unsigned i = 0; i < md_len; ++i)
		sprintf (&HA2Hex[2*i], "%02x", md_value[i]);

	/* calculate response */
	EVP_DigestInit (mdctx, EVP_md5());
	EVP_DigestUpdate (mdctx, HA1, HASHHEXLEN);
	EVP_DigestUpdate (mdctx, ":", 1);
	EVP_DigestUpdate (mdctx, pszNonce, strlen(pszNonce));
	EVP_DigestUpdate (mdctx, ":", 1);
	if (*pszQop) {
		EVP_DigestUpdate (mdctx, pszNonceCount, strlen(pszNonceCount));
		EVP_DigestUpdate (mdctx, ":", 1);
		EVP_DigestUpdate (mdctx, pszCNonce, strlen(pszCNonce));
		EVP_DigestUpdate (mdctx, ":", 1);
		EVP_DigestUpdate (mdctx, pszQop, strlen(pszQop));
		EVP_DigestUpdate (mdctx, ":", 1);
	};
	EVP_DigestUpdate (mdctx, HA2Hex, HASHHEXLEN);
	EVP_DigestFinal_ex (mdctx, md_value, &md_len);
	EVP_MD_CTX_free (mdctx);

	for (unsigned i = 0; i < md_len; ++i)
		sprintf (&Response[2*i], "%02x", md_value[i]);
};
