/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */



/*-*************************************
*  Dependencies
***************************************/
#define ZSTD_v1_5_2_DEPS_NEED_MALLOC
#include "ZSTD_deps.h"   /* ZSTD_v1_5_2_malloc, ZSTD_v1_5_2_calloc, ZSTD_v1_5_2_free, ZSTD_v1_5_2_memset */
#include "error_private.h"
#include "ZSTD_internal.h"


/*-****************************************
*  Version
******************************************/
unsigned ZSTD_v1_5_2_versionNumber(void) { return ZSTD_v1_5_2_VERSION_NUMBER; }

const char* ZSTD_v1_5_2_versionString(void) { return ZSTD_v1_5_2_VERSION_STRING; }


/*-****************************************
*  ZSTD Error Management
******************************************/
#undef ZSTD_v1_5_2_isError   /* defined within ZSTD_internal.h */
/*! ZSTD_v1_5_2_isError() :
 *  tells if a return value is an error code
 *  symbol is required for external callers */
unsigned ZSTD_v1_5_2_isError(size_t code) { return ERR_isError(code); }

/*! ZSTD_v1_5_2_getErrorName() :
 *  provides error code string from function result (useful for debugging) */
const char* ZSTD_v1_5_2_getErrorName(size_t code) { return ERR_getErrorName(code); }

/*! ZSTD_v1_5_2_getError() :
 *  convert a `size_t` function result into a proper ZSTD_v1_5_2_errorCode enum */
ZSTD_v1_5_2_ErrorCode ZSTD_v1_5_2_getErrorCode(size_t code) { return ERR_getErrorCode(code); }

/*! ZSTD_v1_5_2_getErrorString() :
 *  provides error code string from enum */
const char* ZSTD_v1_5_2_getErrorString(ZSTD_v1_5_2_ErrorCode code) { return ERR_getErrorString(code); }



/*=**************************************************************
*  Custom allocator
****************************************************************/
void* ZSTD_v1_5_2_customMalloc(size_t size, ZSTD_v1_5_2_customMem customMem)
{
    if (customMem.customAlloc)
        return customMem.customAlloc(customMem.opaque, size);
    return ZSTD_v1_5_2_malloc(size);
}

void* ZSTD_v1_5_2_customCalloc(size_t size, ZSTD_v1_5_2_customMem customMem)
{
    if (customMem.customAlloc) {
        /* calloc implemented as malloc+memset;
         * not as efficient as calloc, but next best guess for custom malloc */
        void* const ptr = customMem.customAlloc(customMem.opaque, size);
        ZSTD_v1_5_2_memset(ptr, 0, size);
        return ptr;
    }
    return ZSTD_v1_5_2_calloc(1, size);
}

void ZSTD_v1_5_2_customFree(void* ptr, ZSTD_v1_5_2_customMem customMem)
{
    if (ptr!=NULL) {
        if (customMem.customFree)
            customMem.customFree(customMem.opaque, ptr);
        else
            ZSTD_v1_5_2_free(ptr);
    }
}
