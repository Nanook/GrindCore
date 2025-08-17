/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */



/* *************************************
*  Dependencies
***************************************/
#define ZBUFF_STATIC_LINKING_ONLY
#include "zbuff.h"


ZBUFF_DCtx* ZBUFF_createDCtx(void)
{
    return ZSTD_v1_5_2_createDStream();
}

ZBUFF_DCtx* ZBUFF_createDCtx_advanced(ZSTD_v1_5_2_customMem customMem)
{
    return ZSTD_v1_5_2_createDStream_advanced(customMem);
}

size_t ZBUFF_freeDCtx(ZBUFF_DCtx* zbd)
{
    return ZSTD_v1_5_2_freeDStream(zbd);
}


/* *** Initialization *** */

size_t ZBUFF_decompressInitDictionary(ZBUFF_DCtx* zbd, const void* dict, size_t dictSize)
{
    return ZSTD_v1_5_2_initDStream_usingDict(zbd, dict, dictSize);
}

size_t ZBUFF_decompressInit(ZBUFF_DCtx* zbd)
{
    return ZSTD_v1_5_2_initDStream(zbd);
}


/* *** Decompression *** */

size_t ZBUFF_decompressContinue(ZBUFF_DCtx* zbd,
                                void* dst, size_t* dstCapacityPtr,
                          const void* src, size_t* srcSizePtr)
{
    ZSTD_v1_5_2_outBuffer outBuff;
    ZSTD_v1_5_2_inBuffer inBuff;
    size_t result;
    outBuff.dst  = dst;
    outBuff.pos  = 0;
    outBuff.size = *dstCapacityPtr;
    inBuff.src  = src;
    inBuff.pos  = 0;
    inBuff.size = *srcSizePtr;
    result = ZSTD_v1_5_2_decompressStream(zbd, &outBuff, &inBuff);
    *dstCapacityPtr = outBuff.pos;
    *srcSizePtr = inBuff.pos;
    return result;
}


/* *************************************
*  Tool functions
***************************************/
size_t ZBUFF_recommendedDInSize(void)  { return ZSTD_v1_5_2_DStreamInSize(); }
size_t ZBUFF_recommendedDOutSize(void) { return ZSTD_v1_5_2_DStreamOutSize(); }
