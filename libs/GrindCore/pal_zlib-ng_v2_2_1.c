// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include <assert.h>
#include <stdlib.h>
#include "pal_zlib-ng_v2_2_1.h"

// #ifdef INTERNAL_ZLIB
    #ifdef  _WIN32
        #define c_static_assert(e) static_assert((e),"")
    #endif
    // #ifdef INTERNAL_ZLIB_INTEL
    //     #include <external/dotnet8_0/zlib-intel_v1_2_13/zlib.h>
    // #else
    //    #include <external/dotnet8_0/zlib_v1_3_1/zlib.h>
        //#include <../../artifacts/obj/native/linux-x64/_deps/fetchzlibng-build/zlib.h>
        #include "zlib.h"
    // #endif
// #else
//     #include "pal_utilities.h"
//     #include <zlib.h> //zlib-ng_v2_2_1
// #endif

// c_static_assert(PAL_Z_NOFLUSH == Z_NO_FLUSH);
// c_static_assert(PAL_Z_FINISH == Z_FINISH);

// c_static_assert(PAL_Z_OK == Z_OK);
// c_static_assert(PAL_Z_STREAMEND == Z_STREAM_END);
// c_static_assert(PAL_Z_STREAMERROR == Z_STREAM_ERROR);
// c_static_assert(PAL_Z_DATAERROR == Z_DATA_ERROR);
// c_static_assert(PAL_Z_MEMERROR == Z_MEM_ERROR);
// c_static_assert(PAL_Z_BUFERROR == Z_BUF_ERROR);
// c_static_assert(PAL_Z_VERSIONERROR == Z_VERSION_ERROR);

// c_static_assert(PAL_Z_NOCOMPRESSION == Z_NO_COMPRESSION);
// c_static_assert(PAL_Z_BESTSPEED == Z_BEST_SPEED);
// c_static_assert(PAL_Z_DEFAULTCOMPRESSION == Z_DEFAULT_COMPRESSION);

// c_static_assert(PAL_Z_DEFAULTSTRATEGY == Z_DEFAULT_STRATEGY);

// c_static_assert(PAL_Z_DEFLATED == Z_DEFLATED);

/*
Initializes the PAL_ZStream by creating and setting its underlying z_stream.
*/
static int32_t Init(PAL_ZStream* stream)
{
    z_stream* zStream = (z_stream*)calloc(1, sizeof(z_stream));
    stream->internalState = zStream;

    if (zStream != NULL)
    {
        return PAL_Z_OK;
    }
    else
    {
        return PAL_Z_MEMERROR;
    }
}

/*
Frees any memory on the PAL_ZStream that was created by Init.
*/
static void End(PAL_ZStream* stream)
{
    z_stream* zStream = (z_stream*)(stream->internalState);
    assert(zStream != NULL);
    if (zStream != NULL)
    {
        free(zStream);
        stream->internalState = NULL;
    }
}

/*
Transfers the output values from the underlying z_stream to the PAL_ZStream.
*/
static void TransferStateToPalZStream(z_stream* from, PAL_ZStream* to)
{
    to->nextIn = from->next_in;
    to->availIn = from->avail_in;

    to->nextOut = from->next_out;
    to->availOut = from->avail_out;

    to->msg = from->msg;
}

/*
Transfers the input values from the PAL_ZStream to the underlying z_stream object.
*/
static void TransferStateFromPalZStream(PAL_ZStream* from, z_stream* to)
{
    to->next_in = from->nextIn;
    to->avail_in = from->availIn;

    to->next_out = from->nextOut;
    to->avail_out = from->availOut;
}

