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

#include "digcalc.h"

// calculate H(A1) as per spec
void DigestCalcHA1 (const char* pszAlg, const char* pszUserName, const char* pszRealm,
	const char* pszPassword, const char* pszNonce, const char* pszCNonce,
	HASHHEX SessionKey)
{
	struct HashMD5 hash;
	hash_md5_init (&hash);
	hash_md5_data (&hash, pszUserName, strlen(pszUserName));
	hash_md5_data (&hash, ":", 1);
	hash_md5_data (&hash, pszRealm, strlen(pszRealm));
	hash_md5_data (&hash, ":", 1);
	hash_md5_data (&hash, pszPassword, strlen(pszPassword));
	hash_md5_finish (&hash);
	if (strcmp (pszAlg, "md5-sess") == 0) {
		hash_md5_init (&hash);
		hash_md5_data (&hash, hash.hash, sizeof(hash.hash));
		hash_md5_data (&hash, ":", 1);
		hash_md5_data (&hash, pszNonce, strlen(pszNonce));
		hash_md5_data (&hash, ":", 1);
		hash_md5_data (&hash, pszCNonce, strlen(pszCNonce));
		hash_md5_finish (&hash);
	}
	hash_md5_to_text (&hash, SessionKey);
}

// calculate request-digest/response-digest as per HTTP Digest spec
void DigestCalcResponse(
    const HASHHEX HA1,		// H(A1)
    const char* pszNonce,	// nonce from server
    const char* pszNonceCount,	// 8 hex digits
    const char* pszCNonce,	// client nonce
    const char* pszQop,		// qop-value: "", "auth", "auth-int"
    const char* pszMethod,	// method from the request
    const char* pszDigestUri,	// requested URL
    const HASHHEX HEntity,	// H(entity body) if qop="auth-int"
    HASHHEX Response		// request-digest or response-digest
    )
{
	// calculate H(A2)
	struct HashMD5 hash;
	hash_md5_init (&hash);
	hash_md5_data (&hash, pszMethod, strlen(pszMethod));
	hash_md5_data (&hash, ":", 1);
	hash_md5_data (&hash, pszDigestUri, strlen(pszDigestUri));
	if (strcmp (pszQop, "auth-int") == 0) {
		hash_md5_data (&hash, ":", 1);
		hash_md5_data (&hash, HEntity, HASHHEXLEN);
	}
	hash_md5_finish (&hash);
	HASHHEX HA2Hex;
	hash_md5_to_text (&hash, HA2Hex);

	// calculate response
	hash_md5_init (&hash);
	hash_md5_data (&hash, HA1, HASHHEXLEN);
	hash_md5_data (&hash, ":", 1);
	hash_md5_data (&hash, pszNonce, strlen(pszNonce));
	hash_md5_data (&hash, ":", 1);
	if (*pszQop) {
		hash_md5_data (&hash, pszNonceCount, strlen(pszNonceCount));
		hash_md5_data (&hash, ":", 1);
		hash_md5_data (&hash, pszCNonce, strlen(pszCNonce));
		hash_md5_data (&hash, ":", 1);
		hash_md5_data (&hash, pszQop, strlen(pszQop));
		hash_md5_data (&hash, ":", 1);
	};
	hash_md5_data (&hash, HA2Hex, HASHHEXLEN);
	hash_md5_finish (&hash);
	hash_md5_to_text (&hash, Response);
}
