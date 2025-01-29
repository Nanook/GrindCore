// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
#pragma once
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

/*
A structure that holds the input and output values for the zlib functions.
*/
typedef struct PAL_ZStream
{
    uint8_t* nextIn;  // next input byte
    uint8_t* nextOut; // next output byte should be put there

    char* msg; // last error message, NULL if no error

    // the underlying z_stream object is held in internalState, but it should not be used by external callers
    void* internalState;

    uint32_t availIn;  // number of bytes available at nextIn
    uint32_t availOut; // remaining free space at nextOut
} PAL_ZStream;

/*
Allowed flush values for the Deflate and Inflate functions.
*/
enum PAL_FlushCode
{
    PAL_Z_NOFLUSH = 0,
    PAL_Z_FINISH = 4,
};

/*
Error codes from the zlib functions.
*/
enum PAL_ErrorCode
{
    PAL_Z_OK = 0,
    PAL_Z_STREAMEND = 1,
    PAL_Z_STREAMERROR = -2,
    PAL_Z_DATAERROR = -3,
    PAL_Z_MEMERROR = -4,
    PAL_Z_BUFERROR = -5,
    PAL_Z_VERSIONERROR = -6
};

/*
Compression levels
*/
enum PAL_CompressionLevel
{
    PAL_Z_NOCOMPRESSION = 0,
    PAL_Z_BESTSPEED = 1,
    PAL_Z_DEFAULTCOMPRESSION = -1,
};

/*
Compression strategy
*/
enum PAL_CompressionStrategy
{
    PAL_Z_DEFAULTSTRATEGY = 0
};

/*
The deflate compression method (the only one supported in this version)
*/
enum PAL_CompressionMethod
{
    PAL_Z_DEFLATED = 8
};
