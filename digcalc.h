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

#pragma once

#define HASHLEN 16
typedef char HASH[HASHLEN];
#define HASHHEXLEN 32
typedef char HASHHEX[HASHHEXLEN+1];

// calculate H(A1) as per HTTP Digest spec
void DigestCalcHA1 (const char* pszAlg, const char* pszUserName, const char* pszRealm,
			const char* pszPassword, const char* pszNonce, const char* pszCNonce,
			HASHHEX SessionKey);

// calculate request-digest/response-digest as per HTTP Digest spec
void DigestCalcResponse (
    const HASHHEX HA1,		// H(A1)
    const char* pszNonce,	// nonce from server
    const char* pszNonceCount,	// 8 hex digits
    const char* pszCNonce,	// client nonce
    const char* pszQop,		// qop-value: "", "auth", "auth-int"
    const char* pszMethod,	// method from the request
    const char* pszDigestUri,	// requested URL
    const HASHHEX HEntity,	// H(entity body) if qop="auth-int"
    HASHHEX Response		// request-digest or response-digest
    );
