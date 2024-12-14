// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include <minipal/entrypoints.h>

// Include GrindCore headers
#include "pal_zlib.h"
#include <brotli/decode.h>
#include <brotli/encode.h>
#include <brotli/port.h>
#include <brotli/types.h>

static const Entry s_compressionNative[] =
{
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
