#ifdef _WIN32
    #include <stdint.h>
    #include <windows.h>
    #define FUNCTIONEXPORT
    #define FUNCTIONCALLINGCONVENCTION WINAPI
#else
    #include "pal_types.h"
    #include "pal_compiler.h"
    #define FUNCTIONEXPORT PALEXPORT
    #define FUNCTIONCALLINGCONVENCTION
#endif

#include <stddef.h> // Ensure size_t is defined
#include <external/hashes/md2.h>
#include <external/hashes/md4.h>
#include <external/hashes/md5.h>
#include <external/hashes/blake3.h>
#include <external/hashes/sha3.h>
#include <external/hashes/sha512.h>

// MD2
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD2_Init(struct md2 *m);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD2_Update(struct md2 *m, const void *v, size_t len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD2_Final(void *res, struct md2 *m);

// MD4
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD4_Init(struct md4 *m);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD4_Update(struct md4 *m, const void *v, size_t len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD4_Final(void *res, struct md4 *m);

// MD5
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD5_Init(struct md5 *m);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD5_Update(struct md5 *m, const void *v, size_t len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_MD5_Final(void *res, struct md5 *m);

// SHA3
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA3_Init(SHA3_CTX *m, unsigned bitSize);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA3_Update(SHA3_CTX *m, const void *v, size_t len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA3_Final(void *res, SHA3_CTX *m);

// SHA384
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA384_Init(SHA384_CTX *m);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA384_Update(SHA384_CTX *m, const void *v, size_t len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA384_Final(void *res, SHA384_CTX *m);

// SHA512
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA512_Init(SHA512_CTX *m);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA512_Update(SHA512_CTX *m, const void *v, size_t len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA512_Final(void *res, SHA512_CTX *m);

// Blake3
FUNCTIONEXPORT const char* FUNCTIONCALLINGCONVENCTION SZ_blake3_version(void);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_init(blake3_hasher *self);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_init_keyed(blake3_hasher *self, const uint8_t key[BLAKE3_KEY_LEN]);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_init_derive_key(blake3_hasher *self, const char *context);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_init_derive_key_raw(blake3_hasher *self, const void *context, size_t context_len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_update(blake3_hasher *self, const void *input, size_t input_len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_finalize(const blake3_hasher *self, uint8_t *out, size_t out_len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_finalize_seek(const blake3_hasher *self, uint64_t seek, uint8_t *out, size_t out_len);
