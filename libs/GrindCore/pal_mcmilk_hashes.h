#ifdef _WIN32
    #include <stdint.h>
    #include <windows.h>
    #define FUNCTIONEXPORT __declspec(dllexport)
    #define FUNCTIONCALLINGCONVENCTION __cdecl
#else
    #include "pal_types.h"
    #include "pal_compiler.h"
    #define FUNCTIONEXPORT PALEXPORT
    #define FUNCTIONCALLINGCONVENCTION
#endif

#include <stddef.h> // Ensure size_t is defined
#include <external/mcmilk/C/hashes/md2.h>
#include <external/mcmilk/C/hashes/md4.h>
#include <external/mcmilk/C/hashes/md5.h>
#include <external/mcmilk/C/hashes/blake3.h>
#include <external/mcmilk/C/hashes/sha3.h>
#include <external/mcmilk/C/hashes/sha512.h>
#include <external/mcmilk/C/Sha1.h>
#include <external/mcmilk/C/Sha256.h>
#include <external/mcmilk/C/Blake2.h>
#include <external/mcmilk/C/zstd/xxhash.h>

// XXH32
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_XXH32_Reset(XXH32_state_t *ctx);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_XXH32_Update(XXH32_state_t *ctx, const void *data, size_t len);
FUNCTIONEXPORT UInt32 FUNCTIONCALLINGCONVENCTION SZ_XXH32_Digest(XXH32_state_t *ctx);

// XXH64
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_XXH64_Reset(XXH64_state_t *ctx);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_XXH64_Update(XXH64_state_t *ctx, const void *data, size_t len);
FUNCTIONEXPORT UInt64 FUNCTIONCALLINGCONVENCTION SZ_XXH64_Digest(XXH64_state_t *ctx);

// Blake2
FUNCTIONEXPORT BoolInt FUNCTIONCALLINGCONVENCTION SZ_Blake2sp_SetFunction(CBlake2sp *p, unsigned algo);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Blake2sp_Init(CBlake2sp *p);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Blake2sp_InitState(CBlake2sp *p);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Blake2sp_Update(CBlake2sp *p, const Byte *data, size_t size);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Blake2sp_Final(CBlake2sp *p, Byte *digest);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Black2sp_Prepare(void);

// Blake3
FUNCTIONEXPORT const char* FUNCTIONCALLINGCONVENCTION SZ_blake3_version(void);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_init(blake3_hasher *self);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_init_keyed(blake3_hasher *self, const uint8_t key[BLAKE3_KEY_LEN]);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_init_derive_key(blake3_hasher *self, const char *context);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_init_derive_key_raw(blake3_hasher *self, const void *context, size_t context_len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_update(blake3_hasher *self, const void *input, size_t input_len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_finalize(const blake3_hasher *self, uint8_t *out, size_t out_len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_blake3_hasher_finalize_seek(const blake3_hasher *self, uint64_t seek, uint8_t *out, size_t out_len);

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

// SHA1
FUNCTIONEXPORT BoolInt FUNCTIONCALLINGCONVENCTION SZ_Sha1_SetFunction(CSha1 *p, unsigned algo);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Sha1_InitState(CSha1 *p);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Sha1_Init(CSha1 *p);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Sha1_Update(CSha1 *p, const Byte *data, size_t size);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Sha1_Final(CSha1 *p, Byte *digest);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Sha1_PrepareBlock(const CSha1 *p, Byte *block, unsigned size);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Sha1_GetBlockDigest(const CSha1 *p, const Byte *data, Byte *destDigest);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Sha1Prepare(void);

// SHA2 256
FUNCTIONEXPORT BoolInt FUNCTIONCALLINGCONVENCTION SZ_Sha256_SetFunction(CSha256 *p, unsigned algo);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Sha256_InitState(CSha256 *p);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Sha256_Init(CSha256 *m);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Sha256_Update(CSha256 *m, const Byte *data, size_t size);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Sha256_Final(CSha256 *m, Byte *digest);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Sha256Prepare(void);

// SHA2 384
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA384_Init(SHA384_CTX *m);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA384_Update(SHA384_CTX *m, const void *v, size_t len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA384_Final(void *res, SHA384_CTX *m);

// SHA2 512
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA512_Init(SHA512_CTX *m);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA512_Update(SHA512_CTX *m, const void *v, size_t len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA512_Final(void *res, SHA512_CTX *m);

// SHA3
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA3_Init(SHA3_CTX *m, unsigned bitSize);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA3_Update(SHA3_CTX *m, const void *v, size_t len);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_SHA3_Final(void *res, SHA3_CTX *m);

