// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include <minipal/entrypoints.h>

// Include GrindCore headers
#include "pal_zlib.h"
#include <brotli/decode.h>
#include <brotli/encode.h>
#include <brotli/port.h>
#include <brotli/types.h>

#include <blake3.h>
#include <md2.h>
#include <md4.h>
#include <md5.h>
#include <sha3.h>
#include <sha512.h>

static const Entry s_compressionNative[] =
{
    DllImportEntry(MD2_Init)
    DllImportEntry(MD2_Update)
    DllImportEntry(MD2_Final)
    DllImportEntry(MD4_Init)
    DllImportEntry(MD4_Update)
    DllImportEntry(MD4_Final)
    DllImportEntry(MD5_Init)
    DllImportEntry(MD5_Update)
    DllImportEntry(MD5_Final)
    DllImportEntry(SHA3_Init)
    DllImportEntry(SHA3_Update)
    DllImportEntry(SHA3_Final)
    DllImportEntry(SHA384_Init)
    DllImportEntry(SHA384_Update)
    DllImportEntry(SHA384_Final)
    DllImportEntry(SHA512_Init)
    DllImportEntry(SHA512_Update)
    DllImportEntry(SHA512_Final)
    DllImportEntry(blake3_version)
    DllImportEntry(blake3_hasher_init)
    DllImportEntry(blake3_hasher_init_keyed)
    DllImportEntry(blake3_hasher_init_derive_key)
    DllImportEntry(blake3_hasher_init_derive_key_raw)
    DllImportEntry(blake3_hasher_update)
    DllImportEntry(blake3_hasher_finalize)
    DllImportEntry(blake3_hasher_finalize_seek)
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
    DllImportEntry(GrindCore_Crc32)
    DllImportEntry(GrindCore_Deflate)
    DllImportEntry(GrindCore_DeflateEnd)
    DllImportEntry(GrindCore_DeflateInit2_)
    DllImportEntry(GrindCore_Inflate)
    DllImportEntry(GrindCore_InflateEnd)
    DllImportEntry(GrindCore_InflateInit2_)
    DllImportEntry(GrindCore_Compress3)
    DllImportEntry(GrindCore_Uncompress3)
    DllImportEntry(GrindCore_Compress)
    DllImportEntry(GrindCore_Compress2)
    DllImportEntry(GrindCore_Uncompress)
    DllImportEntry(GrindCore_Uncompress2)
};

EXTERN_C const void* CompressionResolveDllImport(const char* name);

EXTERN_C const void* CompressionResolveDllImport(const char* name)
{
    return minipal_resolve_dllimport(s_compressionNative, ARRAY_SIZE(s_compressionNative), name);
}
