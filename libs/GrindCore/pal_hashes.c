#include <assert.h>
#include <stdlib.h>
#include "pal_hashes.h"

// Include the original headers
#include <external/hashes/md2.h>
#include <external/hashes/md4.h>
#include <external/hashes/md5.h>
#include <external/hashes/sha3.h>
#include <external/hashes/sha512.h>
#include <external/hashes/blake3.h>

// Wrapper functions for MD2
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD2_Init(struct md2 *m) {
    MD2_Init(m);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD2_Update(struct md2 *m, const void *v, size_t len) {
    MD2_Update(m, v, len);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD2_Final(void *res, struct md2 *m) {
    MD2_Final(res, m);
}

// Wrapper functions for MD4
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD4_Init(struct md4 *m) {
    MD4_Init(m);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD4_Update(struct md4 *m, const void *v, size_t len) {
    MD4_Update(m, v, len);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD4_Final(void *res, struct md4 *m) {
    MD4_Final(res, m);
}

// Wrapper functions for MD5
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD5_Init(struct md5 *m) {
    MD5_Init(m);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD5_Update(struct md5 *m, const void *v, size_t len) {
    MD5_Update(m, v, len);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD5_Final(void *res, struct md5 *m) {
    MD5_Final(res, m);
}

// Wrapper functions for SHA3
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA3_Init(SHA3_CTX *m, unsigned bitSize) {
    SHA3_Init(m, bitSize);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA3_Update(SHA3_CTX *m, const void *v, size_t len) {
    SHA3_Update(m, v, len);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA3_Final(void *res, SHA3_CTX *m) {
    SHA3_Final(res, m);
}

// Wrapper functions for SHA384
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA384_Init(SHA384_CTX *m) {
    SHA384_Init(m);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA384_Update(SHA384_CTX *m, const void *v, size_t len) {
    SHA384_Update(m, v, len);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA384_Final(void *res, SHA384_CTX *m) {
    SHA384_Final(res, m);
}

// Wrapper functions for SHA512
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA512_Init(SHA512_CTX *m) {
    SHA512_Init(m);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA512_Update(SHA512_CTX *m, const void *v, size_t len) {
    SHA512_Update(m, v, len);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA512_Final(void *res, SHA512_CTX *m) {
    SHA512_Final(res, m);
}

// Wrapper functions for Blake3
FUNCTIONEXPORT const char* FUNCTIONCALLINGCONVENCTION SZ_blake3_version(void) {
    return blake3_version();
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_init(blake3_hasher *self) {
    blake3_hasher_init(self);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_init_keyed(blake3_hasher *self, const uint8_t key[BLAKE3_KEY_LEN]) {
    blake3_hasher_init_keyed(self, key);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_init_derive_key(blake3_hasher *self, const char *context) {
    blake3_hasher_init_derive_key(self, context);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_init_derive_key_raw(blake3_hasher *self, const void *context, size_t context_len) {
    blake3_hasher_init_derive_key_raw(self, context, context_len);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_update(blake3_hasher *self, const void *input, size_t input_len) {
    blake3_hasher_update(self, input, input_len);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_finalize(const blake3_hasher *self, uint8_t *out, size_t out_len) {
    blake3_hasher_finalize(self, out, out_len);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_finalize_seek(const blake3_hasher *self, uint64_t seek, uint8_t *out, size_t out_len) {
    blake3_hasher_finalize_seek(self, seek, out, out_len);
}
