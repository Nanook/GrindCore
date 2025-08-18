/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* ZSTD_ddict.c :
 * concentrates all logic that needs to know the internals of ZSTD_v1_5_2_DDict object */

/*-*******************************************************
*  Dependencies
*********************************************************/
#include "../common/zstd_deps.h"   /* ZSTD_v1_5_2_memcpy, ZSTD_v1_5_2_memmove, ZSTD_v1_5_2_memset */
#include "../common/cpu.h"         /* bmi2 */
#include "../common/mem.h"         /* low level memory routines */
#define FSE_v1_5_2_STATIC_LINKING_ONLY
#include "../common/fse.h"
#define HUF_v1_5_2_STATIC_LINKING_ONLY
#include "../common/huf.h"
#include "zstd_decompress_internal.h"
#include "zstd_ddict.h"

#if defined(ZSTD_v1_5_2_LEGACY_SUPPORT) && (ZSTD_v1_5_2_LEGACY_SUPPORT>=1)
#  include "../legacy/zstd_legacy.h"
#endif



/*-*******************************************************
*  Types
*********************************************************/
struct ZSTD_v1_5_2_DDict_s {
    void* dictBuffer;
    const void* dictContent;
    size_t dictSize;
    ZSTD_v1_5_2_entropyDTables_t entropy;
    U32 dictID;
    U32 entropyPresent;
    ZSTD_v1_5_2_customMem cMem;
};  /* typedef'd to ZSTD_v1_5_2_DDict within "zstd.h" */

const void* ZSTD_v1_5_2_DDict_dictContent(const ZSTD_v1_5_2_DDict* ddict)
{
    assert(ddict != NULL);
    return ddict->dictContent;
}

size_t ZSTD_v1_5_2_DDict_dictSize(const ZSTD_v1_5_2_DDict* ddict)
{
    assert(ddict != NULL);
    return ddict->dictSize;
}

void ZSTD_v1_5_2_copyDDictParameters(ZSTD_v1_5_2_DCtx* dctx, const ZSTD_v1_5_2_DDict* ddict)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_copyDDictParameters");
    assert(dctx != NULL);
    assert(ddict != NULL);
    dctx->dictID = ddict->dictID;
    dctx->prefixStart = ddict->dictContent;
    dctx->virtualStart = ddict->dictContent;
    dctx->dictEnd = (const BYTE*)ddict->dictContent + ddict->dictSize;
    dctx->previousDstEnd = dctx->dictEnd;
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    dctx->dictContentBeginForFuzzing = dctx->prefixStart;
    dctx->dictContentEndForFuzzing = dctx->previousDstEnd;
#endif
    if (ddict->entropyPresent) {
        dctx->litEntropy = 1;
        dctx->fseEntropy = 1;
        dctx->LLTptr = ddict->entropy.LLTable;
        dctx->MLTptr = ddict->entropy.MLTable;
        dctx->OFTptr = ddict->entropy.OFTable;
        dctx->HUFptr = ddict->entropy.hufTable;
        dctx->entropy.rep[0] = ddict->entropy.rep[0];
        dctx->entropy.rep[1] = ddict->entropy.rep[1];
        dctx->entropy.rep[2] = ddict->entropy.rep[2];
    } else {
        dctx->litEntropy = 0;
        dctx->fseEntropy = 0;
    }
}


