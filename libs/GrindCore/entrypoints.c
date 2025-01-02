// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include <minipal/entrypoints.h>

// Include GrindCore headers
#include "pal_zlib.h"
#include <brotli/decode.h>
#include <brotli/encode.h>
#include <brotli/port.h>
#include <brotli/types.h>
#include "pal_mcmilk.h"

static const Entry s_compressionNative[] =
{
    DllImportEntry(SZ_XXH32_Reset)
    DllImportEntry(SZ_XXH32_Update)
    DllImportEntry(SZ_XXH32_Digest)
    DllImportEntry(SZ_XXH64_Reset)
    DllImportEntry(SZ_XXH64_Update)
    DllImportEntry(SZ_XXH64_Digest)
    DllImportEntry(SZ_MD2_Init)
    DllImportEntry(SZ_MD2_Update)
    DllImportEntry(SZ_MD2_Final)
    DllImportEntry(SZ_MD4_Init)
    DllImportEntry(SZ_MD4_Update)
    DllImportEntry(SZ_MD4_Final)
    DllImportEntry(SZ_MD5_Init)
    DllImportEntry(SZ_MD5_Update)
    DllImportEntry(SZ_MD5_Final)
    DllImportEntry(SZ_SHA3_Init)
    DllImportEntry(SZ_SHA3_Update)
    DllImportEntry(SZ_SHA3_Final)
    DllImportEntry(SZ_SHA384_Init)
    DllImportEntry(SZ_SHA384_Update)
    DllImportEntry(SZ_SHA384_Final)
    DllImportEntry(SZ_SHA512_Init)
    DllImportEntry(SZ_SHA512_Update)
    DllImportEntry(SZ_SHA512_Final)
    DllImportEntry(SZ_blake3_version)
    DllImportEntry(SZ_blake3_hasher_init)
    DllImportEntry(SZ_blake3_hasher_init_keyed)
    DllImportEntry(SZ_blake3_hasher_init_derive_key)
    DllImportEntry(SZ_blake3_hasher_init_derive_key_raw)
    DllImportEntry(SZ_blake3_hasher_update)
    DllImportEntry(SZ_blake3_hasher_finalize)
    DllImportEntry(SZ_blake3_hasher_finalize_seek)
    DllImportEntry(SZ_Blake2sp_SetFunction)
    DllImportEntry(SZ_Blake2sp_Init)
    DllImportEntry(SZ_Blake2sp_InitState)
    DllImportEntry(SZ_Blake2sp_Update)
    DllImportEntry(SZ_Blake2sp_Final)
    DllImportEntry(SZ_Black2sp_Prepare)
    DllImportEntry(SZ_Sha1_SetFunction)
    DllImportEntry(SZ_Sha1_InitState)
    DllImportEntry(SZ_Sha1_Init)
    DllImportEntry(SZ_Sha1_Update)
    DllImportEntry(SZ_Sha1_Final)
    DllImportEntry(SZ_Sha1_PrepareBlock)
    DllImportEntry(SZ_Sha1_GetBlockDigest)
    DllImportEntry(SZ_Sha1Prepare)
    DllImportEntry(SZ_Sha256_SetFunction)
    DllImportEntry(SZ_Sha256_InitState)
    DllImportEntry(SZ_Sha256_Init)
    DllImportEntry(SZ_Sha256_Update)
    DllImportEntry(SZ_Sha256_Final)
    DllImportEntry(SZ_Sha256Prepare)
    DllImportEntry(SZ_SHA384_Init)
    DllImportEntry(SZ_SHA384_Update)
    DllImportEntry(SZ_SHA384_Final)
    DllImportEntry(SZ_SHA512_Init)
    DllImportEntry(SZ_SHA512_Update)
    DllImportEntry(SZ_SHA512_Final)
    DllImportEntry(BrotliDecoderCreateInstance)
    DllImportEntry(BrotliDecoderDecompress)
    DllImportEntry(BrotliDecoderDecompressStream)
    DllImportEntry(BrotliDecoderDestroyInstance)
    DllImportEntry(BrotliDecoderIsFinished)
    DllImportEntry(BrotliEncoderCompress)
    DllImportEntry(BrotliEncoderCompressStream)
    DllImportEntry(BrotliEncoderCreateInstance)
    DllImportEntry(BrotliEncoderDestroyInstance)
    DllImportEntry(BrotliEncoderHasMoreOutput)
    DllImportEntry(BrotliEncoderSetParameter)
    DllImportEntry(DN8_ZLib_Crc32)
    DllImportEntry(DN8_ZLib_Deflate)
    DllImportEntry(DN8_ZLib_DeflateEnd)
    DllImportEntry(DN8_ZLib_DeflateInit2_)
    DllImportEntry(DN8_ZLib_Inflate)
    DllImportEntry(DN8_ZLib_InflateEnd)
    DllImportEntry(DN8_ZLib_InflateInit2_)
    DllImportEntry(DN8_ZLib_Compress)
    DllImportEntry(DN8_ZLib_Compress2)
    DllImportEntry(DN8_ZLib_Compress3)
    DllImportEntry(DN8_ZLib_Uncompress)
    DllImportEntry(DN8_ZLib_Uncompress2)
    DllImportEntry(DN8_ZLib_Uncompress3)
};

EXTERN_C const void* CompressionResolveDllImport(const char* name);

EXTERN_C const void* CompressionResolveDllImport(const char* name)
{
    return minipal_resolve_dllimport(s_compressionNative, ARRAY_SIZE(s_compressionNative), name);
}