/*
Gets the current z_stream object for the specified PAL_ZStream.

This ensures any inputs are transferred from the PAL_ZStream to the underlying z_stream,
since the current values are always needed.
*/
static z_stream* GetCurrentZStream(PAL_ZStream* stream)
{
    z_stream* zStream = (z_stream*)(stream->internalState);
    assert(zStream != NULL);

    TransferStateFromPalZStream(stream, zStream);
    return zStream;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_DeflateInit2_(
    PAL_ZStream* stream, int32_t level, int32_t method, int32_t windowBits, int32_t memLevel, int32_t strategy)
{
    assert(stream != NULL);

    int32_t result = Init(stream);
    if (result == PAL_Z_OK)
    {
        z_stream* zStream = GetCurrentZStream(stream);
        result = Z_v2_2_1_deflateInit2(zStream, level, method, windowBits, memLevel, strategy);
        TransferStateToPalZStream(zStream, stream);
    }

    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_Deflate(PAL_ZStream* stream, int32_t flush)
{
    assert(stream != NULL);

    z_stream* zStream = GetCurrentZStream(stream);
    int32_t result = Z_v2_2_1_deflate(zStream, flush);
    TransferStateToPalZStream(zStream, stream);

    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_DeflateEnd(PAL_ZStream* stream)
{
    assert(stream != NULL);

    z_stream* zStream = GetCurrentZStream(stream);
    int32_t result = Z_v2_2_1_deflateEnd(zStream);
    End(stream);

    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_InflateInit2_(PAL_ZStream* stream, int32_t windowBits)
{
    assert(stream != NULL);

    int32_t result = Init(stream);
    if (result == PAL_Z_OK)
    {
        z_stream* zStream = GetCurrentZStream(stream);
        result = Z_v2_2_1_inflateInit2(zStream, windowBits);
        TransferStateToPalZStream(zStream, stream);
    }

    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_Inflate(PAL_ZStream* stream, int32_t flush)
{
    assert(stream != NULL);

    z_stream* zStream = GetCurrentZStream(stream);
    int32_t result = Z_v2_2_1_inflate(zStream, flush);
    TransferStateToPalZStream(zStream, stream);

    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_InflateEnd(PAL_ZStream* stream)
{
    assert(stream != NULL);

    z_stream* zStream = GetCurrentZStream(stream);
    int32_t result = Z_v2_2_1_inflateEnd(zStream);
    End(stream);

    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_Crc32(uint32_t crc, uint8_t* buffer, int32_t len)
{
    assert(buffer != NULL);

    unsigned long result = Z_v2_2_1_crc32(crc, buffer, len);
    assert(result <= UINT32_MAX);
    return (uint32_t)result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_Compress(uint8_t *dest, uint32_t *destLen, const uint8_t *source, uint32_t sourceLen)
{
    uLongf uLongfDestLen = *destLen;
    int32_t result = (int32_t)Z_v2_2_1_compress(dest, &uLongfDestLen, source, sourceLen);
    *destLen = (uint32_t)uLongfDestLen;
    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_Compress2(uint8_t* dest, uint32_t* destLen, const uint8_t* source, uint32_t sourceLen, int32_t level)
{
    uLongf uLongfDestLen = *destLen;
    int32_t result = (int32_t)Z_v2_2_1_compress2(dest, &uLongfDestLen, source, sourceLen, level);
    *destLen = (uint32_t)uLongfDestLen;
    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_Compress3(uint8_t* dest, uint32_t* destLen, const uint8_t* source, uint32_t sourceLen, int32_t level, int32_t windowBits, int32_t memLevel, int32_t strategy)
{
    uLongf uLongfDestLen = *destLen;
    int32_t result = (int32_t)Z_v2_2_1_compress3(dest, &uLongfDestLen, source, sourceLen, level, windowBits, memLevel, strategy);
    *destLen = (uint32_t)uLongfDestLen;
    return (uint32_t)result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_Uncompress(uint8_t *dest, uint32_t *destLen, const uint8_t *source, uint32_t sourceLen)
{
    uLongf uLongfDestLen = *destLen;
    int32_t result = (int32_t)Z_v2_2_1_uncompress(dest, &uLongfDestLen, source, sourceLen);
    *destLen = (uint32_t)uLongfDestLen;
    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_Uncompress2(uint8_t* dest, uint32_t* destLen, const uint8_t* source, uint32_t* sourceLen)
{
    uLongf uLongfDestLen = *destLen;
    uLong uLongSourceLen = *sourceLen;
    int32_t result = (int32_t)Z_v2_2_1_uncompress2(dest, &uLongfDestLen, source, &uLongSourceLen);
    *destLen = (uint32_t)uLongfDestLen;
    *sourceLen = (uint32_t)uLongSourceLen;
    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION DN9_ZLibNg_v2_2_1_Uncompress3(uint8_t* dest, uint32_t* destLen, const uint8_t* source, uint32_t* sourceLen, int32_t windowBits)
{
    uLongf uLongfDestLen = *destLen;
    uLong uLongSourceLen = *sourceLen;
    int32_t result = (int32_t)Z_v2_2_1_uncompress3(dest, &uLongfDestLen, source, &uLongSourceLen, windowBits);
    *destLen = (uint32_t)uLongfDestLen;
    *sourceLen = (uint32_t)uLongSourceLen;
    return result;
}