static size_t
ZSTD_v1_5_2_loadEntropy_intoDDict(ZSTD_v1_5_2_DDict* ddict,
                           ZSTD_v1_5_2_dictContentType_e dictContentType)
{
    ddict->dictID = 0;
    ddict->entropyPresent = 0;
    if (dictContentType == ZSTD_v1_5_2_dct_rawContent) return 0;

    if (ddict->dictSize < 8) {
        if (dictContentType == ZSTD_v1_5_2_dct_fullDict)
            return ERROR(dictionary_corrupted);   /* only accept specified dictionaries */
        return 0;   /* pure content mode */
    }
    {   U32 const magic = MEM_readLE32(ddict->dictContent);
        if (magic != ZSTD_v1_5_2_MAGIC_DICTIONARY) {
            if (dictContentType == ZSTD_v1_5_2_dct_fullDict)
                return ERROR(dictionary_corrupted);   /* only accept specified dictionaries */
            return 0;   /* pure content mode */
        }
    }
    ddict->dictID = MEM_readLE32((const char*)ddict->dictContent + ZSTD_v1_5_2_FRAMEIDSIZE);

    /* load entropy tables */
    RETURN_ERROR_IF(ZSTD_v1_5_2_isError(ZSTD_v1_5_2_loadDEntropy(
            &ddict->entropy, ddict->dictContent, ddict->dictSize)),
        dictionary_corrupted, "");
    ddict->entropyPresent = 1;
    return 0;
}


static size_t ZSTD_v1_5_2_initDDict_internal(ZSTD_v1_5_2_DDict* ddict,
                                      const void* dict, size_t dictSize,
                                      ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
                                      ZSTD_v1_5_2_dictContentType_e dictContentType)
{
    if ((dictLoadMethod == ZSTD_v1_5_2_dlm_byRef) || (!dict) || (!dictSize)) {
        ddict->dictBuffer = NULL;
        ddict->dictContent = dict;
        if (!dict) dictSize = 0;
    } else {
        void* const internalBuffer = ZSTD_v1_5_2_customMalloc(dictSize, ddict->cMem);
        ddict->dictBuffer = internalBuffer;
        ddict->dictContent = internalBuffer;
        if (!internalBuffer) return ERROR(memory_allocation);
        ZSTD_v1_5_2_memcpy(internalBuffer, dict, dictSize);
    }
    ddict->dictSize = dictSize;
    ddict->entropy.hufTable[0] = (HUF_v1_5_2_DTable)((HufLog)*0x1000001);  /* cover both little and big endian */

    /* parse dictionary content */
    FORWARD_IF_ERROR( ZSTD_v1_5_2_loadEntropy_intoDDict(ddict, dictContentType) , "");

    return 0;
}

ZSTD_v1_5_2_DDict* ZSTD_v1_5_2_createDDict_advanced(const void* dict, size_t dictSize,
                                      ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
                                      ZSTD_v1_5_2_dictContentType_e dictContentType,
                                      ZSTD_v1_5_2_customMem customMem)
{
    if ((!customMem.customAlloc) ^ (!customMem.customFree)) return NULL;

    {   ZSTD_v1_5_2_DDict* const ddict = (ZSTD_v1_5_2_DDict*) ZSTD_v1_5_2_customMalloc(sizeof(ZSTD_v1_5_2_DDict), customMem);
        if (ddict == NULL) return NULL;
        ddict->cMem = customMem;
        {   size_t const initResult = ZSTD_v1_5_2_initDDict_internal(ddict,
                                            dict, dictSize,
                                            dictLoadMethod, dictContentType);
            if (ZSTD_v1_5_2_isError(initResult)) {
                ZSTD_v1_5_2_freeDDict(ddict);
                return NULL;
        }   }
        return ddict;
    }
}

/*! ZSTD_v1_5_2_createDDict() :
*   Create a digested dictionary, to start decompression without startup delay.
*   `dict` content is copied inside DDict.
*   Consequently, `dict` can be released after `ZSTD_v1_5_2_DDict` creation */
ZSTD_v1_5_2_DDict* ZSTD_v1_5_2_createDDict(const void* dict, size_t dictSize)
{
    ZSTD_v1_5_2_customMem const allocator = { NULL, NULL, NULL };
    return ZSTD_v1_5_2_createDDict_advanced(dict, dictSize, ZSTD_v1_5_2_dlm_byCopy, ZSTD_v1_5_2_dct_auto, allocator);
}

/*! ZSTD_v1_5_2_createDDict_byReference() :
 *  Create a digested dictionary, to start decompression without startup delay.
 *  Dictionary content is simply referenced, it will be accessed during decompression.
 *  Warning : dictBuffer must outlive DDict (DDict must be freed before dictBuffer) */
ZSTD_v1_5_2_DDict* ZSTD_v1_5_2_createDDict_byReference(const void* dictBuffer, size_t dictSize)
{
    ZSTD_v1_5_2_customMem const allocator = { NULL, NULL, NULL };
    return ZSTD_v1_5_2_createDDict_advanced(dictBuffer, dictSize, ZSTD_v1_5_2_dlm_byRef, ZSTD_v1_5_2_dct_auto, allocator);
}


const ZSTD_v1_5_2_DDict* ZSTD_v1_5_2_initStaticDDict(
                                void* sBuffer, size_t sBufferSize,
                                const void* dict, size_t dictSize,
                                ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
                                ZSTD_v1_5_2_dictContentType_e dictContentType)
{
    size_t const neededSpace = sizeof(ZSTD_v1_5_2_DDict)
                             + (dictLoadMethod == ZSTD_v1_5_2_dlm_byRef ? 0 : dictSize);
    ZSTD_v1_5_2_DDict* const ddict = (ZSTD_v1_5_2_DDict*)sBuffer;
    assert(sBuffer != NULL);
    assert(dict != NULL);
    if ((size_t)sBuffer & 7) return NULL;   /* 8-aligned */
    if (sBufferSize < neededSpace) return NULL;
    if (dictLoadMethod == ZSTD_v1_5_2_dlm_byCopy) {
        ZSTD_v1_5_2_memcpy(ddict+1, dict, dictSize);  /* local copy */
        dict = ddict+1;
    }
    if (ZSTD_v1_5_2_isError( ZSTD_v1_5_2_initDDict_internal(ddict,
                                              dict, dictSize,
                                              ZSTD_v1_5_2_dlm_byRef, dictContentType) ))
        return NULL;
    return ddict;
}


size_t ZSTD_v1_5_2_freeDDict(ZSTD_v1_5_2_DDict* ddict)
{
    if (ddict==NULL) return 0;   /* support free on NULL */
    {   ZSTD_v1_5_2_customMem const cMem = ddict->cMem;
        ZSTD_v1_5_2_customFree(ddict->dictBuffer, cMem);
        ZSTD_v1_5_2_customFree(ddict, cMem);
        return 0;
    }
}

/*! ZSTD_v1_5_2_estimateDDictSize() :
 *  Estimate amount of memory that will be needed to create a dictionary for decompression.
 *  Note : dictionary created by reference using ZSTD_v1_5_2_dlm_byRef are smaller */
size_t ZSTD_v1_5_2_estimateDDictSize(size_t dictSize, ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod)
{
    return sizeof(ZSTD_v1_5_2_DDict) + (dictLoadMethod == ZSTD_v1_5_2_dlm_byRef ? 0 : dictSize);
}

size_t ZSTD_v1_5_2_sizeof_DDict(const ZSTD_v1_5_2_DDict* ddict)
{
    if (ddict==NULL) return 0;   /* support sizeof on NULL */
    return sizeof(*ddict) + (ddict->dictBuffer ? ddict->dictSize : 0) ;
}

/*! ZSTD_v1_5_2_getDictID_fromDDict() :
 *  Provides the dictID of the dictionary loaded into `ddict`.
 *  If @return == 0, the dictionary is not conformant to Zstandard specification, or empty.
 *  Non-conformant dictionaries can still be loaded, but as content-only dictionaries. */
unsigned ZSTD_v1_5_2_getDictID_fromDDict(const ZSTD_v1_5_2_DDict* ddict)
{
    if (ddict==NULL) return 0;
    return ZSTD_v1_5_2_getDictID_fromDict(ddict->dictContent, ddict->dictSize);
}
