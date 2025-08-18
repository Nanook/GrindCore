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
#include "../common/zstd_deps.h"  /* INT_MAX, ZSTD_v1_5_2_memset, ZSTD_v1_5_2_memcpy */
#include "../common/mem.h"
#include "hist.h"           /* HIST_countFast_wksp */
#define FSE_v1_5_2_STATIC_LINKING_ONLY   /* FSE_v1_5_2_encodeSymbol */
#include "../common/fse.h"
#define HUF_v1_5_2_STATIC_LINKING_ONLY
#include "../common/huf.h"
#include "zstd_compress_internal.h"
#include "zstd_compress_sequences.h"
#include "zstd_compress_literals.h"
#include "zstd_fast.h"
#include "zstd_double_fast.h"
#include "zstd_lazy.h"
#include "zstd_opt.h"
#include "zstd_ldm.h"
#include "zstd_compress_superblock.h"

/* ***************************************************************
*  Tuning parameters
*****************************************************************/
/*!
 * COMPRESS_HEAPMODE :
 * Select how default decompression function ZSTD_v1_5_2_compress() allocates its context,
 * on stack (0, default), or into heap (1).
 * Note that functions with explicit context such as ZSTD_v1_5_2_compressCCtx() are unaffected.
 */
#ifndef ZSTD_v1_5_2_COMPRESS_HEAPMODE
#  define ZSTD_v1_5_2_COMPRESS_HEAPMODE 0
#endif

/*!
 * ZSTD_v1_5_2_HASHLOG3_MAX :
 * Maximum size of the hash table dedicated to find 3-bytes matches,
 * in log format, aka 17 => 1 << 17 == 128Ki positions.
 * This structure is only used in ZSTD_v1_5_2_opt.
 * Since allocation is centralized for all strategies, it has to be known here.
 * The actual (selected) size of the hash table is then stored in ZSTD_matchState_t.hashLog3,
 * so that ZSTD_opt.c doesn't need to know about this constant.
 */
#ifndef ZSTD_v1_5_2_HASHLOG3_MAX
#  define ZSTD_v1_5_2_HASHLOG3_MAX 17
#endif

/*-*************************************
*  Helper functions
***************************************/
/* ZSTD_v1_5_2_compressBound()
 * Note that the result from this function is only compatible with the "normal"
 * full-block strategy.
 * When there are a lot of small blocks due to frequent flush in streaming mode
 * the overhead of headers can make the compressed data to be larger than the
 * return value of ZSTD_v1_5_2_compressBound().
 */
size_t ZSTD_v1_5_2_compressBound(size_t srcSize) {
    return ZSTD_v1_5_2_COMPRESSBOUND(srcSize);
}


/*-*************************************
*  Context memory management
***************************************/
struct ZSTD_v1_5_2_CDict_s {
    const void* dictContent;
    size_t dictContentSize;
    ZSTD_v1_5_2_dictContentType_e dictContentType; /* The dictContentType the CDict was created with */
    U32* entropyWorkspace; /* entropy workspace of HUF_v1_5_2_WORKSPACE_SIZE bytes */
    ZSTD_v1_5_2_cwksp workspace;
    ZSTD_v1_5_2_matchState_t matchState;
    ZSTD_v1_5_2_compressedBlockState_t cBlockState;
    ZSTD_v1_5_2_customMem customMem;
    U32 dictID;
    int compressionLevel; /* 0 indicates that advanced API was used to select CDict params */
    ZSTD_v1_5_2_paramSwitch_e useRowMatchFinder; /* Indicates whether the CDict was created with params that would use
                                           * row-based matchfinder. Unless the cdict is reloaded, we will use
                                           * the same greedy/lazy matchfinder at compression time.
                                           */
};  /* typedef'd to ZSTD_v1_5_2_CDict within "zstd.h" */

ZSTD_v1_5_2_CCtx* ZSTD_v1_5_2_createCCtx(void)
{
    return ZSTD_v1_5_2_createCCtx_advanced(ZSTD_v1_5_2_defaultCMem);
}

static void ZSTD_v1_5_2_initCCtx(ZSTD_v1_5_2_CCtx* cctx, ZSTD_v1_5_2_customMem memManager)
{
    assert(cctx != NULL);
    ZSTD_v1_5_2_memset(cctx, 0, sizeof(*cctx));
    cctx->customMem = memManager;
    cctx->bmi2 = ZSTD_v1_5_2_cpuSupportsBmi2();
    {   size_t const err = ZSTD_v1_5_2_CCtx_reset(cctx, ZSTD_v1_5_2_reset_parameters);
        assert(!ZSTD_v1_5_2_isError(err));
        (void)err;
    }
}

ZSTD_v1_5_2_CCtx* ZSTD_v1_5_2_createCCtx_advanced(ZSTD_v1_5_2_customMem customMem)
{
    ZSTD_v1_5_2_STATIC_ASSERT(zcss_init==0);
    ZSTD_v1_5_2_STATIC_ASSERT(ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN==(0ULL - 1));
    if ((!customMem.customAlloc) ^ (!customMem.customFree)) return NULL;
    {   ZSTD_v1_5_2_CCtx* const cctx = (ZSTD_v1_5_2_CCtx*)ZSTD_v1_5_2_customMalloc(sizeof(ZSTD_v1_5_2_CCtx), customMem);
        if (!cctx) return NULL;
        ZSTD_v1_5_2_initCCtx(cctx, customMem);
        return cctx;
    }
}

ZSTD_v1_5_2_CCtx* ZSTD_v1_5_2_initStaticCCtx(void* workspace, size_t workspaceSize)
{
    ZSTD_v1_5_2_cwksp ws;
    ZSTD_v1_5_2_CCtx* cctx;
    if (workspaceSize <= sizeof(ZSTD_v1_5_2_CCtx)) return NULL;  /* minimum size */
    if ((size_t)workspace & 7) return NULL;  /* must be 8-aligned */
    ZSTD_v1_5_2_cwksp_init(&ws, workspace, workspaceSize, ZSTD_v1_5_2_cwksp_static_alloc);

    cctx = (ZSTD_v1_5_2_CCtx*)ZSTD_v1_5_2_cwksp_reserve_object(&ws, sizeof(ZSTD_v1_5_2_CCtx));
    if (cctx == NULL) return NULL;

    ZSTD_v1_5_2_memset(cctx, 0, sizeof(ZSTD_v1_5_2_CCtx));
    ZSTD_v1_5_2_cwksp_move(&cctx->workspace, &ws);
    cctx->staticSize = workspaceSize;

    /* statically sized space. entropyWorkspace never moves (but prev/next block swap places) */
    if (!ZSTD_v1_5_2_cwksp_check_available(&cctx->workspace, ENTROPY_WORKSPACE_SIZE + 2 * sizeof(ZSTD_v1_5_2_compressedBlockState_t))) return NULL;
    cctx->blockState.prevCBlock = (ZSTD_v1_5_2_compressedBlockState_t*)ZSTD_v1_5_2_cwksp_reserve_object(&cctx->workspace, sizeof(ZSTD_v1_5_2_compressedBlockState_t));
    cctx->blockState.nextCBlock = (ZSTD_v1_5_2_compressedBlockState_t*)ZSTD_v1_5_2_cwksp_reserve_object(&cctx->workspace, sizeof(ZSTD_v1_5_2_compressedBlockState_t));
    cctx->entropyWorkspace = (U32*)ZSTD_v1_5_2_cwksp_reserve_object(&cctx->workspace, ENTROPY_WORKSPACE_SIZE);
    cctx->bmi2 = ZSTD_v1_5_2_cpuid_bmi2(ZSTD_v1_5_2_cpuid());
    return cctx;
}

/**
 * Clears and frees all of the dictionaries in the CCtx.
 */
static void ZSTD_v1_5_2_clearAllDicts(ZSTD_v1_5_2_CCtx* cctx)
{
    ZSTD_v1_5_2_customFree(cctx->localDict.dictBuffer, cctx->customMem);
    ZSTD_v1_5_2_freeCDict(cctx->localDict.cdict);
    ZSTD_v1_5_2_memset(&cctx->localDict, 0, sizeof(cctx->localDict));
    ZSTD_v1_5_2_memset(&cctx->prefixDict, 0, sizeof(cctx->prefixDict));
    cctx->cdict = NULL;
}

static size_t ZSTD_v1_5_2_sizeof_localDict(ZSTD_v1_5_2_localDict dict)
{
    size_t const bufferSize = dict.dictBuffer != NULL ? dict.dictSize : 0;
    size_t const cdictSize = ZSTD_v1_5_2_sizeof_CDict(dict.cdict);
    return bufferSize + cdictSize;
}

static void ZSTD_v1_5_2_freeCCtxContent(ZSTD_v1_5_2_CCtx* cctx)
{
    assert(cctx != NULL);
    assert(cctx->staticSize == 0);
    ZSTD_v1_5_2_clearAllDicts(cctx);
#ifdef ZSTD_v1_5_2_MULTITHREAD
    ZSTDMT_freeCCtx(cctx->mtctx); cctx->mtctx = NULL;
#endif
    ZSTD_v1_5_2_cwksp_free(&cctx->workspace, cctx->customMem);
}

size_t ZSTD_v1_5_2_freeCCtx(ZSTD_v1_5_2_CCtx* cctx)
{
    if (cctx==NULL) return 0;   /* support free on NULL */
    RETURN_ERROR_IF(cctx->staticSize, memory_allocation,
                    "not compatible with static CCtx");
    {
        int cctxInWorkspace = ZSTD_v1_5_2_cwksp_owns_buffer(&cctx->workspace, cctx);
        ZSTD_v1_5_2_freeCCtxContent(cctx);
        if (!cctxInWorkspace) {
            ZSTD_v1_5_2_customFree(cctx, cctx->customMem);
        }
    }
    return 0;
}


static size_t ZSTD_v1_5_2_sizeof_mtctx(const ZSTD_v1_5_2_CCtx* cctx)
{
#ifdef ZSTD_v1_5_2_MULTITHREAD
    return ZSTDMT_sizeof_CCtx(cctx->mtctx);
#else
    (void)cctx;
    return 0;
#endif
}


size_t ZSTD_v1_5_2_sizeof_CCtx(const ZSTD_v1_5_2_CCtx* cctx)
{
    if (cctx==NULL) return 0;   /* support sizeof on NULL */
    /* cctx may be in the workspace */
    return (cctx->workspace.workspace == cctx ? 0 : sizeof(*cctx))
           + ZSTD_v1_5_2_cwksp_sizeof(&cctx->workspace)
           + ZSTD_v1_5_2_sizeof_localDict(cctx->localDict)
           + ZSTD_v1_5_2_sizeof_mtctx(cctx);
}

size_t ZSTD_v1_5_2_sizeof_CStream(const ZSTD_v1_5_2_CStream* zcs)
{
    return ZSTD_v1_5_2_sizeof_CCtx(zcs);  /* same object */
}

/* private API call, for dictBuilder only */
const seqStore_t* ZSTD_v1_5_2_getSeqStore(const ZSTD_v1_5_2_CCtx* ctx) { return &(ctx->seqStore); }

/* Returns true if the strategy supports using a row based matchfinder */
static int ZSTD_v1_5_2_rowMatchFinderSupported(const ZSTD_v1_5_2_strategy strategy) {
    return (strategy >= ZSTD_v1_5_2_greedy && strategy <= ZSTD_v1_5_2_lazy2);
}

/* Returns true if the strategy and useRowMatchFinder mode indicate that we will use the row based matchfinder
 * for this compression.
 */
static int ZSTD_v1_5_2_rowMatchFinderUsed(const ZSTD_v1_5_2_strategy strategy, const ZSTD_v1_5_2_paramSwitch_e mode) {
    assert(mode != ZSTD_v1_5_2_ps_auto);
    return ZSTD_v1_5_2_rowMatchFinderSupported(strategy) && (mode == ZSTD_v1_5_2_ps_enable);
}

/* Returns row matchfinder usage given an initial mode and cParams */
static ZSTD_v1_5_2_paramSwitch_e ZSTD_v1_5_2_resolveRowMatchFinderMode(ZSTD_v1_5_2_paramSwitch_e mode,
                                                         const ZSTD_v1_5_2_compressionParameters* const cParams) {
#if defined(ZSTD_v1_5_2_ARCH_X86_SSE2) || defined(ZSTD_v1_5_2_ARCH_ARM_NEON)
    int const kHasSIMD128 = 1;
#else
    int const kHasSIMD128 = 0;
#endif
    if (mode != ZSTD_v1_5_2_ps_auto) return mode; /* if requested enabled, but no SIMD, we still will use row matchfinder */
    mode = ZSTD_v1_5_2_ps_disable;
    if (!ZSTD_v1_5_2_rowMatchFinderSupported(cParams->strategy)) return mode;
    if (kHasSIMD128) {
        if (cParams->windowLog > 14) mode = ZSTD_v1_5_2_ps_enable;
    } else {
        if (cParams->windowLog > 17) mode = ZSTD_v1_5_2_ps_enable;
    }
    return mode;
}

/* Returns block splitter usage (generally speaking, when using slower/stronger compression modes) */
static ZSTD_v1_5_2_paramSwitch_e ZSTD_v1_5_2_resolveBlockSplitterMode(ZSTD_v1_5_2_paramSwitch_e mode,
                                                        const ZSTD_v1_5_2_compressionParameters* const cParams) {
    if (mode != ZSTD_v1_5_2_ps_auto) return mode;
    return (cParams->strategy >= ZSTD_v1_5_2_btopt && cParams->windowLog >= 17) ? ZSTD_v1_5_2_ps_enable : ZSTD_v1_5_2_ps_disable;
}

/* Returns 1 if the arguments indicate that we should allocate a chainTable, 0 otherwise */
static int ZSTD_v1_5_2_allocateChainTable(const ZSTD_v1_5_2_strategy strategy,
                                   const ZSTD_v1_5_2_paramSwitch_e useRowMatchFinder,
                                   const U32 forDDSDict) {
    assert(useRowMatchFinder != ZSTD_v1_5_2_ps_auto);
    /* We always should allocate a chaintable if we are allocating a matchstate for a DDS dictionary matchstate.
     * We do not allocate a chaintable if we are using ZSTD_v1_5_2_fast, or are using the row-based matchfinder.
     */
    return forDDSDict || ((strategy != ZSTD_v1_5_2_fast) && !ZSTD_v1_5_2_rowMatchFinderUsed(strategy, useRowMatchFinder));
}

/* Returns 1 if compression parameters are such that we should
 * enable long distance matching (wlog >= 27, strategy >= btopt).
 * Returns 0 otherwise.
 */
static ZSTD_v1_5_2_paramSwitch_e ZSTD_v1_5_2_resolveEnableLdm(ZSTD_v1_5_2_paramSwitch_e mode,
                                 const ZSTD_v1_5_2_compressionParameters* const cParams) {
    if (mode != ZSTD_v1_5_2_ps_auto) return mode;
    return (cParams->strategy >= ZSTD_v1_5_2_btopt && cParams->windowLog >= 27) ? ZSTD_v1_5_2_ps_enable : ZSTD_v1_5_2_ps_disable;
}

static ZSTD_v1_5_2_CCtx_params ZSTD_v1_5_2_makeCCtxParamsFromCParams(
        ZSTD_v1_5_2_compressionParameters cParams)
{
    ZSTD_v1_5_2_CCtx_params cctxParams;
    /* should not matter, as all cParams are presumed properly defined */
    ZSTD_v1_5_2_CCtxParams_init(&cctxParams, ZSTD_v1_5_2_CLEVEL_DEFAULT);
    cctxParams.cParams = cParams;

    /* Adjust advanced params according to cParams */
    cctxParams.ldmParams.enableLdm = ZSTD_v1_5_2_resolveEnableLdm(cctxParams.ldmParams.enableLdm, &cParams);
    if (cctxParams.ldmParams.enableLdm == ZSTD_v1_5_2_ps_enable) {
        ZSTD_v1_5_2_ldm_adjustParameters(&cctxParams.ldmParams, &cParams);
        assert(cctxParams.ldmParams.hashLog >= cctxParams.ldmParams.bucketSizeLog);
        assert(cctxParams.ldmParams.hashRateLog < 32);
    }
    cctxParams.useBlockSplitter = ZSTD_v1_5_2_resolveBlockSplitterMode(cctxParams.useBlockSplitter, &cParams);
    cctxParams.useRowMatchFinder = ZSTD_v1_5_2_resolveRowMatchFinderMode(cctxParams.useRowMatchFinder, &cParams);
    assert(!ZSTD_v1_5_2_checkCParams(cParams));
    return cctxParams;
}

static ZSTD_v1_5_2_CCtx_params* ZSTD_v1_5_2_createCCtxParams_advanced(
        ZSTD_v1_5_2_customMem customMem)
{
    ZSTD_v1_5_2_CCtx_params* params;
    if ((!customMem.customAlloc) ^ (!customMem.customFree)) return NULL;
    params = (ZSTD_v1_5_2_CCtx_params*)ZSTD_v1_5_2_customCalloc(
            sizeof(ZSTD_v1_5_2_CCtx_params), customMem);
    if (!params) { return NULL; }
    ZSTD_v1_5_2_CCtxParams_init(params, ZSTD_v1_5_2_CLEVEL_DEFAULT);
    params->customMem = customMem;
    return params;
}

ZSTD_v1_5_2_CCtx_params* ZSTD_v1_5_2_createCCtxParams(void)
{
    return ZSTD_v1_5_2_createCCtxParams_advanced(ZSTD_v1_5_2_defaultCMem);
}

size_t ZSTD_v1_5_2_freeCCtxParams(ZSTD_v1_5_2_CCtx_params* params)
{
    if (params == NULL) { return 0; }
    ZSTD_v1_5_2_customFree(params, params->customMem);
    return 0;
}

size_t ZSTD_v1_5_2_CCtxParams_reset(ZSTD_v1_5_2_CCtx_params* params)
{
    return ZSTD_v1_5_2_CCtxParams_init(params, ZSTD_v1_5_2_CLEVEL_DEFAULT);
}

size_t ZSTD_v1_5_2_CCtxParams_init(ZSTD_v1_5_2_CCtx_params* cctxParams, int compressionLevel) {
    RETURN_ERROR_IF(!cctxParams, GENERIC, "NULL pointer!");
    ZSTD_v1_5_2_memset(cctxParams, 0, sizeof(*cctxParams));
    cctxParams->compressionLevel = compressionLevel;
    cctxParams->fParams.contentSizeFlag = 1;
    return 0;
}

#define ZSTD_v1_5_2_NO_CLEVEL 0

/**
 * Initializes the cctxParams from params and compressionLevel.
 * @param compressionLevel If params are derived from a compression level then that compression level, otherwise ZSTD_v1_5_2_NO_CLEVEL.
 */
static void ZSTD_v1_5_2_CCtxParams_init_internal(ZSTD_v1_5_2_CCtx_params* cctxParams, ZSTD_v1_5_2_parameters const* params, int compressionLevel)
{
    assert(!ZSTD_v1_5_2_checkCParams(params->cParams));
    ZSTD_v1_5_2_memset(cctxParams, 0, sizeof(*cctxParams));
    cctxParams->cParams = params->cParams;
    cctxParams->fParams = params->fParams;
    /* Should not matter, as all cParams are presumed properly defined.
     * But, set it for tracing anyway.
     */
    cctxParams->compressionLevel = compressionLevel;
    cctxParams->useRowMatchFinder = ZSTD_v1_5_2_resolveRowMatchFinderMode(cctxParams->useRowMatchFinder, &params->cParams);
    cctxParams->useBlockSplitter = ZSTD_v1_5_2_resolveBlockSplitterMode(cctxParams->useBlockSplitter, &params->cParams);
    cctxParams->ldmParams.enableLdm = ZSTD_v1_5_2_resolveEnableLdm(cctxParams->ldmParams.enableLdm, &params->cParams);
    DEBUGLOG(4, "ZSTD_v1_5_2_CCtxParams_init_internal: useRowMatchFinder=%d, useBlockSplitter=%d ldm=%d",
                cctxParams->useRowMatchFinder, cctxParams->useBlockSplitter, cctxParams->ldmParams.enableLdm);
}

size_t ZSTD_v1_5_2_CCtxParams_init_advanced(ZSTD_v1_5_2_CCtx_params* cctxParams, ZSTD_v1_5_2_parameters params)
{
    RETURN_ERROR_IF(!cctxParams, GENERIC, "NULL pointer!");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_checkCParams(params.cParams) , "");
    ZSTD_v1_5_2_CCtxParams_init_internal(cctxParams, &params, ZSTD_v1_5_2_NO_CLEVEL);
    return 0;
}

/**
 * Sets cctxParams' cParams and fParams from params, but otherwise leaves them alone.
 * @param param Validated zstd parameters.
 */
static void ZSTD_v1_5_2_CCtxParams_setZstdParams(
        ZSTD_v1_5_2_CCtx_params* cctxParams, const ZSTD_v1_5_2_parameters* params)
{
    assert(!ZSTD_v1_5_2_checkCParams(params->cParams));
    cctxParams->cParams = params->cParams;
    cctxParams->fParams = params->fParams;
    /* Should not matter, as all cParams are presumed properly defined.
     * But, set it for tracing anyway.
     */
    cctxParams->compressionLevel = ZSTD_v1_5_2_NO_CLEVEL;
}

ZSTD_v1_5_2_bounds ZSTD_v1_5_2_cParam_getBounds(ZSTD_v1_5_2_cParameter param)
{
    ZSTD_v1_5_2_bounds bounds = { 0, 0, 0 };

    switch(param)
    {
    case ZSTD_v1_5_2_c_compressionLevel:
        bounds.lowerBound = ZSTD_v1_5_2_minCLevel();
        bounds.upperBound = ZSTD_v1_5_2_maxCLevel();
        return bounds;

    case ZSTD_v1_5_2_c_windowLog:
        bounds.lowerBound = ZSTD_v1_5_2_WINDOWLOG_MIN;
        bounds.upperBound = ZSTD_v1_5_2_WINDOWLOG_MAX;
        return bounds;

    case ZSTD_v1_5_2_c_hashLog:
        bounds.lowerBound = ZSTD_v1_5_2_HASHLOG_MIN;
        bounds.upperBound = ZSTD_v1_5_2_HASHLOG_MAX;
        return bounds;

    case ZSTD_v1_5_2_c_chainLog:
        bounds.lowerBound = ZSTD_v1_5_2_CHAINLOG_MIN;
        bounds.upperBound = ZSTD_v1_5_2_CHAINLOG_MAX;
        return bounds;

    case ZSTD_v1_5_2_c_searchLog:
        bounds.lowerBound = ZSTD_v1_5_2_SEARCHLOG_MIN;
        bounds.upperBound = ZSTD_v1_5_2_SEARCHLOG_MAX;
        return bounds;

    case ZSTD_v1_5_2_c_minMatch:
        bounds.lowerBound = ZSTD_v1_5_2_MINMATCH_MIN;
        bounds.upperBound = ZSTD_v1_5_2_MINMATCH_MAX;
        return bounds;

    case ZSTD_v1_5_2_c_targetLength:
        bounds.lowerBound = ZSTD_v1_5_2_TARGETLENGTH_MIN;
        bounds.upperBound = ZSTD_v1_5_2_TARGETLENGTH_MAX;
        return bounds;

    case ZSTD_v1_5_2_c_strategy:
        bounds.lowerBound = ZSTD_v1_5_2_STRATEGY_MIN;
        bounds.upperBound = ZSTD_v1_5_2_STRATEGY_MAX;
        return bounds;

    case ZSTD_v1_5_2_c_contentSizeFlag:
        bounds.lowerBound = 0;
        bounds.upperBound = 1;
        return bounds;

    case ZSTD_v1_5_2_c_checksumFlag:
        bounds.lowerBound = 0;
        bounds.upperBound = 1;
        return bounds;

    case ZSTD_v1_5_2_c_dictIDFlag:
        bounds.lowerBound = 0;
        bounds.upperBound = 1;
        return bounds;

    case ZSTD_v1_5_2_c_nbWorkers:
        bounds.lowerBound = 0;
#ifdef ZSTD_v1_5_2_MULTITHREAD
        bounds.upperBound = ZSTDMT_NBWORKERS_MAX;
#else
        bounds.upperBound = 0;
#endif
        return bounds;

    case ZSTD_v1_5_2_c_jobSize:
        bounds.lowerBound = 0;
#ifdef ZSTD_v1_5_2_MULTITHREAD
        bounds.upperBound = ZSTDMT_JOBSIZE_MAX;
#else
        bounds.upperBound = 0;
#endif
        return bounds;

    case ZSTD_v1_5_2_c_overlapLog:
#ifdef ZSTD_v1_5_2_MULTITHREAD
        bounds.lowerBound = ZSTD_v1_5_2_OVERLAPLOG_MIN;
        bounds.upperBound = ZSTD_v1_5_2_OVERLAPLOG_MAX;
#else
        bounds.lowerBound = 0;
        bounds.upperBound = 0;
#endif
        return bounds;

    case ZSTD_v1_5_2_c_enableDedicatedDictSearch:
        bounds.lowerBound = 0;
        bounds.upperBound = 1;
        return bounds;

    case ZSTD_v1_5_2_c_enableLongDistanceMatching:
        bounds.lowerBound = 0;
        bounds.upperBound = 1;
        return bounds;

    case ZSTD_v1_5_2_c_ldmHashLog:
        bounds.lowerBound = ZSTD_v1_5_2_LDM_HASHLOG_MIN;
        bounds.upperBound = ZSTD_v1_5_2_LDM_HASHLOG_MAX;
        return bounds;

    case ZSTD_v1_5_2_c_ldmMinMatch:
        bounds.lowerBound = ZSTD_v1_5_2_LDM_MINMATCH_MIN;
        bounds.upperBound = ZSTD_v1_5_2_LDM_MINMATCH_MAX;
        return bounds;

    case ZSTD_v1_5_2_c_ldmBucketSizeLog:
        bounds.lowerBound = ZSTD_v1_5_2_LDM_BUCKETSIZELOG_MIN;
        bounds.upperBound = ZSTD_v1_5_2_LDM_BUCKETSIZELOG_MAX;
        return bounds;

    case ZSTD_v1_5_2_c_ldmHashRateLog:
        bounds.lowerBound = ZSTD_v1_5_2_LDM_HASHRATELOG_MIN;
        bounds.upperBound = ZSTD_v1_5_2_LDM_HASHRATELOG_MAX;
        return bounds;

    /* experimental parameters */
    case ZSTD_v1_5_2_c_rsyncable:
        bounds.lowerBound = 0;
        bounds.upperBound = 1;
        return bounds;

    case ZSTD_v1_5_2_c_forceMaxWindow :
        bounds.lowerBound = 0;
        bounds.upperBound = 1;
        return bounds;

    case ZSTD_v1_5_2_c_format:
        ZSTD_v1_5_2_STATIC_ASSERT(ZSTD_v1_5_2_f_zstd1 < ZSTD_v1_5_2_f_zstd1_magicless);
        bounds.lowerBound = ZSTD_v1_5_2_f_zstd1;
        bounds.upperBound = ZSTD_v1_5_2_f_zstd1_magicless;   /* note : how to ensure at compile time that this is the highest value enum ? */
        return bounds;

    case ZSTD_v1_5_2_c_forceAttachDict:
        ZSTD_v1_5_2_STATIC_ASSERT(ZSTD_v1_5_2_dictDefaultAttach < ZSTD_v1_5_2_dictForceLoad);
        bounds.lowerBound = ZSTD_v1_5_2_dictDefaultAttach;
        bounds.upperBound = ZSTD_v1_5_2_dictForceLoad;       /* note : how to ensure at compile time that this is the highest value enum ? */
        return bounds;

    case ZSTD_v1_5_2_c_literalCompressionMode:
        ZSTD_v1_5_2_STATIC_ASSERT(ZSTD_v1_5_2_ps_auto < ZSTD_v1_5_2_ps_enable && ZSTD_v1_5_2_ps_enable < ZSTD_v1_5_2_ps_disable);
        bounds.lowerBound = (int)ZSTD_v1_5_2_ps_auto;
        bounds.upperBound = (int)ZSTD_v1_5_2_ps_disable;
        return bounds;

    case ZSTD_v1_5_2_c_targetCBlockSize:
        bounds.lowerBound = ZSTD_v1_5_2_TARGETCBLOCKSIZE_MIN;
        bounds.upperBound = ZSTD_v1_5_2_TARGETCBLOCKSIZE_MAX;
        return bounds;

    case ZSTD_v1_5_2_c_srcSizeHint:
        bounds.lowerBound = ZSTD_v1_5_2_SRCSIZEHINT_MIN;
        bounds.upperBound = ZSTD_v1_5_2_SRCSIZEHINT_MAX;
        return bounds;

    case ZSTD_v1_5_2_c_stableInBuffer:
    case ZSTD_v1_5_2_c_stableOutBuffer:
        bounds.lowerBound = (int)ZSTD_v1_5_2_bm_buffered;
        bounds.upperBound = (int)ZSTD_v1_5_2_bm_stable;
        return bounds;

    case ZSTD_v1_5_2_c_blockDelimiters:
        bounds.lowerBound = (int)ZSTD_v1_5_2_sf_noBlockDelimiters;
        bounds.upperBound = (int)ZSTD_v1_5_2_sf_explicitBlockDelimiters;
        return bounds;

    case ZSTD_v1_5_2_c_validateSequences:
        bounds.lowerBound = 0;
        bounds.upperBound = 1;
        return bounds;

    case ZSTD_v1_5_2_c_useBlockSplitter:
        bounds.lowerBound = (int)ZSTD_v1_5_2_ps_auto;
        bounds.upperBound = (int)ZSTD_v1_5_2_ps_disable;
        return bounds;

    case ZSTD_v1_5_2_c_useRowMatchFinder:
        bounds.lowerBound = (int)ZSTD_v1_5_2_ps_auto;
        bounds.upperBound = (int)ZSTD_v1_5_2_ps_disable;
        return bounds;

    case ZSTD_v1_5_2_c_deterministicRefPrefix:
        bounds.lowerBound = 0;
        bounds.upperBound = 1;
        return bounds;

    default:
        bounds.error = ERROR(parameter_unsupported);
        return bounds;
    }
}

/* ZSTD_v1_5_2_cParam_clampBounds:
 * Clamps the value into the bounded range.
 */
static size_t ZSTD_v1_5_2_cParam_clampBounds(ZSTD_v1_5_2_cParameter cParam, int* value)
{
    ZSTD_v1_5_2_bounds const bounds = ZSTD_v1_5_2_cParam_getBounds(cParam);
    if (ZSTD_v1_5_2_isError(bounds.error)) return bounds.error;
    if (*value < bounds.lowerBound) *value = bounds.lowerBound;
    if (*value > bounds.upperBound) *value = bounds.upperBound;
    return 0;
}

#define BOUNDCHECK(cParam, val) { \
    RETURN_ERROR_IF(!ZSTD_v1_5_2_cParam_withinBounds(cParam,val), \
                    parameter_outOfBound, "Param out of bounds"); \
}


static int ZSTD_v1_5_2_isUpdateAuthorized(ZSTD_v1_5_2_cParameter param)
{
    switch(param)
    {
    case ZSTD_v1_5_2_c_compressionLevel:
    case ZSTD_v1_5_2_c_hashLog:
    case ZSTD_v1_5_2_c_chainLog:
    case ZSTD_v1_5_2_c_searchLog:
    case ZSTD_v1_5_2_c_minMatch:
    case ZSTD_v1_5_2_c_targetLength:
    case ZSTD_v1_5_2_c_strategy:
        return 1;

    case ZSTD_v1_5_2_c_format:
    case ZSTD_v1_5_2_c_windowLog:
    case ZSTD_v1_5_2_c_contentSizeFlag:
    case ZSTD_v1_5_2_c_checksumFlag:
    case ZSTD_v1_5_2_c_dictIDFlag:
    case ZSTD_v1_5_2_c_forceMaxWindow :
    case ZSTD_v1_5_2_c_nbWorkers:
    case ZSTD_v1_5_2_c_jobSize:
    case ZSTD_v1_5_2_c_overlapLog:
    case ZSTD_v1_5_2_c_rsyncable:
    case ZSTD_v1_5_2_c_enableDedicatedDictSearch:
    case ZSTD_v1_5_2_c_enableLongDistanceMatching:
    case ZSTD_v1_5_2_c_ldmHashLog:
    case ZSTD_v1_5_2_c_ldmMinMatch:
    case ZSTD_v1_5_2_c_ldmBucketSizeLog:
    case ZSTD_v1_5_2_c_ldmHashRateLog:
    case ZSTD_v1_5_2_c_forceAttachDict:
    case ZSTD_v1_5_2_c_literalCompressionMode:
    case ZSTD_v1_5_2_c_targetCBlockSize:
    case ZSTD_v1_5_2_c_srcSizeHint:
    case ZSTD_v1_5_2_c_stableInBuffer:
    case ZSTD_v1_5_2_c_stableOutBuffer:
    case ZSTD_v1_5_2_c_blockDelimiters:
    case ZSTD_v1_5_2_c_validateSequences:
    case ZSTD_v1_5_2_c_useBlockSplitter:
    case ZSTD_v1_5_2_c_useRowMatchFinder:
    case ZSTD_v1_5_2_c_deterministicRefPrefix:
    default:
        return 0;
    }
}

size_t ZSTD_v1_5_2_CCtx_setParameter(ZSTD_v1_5_2_CCtx* cctx, ZSTD_v1_5_2_cParameter param, int value)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_CCtx_setParameter (%i, %i)", (int)param, value);
    if (cctx->streamStage != zcss_init) {
        if (ZSTD_v1_5_2_isUpdateAuthorized(param)) {
            cctx->cParamsChanged = 1;
        } else {
            RETURN_ERROR(stage_wrong, "can only set params in ctx init stage");
    }   }

    switch(param)
    {
    case ZSTD_v1_5_2_c_nbWorkers:
        RETURN_ERROR_IF((value!=0) && cctx->staticSize, parameter_unsupported,
                        "MT not compatible with static alloc");
        break;

    case ZSTD_v1_5_2_c_compressionLevel:
    case ZSTD_v1_5_2_c_windowLog:
    case ZSTD_v1_5_2_c_hashLog:
    case ZSTD_v1_5_2_c_chainLog:
    case ZSTD_v1_5_2_c_searchLog:
    case ZSTD_v1_5_2_c_minMatch:
    case ZSTD_v1_5_2_c_targetLength:
    case ZSTD_v1_5_2_c_strategy:
    case ZSTD_v1_5_2_c_ldmHashRateLog:
    case ZSTD_v1_5_2_c_format:
    case ZSTD_v1_5_2_c_contentSizeFlag:
    case ZSTD_v1_5_2_c_checksumFlag:
    case ZSTD_v1_5_2_c_dictIDFlag:
    case ZSTD_v1_5_2_c_forceMaxWindow:
    case ZSTD_v1_5_2_c_forceAttachDict:
    case ZSTD_v1_5_2_c_literalCompressionMode:
    case ZSTD_v1_5_2_c_jobSize:
    case ZSTD_v1_5_2_c_overlapLog:
    case ZSTD_v1_5_2_c_rsyncable:
    case ZSTD_v1_5_2_c_enableDedicatedDictSearch:
    case ZSTD_v1_5_2_c_enableLongDistanceMatching:
    case ZSTD_v1_5_2_c_ldmHashLog:
    case ZSTD_v1_5_2_c_ldmMinMatch:
    case ZSTD_v1_5_2_c_ldmBucketSizeLog:
    case ZSTD_v1_5_2_c_targetCBlockSize:
    case ZSTD_v1_5_2_c_srcSizeHint:
    case ZSTD_v1_5_2_c_stableInBuffer:
    case ZSTD_v1_5_2_c_stableOutBuffer:
    case ZSTD_v1_5_2_c_blockDelimiters:
    case ZSTD_v1_5_2_c_validateSequences:
    case ZSTD_v1_5_2_c_useBlockSplitter:
    case ZSTD_v1_5_2_c_useRowMatchFinder:
    case ZSTD_v1_5_2_c_deterministicRefPrefix:
        break;

    default: RETURN_ERROR(parameter_unsupported, "unknown parameter");
    }
    return ZSTD_v1_5_2_CCtxParams_setParameter(&cctx->requestedParams, param, value);
}

size_t ZSTD_v1_5_2_CCtxParams_setParameter(ZSTD_v1_5_2_CCtx_params* CCtxParams,
                                    ZSTD_v1_5_2_cParameter param, int value)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_CCtxParams_setParameter (%i, %i)", (int)param, value);
    switch(param)
    {
    case ZSTD_v1_5_2_c_format :
        BOUNDCHECK(ZSTD_v1_5_2_c_format, value);
        CCtxParams->format = (ZSTD_v1_5_2_format_e)value;
        return (size_t)CCtxParams->format;

    case ZSTD_v1_5_2_c_compressionLevel : {
        FORWARD_IF_ERROR(ZSTD_v1_5_2_cParam_clampBounds(param, &value), "");
        if (value == 0)
            CCtxParams->compressionLevel = ZSTD_v1_5_2_CLEVEL_DEFAULT; /* 0 == default */
        else
            CCtxParams->compressionLevel = value;
        if (CCtxParams->compressionLevel >= 0) return (size_t)CCtxParams->compressionLevel;
        return 0;  /* return type (size_t) cannot represent negative values */
    }

    case ZSTD_v1_5_2_c_windowLog :
        if (value!=0)   /* 0 => use default */
            BOUNDCHECK(ZSTD_v1_5_2_c_windowLog, value);
        CCtxParams->cParams.windowLog = (U32)value;
        return CCtxParams->cParams.windowLog;

    case ZSTD_v1_5_2_c_hashLog :
        if (value!=0)   /* 0 => use default */
            BOUNDCHECK(ZSTD_v1_5_2_c_hashLog, value);
        CCtxParams->cParams.hashLog = (U32)value;
        return CCtxParams->cParams.hashLog;

    case ZSTD_v1_5_2_c_chainLog :
        if (value!=0)   /* 0 => use default */
            BOUNDCHECK(ZSTD_v1_5_2_c_chainLog, value);
        CCtxParams->cParams.chainLog = (U32)value;
        return CCtxParams->cParams.chainLog;

    case ZSTD_v1_5_2_c_searchLog :
        if (value!=0)   /* 0 => use default */
            BOUNDCHECK(ZSTD_v1_5_2_c_searchLog, value);
        CCtxParams->cParams.searchLog = (U32)value;
        return (size_t)value;

    case ZSTD_v1_5_2_c_minMatch :
        if (value!=0)   /* 0 => use default */
            BOUNDCHECK(ZSTD_v1_5_2_c_minMatch, value);
        CCtxParams->cParams.minMatch = value;
        return CCtxParams->cParams.minMatch;

    case ZSTD_v1_5_2_c_targetLength :
        BOUNDCHECK(ZSTD_v1_5_2_c_targetLength, value);
        CCtxParams->cParams.targetLength = value;
        return CCtxParams->cParams.targetLength;

    case ZSTD_v1_5_2_c_strategy :
        if (value!=0)   /* 0 => use default */
            BOUNDCHECK(ZSTD_v1_5_2_c_strategy, value);
        CCtxParams->cParams.strategy = (ZSTD_v1_5_2_strategy)value;
        return (size_t)CCtxParams->cParams.strategy;

    case ZSTD_v1_5_2_c_contentSizeFlag :
        /* Content size written in frame header _when known_ (default:1) */
        DEBUGLOG(4, "set content size flag = %u", (value!=0));
        CCtxParams->fParams.contentSizeFlag = value != 0;
        return CCtxParams->fParams.contentSizeFlag;

    case ZSTD_v1_5_2_c_checksumFlag :
        /* A 32-bits content checksum will be calculated and written at end of frame (default:0) */
        CCtxParams->fParams.checksumFlag = value != 0;
        return CCtxParams->fParams.checksumFlag;

    case ZSTD_v1_5_2_c_dictIDFlag : /* When applicable, dictionary's dictID is provided in frame header (default:1) */
        DEBUGLOG(4, "set dictIDFlag = %u", (value!=0));
        CCtxParams->fParams.noDictIDFlag = !value;
        return !CCtxParams->fParams.noDictIDFlag;

    case ZSTD_v1_5_2_c_forceMaxWindow :
        CCtxParams->forceWindow = (value != 0);
        return CCtxParams->forceWindow;

    case ZSTD_v1_5_2_c_forceAttachDict : {
        const ZSTD_v1_5_2_dictAttachPref_e pref = (ZSTD_v1_5_2_dictAttachPref_e)value;
        BOUNDCHECK(ZSTD_v1_5_2_c_forceAttachDict, pref);
        CCtxParams->attachDictPref = pref;
        return CCtxParams->attachDictPref;
    }

    case ZSTD_v1_5_2_c_literalCompressionMode : {
        const ZSTD_v1_5_2_paramSwitch_e lcm = (ZSTD_v1_5_2_paramSwitch_e)value;
        BOUNDCHECK(ZSTD_v1_5_2_c_literalCompressionMode, lcm);
        CCtxParams->literalCompressionMode = lcm;
        return CCtxParams->literalCompressionMode;
    }

    case ZSTD_v1_5_2_c_nbWorkers :
#ifndef ZSTD_v1_5_2_MULTITHREAD
        RETURN_ERROR_IF(value!=0, parameter_unsupported, "not compiled with multithreading");
        return 0;
#else
        FORWARD_IF_ERROR(ZSTD_v1_5_2_cParam_clampBounds(param, &value), "");
        CCtxParams->nbWorkers = value;
        return CCtxParams->nbWorkers;
#endif

    case ZSTD_v1_5_2_c_jobSize :
#ifndef ZSTD_v1_5_2_MULTITHREAD
        RETURN_ERROR_IF(value!=0, parameter_unsupported, "not compiled with multithreading");
        return 0;
#else
        /* Adjust to the minimum non-default value. */
        if (value != 0 && value < ZSTDMT_JOBSIZE_MIN)
            value = ZSTDMT_JOBSIZE_MIN;
        FORWARD_IF_ERROR(ZSTD_v1_5_2_cParam_clampBounds(param, &value), "");
        assert(value >= 0);
        CCtxParams->jobSize = value;
        return CCtxParams->jobSize;
#endif

    case ZSTD_v1_5_2_c_overlapLog :
#ifndef ZSTD_v1_5_2_MULTITHREAD
        RETURN_ERROR_IF(value!=0, parameter_unsupported, "not compiled with multithreading");
        return 0;
#else
        FORWARD_IF_ERROR(ZSTD_v1_5_2_cParam_clampBounds(ZSTD_v1_5_2_c_overlapLog, &value), "");
        CCtxParams->overlapLog = value;
        return CCtxParams->overlapLog;
#endif

    case ZSTD_v1_5_2_c_rsyncable :
#ifndef ZSTD_v1_5_2_MULTITHREAD
        RETURN_ERROR_IF(value!=0, parameter_unsupported, "not compiled with multithreading");
        return 0;
#else
        FORWARD_IF_ERROR(ZSTD_v1_5_2_cParam_clampBounds(ZSTD_v1_5_2_c_overlapLog, &value), "");
        CCtxParams->rsyncable = value;
        return CCtxParams->rsyncable;
#endif

    case ZSTD_v1_5_2_c_enableDedicatedDictSearch :
        CCtxParams->enableDedicatedDictSearch = (value!=0);
        return CCtxParams->enableDedicatedDictSearch;

    case ZSTD_v1_5_2_c_enableLongDistanceMatching :
        CCtxParams->ldmParams.enableLdm = (ZSTD_v1_5_2_paramSwitch_e)value;
        return CCtxParams->ldmParams.enableLdm;

    case ZSTD_v1_5_2_c_ldmHashLog :
        if (value!=0)   /* 0 ==> auto */
            BOUNDCHECK(ZSTD_v1_5_2_c_ldmHashLog, value);
        CCtxParams->ldmParams.hashLog = value;
        return CCtxParams->ldmParams.hashLog;

    case ZSTD_v1_5_2_c_ldmMinMatch :
        if (value!=0)   /* 0 ==> default */
            BOUNDCHECK(ZSTD_v1_5_2_c_ldmMinMatch, value);
        CCtxParams->ldmParams.minMatchLength = value;
        return CCtxParams->ldmParams.minMatchLength;

    case ZSTD_v1_5_2_c_ldmBucketSizeLog :
        if (value!=0)   /* 0 ==> default */
            BOUNDCHECK(ZSTD_v1_5_2_c_ldmBucketSizeLog, value);
        CCtxParams->ldmParams.bucketSizeLog = value;
        return CCtxParams->ldmParams.bucketSizeLog;

    case ZSTD_v1_5_2_c_ldmHashRateLog :
        if (value!=0)   /* 0 ==> default */
            BOUNDCHECK(ZSTD_v1_5_2_c_ldmHashRateLog, value);
        CCtxParams->ldmParams.hashRateLog = value;
        return CCtxParams->ldmParams.hashRateLog;

    case ZSTD_v1_5_2_c_targetCBlockSize :
        if (value!=0)   /* 0 ==> default */
            BOUNDCHECK(ZSTD_v1_5_2_c_targetCBlockSize, value);
        CCtxParams->targetCBlockSize = value;
        return CCtxParams->targetCBlockSize;

    case ZSTD_v1_5_2_c_srcSizeHint :
        if (value!=0)    /* 0 ==> default */
            BOUNDCHECK(ZSTD_v1_5_2_c_srcSizeHint, value);
        CCtxParams->srcSizeHint = value;
        return CCtxParams->srcSizeHint;

    case ZSTD_v1_5_2_c_stableInBuffer:
        BOUNDCHECK(ZSTD_v1_5_2_c_stableInBuffer, value);
        CCtxParams->inBufferMode = (ZSTD_v1_5_2_bufferMode_e)value;
        return CCtxParams->inBufferMode;

    case ZSTD_v1_5_2_c_stableOutBuffer:
        BOUNDCHECK(ZSTD_v1_5_2_c_stableOutBuffer, value);
        CCtxParams->outBufferMode = (ZSTD_v1_5_2_bufferMode_e)value;
        return CCtxParams->outBufferMode;

    case ZSTD_v1_5_2_c_blockDelimiters:
        BOUNDCHECK(ZSTD_v1_5_2_c_blockDelimiters, value);
        CCtxParams->blockDelimiters = (ZSTD_v1_5_2_sequenceFormat_e)value;
        return CCtxParams->blockDelimiters;

    case ZSTD_v1_5_2_c_validateSequences:
        BOUNDCHECK(ZSTD_v1_5_2_c_validateSequences, value);
        CCtxParams->validateSequences = value;
        return CCtxParams->validateSequences;

    case ZSTD_v1_5_2_c_useBlockSplitter:
        BOUNDCHECK(ZSTD_v1_5_2_c_useBlockSplitter, value);
        CCtxParams->useBlockSplitter = (ZSTD_v1_5_2_paramSwitch_e)value;
        return CCtxParams->useBlockSplitter;

    case ZSTD_v1_5_2_c_useRowMatchFinder:
        BOUNDCHECK(ZSTD_v1_5_2_c_useRowMatchFinder, value);
        CCtxParams->useRowMatchFinder = (ZSTD_v1_5_2_paramSwitch_e)value;
        return CCtxParams->useRowMatchFinder;

    case ZSTD_v1_5_2_c_deterministicRefPrefix:
        BOUNDCHECK(ZSTD_v1_5_2_c_deterministicRefPrefix, value);
        CCtxParams->deterministicRefPrefix = !!value;
        return CCtxParams->deterministicRefPrefix;

    default: RETURN_ERROR(parameter_unsupported, "unknown parameter");
    }
}

size_t ZSTD_v1_5_2_CCtx_getParameter(ZSTD_v1_5_2_CCtx const* cctx, ZSTD_v1_5_2_cParameter param, int* value)
{
    return ZSTD_v1_5_2_CCtxParams_getParameter(&cctx->requestedParams, param, value);
}

size_t ZSTD_v1_5_2_CCtxParams_getParameter(
        ZSTD_v1_5_2_CCtx_params const* CCtxParams, ZSTD_v1_5_2_cParameter param, int* value)
{
    switch(param)
    {
    case ZSTD_v1_5_2_c_format :
        *value = CCtxParams->format;
        break;
    case ZSTD_v1_5_2_c_compressionLevel :
        *value = CCtxParams->compressionLevel;
        break;
    case ZSTD_v1_5_2_c_windowLog :
        *value = (int)CCtxParams->cParams.windowLog;
        break;
    case ZSTD_v1_5_2_c_hashLog :
        *value = (int)CCtxParams->cParams.hashLog;
        break;
    case ZSTD_v1_5_2_c_chainLog :
        *value = (int)CCtxParams->cParams.chainLog;
        break;
    case ZSTD_v1_5_2_c_searchLog :
        *value = CCtxParams->cParams.searchLog;
        break;
    case ZSTD_v1_5_2_c_minMatch :
        *value = CCtxParams->cParams.minMatch;
        break;
    case ZSTD_v1_5_2_c_targetLength :
        *value = CCtxParams->cParams.targetLength;
        break;
    case ZSTD_v1_5_2_c_strategy :
        *value = (unsigned)CCtxParams->cParams.strategy;
        break;
    case ZSTD_v1_5_2_c_contentSizeFlag :
        *value = CCtxParams->fParams.contentSizeFlag;
        break;
    case ZSTD_v1_5_2_c_checksumFlag :
        *value = CCtxParams->fParams.checksumFlag;
        break;
    case ZSTD_v1_5_2_c_dictIDFlag :
        *value = !CCtxParams->fParams.noDictIDFlag;
        break;
    case ZSTD_v1_5_2_c_forceMaxWindow :
        *value = CCtxParams->forceWindow;
        break;
    case ZSTD_v1_5_2_c_forceAttachDict :
        *value = CCtxParams->attachDictPref;
        break;
    case ZSTD_v1_5_2_c_literalCompressionMode :
        *value = CCtxParams->literalCompressionMode;
        break;
    case ZSTD_v1_5_2_c_nbWorkers :
#ifndef ZSTD_v1_5_2_MULTITHREAD
        assert(CCtxParams->nbWorkers == 0);
#endif
        *value = CCtxParams->nbWorkers;
        break;
    case ZSTD_v1_5_2_c_jobSize :
#ifndef ZSTD_v1_5_2_MULTITHREAD
        RETURN_ERROR(parameter_unsupported, "not compiled with multithreading");
#else
        assert(CCtxParams->jobSize <= INT_MAX);
        *value = (int)CCtxParams->jobSize;
        break;
#endif
    case ZSTD_v1_5_2_c_overlapLog :
#ifndef ZSTD_v1_5_2_MULTITHREAD
        RETURN_ERROR(parameter_unsupported, "not compiled with multithreading");
#else
        *value = CCtxParams->overlapLog;
        break;
#endif
    case ZSTD_v1_5_2_c_rsyncable :
#ifndef ZSTD_v1_5_2_MULTITHREAD
        RETURN_ERROR(parameter_unsupported, "not compiled with multithreading");
#else
        *value = CCtxParams->rsyncable;
        break;
#endif
    case ZSTD_v1_5_2_c_enableDedicatedDictSearch :
        *value = CCtxParams->enableDedicatedDictSearch;
        break;
    case ZSTD_v1_5_2_c_enableLongDistanceMatching :
        *value = CCtxParams->ldmParams.enableLdm;
        break;
    case ZSTD_v1_5_2_c_ldmHashLog :
        *value = CCtxParams->ldmParams.hashLog;
        break;
    case ZSTD_v1_5_2_c_ldmMinMatch :
        *value = CCtxParams->ldmParams.minMatchLength;
        break;
    case ZSTD_v1_5_2_c_ldmBucketSizeLog :
        *value = CCtxParams->ldmParams.bucketSizeLog;
        break;
    case ZSTD_v1_5_2_c_ldmHashRateLog :
        *value = CCtxParams->ldmParams.hashRateLog;
        break;
    case ZSTD_v1_5_2_c_targetCBlockSize :
        *value = (int)CCtxParams->targetCBlockSize;
        break;
    case ZSTD_v1_5_2_c_srcSizeHint :
        *value = (int)CCtxParams->srcSizeHint;
        break;
    case ZSTD_v1_5_2_c_stableInBuffer :
        *value = (int)CCtxParams->inBufferMode;
        break;
    case ZSTD_v1_5_2_c_stableOutBuffer :
        *value = (int)CCtxParams->outBufferMode;
        break;
    case ZSTD_v1_5_2_c_blockDelimiters :
        *value = (int)CCtxParams->blockDelimiters;
        break;
    case ZSTD_v1_5_2_c_validateSequences :
        *value = (int)CCtxParams->validateSequences;
        break;
    case ZSTD_v1_5_2_c_useBlockSplitter :
        *value = (int)CCtxParams->useBlockSplitter;
        break;
    case ZSTD_v1_5_2_c_useRowMatchFinder :
        *value = (int)CCtxParams->useRowMatchFinder;
        break;
    case ZSTD_v1_5_2_c_deterministicRefPrefix:
        *value = (int)CCtxParams->deterministicRefPrefix;
        break;
    default: RETURN_ERROR(parameter_unsupported, "unknown parameter");
    }
    return 0;
}

/** ZSTD_v1_5_2_CCtx_setParametersUsingCCtxParams() :
 *  just applies `params` into `cctx`
 *  no action is performed, parameters are merely stored.
 *  If ZSTDMT is enabled, parameters are pushed to cctx->mtctx.
 *    This is possible even if a compression is ongoing.
 *    In which case, new parameters will be applied on the fly, starting with next compression job.
 */
size_t ZSTD_v1_5_2_CCtx_setParametersUsingCCtxParams(
        ZSTD_v1_5_2_CCtx* cctx, const ZSTD_v1_5_2_CCtx_params* params)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_CCtx_setParametersUsingCCtxParams");
    RETURN_ERROR_IF(cctx->streamStage != zcss_init, stage_wrong,
                    "The context is in the wrong stage!");
    RETURN_ERROR_IF(cctx->cdict, stage_wrong,
                    "Can't override parameters with cdict attached (some must "
                    "be inherited from the cdict).");

    cctx->requestedParams = *params;
    return 0;
}

size_t ZSTD_v1_5_2_CCtx_setPledgedSrcSize(ZSTD_v1_5_2_CCtx* cctx, unsigned long long pledgedSrcSize)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_CCtx_setPledgedSrcSize to %u bytes", (U32)pledgedSrcSize);
    RETURN_ERROR_IF(cctx->streamStage != zcss_init, stage_wrong,
                    "Can't set pledgedSrcSize when not in init stage.");
    cctx->pledgedSrcSizePlusOne = pledgedSrcSize+1;
    return 0;
}

static ZSTD_v1_5_2_compressionParameters ZSTD_v1_5_2_dedicatedDictSearch_getCParams(
        int const compressionLevel,
        size_t const dictSize);
static int ZSTD_v1_5_2_dedicatedDictSearch_isSupported(
        const ZSTD_v1_5_2_compressionParameters* cParams);
static void ZSTD_v1_5_2_dedicatedDictSearch_revertCParams(
        ZSTD_v1_5_2_compressionParameters* cParams);

/**
 * Initializes the local dict using the requested parameters.
 * NOTE: This does not use the pledged src size, because it may be used for more
 * than one compression.
 */
static size_t ZSTD_v1_5_2_initLocalDict(ZSTD_v1_5_2_CCtx* cctx)
{
    ZSTD_v1_5_2_localDict* const dl = &cctx->localDict;
    if (dl->dict == NULL) {
        /* No local dictionary. */
        assert(dl->dictBuffer == NULL);
        assert(dl->cdict == NULL);
        assert(dl->dictSize == 0);
        return 0;
    }
    if (dl->cdict != NULL) {
        assert(cctx->cdict == dl->cdict);
        /* Local dictionary already initialized. */
        return 0;
    }
    assert(dl->dictSize > 0);
    assert(cctx->cdict == NULL);
    assert(cctx->prefixDict.dict == NULL);

    dl->cdict = ZSTD_v1_5_2_createCDict_advanced2(
            dl->dict,
            dl->dictSize,
            ZSTD_v1_5_2_dlm_byRef,
            dl->dictContentType,
            &cctx->requestedParams,
            cctx->customMem);
    RETURN_ERROR_IF(!dl->cdict, memory_allocation, "ZSTD_v1_5_2_createCDict_advanced failed");
    cctx->cdict = dl->cdict;
    return 0;
}

size_t ZSTD_v1_5_2_CCtx_loadDictionary_advanced(
        ZSTD_v1_5_2_CCtx* cctx, const void* dict, size_t dictSize,
        ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod, ZSTD_v1_5_2_dictContentType_e dictContentType)
{
    RETURN_ERROR_IF(cctx->streamStage != zcss_init, stage_wrong,
                    "Can't load a dictionary when ctx is not in init stage.");
    DEBUGLOG(4, "ZSTD_v1_5_2_CCtx_loadDictionary_advanced (size: %u)", (U32)dictSize);
    ZSTD_v1_5_2_clearAllDicts(cctx);  /* in case one already exists */
    if (dict == NULL || dictSize == 0)  /* no dictionary mode */
        return 0;
    if (dictLoadMethod == ZSTD_v1_5_2_dlm_byRef) {
        cctx->localDict.dict = dict;
    } else {
        void* dictBuffer;
        RETURN_ERROR_IF(cctx->staticSize, memory_allocation,
                        "no malloc for static CCtx");
        dictBuffer = ZSTD_v1_5_2_customMalloc(dictSize, cctx->customMem);
        RETURN_ERROR_IF(!dictBuffer, memory_allocation, "NULL pointer!");
        ZSTD_v1_5_2_memcpy(dictBuffer, dict, dictSize);
        cctx->localDict.dictBuffer = dictBuffer;
        cctx->localDict.dict = dictBuffer;
    }
    cctx->localDict.dictSize = dictSize;
    cctx->localDict.dictContentType = dictContentType;
    return 0;
}

size_t ZSTD_v1_5_2_CCtx_loadDictionary_byReference(
      ZSTD_v1_5_2_CCtx* cctx, const void* dict, size_t dictSize)
{
    return ZSTD_v1_5_2_CCtx_loadDictionary_advanced(
            cctx, dict, dictSize, ZSTD_v1_5_2_dlm_byRef, ZSTD_v1_5_2_dct_auto);
}

size_t ZSTD_v1_5_2_CCtx_loadDictionary(ZSTD_v1_5_2_CCtx* cctx, const void* dict, size_t dictSize)
{
    return ZSTD_v1_5_2_CCtx_loadDictionary_advanced(
            cctx, dict, dictSize, ZSTD_v1_5_2_dlm_byCopy, ZSTD_v1_5_2_dct_auto);
}


size_t ZSTD_v1_5_2_CCtx_refCDict(ZSTD_v1_5_2_CCtx* cctx, const ZSTD_v1_5_2_CDict* cdict)
{
    RETURN_ERROR_IF(cctx->streamStage != zcss_init, stage_wrong,
                    "Can't ref a dict when ctx not in init stage.");
    /* Free the existing local cdict (if any) to save memory. */
    ZSTD_v1_5_2_clearAllDicts(cctx);
    cctx->cdict = cdict;
    return 0;
}

size_t ZSTD_v1_5_2_CCtx_refThreadPool(ZSTD_v1_5_2_CCtx* cctx, ZSTD_v1_5_2_threadPool* pool)
{
    RETURN_ERROR_IF(cctx->streamStage != zcss_init, stage_wrong,
                    "Can't ref a pool when ctx not in init stage.");
    cctx->pool = pool;
    return 0;
}

size_t ZSTD_v1_5_2_CCtx_refPrefix(ZSTD_v1_5_2_CCtx* cctx, const void* prefix, size_t prefixSize)
{
    return ZSTD_v1_5_2_CCtx_refPrefix_advanced(cctx, prefix, prefixSize, ZSTD_v1_5_2_dct_rawContent);
}

size_t ZSTD_v1_5_2_CCtx_refPrefix_advanced(
        ZSTD_v1_5_2_CCtx* cctx, const void* prefix, size_t prefixSize, ZSTD_v1_5_2_dictContentType_e dictContentType)
{
    RETURN_ERROR_IF(cctx->streamStage != zcss_init, stage_wrong,
                    "Can't ref a prefix when ctx not in init stage.");
    ZSTD_v1_5_2_clearAllDicts(cctx);
    if (prefix != NULL && prefixSize > 0) {
        cctx->prefixDict.dict = prefix;
        cctx->prefixDict.dictSize = prefixSize;
        cctx->prefixDict.dictContentType = dictContentType;
    }
    return 0;
}

/*! ZSTD_v1_5_2_CCtx_reset() :
 *  Also dumps dictionary */
size_t ZSTD_v1_5_2_CCtx_reset(ZSTD_v1_5_2_CCtx* cctx, ZSTD_v1_5_2_ResetDirective reset)
{
    if ( (reset == ZSTD_v1_5_2_reset_session_only)
      || (reset == ZSTD_v1_5_2_reset_session_and_parameters) ) {
        cctx->streamStage = zcss_init;
        cctx->pledgedSrcSizePlusOne = 0;
    }
    if ( (reset == ZSTD_v1_5_2_reset_parameters)
      || (reset == ZSTD_v1_5_2_reset_session_and_parameters) ) {
        RETURN_ERROR_IF(cctx->streamStage != zcss_init, stage_wrong,
                        "Can't reset parameters only when not in init stage.");
        ZSTD_v1_5_2_clearAllDicts(cctx);
        return ZSTD_v1_5_2_CCtxParams_reset(&cctx->requestedParams);
    }
    return 0;
}


/** ZSTD_v1_5_2_checkCParams() :
    control CParam values remain within authorized range.
    @return : 0, or an error code if one value is beyond authorized range */
size_t ZSTD_v1_5_2_checkCParams(ZSTD_v1_5_2_compressionParameters cParams)
{
    BOUNDCHECK(ZSTD_v1_5_2_c_windowLog, (int)cParams.windowLog);
    BOUNDCHECK(ZSTD_v1_5_2_c_chainLog,  (int)cParams.chainLog);
    BOUNDCHECK(ZSTD_v1_5_2_c_hashLog,   (int)cParams.hashLog);
    BOUNDCHECK(ZSTD_v1_5_2_c_searchLog, (int)cParams.searchLog);
    BOUNDCHECK(ZSTD_v1_5_2_c_minMatch,  (int)cParams.minMatch);
    BOUNDCHECK(ZSTD_v1_5_2_c_targetLength,(int)cParams.targetLength);
    BOUNDCHECK(ZSTD_v1_5_2_c_strategy,  cParams.strategy);
    return 0;
}

/** ZSTD_v1_5_2_clampCParams() :
 *  make CParam values within valid range.
 *  @return : valid CParams */
static ZSTD_v1_5_2_compressionParameters
ZSTD_v1_5_2_clampCParams(ZSTD_v1_5_2_compressionParameters cParams)
{
#   define CLAMP_TYPE(cParam, val, type) {                                \
        ZSTD_v1_5_2_bounds const bounds = ZSTD_v1_5_2_cParam_getBounds(cParam);         \
        if ((int)val<bounds.lowerBound) val=(type)bounds.lowerBound;      \
        else if ((int)val>bounds.upperBound) val=(type)bounds.upperBound; \
    }
#   define CLAMP(cParam, val) CLAMP_TYPE(cParam, val, unsigned)
    CLAMP(ZSTD_v1_5_2_c_windowLog, cParams.windowLog);
    CLAMP(ZSTD_v1_5_2_c_chainLog,  cParams.chainLog);
    CLAMP(ZSTD_v1_5_2_c_hashLog,   cParams.hashLog);
    CLAMP(ZSTD_v1_5_2_c_searchLog, cParams.searchLog);
    CLAMP(ZSTD_v1_5_2_c_minMatch,  cParams.minMatch);
    CLAMP(ZSTD_v1_5_2_c_targetLength,cParams.targetLength);
    CLAMP_TYPE(ZSTD_v1_5_2_c_strategy,cParams.strategy, ZSTD_v1_5_2_strategy);
    return cParams;
}

/** ZSTD_v1_5_2_cycleLog() :
 *  condition for correct operation : hashLog > 1 */
U32 ZSTD_v1_5_2_cycleLog(U32 hashLog, ZSTD_v1_5_2_strategy strat)
{
    U32 const btScale = ((U32)strat >= (U32)ZSTD_v1_5_2_btlazy2);
    return hashLog - btScale;
}

/** ZSTD_v1_5_2_dictAndWindowLog() :
 * Returns an adjusted window log that is large enough to fit the source and the dictionary.
 * The zstd format says that the entire dictionary is valid if one byte of the dictionary
 * is within the window. So the hashLog and chainLog should be large enough to reference both
 * the dictionary and the window. So we must use this adjusted dictAndWindowLog when downsizing
 * the hashLog and windowLog.
 * NOTE: srcSize must not be ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN.
 */
static U32 ZSTD_v1_5_2_dictAndWindowLog(U32 windowLog, U64 srcSize, U64 dictSize)
{
    const U64 maxWindowSize = 1ULL << ZSTD_v1_5_2_WINDOWLOG_MAX;
    /* No dictionary ==> No change */
    if (dictSize == 0) {
        return windowLog;
    }
    assert(windowLog <= ZSTD_v1_5_2_WINDOWLOG_MAX);
    assert(srcSize != ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN); /* Handled in ZSTD_v1_5_2_adjustCParams_internal() */
    {
        U64 const windowSize = 1ULL << windowLog;
        U64 const dictAndWindowSize = dictSize + windowSize;
        /* If the window size is already large enough to fit both the source and the dictionary
         * then just use the window size. Otherwise adjust so that it fits the dictionary and
         * the window.
         */
        if (windowSize >= dictSize + srcSize) {
            return windowLog; /* Window size large enough already */
        } else if (dictAndWindowSize >= maxWindowSize) {
            return ZSTD_v1_5_2_WINDOWLOG_MAX; /* Larger than max window log */
        } else  {
            return ZSTD_v1_5_2_highbit32((U32)dictAndWindowSize - 1) + 1;
        }
    }
}

/** ZSTD_v1_5_2_adjustCParams_internal() :
 *  optimize `cPar` for a specified input (`srcSize` and `dictSize`).
 *  mostly downsize to reduce memory consumption and initialization latency.
 * `srcSize` can be ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN when not known.
 * `mode` is the mode for parameter adjustment. See docs for `ZSTD_v1_5_2_cParamMode_e`.
 *  note : `srcSize==0` means 0!
 *  condition : cPar is presumed validated (can be checked using ZSTD_v1_5_2_checkCParams()). */
static ZSTD_v1_5_2_compressionParameters
ZSTD_v1_5_2_adjustCParams_internal(ZSTD_v1_5_2_compressionParameters cPar,
                            unsigned long long srcSize,
                            size_t dictSize,
                            ZSTD_v1_5_2_cParamMode_e mode)
{
    const U64 minSrcSize = 513; /* (1<<9) + 1 */
    const U64 maxWindowResize = 1ULL << (ZSTD_v1_5_2_WINDOWLOG_MAX-1);
    assert(ZSTD_v1_5_2_checkCParams(cPar)==0);

    switch (mode) {
    case ZSTD_v1_5_2_cpm_unknown:
    case ZSTD_v1_5_2_cpm_noAttachDict:
        /* If we don't know the source size, don't make any
         * assumptions about it. We will already have selected
         * smaller parameters if a dictionary is in use.
         */
        break;
    case ZSTD_v1_5_2_cpm_createCDict:
        /* Assume a small source size when creating a dictionary
         * with an unknown source size.
         */
        if (dictSize && srcSize == ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN)
            srcSize = minSrcSize;
        break;
    case ZSTD_v1_5_2_cpm_attachDict:
        /* Dictionary has its own dedicated parameters which have
         * already been selected. We are selecting parameters
         * for only the source.
         */
        dictSize = 0;
        break;
    default:
        assert(0);
        break;
    }

    /* resize windowLog if input is small enough, to use less memory */
    if ( (srcSize < maxWindowResize)
      && (dictSize < maxWindowResize) )  {
        U32 const tSize = (U32)(srcSize + dictSize);
        static U32 const hashSizeMin = 1 << ZSTD_v1_5_2_HASHLOG_MIN;
        U32 const srcLog = (tSize < hashSizeMin) ? ZSTD_v1_5_2_HASHLOG_MIN :
                            ZSTD_v1_5_2_highbit32(tSize-1) + 1;
        if (cPar.windowLog > srcLog) cPar.windowLog = srcLog;
    }
    if (srcSize != ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN) {
        U32 const dictAndWindowLog = ZSTD_v1_5_2_dictAndWindowLog(cPar.windowLog, (U64)srcSize, (U64)dictSize);
        U32 const cycleLog = ZSTD_v1_5_2_cycleLog(cPar.chainLog, cPar.strategy);
        if (cPar.hashLog > dictAndWindowLog+1) cPar.hashLog = dictAndWindowLog+1;
        if (cycleLog > dictAndWindowLog)
            cPar.chainLog -= (cycleLog - dictAndWindowLog);
    }

    if (cPar.windowLog < ZSTD_v1_5_2_WINDOWLOG_ABSOLUTEMIN)
        cPar.windowLog = ZSTD_v1_5_2_WINDOWLOG_ABSOLUTEMIN;  /* minimum wlog required for valid frame header */

    return cPar;
}

ZSTD_v1_5_2_compressionParameters
ZSTD_v1_5_2_adjustCParams(ZSTD_v1_5_2_compressionParameters cPar,
                   unsigned long long srcSize,
                   size_t dictSize)
{
    cPar = ZSTD_v1_5_2_clampCParams(cPar);   /* resulting cPar is necessarily valid (all parameters within range) */
    if (srcSize == 0) srcSize = ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN;
    return ZSTD_v1_5_2_adjustCParams_internal(cPar, srcSize, dictSize, ZSTD_v1_5_2_cpm_unknown);
}

static ZSTD_v1_5_2_compressionParameters ZSTD_v1_5_2_getCParams_internal(int compressionLevel, unsigned long long srcSizeHint, size_t dictSize, ZSTD_v1_5_2_cParamMode_e mode);
static ZSTD_v1_5_2_parameters ZSTD_v1_5_2_getParams_internal(int compressionLevel, unsigned long long srcSizeHint, size_t dictSize, ZSTD_v1_5_2_cParamMode_e mode);

static void ZSTD_v1_5_2_overrideCParams(
              ZSTD_v1_5_2_compressionParameters* cParams,
        const ZSTD_v1_5_2_compressionParameters* overrides)
{
    if (overrides->windowLog)    cParams->windowLog    = overrides->windowLog;
    if (overrides->hashLog)      cParams->hashLog      = overrides->hashLog;
    if (overrides->chainLog)     cParams->chainLog     = overrides->chainLog;
    if (overrides->searchLog)    cParams->searchLog    = overrides->searchLog;
    if (overrides->minMatch)     cParams->minMatch     = overrides->minMatch;
    if (overrides->targetLength) cParams->targetLength = overrides->targetLength;
    if (overrides->strategy)     cParams->strategy     = overrides->strategy;
}

ZSTD_v1_5_2_compressionParameters ZSTD_v1_5_2_getCParamsFromCCtxParams(
        const ZSTD_v1_5_2_CCtx_params* CCtxParams, U64 srcSizeHint, size_t dictSize, ZSTD_v1_5_2_cParamMode_e mode)
{
    ZSTD_v1_5_2_compressionParameters cParams;
    if (srcSizeHint == ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN && CCtxParams->srcSizeHint > 0) {
      srcSizeHint = CCtxParams->srcSizeHint;
    }
    cParams = ZSTD_v1_5_2_getCParams_internal(CCtxParams->compressionLevel, srcSizeHint, dictSize, mode);
    if (CCtxParams->ldmParams.enableLdm == ZSTD_v1_5_2_ps_enable) cParams.windowLog = ZSTD_v1_5_2_LDM_DEFAULT_WINDOW_LOG;
    ZSTD_v1_5_2_overrideCParams(&cParams, &CCtxParams->cParams);
    assert(!ZSTD_v1_5_2_checkCParams(cParams));
    /* srcSizeHint == 0 means 0 */
    return ZSTD_v1_5_2_adjustCParams_internal(cParams, srcSizeHint, dictSize, mode);
}

static size_t
ZSTD_v1_5_2_sizeof_matchState(const ZSTD_v1_5_2_compressionParameters* const cParams,
                       const ZSTD_v1_5_2_paramSwitch_e useRowMatchFinder,
                       const U32 enableDedicatedDictSearch,
                       const U32 forCCtx)
{
    /* chain table size should be 0 for fast or row-hash strategies */
    size_t const chainSize = ZSTD_v1_5_2_allocateChainTable(cParams->strategy, useRowMatchFinder, enableDedicatedDictSearch && !forCCtx)
                                ? ((size_t)1 << cParams->chainLog)
                                : 0;
    size_t const hSize = ((size_t)1) << cParams->hashLog;
    U32    const hashLog3 = (forCCtx && cParams->minMatch==3) ? MIN(ZSTD_v1_5_2_HASHLOG3_MAX, cParams->windowLog) : 0;
    size_t const h3Size = hashLog3 ? ((size_t)1) << hashLog3 : 0;
    /* We don't use ZSTD_v1_5_2_cwksp_alloc_size() here because the tables aren't
     * surrounded by redzones in ASAN. */
    size_t const tableSpace = chainSize * sizeof(U32)
                            + hSize * sizeof(U32)
                            + h3Size * sizeof(U32);
    size_t const optPotentialSpace =
        ZSTD_v1_5_2_cwksp_aligned_alloc_size((MaxML+1) * sizeof(U32))
      + ZSTD_v1_5_2_cwksp_aligned_alloc_size((MaxLL+1) * sizeof(U32))
      + ZSTD_v1_5_2_cwksp_aligned_alloc_size((MaxOff+1) * sizeof(U32))
      + ZSTD_v1_5_2_cwksp_aligned_alloc_size((1<<Litbits) * sizeof(U32))
      + ZSTD_v1_5_2_cwksp_aligned_alloc_size((ZSTD_v1_5_2_OPT_NUM+1) * sizeof(ZSTD_v1_5_2_match_t))
      + ZSTD_v1_5_2_cwksp_aligned_alloc_size((ZSTD_v1_5_2_OPT_NUM+1) * sizeof(ZSTD_v1_5_2_optimal_t));
    size_t const lazyAdditionalSpace = ZSTD_v1_5_2_rowMatchFinderUsed(cParams->strategy, useRowMatchFinder)
                                            ? ZSTD_v1_5_2_cwksp_aligned_alloc_size(hSize*sizeof(U16))
                                            : 0;
    size_t const optSpace = (forCCtx && (cParams->strategy >= ZSTD_v1_5_2_btopt))
                                ? optPotentialSpace
                                : 0;
    size_t const slackSpace = ZSTD_v1_5_2_cwksp_slack_space_required();

    /* tables are guaranteed to be sized in multiples of 64 bytes (or 16 uint32_t) */
    ZSTD_v1_5_2_STATIC_ASSERT(ZSTD_v1_5_2_HASHLOG_MIN >= 4 && ZSTD_v1_5_2_WINDOWLOG_MIN >= 4 && ZSTD_v1_5_2_CHAINLOG_MIN >= 4);
    assert(useRowMatchFinder != ZSTD_v1_5_2_ps_auto);

    DEBUGLOG(4, "chainSize: %u - hSize: %u - h3Size: %u",
                (U32)chainSize, (U32)hSize, (U32)h3Size);
    return tableSpace + optSpace + slackSpace + lazyAdditionalSpace;
}

static size_t ZSTD_v1_5_2_estimateCCtxSize_usingCCtxParams_internal(
        const ZSTD_v1_5_2_compressionParameters* cParams,
        const ldmParams_t* ldmParams,
        const int isStatic,
        const ZSTD_v1_5_2_paramSwitch_e useRowMatchFinder,
        const size_t buffInSize,
        const size_t buffOutSize,
        const U64 pledgedSrcSize)
{
    size_t const windowSize = (size_t) BOUNDED(1ULL, 1ULL << cParams->windowLog, pledgedSrcSize);
    size_t const blockSize = MIN(ZSTD_v1_5_2_BLOCKSIZE_MAX, windowSize);
    U32    const divider = (cParams->minMatch==3) ? 3 : 4;
    size_t const maxNbSeq = blockSize / divider;
    size_t const tokenSpace = ZSTD_v1_5_2_cwksp_alloc_size(WILDCOPY_OVERLENGTH + blockSize)
                            + ZSTD_v1_5_2_cwksp_aligned_alloc_size(maxNbSeq * sizeof(seqDef))
                            + 3 * ZSTD_v1_5_2_cwksp_alloc_size(maxNbSeq * sizeof(BYTE));
    size_t const entropySpace = ZSTD_v1_5_2_cwksp_alloc_size(ENTROPY_WORKSPACE_SIZE);
    size_t const blockStateSpace = 2 * ZSTD_v1_5_2_cwksp_alloc_size(sizeof(ZSTD_v1_5_2_compressedBlockState_t));
    size_t const matchStateSize = ZSTD_v1_5_2_sizeof_matchState(cParams, useRowMatchFinder, /* enableDedicatedDictSearch */ 0, /* forCCtx */ 1);

    size_t const ldmSpace = ZSTD_v1_5_2_ldm_getTableSize(*ldmParams);
    size_t const maxNbLdmSeq = ZSTD_v1_5_2_ldm_getMaxNbSeq(*ldmParams, blockSize);
    size_t const ldmSeqSpace = ldmParams->enableLdm == ZSTD_v1_5_2_ps_enable ?
        ZSTD_v1_5_2_cwksp_aligned_alloc_size(maxNbLdmSeq * sizeof(rawSeq)) : 0;


    size_t const bufferSpace = ZSTD_v1_5_2_cwksp_alloc_size(buffInSize)
                             + ZSTD_v1_5_2_cwksp_alloc_size(buffOutSize);

    size_t const cctxSpace = isStatic ? ZSTD_v1_5_2_cwksp_alloc_size(sizeof(ZSTD_v1_5_2_CCtx)) : 0;

    size_t const neededSpace =
        cctxSpace +
        entropySpace +
        blockStateSpace +
        ldmSpace +
        ldmSeqSpace +
        matchStateSize +
        tokenSpace +
        bufferSpace;

    DEBUGLOG(5, "estimate workspace : %u", (U32)neededSpace);
    return neededSpace;
}

size_t ZSTD_v1_5_2_estimateCCtxSize_usingCCtxParams(const ZSTD_v1_5_2_CCtx_params* params)
{
    ZSTD_v1_5_2_compressionParameters const cParams =
                ZSTD_v1_5_2_getCParamsFromCCtxParams(params, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN, 0, ZSTD_v1_5_2_cpm_noAttachDict);
    ZSTD_v1_5_2_paramSwitch_e const useRowMatchFinder = ZSTD_v1_5_2_resolveRowMatchFinderMode(params->useRowMatchFinder,
                                                                               &cParams);

    RETURN_ERROR_IF(params->nbWorkers > 0, GENERIC, "Estimate CCtx size is supported for single-threaded compression only.");
    /* estimateCCtxSize is for one-shot compression. So no buffers should
     * be needed. However, we still allocate two 0-sized buffers, which can
     * take space under ASAN. */
    return ZSTD_v1_5_2_estimateCCtxSize_usingCCtxParams_internal(
        &cParams, &params->ldmParams, 1, useRowMatchFinder, 0, 0, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN);
}

size_t ZSTD_v1_5_2_estimateCCtxSize_usingCParams(ZSTD_v1_5_2_compressionParameters cParams)
{
    ZSTD_v1_5_2_CCtx_params initialParams = ZSTD_v1_5_2_makeCCtxParamsFromCParams(cParams);
    if (ZSTD_v1_5_2_rowMatchFinderSupported(cParams.strategy)) {
        /* Pick bigger of not using and using row-based matchfinder for greedy and lazy strategies */
        size_t noRowCCtxSize;
        size_t rowCCtxSize;
        initialParams.useRowMatchFinder = ZSTD_v1_5_2_ps_disable;
        noRowCCtxSize = ZSTD_v1_5_2_estimateCCtxSize_usingCCtxParams(&initialParams);
        initialParams.useRowMatchFinder = ZSTD_v1_5_2_ps_enable;
        rowCCtxSize = ZSTD_v1_5_2_estimateCCtxSize_usingCCtxParams(&initialParams);
        return MAX(noRowCCtxSize, rowCCtxSize);
    } else {
        return ZSTD_v1_5_2_estimateCCtxSize_usingCCtxParams(&initialParams);
    }
}

static size_t ZSTD_v1_5_2_estimateCCtxSize_internal(int compressionLevel)
{
    int tier = 0;
    size_t largestSize = 0;
    static const unsigned long long srcSizeTiers[4] = {16 KB, 128 KB, 256 KB, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN};
    for (; tier < 4; ++tier) {
        /* Choose the set of cParams for a given level across all srcSizes that give the largest cctxSize */
        ZSTD_v1_5_2_compressionParameters const cParams = ZSTD_v1_5_2_getCParams_internal(compressionLevel, srcSizeTiers[tier], 0, ZSTD_v1_5_2_cpm_noAttachDict);
        largestSize = MAX(ZSTD_v1_5_2_estimateCCtxSize_usingCParams(cParams), largestSize);
    }
    return largestSize;
}

size_t ZSTD_v1_5_2_estimateCCtxSize(int compressionLevel)
{
    int level;
    size_t memBudget = 0;
    for (level=MIN(compressionLevel, 1); level<=compressionLevel; level++) {
        /* Ensure monotonically increasing memory usage as compression level increases */
        size_t const newMB = ZSTD_v1_5_2_estimateCCtxSize_internal(level);
        if (newMB > memBudget) memBudget = newMB;
    }
    return memBudget;
}

size_t ZSTD_v1_5_2_estimateCStreamSize_usingCCtxParams(const ZSTD_v1_5_2_CCtx_params* params)
{
    RETURN_ERROR_IF(params->nbWorkers > 0, GENERIC, "Estimate CCtx size is supported for single-threaded compression only.");
    {   ZSTD_v1_5_2_compressionParameters const cParams =
                ZSTD_v1_5_2_getCParamsFromCCtxParams(params, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN, 0, ZSTD_v1_5_2_cpm_noAttachDict);
        size_t const blockSize = MIN(ZSTD_v1_5_2_BLOCKSIZE_MAX, (size_t)1 << cParams.windowLog);
        size_t const inBuffSize = (params->inBufferMode == ZSTD_v1_5_2_bm_buffered)
                ? ((size_t)1 << cParams.windowLog) + blockSize
                : 0;
        size_t const outBuffSize = (params->outBufferMode == ZSTD_v1_5_2_bm_buffered)
                ? ZSTD_v1_5_2_compressBound(blockSize) + 1
                : 0;
        ZSTD_v1_5_2_paramSwitch_e const useRowMatchFinder = ZSTD_v1_5_2_resolveRowMatchFinderMode(params->useRowMatchFinder, &params->cParams);

        return ZSTD_v1_5_2_estimateCCtxSize_usingCCtxParams_internal(
            &cParams, &params->ldmParams, 1, useRowMatchFinder, inBuffSize, outBuffSize,
            ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN);
    }
}

size_t ZSTD_v1_5_2_estimateCStreamSize_usingCParams(ZSTD_v1_5_2_compressionParameters cParams)
{
    ZSTD_v1_5_2_CCtx_params initialParams = ZSTD_v1_5_2_makeCCtxParamsFromCParams(cParams);
    if (ZSTD_v1_5_2_rowMatchFinderSupported(cParams.strategy)) {
        /* Pick bigger of not using and using row-based matchfinder for greedy and lazy strategies */
        size_t noRowCCtxSize;
        size_t rowCCtxSize;
        initialParams.useRowMatchFinder = ZSTD_v1_5_2_ps_disable;
        noRowCCtxSize = ZSTD_v1_5_2_estimateCStreamSize_usingCCtxParams(&initialParams);
        initialParams.useRowMatchFinder = ZSTD_v1_5_2_ps_enable;
        rowCCtxSize = ZSTD_v1_5_2_estimateCStreamSize_usingCCtxParams(&initialParams);
        return MAX(noRowCCtxSize, rowCCtxSize);
    } else {
        return ZSTD_v1_5_2_estimateCStreamSize_usingCCtxParams(&initialParams);
    }
}

static size_t ZSTD_v1_5_2_estimateCStreamSize_internal(int compressionLevel)
{
    ZSTD_v1_5_2_compressionParameters const cParams = ZSTD_v1_5_2_getCParams_internal(compressionLevel, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN, 0, ZSTD_v1_5_2_cpm_noAttachDict);
    return ZSTD_v1_5_2_estimateCStreamSize_usingCParams(cParams);
}

size_t ZSTD_v1_5_2_estimateCStreamSize(int compressionLevel)
{
    int level;
    size_t memBudget = 0;
    for (level=MIN(compressionLevel, 1); level<=compressionLevel; level++) {
        size_t const newMB = ZSTD_v1_5_2_estimateCStreamSize_internal(level);
        if (newMB > memBudget) memBudget = newMB;
    }
    return memBudget;
}

/* ZSTD_v1_5_2_getFrameProgression():
 * tells how much data has been consumed (input) and produced (output) for current frame.
 * able to count progression inside worker threads (non-blocking mode).
 */
ZSTD_v1_5_2_frameProgression ZSTD_v1_5_2_getFrameProgression(const ZSTD_v1_5_2_CCtx* cctx)
{
#ifdef ZSTD_v1_5_2_MULTITHREAD
    if (cctx->appliedParams.nbWorkers > 0) {
        return ZSTDMT_getFrameProgression(cctx->mtctx);
    }
#endif
    {   ZSTD_v1_5_2_frameProgression fp;
        size_t const buffered = (cctx->inBuff == NULL) ? 0 :
                                cctx->inBuffPos - cctx->inToCompress;
        if (buffered) assert(cctx->inBuffPos >= cctx->inToCompress);
        assert(buffered <= ZSTD_v1_5_2_BLOCKSIZE_MAX);
        fp.ingested = cctx->consumedSrcSize + buffered;
        fp.consumed = cctx->consumedSrcSize;
        fp.produced = cctx->producedCSize;
        fp.flushed  = cctx->producedCSize;   /* simplified; some data might still be left within streaming output buffer */
        fp.currentJobID = 0;
        fp.nbActiveWorkers = 0;
        return fp;
}   }

/*! ZSTD_v1_5_2_toFlushNow()
 *  Only useful for multithreading scenarios currently (nbWorkers >= 1).
 */
size_t ZSTD_v1_5_2_toFlushNow(ZSTD_v1_5_2_CCtx* cctx)
{
#ifdef ZSTD_v1_5_2_MULTITHREAD
    if (cctx->appliedParams.nbWorkers > 0) {
        return ZSTDMT_toFlushNow(cctx->mtctx);
    }
#endif
    (void)cctx;
    return 0;   /* over-simplification; could also check if context is currently running in streaming mode, and in which case, report how many bytes are left to be flushed within output buffer */
}

static void ZSTD_v1_5_2_assertEqualCParams(ZSTD_v1_5_2_compressionParameters cParams1,
                                    ZSTD_v1_5_2_compressionParameters cParams2)
{
    (void)cParams1;
    (void)cParams2;
    assert(cParams1.windowLog    == cParams2.windowLog);
    assert(cParams1.chainLog     == cParams2.chainLog);
    assert(cParams1.hashLog      == cParams2.hashLog);
    assert(cParams1.searchLog    == cParams2.searchLog);
    assert(cParams1.minMatch     == cParams2.minMatch);
    assert(cParams1.targetLength == cParams2.targetLength);
    assert(cParams1.strategy     == cParams2.strategy);
}

void ZSTD_v1_5_2_reset_compressedBlockState(ZSTD_v1_5_2_compressedBlockState_t* bs)
{
    int i;
    for (i = 0; i < ZSTD_v1_5_2_REP_NUM; ++i)
        bs->rep[i] = repStartValue[i];
    bs->entropy.huf.repeatMode = HUF_v1_5_2_repeat_none;
    bs->entropy.fse.offcode_repeatMode = FSE_v1_5_2_repeat_none;
    bs->entropy.fse.matchlength_repeatMode = FSE_v1_5_2_repeat_none;
    bs->entropy.fse.litlength_repeatMode = FSE_v1_5_2_repeat_none;
}

/*! ZSTD_v1_5_2_invalidateMatchState()
 *  Invalidate all the matches in the match finder tables.
 *  Requires nextSrc and base to be set (can be NULL).
 */
static void ZSTD_v1_5_2_invalidateMatchState(ZSTD_v1_5_2_matchState_t* ms)
{
    ZSTD_v1_5_2_window_clear(&ms->window);

    ms->nextToUpdate = ms->window.dictLimit;
    ms->loadedDictEnd = 0;
    ms->opt.litLengthSum = 0;  /* force reset of btopt stats */
    ms->dictMatchState = NULL;
}

/**
 * Controls, for this matchState reset, whether the tables need to be cleared /
 * prepared for the coming compression (ZSTDcrp_makeClean), or whether the
 * tables can be left unclean (ZSTDcrp_leaveDirty), because we know that a
 * subsequent operation will overwrite the table space anyways (e.g., copying
 * the matchState contents in from a CDict).
 */
typedef enum {
    ZSTDcrp_makeClean,
    ZSTDcrp_leaveDirty
} ZSTD_v1_5_2_compResetPolicy_e;

/**
 * Controls, for this matchState reset, whether indexing can continue where it
 * left off (ZSTDirp_continue), or whether it needs to be restarted from zero
 * (ZSTDirp_reset).
 */
typedef enum {
    ZSTDirp_continue,
    ZSTDirp_reset
} ZSTD_v1_5_2_indexResetPolicy_e;

typedef enum {
    ZSTD_v1_5_2_resetTarget_CDict,
    ZSTD_v1_5_2_resetTarget_CCtx
} ZSTD_v1_5_2_resetTarget_e;


static size_t
ZSTD_v1_5_2_reset_matchState(ZSTD_v1_5_2_matchState_t* ms,
                      ZSTD_v1_5_2_cwksp* ws,
                const ZSTD_v1_5_2_compressionParameters* cParams,
                const ZSTD_v1_5_2_paramSwitch_e useRowMatchFinder,
                const ZSTD_v1_5_2_compResetPolicy_e crp,
                const ZSTD_v1_5_2_indexResetPolicy_e forceResetIndex,
                const ZSTD_v1_5_2_resetTarget_e forWho)
{
    /* disable chain table allocation for fast or row-based strategies */
    size_t const chainSize = ZSTD_v1_5_2_allocateChainTable(cParams->strategy, useRowMatchFinder,
                                                     ms->dedicatedDictSearch && (forWho == ZSTD_v1_5_2_resetTarget_CDict))
                                ? ((size_t)1 << cParams->chainLog)
                                : 0;
    size_t const hSize = ((size_t)1) << cParams->hashLog;
    U32    const hashLog3 = ((forWho == ZSTD_v1_5_2_resetTarget_CCtx) && cParams->minMatch==3) ? MIN(ZSTD_v1_5_2_HASHLOG3_MAX, cParams->windowLog) : 0;
    size_t const h3Size = hashLog3 ? ((size_t)1) << hashLog3 : 0;

    DEBUGLOG(4, "reset indices : %u", forceResetIndex == ZSTDirp_reset);
    assert(useRowMatchFinder != ZSTD_v1_5_2_ps_auto);
    if (forceResetIndex == ZSTDirp_reset) {
        ZSTD_v1_5_2_window_init(&ms->window);
        ZSTD_v1_5_2_cwksp_mark_tables_dirty(ws);
    }

    ms->hashLog3 = hashLog3;

    ZSTD_v1_5_2_invalidateMatchState(ms);

    assert(!ZSTD_v1_5_2_cwksp_reserve_failed(ws)); /* check that allocation hasn't already failed */

    ZSTD_v1_5_2_cwksp_clear_tables(ws);

    DEBUGLOG(5, "reserving table space");
    /* table Space */
    ms->hashTable = (U32*)ZSTD_v1_5_2_cwksp_reserve_table(ws, hSize * sizeof(U32));
    ms->chainTable = (U32*)ZSTD_v1_5_2_cwksp_reserve_table(ws, chainSize * sizeof(U32));
    ms->hashTable3 = (U32*)ZSTD_v1_5_2_cwksp_reserve_table(ws, h3Size * sizeof(U32));
    RETURN_ERROR_IF(ZSTD_v1_5_2_cwksp_reserve_failed(ws), memory_allocation,
                    "failed a workspace allocation in ZSTD_v1_5_2_reset_matchState");

    DEBUGLOG(4, "reset table : %u", crp!=ZSTDcrp_leaveDirty);
    if (crp!=ZSTDcrp_leaveDirty) {
        /* reset tables only */
        ZSTD_v1_5_2_cwksp_clean_tables(ws);
    }

    /* opt parser space */
    if ((forWho == ZSTD_v1_5_2_resetTarget_CCtx) && (cParams->strategy >= ZSTD_v1_5_2_btopt)) {
        DEBUGLOG(4, "reserving optimal parser space");
        ms->opt.litFreq = (unsigned*)ZSTD_v1_5_2_cwksp_reserve_aligned(ws, (1<<Litbits) * sizeof(unsigned));
        ms->opt.litLengthFreq = (unsigned*)ZSTD_v1_5_2_cwksp_reserve_aligned(ws, (MaxLL+1) * sizeof(unsigned));
        ms->opt.matchLengthFreq = (unsigned*)ZSTD_v1_5_2_cwksp_reserve_aligned(ws, (MaxML+1) * sizeof(unsigned));
        ms->opt.offCodeFreq = (unsigned*)ZSTD_v1_5_2_cwksp_reserve_aligned(ws, (MaxOff+1) * sizeof(unsigned));
        ms->opt.matchTable = (ZSTD_v1_5_2_match_t*)ZSTD_v1_5_2_cwksp_reserve_aligned(ws, (ZSTD_v1_5_2_OPT_NUM+1) * sizeof(ZSTD_v1_5_2_match_t));
        ms->opt.priceTable = (ZSTD_v1_5_2_optimal_t*)ZSTD_v1_5_2_cwksp_reserve_aligned(ws, (ZSTD_v1_5_2_OPT_NUM+1) * sizeof(ZSTD_v1_5_2_optimal_t));
    }

    if (ZSTD_v1_5_2_rowMatchFinderUsed(cParams->strategy, useRowMatchFinder)) {
        {   /* Row match finder needs an additional table of hashes ("tags") */
            size_t const tagTableSize = hSize*sizeof(U16);
            ms->tagTable = (U16*)ZSTD_v1_5_2_cwksp_reserve_aligned(ws, tagTableSize);
            if (ms->tagTable) ZSTD_v1_5_2_memset(ms->tagTable, 0, tagTableSize);
        }
        {   /* Switch to 32-entry rows if searchLog is 5 (or more) */
            U32 const rowLog = BOUNDED(4, cParams->searchLog, 6);
            assert(cParams->hashLog >= rowLog);
            ms->rowHashLog = cParams->hashLog - rowLog;
        }
    }

    ms->cParams = *cParams;

    RETURN_ERROR_IF(ZSTD_v1_5_2_cwksp_reserve_failed(ws), memory_allocation,
                    "failed a workspace allocation in ZSTD_v1_5_2_reset_matchState");
    return 0;
}

/* ZSTD_v1_5_2_indexTooCloseToMax() :
 * minor optimization : prefer memset() rather than reduceIndex()
 * which is measurably slow in some circumstances (reported for Visual Studio).
 * Works when re-using a context for a lot of smallish inputs :
 * if all inputs are smaller than ZSTD_v1_5_2_INDEXOVERFLOW_MARGIN,
 * memset() will be triggered before reduceIndex().
 */
#define ZSTD_v1_5_2_INDEXOVERFLOW_MARGIN (16 MB)
static int ZSTD_v1_5_2_indexTooCloseToMax(ZSTD_v1_5_2_window_t w)
{
    return (size_t)(w.nextSrc - w.base) > (ZSTD_v1_5_2_CURRENT_MAX - ZSTD_v1_5_2_INDEXOVERFLOW_MARGIN);
}

/** ZSTD_v1_5_2_dictTooBig():
 * When dictionaries are larger than ZSTD_v1_5_2_CHUNKSIZE_MAX they can't be loaded in
 * one go generically. So we ensure that in that case we reset the tables to zero,
 * so that we can load as much of the dictionary as possible.
 */
static int ZSTD_v1_5_2_dictTooBig(size_t const loadedDictSize)
{
    return loadedDictSize > ZSTD_v1_5_2_CHUNKSIZE_MAX;
}

/*! ZSTD_v1_5_2_resetCCtx_internal() :
 * @param loadedDictSize The size of the dictionary to be loaded
 * into the context, if any. If no dictionary is used, or the
 * dictionary is being attached / copied, then pass 0.
 * note : `params` are assumed fully validated at this stage.
 */
static size_t ZSTD_v1_5_2_resetCCtx_internal(ZSTD_v1_5_2_CCtx* zc,
                                      ZSTD_v1_5_2_CCtx_params const* params,
                                      U64 const pledgedSrcSize,
                                      size_t const loadedDictSize,
                                      ZSTD_v1_5_2_compResetPolicy_e const crp,
                                      ZSTD_v1_5_2_buffered_policy_e const zbuff)
{
    ZSTD_v1_5_2_cwksp* const ws = &zc->workspace;
    DEBUGLOG(4, "ZSTD_v1_5_2_resetCCtx_internal: pledgedSrcSize=%u, wlog=%u, useRowMatchFinder=%d useBlockSplitter=%d",
                (U32)pledgedSrcSize, params->cParams.windowLog, (int)params->useRowMatchFinder, (int)params->useBlockSplitter);
    assert(!ZSTD_v1_5_2_isError(ZSTD_v1_5_2_checkCParams(params->cParams)));

    zc->isFirstBlock = 1;

    /* Set applied params early so we can modify them for LDM,
     * and point params at the applied params.
     */
    zc->appliedParams = *params;
    params = &zc->appliedParams;

    assert(params->useRowMatchFinder != ZSTD_v1_5_2_ps_auto);
    assert(params->useBlockSplitter != ZSTD_v1_5_2_ps_auto);
    assert(params->ldmParams.enableLdm != ZSTD_v1_5_2_ps_auto);
    if (params->ldmParams.enableLdm == ZSTD_v1_5_2_ps_enable) {
        /* Adjust long distance matching parameters */
        ZSTD_v1_5_2_ldm_adjustParameters(&zc->appliedParams.ldmParams, &params->cParams);
        assert(params->ldmParams.hashLog >= params->ldmParams.bucketSizeLog);
        assert(params->ldmParams.hashRateLog < 32);
    }

    {   size_t const windowSize = MAX(1, (size_t)MIN(((U64)1 << params->cParams.windowLog), pledgedSrcSize));
        size_t const blockSize = MIN(ZSTD_v1_5_2_BLOCKSIZE_MAX, windowSize);
        U32    const divider = (params->cParams.minMatch==3) ? 3 : 4;
        size_t const maxNbSeq = blockSize / divider;
        size_t const buffOutSize = (zbuff == ZSTDb_buffered && params->outBufferMode == ZSTD_v1_5_2_bm_buffered)
                ? ZSTD_v1_5_2_compressBound(blockSize) + 1
                : 0;
        size_t const buffInSize = (zbuff == ZSTDb_buffered && params->inBufferMode == ZSTD_v1_5_2_bm_buffered)
                ? windowSize + blockSize
                : 0;
        size_t const maxNbLdmSeq = ZSTD_v1_5_2_ldm_getMaxNbSeq(params->ldmParams, blockSize);

        int const indexTooClose = ZSTD_v1_5_2_indexTooCloseToMax(zc->blockState.matchState.window);
        int const dictTooBig = ZSTD_v1_5_2_dictTooBig(loadedDictSize);
        ZSTD_v1_5_2_indexResetPolicy_e needsIndexReset =
            (indexTooClose || dictTooBig || !zc->initialized) ? ZSTDirp_reset : ZSTDirp_continue;

        size_t const neededSpace =
            ZSTD_v1_5_2_estimateCCtxSize_usingCCtxParams_internal(
                &params->cParams, &params->ldmParams, zc->staticSize != 0, params->useRowMatchFinder,
                buffInSize, buffOutSize, pledgedSrcSize);
        int resizeWorkspace;

        FORWARD_IF_ERROR(neededSpace, "cctx size estimate failed!");

        if (!zc->staticSize) ZSTD_v1_5_2_cwksp_bump_oversized_duration(ws, 0);

        {   /* Check if workspace is large enough, alloc a new one if needed */
            int const workspaceTooSmall = ZSTD_v1_5_2_cwksp_sizeof(ws) < neededSpace;
            int const workspaceWasteful = ZSTD_v1_5_2_cwksp_check_wasteful(ws, neededSpace);
            resizeWorkspace = workspaceTooSmall || workspaceWasteful;
            DEBUGLOG(4, "Need %zu B workspace", neededSpace);
            DEBUGLOG(4, "windowSize: %zu - blockSize: %zu", windowSize, blockSize);

            if (resizeWorkspace) {
                DEBUGLOG(4, "Resize workspaceSize from %zuKB to %zuKB",
                            ZSTD_v1_5_2_cwksp_sizeof(ws) >> 10,
                            neededSpace >> 10);

                RETURN_ERROR_IF(zc->staticSize, memory_allocation, "static cctx : no resize");

                needsIndexReset = ZSTDirp_reset;

                ZSTD_v1_5_2_cwksp_free(ws, zc->customMem);
                FORWARD_IF_ERROR(ZSTD_v1_5_2_cwksp_create(ws, neededSpace, zc->customMem), "");

                DEBUGLOG(5, "reserving object space");
                /* Statically sized space.
                 * entropyWorkspace never moves,
                 * though prev/next block swap places */
                assert(ZSTD_v1_5_2_cwksp_check_available(ws, 2 * sizeof(ZSTD_v1_5_2_compressedBlockState_t)));
                zc->blockState.prevCBlock = (ZSTD_v1_5_2_compressedBlockState_t*) ZSTD_v1_5_2_cwksp_reserve_object(ws, sizeof(ZSTD_v1_5_2_compressedBlockState_t));
                RETURN_ERROR_IF(zc->blockState.prevCBlock == NULL, memory_allocation, "couldn't allocate prevCBlock");
                zc->blockState.nextCBlock = (ZSTD_v1_5_2_compressedBlockState_t*) ZSTD_v1_5_2_cwksp_reserve_object(ws, sizeof(ZSTD_v1_5_2_compressedBlockState_t));
                RETURN_ERROR_IF(zc->blockState.nextCBlock == NULL, memory_allocation, "couldn't allocate nextCBlock");
                zc->entropyWorkspace = (U32*) ZSTD_v1_5_2_cwksp_reserve_object(ws, ENTROPY_WORKSPACE_SIZE);
                RETURN_ERROR_IF(zc->entropyWorkspace == NULL, memory_allocation, "couldn't allocate entropyWorkspace");
        }   }

        ZSTD_v1_5_2_cwksp_clear(ws);

        /* init params */
        zc->blockState.matchState.cParams = params->cParams;
        zc->pledgedSrcSizePlusOne = pledgedSrcSize+1;
        zc->consumedSrcSize = 0;
        zc->producedCSize = 0;
        if (pledgedSrcSize == ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN)
            zc->appliedParams.fParams.contentSizeFlag = 0;
        DEBUGLOG(4, "pledged content size : %u ; flag : %u",
            (unsigned)pledgedSrcSize, zc->appliedParams.fParams.contentSizeFlag);
        zc->blockSize = blockSize;

        XXH64_reset(&zc->xxhState, 0);
        zc->stage = ZSTDcs_init;
        zc->dictID = 0;
        zc->dictContentSize = 0;

        ZSTD_v1_5_2_reset_compressedBlockState(zc->blockState.prevCBlock);

        /* ZSTD_v1_5_2_wildcopy() is used to copy into the literals buffer,
         * so we have to oversize the buffer by WILDCOPY_OVERLENGTH bytes.
         */
        zc->seqStore.litStart = ZSTD_v1_5_2_cwksp_reserve_buffer(ws, blockSize + WILDCOPY_OVERLENGTH);
        zc->seqStore.maxNbLit = blockSize;

        /* buffers */
        zc->bufferedPolicy = zbuff;
        zc->inBuffSize = buffInSize;
        zc->inBuff = (char*)ZSTD_v1_5_2_cwksp_reserve_buffer(ws, buffInSize);
        zc->outBuffSize = buffOutSize;
        zc->outBuff = (char*)ZSTD_v1_5_2_cwksp_reserve_buffer(ws, buffOutSize);

        /* ldm bucketOffsets table */
        if (params->ldmParams.enableLdm == ZSTD_v1_5_2_ps_enable) {
            /* TODO: avoid memset? */
            size_t const numBuckets =
                  ((size_t)1) << (params->ldmParams.hashLog -
                                  params->ldmParams.bucketSizeLog);
            zc->ldmState.bucketOffsets = ZSTD_v1_5_2_cwksp_reserve_buffer(ws, numBuckets);
            ZSTD_v1_5_2_memset(zc->ldmState.bucketOffsets, 0, numBuckets);
        }

        /* sequences storage */
        ZSTD_v1_5_2_referenceExternalSequences(zc, NULL, 0);
        zc->seqStore.maxNbSeq = maxNbSeq;
        zc->seqStore.llCode = ZSTD_v1_5_2_cwksp_reserve_buffer(ws, maxNbSeq * sizeof(BYTE));
        zc->seqStore.mlCode = ZSTD_v1_5_2_cwksp_reserve_buffer(ws, maxNbSeq * sizeof(BYTE));
        zc->seqStore.ofCode = ZSTD_v1_5_2_cwksp_reserve_buffer(ws, maxNbSeq * sizeof(BYTE));
        zc->seqStore.sequencesStart = (seqDef*)ZSTD_v1_5_2_cwksp_reserve_aligned(ws, maxNbSeq * sizeof(seqDef));

        FORWARD_IF_ERROR(ZSTD_v1_5_2_reset_matchState(
            &zc->blockState.matchState,
            ws,
            &params->cParams,
            params->useRowMatchFinder,
            crp,
            needsIndexReset,
            ZSTD_v1_5_2_resetTarget_CCtx), "");

        /* ldm hash table */
        if (params->ldmParams.enableLdm == ZSTD_v1_5_2_ps_enable) {
            /* TODO: avoid memset? */
            size_t const ldmHSize = ((size_t)1) << params->ldmParams.hashLog;
            zc->ldmState.hashTable = (ldmEntry_t*)ZSTD_v1_5_2_cwksp_reserve_aligned(ws, ldmHSize * sizeof(ldmEntry_t));
            ZSTD_v1_5_2_memset(zc->ldmState.hashTable, 0, ldmHSize * sizeof(ldmEntry_t));
            zc->ldmSequences = (rawSeq*)ZSTD_v1_5_2_cwksp_reserve_aligned(ws, maxNbLdmSeq * sizeof(rawSeq));
            zc->maxNbLdmSequences = maxNbLdmSeq;

            ZSTD_v1_5_2_window_init(&zc->ldmState.window);
            zc->ldmState.loadedDictEnd = 0;
        }

        DEBUGLOG(3, "wksp: finished allocating, %zd bytes remain available", ZSTD_v1_5_2_cwksp_available_space(ws));
        assert(ZSTD_v1_5_2_cwksp_estimated_space_within_bounds(ws, neededSpace, resizeWorkspace));

        zc->initialized = 1;

        return 0;
    }
}

/* ZSTD_v1_5_2_invalidateRepCodes() :
 * ensures next compression will not use repcodes from previous block.
 * Note : only works with regular variant;
 *        do not use with extDict variant ! */
void ZSTD_v1_5_2_invalidateRepCodes(ZSTD_v1_5_2_CCtx* cctx) {
    int i;
    for (i=0; i<ZSTD_v1_5_2_REP_NUM; i++) cctx->blockState.prevCBlock->rep[i] = 0;
    assert(!ZSTD_v1_5_2_window_hasExtDict(cctx->blockState.matchState.window));
}

/* These are the approximate sizes for each strategy past which copying the
 * dictionary tables into the working context is faster than using them
 * in-place.
 */
static const size_t attachDictSizeCutoffs[ZSTD_v1_5_2_STRATEGY_MAX+1] = {
    8 KB,  /* unused */
    8 KB,  /* ZSTD_v1_5_2_fast */
    16 KB, /* ZSTD_v1_5_2_dfast */
    32 KB, /* ZSTD_v1_5_2_greedy */
    32 KB, /* ZSTD_v1_5_2_lazy */
    32 KB, /* ZSTD_v1_5_2_lazy2 */
    32 KB, /* ZSTD_v1_5_2_btlazy2 */
    32 KB, /* ZSTD_v1_5_2_btopt */
    8 KB,  /* ZSTD_v1_5_2_btultra */
    8 KB   /* ZSTD_v1_5_2_btultra2 */
};

static int ZSTD_v1_5_2_shouldAttachDict(const ZSTD_v1_5_2_CDict* cdict,
                                 const ZSTD_v1_5_2_CCtx_params* params,
                                 U64 pledgedSrcSize)
{
    size_t cutoff = attachDictSizeCutoffs[cdict->matchState.cParams.strategy];
    int const dedicatedDictSearch = cdict->matchState.dedicatedDictSearch;
    return dedicatedDictSearch
        || ( ( pledgedSrcSize <= cutoff
            || pledgedSrcSize == ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN
            || params->attachDictPref == ZSTD_v1_5_2_dictForceAttach )
          && params->attachDictPref != ZSTD_v1_5_2_dictForceCopy
          && !params->forceWindow ); /* dictMatchState isn't correctly
                                      * handled in _enforceMaxDist */
}

static size_t
ZSTD_v1_5_2_resetCCtx_byAttachingCDict(ZSTD_v1_5_2_CCtx* cctx,
                        const ZSTD_v1_5_2_CDict* cdict,
                        ZSTD_v1_5_2_CCtx_params params,
                        U64 pledgedSrcSize,
                        ZSTD_v1_5_2_buffered_policy_e zbuff)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_resetCCtx_byAttachingCDict() pledgedSrcSize=%llu",
                (unsigned long long)pledgedSrcSize);
    {
        ZSTD_v1_5_2_compressionParameters adjusted_cdict_cParams = cdict->matchState.cParams;
        unsigned const windowLog = params.cParams.windowLog;
        assert(windowLog != 0);
        /* Resize working context table params for input only, since the dict
         * has its own tables. */
        /* pledgedSrcSize == 0 means 0! */

        if (cdict->matchState.dedicatedDictSearch) {
            ZSTD_v1_5_2_dedicatedDictSearch_revertCParams(&adjusted_cdict_cParams);
        }

        params.cParams = ZSTD_v1_5_2_adjustCParams_internal(adjusted_cdict_cParams, pledgedSrcSize,
                                                     cdict->dictContentSize, ZSTD_v1_5_2_cpm_attachDict);
        params.cParams.windowLog = windowLog;
        params.useRowMatchFinder = cdict->useRowMatchFinder;    /* cdict overrides */
        FORWARD_IF_ERROR(ZSTD_v1_5_2_resetCCtx_internal(cctx, &params, pledgedSrcSize,
                                                 /* loadedDictSize */ 0,
                                                 ZSTDcrp_makeClean, zbuff), "");
        assert(cctx->appliedParams.cParams.strategy == adjusted_cdict_cParams.strategy);
    }

    {   const U32 cdictEnd = (U32)( cdict->matchState.window.nextSrc
                                  - cdict->matchState.window.base);
        const U32 cdictLen = cdictEnd - cdict->matchState.window.dictLimit;
        if (cdictLen == 0) {
            /* don't even attach dictionaries with no contents */
            DEBUGLOG(4, "skipping attaching empty dictionary");
        } else {
            DEBUGLOG(4, "attaching dictionary into context");
            cctx->blockState.matchState.dictMatchState = &cdict->matchState;

            /* prep working match state so dict matches never have negative indices
             * when they are translated to the working context's index space. */
            if (cctx->blockState.matchState.window.dictLimit < cdictEnd) {
                cctx->blockState.matchState.window.nextSrc =
                    cctx->blockState.matchState.window.base + cdictEnd;
                ZSTD_v1_5_2_window_clear(&cctx->blockState.matchState.window);
            }
            /* loadedDictEnd is expressed within the referential of the active context */
            cctx->blockState.matchState.loadedDictEnd = cctx->blockState.matchState.window.dictLimit;
    }   }

    cctx->dictID = cdict->dictID;
    cctx->dictContentSize = cdict->dictContentSize;

    /* copy block state */
    ZSTD_v1_5_2_memcpy(cctx->blockState.prevCBlock, &cdict->cBlockState, sizeof(cdict->cBlockState));

    return 0;
}

static size_t ZSTD_v1_5_2_resetCCtx_byCopyingCDict(ZSTD_v1_5_2_CCtx* cctx,
                            const ZSTD_v1_5_2_CDict* cdict,
                            ZSTD_v1_5_2_CCtx_params params,
                            U64 pledgedSrcSize,
                            ZSTD_v1_5_2_buffered_policy_e zbuff)
{
    const ZSTD_v1_5_2_compressionParameters *cdict_cParams = &cdict->matchState.cParams;

    assert(!cdict->matchState.dedicatedDictSearch);
    DEBUGLOG(4, "ZSTD_v1_5_2_resetCCtx_byCopyingCDict() pledgedSrcSize=%llu",
                (unsigned long long)pledgedSrcSize);

    {   unsigned const windowLog = params.cParams.windowLog;
        assert(windowLog != 0);
        /* Copy only compression parameters related to tables. */
        params.cParams = *cdict_cParams;
        params.cParams.windowLog = windowLog;
        params.useRowMatchFinder = cdict->useRowMatchFinder;
        FORWARD_IF_ERROR(ZSTD_v1_5_2_resetCCtx_internal(cctx, &params, pledgedSrcSize,
                                                 /* loadedDictSize */ 0,
                                                 ZSTDcrp_leaveDirty, zbuff), "");
        assert(cctx->appliedParams.cParams.strategy == cdict_cParams->strategy);
        assert(cctx->appliedParams.cParams.hashLog == cdict_cParams->hashLog);
        assert(cctx->appliedParams.cParams.chainLog == cdict_cParams->chainLog);
    }

    ZSTD_v1_5_2_cwksp_mark_tables_dirty(&cctx->workspace);
    assert(params.useRowMatchFinder != ZSTD_v1_5_2_ps_auto);

    /* copy tables */
    {   size_t const chainSize = ZSTD_v1_5_2_allocateChainTable(cdict_cParams->strategy, cdict->useRowMatchFinder, 0 /* DDS guaranteed disabled */)
                                                            ? ((size_t)1 << cdict_cParams->chainLog)
                                                            : 0;
        size_t const hSize =  (size_t)1 << cdict_cParams->hashLog;

        ZSTD_v1_5_2_memcpy(cctx->blockState.matchState.hashTable,
               cdict->matchState.hashTable,
               hSize * sizeof(U32));
        /* Do not copy cdict's chainTable if cctx has parameters such that it would not use chainTable */
        if (ZSTD_v1_5_2_allocateChainTable(cctx->appliedParams.cParams.strategy, cctx->appliedParams.useRowMatchFinder, 0 /* forDDSDict */)) {
            ZSTD_v1_5_2_memcpy(cctx->blockState.matchState.chainTable,
               cdict->matchState.chainTable,
               chainSize * sizeof(U32));
        }
        /* copy tag table */
        if (ZSTD_v1_5_2_rowMatchFinderUsed(cdict_cParams->strategy, cdict->useRowMatchFinder)) {
            size_t const tagTableSize = hSize*sizeof(U16);
            ZSTD_v1_5_2_memcpy(cctx->blockState.matchState.tagTable,
                cdict->matchState.tagTable,
                tagTableSize);
        }
    }

    /* Zero the hashTable3, since the cdict never fills it */
    {   int const h3log = cctx->blockState.matchState.hashLog3;
        size_t const h3Size = h3log ? ((size_t)1 << h3log) : 0;
        assert(cdict->matchState.hashLog3 == 0);
        ZSTD_v1_5_2_memset(cctx->blockState.matchState.hashTable3, 0, h3Size * sizeof(U32));
    }

    ZSTD_v1_5_2_cwksp_mark_tables_clean(&cctx->workspace);

    /* copy dictionary offsets */
    {   ZSTD_v1_5_2_matchState_t const* srcMatchState = &cdict->matchState;
        ZSTD_v1_5_2_matchState_t* dstMatchState = &cctx->blockState.matchState;
        dstMatchState->window       = srcMatchState->window;
        dstMatchState->nextToUpdate = srcMatchState->nextToUpdate;
        dstMatchState->loadedDictEnd= srcMatchState->loadedDictEnd;
    }

    cctx->dictID = cdict->dictID;
    cctx->dictContentSize = cdict->dictContentSize;

    /* copy block state */
    ZSTD_v1_5_2_memcpy(cctx->blockState.prevCBlock, &cdict->cBlockState, sizeof(cdict->cBlockState));

    return 0;
}

/* We have a choice between copying the dictionary context into the working
 * context, or referencing the dictionary context from the working context
 * in-place. We decide here which strategy to use. */
static size_t ZSTD_v1_5_2_resetCCtx_usingCDict(ZSTD_v1_5_2_CCtx* cctx,
                            const ZSTD_v1_5_2_CDict* cdict,
                            const ZSTD_v1_5_2_CCtx_params* params,
                            U64 pledgedSrcSize,
                            ZSTD_v1_5_2_buffered_policy_e zbuff)
{

    DEBUGLOG(4, "ZSTD_v1_5_2_resetCCtx_usingCDict (pledgedSrcSize=%u)",
                (unsigned)pledgedSrcSize);

    if (ZSTD_v1_5_2_shouldAttachDict(cdict, params, pledgedSrcSize)) {
        return ZSTD_v1_5_2_resetCCtx_byAttachingCDict(
            cctx, cdict, *params, pledgedSrcSize, zbuff);
    } else {
        return ZSTD_v1_5_2_resetCCtx_byCopyingCDict(
            cctx, cdict, *params, pledgedSrcSize, zbuff);
    }
}

/*! ZSTD_v1_5_2_copyCCtx_internal() :
 *  Duplicate an existing context `srcCCtx` into another one `dstCCtx`.
 *  Only works during stage ZSTDcs_init (i.e. after creation, but before first call to ZSTD_v1_5_2_compressContinue()).
 *  The "context", in this case, refers to the hash and chain tables,
 *  entropy tables, and dictionary references.
 * `windowLog` value is enforced if != 0, otherwise value is copied from srcCCtx.
 * @return : 0, or an error code */
static size_t ZSTD_v1_5_2_copyCCtx_internal(ZSTD_v1_5_2_CCtx* dstCCtx,
                            const ZSTD_v1_5_2_CCtx* srcCCtx,
                            ZSTD_v1_5_2_frameParameters fParams,
                            U64 pledgedSrcSize,
                            ZSTD_v1_5_2_buffered_policy_e zbuff)
{
    RETURN_ERROR_IF(srcCCtx->stage!=ZSTDcs_init, stage_wrong,
                    "Can't copy a ctx that's not in init stage.");
    DEBUGLOG(5, "ZSTD_v1_5_2_copyCCtx_internal");
    ZSTD_v1_5_2_memcpy(&dstCCtx->customMem, &srcCCtx->customMem, sizeof(ZSTD_v1_5_2_customMem));
    {   ZSTD_v1_5_2_CCtx_params params = dstCCtx->requestedParams;
        /* Copy only compression parameters related to tables. */
        params.cParams = srcCCtx->appliedParams.cParams;
        assert(srcCCtx->appliedParams.useRowMatchFinder != ZSTD_v1_5_2_ps_auto);
        assert(srcCCtx->appliedParams.useBlockSplitter != ZSTD_v1_5_2_ps_auto);
        assert(srcCCtx->appliedParams.ldmParams.enableLdm != ZSTD_v1_5_2_ps_auto);
        params.useRowMatchFinder = srcCCtx->appliedParams.useRowMatchFinder;
        params.useBlockSplitter = srcCCtx->appliedParams.useBlockSplitter;
        params.ldmParams = srcCCtx->appliedParams.ldmParams;
        params.fParams = fParams;
        ZSTD_v1_5_2_resetCCtx_internal(dstCCtx, &params, pledgedSrcSize,
                                /* loadedDictSize */ 0,
                                ZSTDcrp_leaveDirty, zbuff);
        assert(dstCCtx->appliedParams.cParams.windowLog == srcCCtx->appliedParams.cParams.windowLog);
        assert(dstCCtx->appliedParams.cParams.strategy == srcCCtx->appliedParams.cParams.strategy);
        assert(dstCCtx->appliedParams.cParams.hashLog == srcCCtx->appliedParams.cParams.hashLog);
        assert(dstCCtx->appliedParams.cParams.chainLog == srcCCtx->appliedParams.cParams.chainLog);
        assert(dstCCtx->blockState.matchState.hashLog3 == srcCCtx->blockState.matchState.hashLog3);
    }

    ZSTD_v1_5_2_cwksp_mark_tables_dirty(&dstCCtx->workspace);

    /* copy tables */
    {   size_t const chainSize = ZSTD_v1_5_2_allocateChainTable(srcCCtx->appliedParams.cParams.strategy,
                                                         srcCCtx->appliedParams.useRowMatchFinder,
                                                         0 /* forDDSDict */)
                                    ? ((size_t)1 << srcCCtx->appliedParams.cParams.chainLog)
                                    : 0;
        size_t const hSize =  (size_t)1 << srcCCtx->appliedParams.cParams.hashLog;
        int const h3log = srcCCtx->blockState.matchState.hashLog3;
        size_t const h3Size = h3log ? ((size_t)1 << h3log) : 0;

        ZSTD_v1_5_2_memcpy(dstCCtx->blockState.matchState.hashTable,
               srcCCtx->blockState.matchState.hashTable,
               hSize * sizeof(U32));
        ZSTD_v1_5_2_memcpy(dstCCtx->blockState.matchState.chainTable,
               srcCCtx->blockState.matchState.chainTable,
               chainSize * sizeof(U32));
        ZSTD_v1_5_2_memcpy(dstCCtx->blockState.matchState.hashTable3,
               srcCCtx->blockState.matchState.hashTable3,
               h3Size * sizeof(U32));
    }

    ZSTD_v1_5_2_cwksp_mark_tables_clean(&dstCCtx->workspace);

    /* copy dictionary offsets */
    {
        const ZSTD_v1_5_2_matchState_t* srcMatchState = &srcCCtx->blockState.matchState;
        ZSTD_v1_5_2_matchState_t* dstMatchState = &dstCCtx->blockState.matchState;
        dstMatchState->window       = srcMatchState->window;
        dstMatchState->nextToUpdate = srcMatchState->nextToUpdate;
        dstMatchState->loadedDictEnd= srcMatchState->loadedDictEnd;
    }
    dstCCtx->dictID = srcCCtx->dictID;
    dstCCtx->dictContentSize = srcCCtx->dictContentSize;

    /* copy block state */
    ZSTD_v1_5_2_memcpy(dstCCtx->blockState.prevCBlock, srcCCtx->blockState.prevCBlock, sizeof(*srcCCtx->blockState.prevCBlock));

    return 0;
}

/*! ZSTD_v1_5_2_copyCCtx() :
 *  Duplicate an existing context `srcCCtx` into another one `dstCCtx`.
 *  Only works during stage ZSTDcs_init (i.e. after creation, but before first call to ZSTD_v1_5_2_compressContinue()).
 *  pledgedSrcSize==0 means "unknown".
*   @return : 0, or an error code */
size_t ZSTD_v1_5_2_copyCCtx(ZSTD_v1_5_2_CCtx* dstCCtx, const ZSTD_v1_5_2_CCtx* srcCCtx, unsigned long long pledgedSrcSize)
{
    ZSTD_v1_5_2_frameParameters fParams = { 1 /*content*/, 0 /*checksum*/, 0 /*noDictID*/ };
    ZSTD_v1_5_2_buffered_policy_e const zbuff = srcCCtx->bufferedPolicy;
    ZSTD_v1_5_2_STATIC_ASSERT((U32)ZSTDb_buffered==1);
    if (pledgedSrcSize==0) pledgedSrcSize = ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN;
    fParams.contentSizeFlag = (pledgedSrcSize != ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN);

    return ZSTD_v1_5_2_copyCCtx_internal(dstCCtx, srcCCtx,
                                fParams, pledgedSrcSize,
                                zbuff);
}


#define ZSTD_v1_5_2_ROWSIZE 16
/*! ZSTD_v1_5_2_reduceTable() :
 *  reduce table indexes by `reducerValue`, or squash to zero.
 *  PreserveMark preserves "unsorted mark" for btlazy2 strategy.
 *  It must be set to a clear 0/1 value, to remove branch during inlining.
 *  Presume table size is a multiple of ZSTD_v1_5_2_ROWSIZE
 *  to help auto-vectorization */
FORCE_INLINE_TEMPLATE void
ZSTD_v1_5_2_reduceTable_internal (U32* const table, U32 const size, U32 const reducerValue, int const preserveMark)
{
    int const nbRows = (int)size / ZSTD_v1_5_2_ROWSIZE;
    int cellNb = 0;
    int rowNb;
    /* Protect special index values < ZSTD_v1_5_2_WINDOW_START_INDEX. */
    U32 const reducerThreshold = reducerValue + ZSTD_v1_5_2_WINDOW_START_INDEX;
    assert((size & (ZSTD_v1_5_2_ROWSIZE-1)) == 0);  /* multiple of ZSTD_v1_5_2_ROWSIZE */
    assert(size < (1U<<31));   /* can be casted to int */

#if ZSTD_v1_5_2_MEMORY_SANITIZER && !defined (ZSTD_v1_5_2_MSAN_DONT_POISON_WORKSPACE)
    /* To validate that the table re-use logic is sound, and that we don't
     * access table space that we haven't cleaned, we re-"poison" the table
     * space every time we mark it dirty.
     *
     * This function however is intended to operate on those dirty tables and
     * re-clean them. So when this function is used correctly, we can unpoison
     * the memory it operated on. This introduces a blind spot though, since
     * if we now try to operate on __actually__ poisoned memory, we will not
     * detect that. */
    __msan_unpoison(table, size * sizeof(U32));
#endif

    for (rowNb=0 ; rowNb < nbRows ; rowNb++) {
        int column;
        for (column=0; column<ZSTD_v1_5_2_ROWSIZE; column++) {
            U32 newVal;
            if (preserveMark && table[cellNb] == ZSTD_v1_5_2_DUBT_UNSORTED_MARK) {
                /* This write is pointless, but is required(?) for the compiler
                 * to auto-vectorize the loop. */
                newVal = ZSTD_v1_5_2_DUBT_UNSORTED_MARK;
            } else if (table[cellNb] < reducerThreshold) {
                newVal = 0;
            } else {
                newVal = table[cellNb] - reducerValue;
            }
            table[cellNb] = newVal;
            cellNb++;
    }   }
}

static void ZSTD_v1_5_2_reduceTable(U32* const table, U32 const size, U32 const reducerValue)
{
    ZSTD_v1_5_2_reduceTable_internal(table, size, reducerValue, 0);
}

static void ZSTD_v1_5_2_reduceTable_btlazy2(U32* const table, U32 const size, U32 const reducerValue)
{
    ZSTD_v1_5_2_reduceTable_internal(table, size, reducerValue, 1);
}

/*! ZSTD_v1_5_2_reduceIndex() :
*   rescale all indexes to avoid future overflow (indexes are U32) */
static void ZSTD_v1_5_2_reduceIndex (ZSTD_v1_5_2_matchState_t* ms, ZSTD_v1_5_2_CCtx_params const* params, const U32 reducerValue)
{
    {   U32 const hSize = (U32)1 << params->cParams.hashLog;
        ZSTD_v1_5_2_reduceTable(ms->hashTable, hSize, reducerValue);
    }

    if (ZSTD_v1_5_2_allocateChainTable(params->cParams.strategy, params->useRowMatchFinder, (U32)ms->dedicatedDictSearch)) {
        U32 const chainSize = (U32)1 << params->cParams.chainLog;
        if (params->cParams.strategy == ZSTD_v1_5_2_btlazy2)
            ZSTD_v1_5_2_reduceTable_btlazy2(ms->chainTable, chainSize, reducerValue);
        else
            ZSTD_v1_5_2_reduceTable(ms->chainTable, chainSize, reducerValue);
    }

    if (ms->hashLog3) {
        U32 const h3Size = (U32)1 << ms->hashLog3;
        ZSTD_v1_5_2_reduceTable(ms->hashTable3, h3Size, reducerValue);
    }
}


/*-*******************************************************
*  Block entropic compression
*********************************************************/

/* See doc/ZSTD_compression_format.md for detailed format description */

void ZSTD_v1_5_2_seqToCodes(const seqStore_t* seqStorePtr)
{
    const seqDef* const sequences = seqStorePtr->sequencesStart;
    BYTE* const llCodeTable = seqStorePtr->llCode;
    BYTE* const ofCodeTable = seqStorePtr->ofCode;
    BYTE* const mlCodeTable = seqStorePtr->mlCode;
    U32 const nbSeq = (U32)(seqStorePtr->sequences - seqStorePtr->sequencesStart);
    U32 u;
    assert(nbSeq <= seqStorePtr->maxNbSeq);
    for (u=0; u<nbSeq; u++) {
        U32 const llv = sequences[u].litLength;
        U32 const mlv = sequences[u].mlBase;
        llCodeTable[u] = (BYTE)ZSTD_v1_5_2_LLcode(llv);
        ofCodeTable[u] = (BYTE)ZSTD_v1_5_2_highbit32(sequences[u].offBase);
        mlCodeTable[u] = (BYTE)ZSTD_v1_5_2_MLcode(mlv);
    }
    if (seqStorePtr->longLengthType==ZSTD_v1_5_2_llt_literalLength)
        llCodeTable[seqStorePtr->longLengthPos] = MaxLL;
    if (seqStorePtr->longLengthType==ZSTD_v1_5_2_llt_matchLength)
        mlCodeTable[seqStorePtr->longLengthPos] = MaxML;
}

/* ZSTD_v1_5_2_useTargetCBlockSize():
 * Returns if target compressed block size param is being used.
 * If used, compression will do best effort to make a compressed block size to be around targetCBlockSize.
 * Returns 1 if true, 0 otherwise. */
static int ZSTD_v1_5_2_useTargetCBlockSize(const ZSTD_v1_5_2_CCtx_params* cctxParams)
{
    DEBUGLOG(5, "ZSTD_v1_5_2_useTargetCBlockSize (targetCBlockSize=%zu)", cctxParams->targetCBlockSize);
    return (cctxParams->targetCBlockSize != 0);
}

/* ZSTD_v1_5_2_blockSplitterEnabled():
 * Returns if block splitting param is being used
 * If used, compression will do best effort to split a block in order to improve compression ratio.
 * At the time this function is called, the parameter must be finalized.
 * Returns 1 if true, 0 otherwise. */
static int ZSTD_v1_5_2_blockSplitterEnabled(ZSTD_v1_5_2_CCtx_params* cctxParams)
{
    DEBUGLOG(5, "ZSTD_v1_5_2_blockSplitterEnabled (useBlockSplitter=%d)", cctxParams->useBlockSplitter);
    assert(cctxParams->useBlockSplitter != ZSTD_v1_5_2_ps_auto);
    return (cctxParams->useBlockSplitter == ZSTD_v1_5_2_ps_enable);
}

/* Type returned by ZSTD_v1_5_2_buildSequencesStatistics containing finalized symbol encoding types
 * and size of the sequences statistics
 */
typedef struct {
    U32 LLtype;
    U32 Offtype;
    U32 MLtype;
    size_t size;
    size_t lastCountSize; /* Accounts for bug in 1.3.4. More detail in ZSTD_v1_5_2_entropyCompressSeqStore_internal() */
} ZSTD_v1_5_2_symbolEncodingTypeStats_t;

/* ZSTD_v1_5_2_buildSequencesStatistics():
 * Returns a ZSTD_v1_5_2_symbolEncodingTypeStats_t, or a zstd error code in the `size` field.
 * Modifies `nextEntropy` to have the appropriate values as a side effect.
 * nbSeq must be greater than 0.
 *
 * entropyWkspSize must be of size at least ENTROPY_WORKSPACE_SIZE - (MaxSeq + 1)*sizeof(U32)
 */
static ZSTD_v1_5_2_symbolEncodingTypeStats_t
ZSTD_v1_5_2_buildSequencesStatistics(seqStore_t* seqStorePtr, size_t nbSeq,
                        const ZSTD_v1_5_2_fseCTables_t* prevEntropy, ZSTD_v1_5_2_fseCTables_t* nextEntropy,
                              BYTE* dst, const BYTE* const dstEnd,
                              ZSTD_v1_5_2_strategy strategy, unsigned* countWorkspace,
                              void* entropyWorkspace, size_t entropyWkspSize) {
    BYTE* const ostart = dst;
    const BYTE* const oend = dstEnd;
    BYTE* op = ostart;
    FSE_v1_5_2_CTable* CTable_LitLength = nextEntropy->litlengthCTable;
    FSE_v1_5_2_CTable* CTable_OffsetBits = nextEntropy->offcodeCTable;
    FSE_v1_5_2_CTable* CTable_MatchLength = nextEntropy->matchlengthCTable;
    const BYTE* const ofCodeTable = seqStorePtr->ofCode;
    const BYTE* const llCodeTable = seqStorePtr->llCode;
    const BYTE* const mlCodeTable = seqStorePtr->mlCode;
    ZSTD_v1_5_2_symbolEncodingTypeStats_t stats;

    stats.lastCountSize = 0;
    /* convert length/distances into codes */
    ZSTD_v1_5_2_seqToCodes(seqStorePtr);
    assert(op <= oend);
    assert(nbSeq != 0); /* ZSTD_v1_5_2_selectEncodingType() divides by nbSeq */
    /* build CTable for Literal Lengths */
    {   unsigned max = MaxLL;
        size_t const mostFrequent = HIST_v1_5_2_countFast_wksp(countWorkspace, &max, llCodeTable, nbSeq, entropyWorkspace, entropyWkspSize);   /* can't fail */
        DEBUGLOG(5, "Building LL table");
        nextEntropy->litlength_repeatMode = prevEntropy->litlength_repeatMode;
        stats.LLtype = ZSTD_v1_5_2_selectEncodingType(&nextEntropy->litlength_repeatMode,
                                        countWorkspace, max, mostFrequent, nbSeq,
                                        LLFSELog, prevEntropy->litlengthCTable,
                                        LL_defaultNorm, LL_defaultNormLog,
                                        ZSTD_v1_5_2_defaultAllowed, strategy);
        assert(set_basic < set_compressed && set_rle < set_compressed);
        assert(!(stats.LLtype < set_compressed && nextEntropy->litlength_repeatMode != FSE_v1_5_2_repeat_none)); /* We don't copy tables */
        {   size_t const countSize = ZSTD_v1_5_2_buildCTable(
                op, (size_t)(oend - op),
                CTable_LitLength, LLFSELog, (symbolEncodingType_e)stats.LLtype,
                countWorkspace, max, llCodeTable, nbSeq,
                LL_defaultNorm, LL_defaultNormLog, MaxLL,
                prevEntropy->litlengthCTable,
                sizeof(prevEntropy->litlengthCTable),
                entropyWorkspace, entropyWkspSize);
            if (ZSTD_v1_5_2_isError(countSize)) {
                DEBUGLOG(3, "ZSTD_v1_5_2_buildCTable for LitLens failed");
                stats.size = countSize;
                return stats;
            }
            if (stats.LLtype == set_compressed)
                stats.lastCountSize = countSize;
            op += countSize;
            assert(op <= oend);
    }   }
    /* build CTable for Offsets */
    {   unsigned max = MaxOff;
        size_t const mostFrequent = HIST_v1_5_2_countFast_wksp(
            countWorkspace, &max, ofCodeTable, nbSeq, entropyWorkspace, entropyWkspSize);  /* can't fail */
        /* We can only use the basic table if max <= DefaultMaxOff, otherwise the offsets are too large */
        ZSTD_v1_5_2_defaultPolicy_e const defaultPolicy = (max <= DefaultMaxOff) ? ZSTD_v1_5_2_defaultAllowed : ZSTD_v1_5_2_defaultDisallowed;
        DEBUGLOG(5, "Building OF table");
        nextEntropy->offcode_repeatMode = prevEntropy->offcode_repeatMode;
        stats.Offtype = ZSTD_v1_5_2_selectEncodingType(&nextEntropy->offcode_repeatMode,
                                        countWorkspace, max, mostFrequent, nbSeq,
                                        OffFSELog, prevEntropy->offcodeCTable,
                                        OF_defaultNorm, OF_defaultNormLog,
                                        defaultPolicy, strategy);
        assert(!(stats.Offtype < set_compressed && nextEntropy->offcode_repeatMode != FSE_v1_5_2_repeat_none)); /* We don't copy tables */
        {   size_t const countSize = ZSTD_v1_5_2_buildCTable(
                op, (size_t)(oend - op),
                CTable_OffsetBits, OffFSELog, (symbolEncodingType_e)stats.Offtype,
                countWorkspace, max, ofCodeTable, nbSeq,
                OF_defaultNorm, OF_defaultNormLog, DefaultMaxOff,
                prevEntropy->offcodeCTable,
                sizeof(prevEntropy->offcodeCTable),
                entropyWorkspace, entropyWkspSize);
            if (ZSTD_v1_5_2_isError(countSize)) {
                DEBUGLOG(3, "ZSTD_v1_5_2_buildCTable for Offsets failed");
                stats.size = countSize;
                return stats;
            }
            if (stats.Offtype == set_compressed)
                stats.lastCountSize = countSize;
            op += countSize;
            assert(op <= oend);
    }   }
    /* build CTable for MatchLengths */
    {   unsigned max = MaxML;
        size_t const mostFrequent = HIST_v1_5_2_countFast_wksp(
            countWorkspace, &max, mlCodeTable, nbSeq, entropyWorkspace, entropyWkspSize);   /* can't fail */
        DEBUGLOG(5, "Building ML table (remaining space : %i)", (int)(oend-op));
        nextEntropy->matchlength_repeatMode = prevEntropy->matchlength_repeatMode;
        stats.MLtype = ZSTD_v1_5_2_selectEncodingType(&nextEntropy->matchlength_repeatMode,
                                        countWorkspace, max, mostFrequent, nbSeq,
                                        MLFSELog, prevEntropy->matchlengthCTable,
                                        ML_defaultNorm, ML_defaultNormLog,
                                        ZSTD_v1_5_2_defaultAllowed, strategy);
        assert(!(stats.MLtype < set_compressed && nextEntropy->matchlength_repeatMode != FSE_v1_5_2_repeat_none)); /* We don't copy tables */
        {   size_t const countSize = ZSTD_v1_5_2_buildCTable(
                op, (size_t)(oend - op),
                CTable_MatchLength, MLFSELog, (symbolEncodingType_e)stats.MLtype,
                countWorkspace, max, mlCodeTable, nbSeq,
                ML_defaultNorm, ML_defaultNormLog, MaxML,
                prevEntropy->matchlengthCTable,
                sizeof(prevEntropy->matchlengthCTable),
                entropyWorkspace, entropyWkspSize);
            if (ZSTD_v1_5_2_isError(countSize)) {
                DEBUGLOG(3, "ZSTD_v1_5_2_buildCTable for MatchLengths failed");
                stats.size = countSize;
                return stats;
            }
            if (stats.MLtype == set_compressed)
                stats.lastCountSize = countSize;
            op += countSize;
            assert(op <= oend);
    }   }
    stats.size = (size_t)(op-ostart);
    return stats;
}

/* ZSTD_v1_5_2_entropyCompressSeqStore_internal():
 * compresses both literals and sequences
 * Returns compressed size of block, or a zstd error.
 */
#define SUSPECT_UNCOMPRESSIBLE_LITERAL_RATIO 20
MEM_STATIC size_t
ZSTD_v1_5_2_entropyCompressSeqStore_internal(seqStore_t* seqStorePtr,
                          const ZSTD_v1_5_2_entropyCTables_t* prevEntropy,
                                ZSTD_v1_5_2_entropyCTables_t* nextEntropy,
                          const ZSTD_v1_5_2_CCtx_params* cctxParams,
                                void* dst, size_t dstCapacity,
                                void* entropyWorkspace, size_t entropyWkspSize,
                          const int bmi2)
{
    const int longOffsets = cctxParams->cParams.windowLog > STREAM_ACCUMULATOR_MIN;
    ZSTD_v1_5_2_strategy const strategy = cctxParams->cParams.strategy;
    unsigned* count = (unsigned*)entropyWorkspace;
    FSE_v1_5_2_CTable* CTable_LitLength = nextEntropy->fse.litlengthCTable;
    FSE_v1_5_2_CTable* CTable_OffsetBits = nextEntropy->fse.offcodeCTable;
    FSE_v1_5_2_CTable* CTable_MatchLength = nextEntropy->fse.matchlengthCTable;
    const seqDef* const sequences = seqStorePtr->sequencesStart;
    const size_t nbSeq = seqStorePtr->sequences - seqStorePtr->sequencesStart;
    const BYTE* const ofCodeTable = seqStorePtr->ofCode;
    const BYTE* const llCodeTable = seqStorePtr->llCode;
    const BYTE* const mlCodeTable = seqStorePtr->mlCode;
    BYTE* const ostart = (BYTE*)dst;
    BYTE* const oend = ostart + dstCapacity;
    BYTE* op = ostart;
    size_t lastCountSize;

    entropyWorkspace = count + (MaxSeq + 1);
    entropyWkspSize -= (MaxSeq + 1) * sizeof(*count);

    DEBUGLOG(4, "ZSTD_v1_5_2_entropyCompressSeqStore_internal (nbSeq=%zu)", nbSeq);
    ZSTD_v1_5_2_STATIC_ASSERT(HUF_v1_5_2_WORKSPACE_SIZE >= (1<<MAX(MLFSELog,LLFSELog)));
    assert(entropyWkspSize >= HUF_v1_5_2_WORKSPACE_SIZE);

    /* Compress literals */
    {   const BYTE* const literals = seqStorePtr->litStart;
        size_t const numSequences = seqStorePtr->sequences - seqStorePtr->sequencesStart;
        size_t const numLiterals = seqStorePtr->lit - seqStorePtr->litStart;
        /* Base suspicion of uncompressibility on ratio of literals to sequences */
        unsigned const suspectUncompressible = (numSequences == 0) || (numLiterals / numSequences >= SUSPECT_UNCOMPRESSIBLE_LITERAL_RATIO);
        size_t const litSize = (size_t)(seqStorePtr->lit - literals);
        size_t const cSize = ZSTD_v1_5_2_compressLiterals(
                                    &prevEntropy->huf, &nextEntropy->huf,
                                    cctxParams->cParams.strategy,
                                    ZSTD_v1_5_2_literalsCompressionIsDisabled(cctxParams),
                                    op, dstCapacity,
                                    literals, litSize,
                                    entropyWorkspace, entropyWkspSize,
                                    bmi2, suspectUncompressible);
        FORWARD_IF_ERROR(cSize, "ZSTD_v1_5_2_compressLiterals failed");
        assert(cSize <= dstCapacity);
        op += cSize;
    }

    /* Sequences Header */
    RETURN_ERROR_IF((oend-op) < 3 /*max nbSeq Size*/ + 1 /*seqHead*/,
                    dstSize_tooSmall, "Can't fit seq hdr in output buf!");
    if (nbSeq < 128) {
        *op++ = (BYTE)nbSeq;
    } else if (nbSeq < LONGNBSEQ) {
        op[0] = (BYTE)((nbSeq>>8) + 0x80);
        op[1] = (BYTE)nbSeq;
        op+=2;
    } else {
        op[0]=0xFF;
        MEM_writeLE16(op+1, (U16)(nbSeq - LONGNBSEQ));
        op+=3;
    }
    assert(op <= oend);
    if (nbSeq==0) {
        /* Copy the old tables over as if we repeated them */
        ZSTD_v1_5_2_memcpy(&nextEntropy->fse, &prevEntropy->fse, sizeof(prevEntropy->fse));
        return (size_t)(op - ostart);
    }
    {
        ZSTD_v1_5_2_symbolEncodingTypeStats_t stats;
        BYTE* seqHead = op++;
        /* build stats for sequences */
        stats = ZSTD_v1_5_2_buildSequencesStatistics(seqStorePtr, nbSeq,
                                             &prevEntropy->fse, &nextEntropy->fse,
                                              op, oend,
                                              strategy, count,
                                              entropyWorkspace, entropyWkspSize);
        FORWARD_IF_ERROR(stats.size, "ZSTD_v1_5_2_buildSequencesStatistics failed!");
        *seqHead = (BYTE)((stats.LLtype<<6) + (stats.Offtype<<4) + (stats.MLtype<<2));
        lastCountSize = stats.lastCountSize;
        op += stats.size;
    }

    {   size_t const bitstreamSize = ZSTD_v1_5_2_encodeSequences(
                                        op, (size_t)(oend - op),
                                        CTable_MatchLength, mlCodeTable,
                                        CTable_OffsetBits, ofCodeTable,
                                        CTable_LitLength, llCodeTable,
                                        sequences, nbSeq,
                                        longOffsets, bmi2);
        FORWARD_IF_ERROR(bitstreamSize, "ZSTD_v1_5_2_encodeSequences failed");
        op += bitstreamSize;
        assert(op <= oend);
        /* zstd versions <= 1.3.4 mistakenly report corruption when
         * FSE_v1_5_2_readNCount() receives a buffer < 4 bytes.
         * Fixed by https://github.com/facebook/zstd/pull/1146.
         * This can happen when the last set_compressed table present is 2
         * bytes and the bitstream is only one byte.
         * In this exceedingly rare case, we will simply emit an uncompressed
         * block, since it isn't worth optimizing.
         */
        if (lastCountSize && (lastCountSize + bitstreamSize) < 4) {
            /* lastCountSize >= 2 && bitstreamSize > 0 ==> lastCountSize == 3 */
            assert(lastCountSize + bitstreamSize == 3);
            DEBUGLOG(5, "Avoiding bug in zstd decoder in versions <= 1.3.4 by "
                        "emitting an uncompressed block.");
            return 0;
        }
    }

    DEBUGLOG(5, "compressed block size : %u", (unsigned)(op - ostart));
    return (size_t)(op - ostart);
}

MEM_STATIC size_t
ZSTD_v1_5_2_entropyCompressSeqStore(seqStore_t* seqStorePtr,
                       const ZSTD_v1_5_2_entropyCTables_t* prevEntropy,
                             ZSTD_v1_5_2_entropyCTables_t* nextEntropy,
                       const ZSTD_v1_5_2_CCtx_params* cctxParams,
                             void* dst, size_t dstCapacity,
                             size_t srcSize,
                             void* entropyWorkspace, size_t entropyWkspSize,
                             int bmi2)
{
    size_t const cSize = ZSTD_v1_5_2_entropyCompressSeqStore_internal(
                            seqStorePtr, prevEntropy, nextEntropy, cctxParams,
                            dst, dstCapacity,
                            entropyWorkspace, entropyWkspSize, bmi2);
    if (cSize == 0) return 0;
    /* When srcSize <= dstCapacity, there is enough space to write a raw uncompressed block.
     * Since we ran out of space, block must be not compressible, so fall back to raw uncompressed block.
     */
    if ((cSize == ERROR(dstSize_tooSmall)) & (srcSize <= dstCapacity))
        return 0;  /* block not compressed */
    FORWARD_IF_ERROR(cSize, "ZSTD_v1_5_2_entropyCompressSeqStore_internal failed");

    /* Check compressibility */
    {   size_t const maxCSize = srcSize - ZSTD_v1_5_2_minGain(srcSize, cctxParams->cParams.strategy);
        if (cSize >= maxCSize) return 0;  /* block not compressed */
    }
    DEBUGLOG(4, "ZSTD_v1_5_2_entropyCompressSeqStore() cSize: %zu", cSize);
    return cSize;
}

/* ZSTD_v1_5_2_selectBlockCompressor() :
 * Not static, but internal use only (used by long distance matcher)
 * assumption : strat is a valid strategy */
ZSTD_v1_5_2_blockCompressor ZSTD_v1_5_2_selectBlockCompressor(ZSTD_v1_5_2_strategy strat, ZSTD_v1_5_2_paramSwitch_e useRowMatchFinder, ZSTD_v1_5_2_dictMode_e dictMode)
{
    static const ZSTD_v1_5_2_blockCompressor blockCompressor[4][ZSTD_v1_5_2_STRATEGY_MAX+1] = {
        { ZSTD_v1_5_2_compressBlock_fast  /* default for 0 */,
          ZSTD_v1_5_2_compressBlock_fast,
          ZSTD_v1_5_2_compressBlock_doubleFast,
          ZSTD_v1_5_2_compressBlock_greedy,
          ZSTD_v1_5_2_compressBlock_lazy,
          ZSTD_v1_5_2_compressBlock_lazy2,
          ZSTD_v1_5_2_compressBlock_btlazy2,
          ZSTD_v1_5_2_compressBlock_btopt,
          ZSTD_v1_5_2_compressBlock_btultra,
          ZSTD_v1_5_2_compressBlock_btultra2 },
        { ZSTD_v1_5_2_compressBlock_fast_extDict  /* default for 0 */,
          ZSTD_v1_5_2_compressBlock_fast_extDict,
          ZSTD_v1_5_2_compressBlock_doubleFast_extDict,
          ZSTD_v1_5_2_compressBlock_greedy_extDict,
          ZSTD_v1_5_2_compressBlock_lazy_extDict,
          ZSTD_v1_5_2_compressBlock_lazy2_extDict,
          ZSTD_v1_5_2_compressBlock_btlazy2_extDict,
          ZSTD_v1_5_2_compressBlock_btopt_extDict,
          ZSTD_v1_5_2_compressBlock_btultra_extDict,
          ZSTD_v1_5_2_compressBlock_btultra_extDict },
        { ZSTD_v1_5_2_compressBlock_fast_dictMatchState  /* default for 0 */,
          ZSTD_v1_5_2_compressBlock_fast_dictMatchState,
          ZSTD_v1_5_2_compressBlock_doubleFast_dictMatchState,
          ZSTD_v1_5_2_compressBlock_greedy_dictMatchState,
          ZSTD_v1_5_2_compressBlock_lazy_dictMatchState,
          ZSTD_v1_5_2_compressBlock_lazy2_dictMatchState,
          ZSTD_v1_5_2_compressBlock_btlazy2_dictMatchState,
          ZSTD_v1_5_2_compressBlock_btopt_dictMatchState,
          ZSTD_v1_5_2_compressBlock_btultra_dictMatchState,
          ZSTD_v1_5_2_compressBlock_btultra_dictMatchState },
        { NULL  /* default for 0 */,
          NULL,
          NULL,
          ZSTD_v1_5_2_compressBlock_greedy_dedicatedDictSearch,
          ZSTD_v1_5_2_compressBlock_lazy_dedicatedDictSearch,
          ZSTD_v1_5_2_compressBlock_lazy2_dedicatedDictSearch,
          NULL,
          NULL,
          NULL,
          NULL }
    };
    ZSTD_v1_5_2_blockCompressor selectedCompressor;
    ZSTD_v1_5_2_STATIC_ASSERT((unsigned)ZSTD_v1_5_2_fast == 1);

    assert(ZSTD_v1_5_2_cParam_withinBounds(ZSTD_v1_5_2_c_strategy, strat));
    DEBUGLOG(4, "Selected block compressor: dictMode=%d strat=%d rowMatchfinder=%d", (int)dictMode, (int)strat, (int)useRowMatchFinder);
    if (ZSTD_v1_5_2_rowMatchFinderUsed(strat, useRowMatchFinder)) {
        static const ZSTD_v1_5_2_blockCompressor rowBasedBlockCompressors[4][3] = {
            { ZSTD_v1_5_2_compressBlock_greedy_row,
            ZSTD_v1_5_2_compressBlock_lazy_row,
            ZSTD_v1_5_2_compressBlock_lazy2_row },
            { ZSTD_v1_5_2_compressBlock_greedy_extDict_row,
            ZSTD_v1_5_2_compressBlock_lazy_extDict_row,
            ZSTD_v1_5_2_compressBlock_lazy2_extDict_row },
            { ZSTD_v1_5_2_compressBlock_greedy_dictMatchState_row,
            ZSTD_v1_5_2_compressBlock_lazy_dictMatchState_row,
            ZSTD_v1_5_2_compressBlock_lazy2_dictMatchState_row },
            { ZSTD_v1_5_2_compressBlock_greedy_dedicatedDictSearch_row,
            ZSTD_v1_5_2_compressBlock_lazy_dedicatedDictSearch_row,
            ZSTD_v1_5_2_compressBlock_lazy2_dedicatedDictSearch_row }
        };
        DEBUGLOG(4, "Selecting a row-based matchfinder");
        assert(useRowMatchFinder != ZSTD_v1_5_2_ps_auto);
        selectedCompressor = rowBasedBlockCompressors[(int)dictMode][(int)strat - (int)ZSTD_v1_5_2_greedy];
    } else {
        selectedCompressor = blockCompressor[(int)dictMode][(int)strat];
    }
    assert(selectedCompressor != NULL);
    return selectedCompressor;
}

static void ZSTD_v1_5_2_storeLastLiterals(seqStore_t* seqStorePtr,
                                   const BYTE* anchor, size_t lastLLSize)
{
    ZSTD_v1_5_2_memcpy(seqStorePtr->lit, anchor, lastLLSize);
    seqStorePtr->lit += lastLLSize;
}

void ZSTD_v1_5_2_resetSeqStore(seqStore_t* ssPtr)
{
    ssPtr->lit = ssPtr->litStart;
    ssPtr->sequences = ssPtr->sequencesStart;
    ssPtr->longLengthType = ZSTD_v1_5_2_llt_none;
}

typedef enum { ZSTDbss_compress, ZSTDbss_noCompress } ZSTD_v1_5_2_buildSeqStore_e;

static size_t ZSTD_v1_5_2_buildSeqStore(ZSTD_v1_5_2_CCtx* zc, const void* src, size_t srcSize)
{
    ZSTD_v1_5_2_matchState_t* const ms = &zc->blockState.matchState;
    DEBUGLOG(5, "ZSTD_v1_5_2_buildSeqStore (srcSize=%zu)", srcSize);
    assert(srcSize <= ZSTD_v1_5_2_BLOCKSIZE_MAX);
    /* Assert that we have correctly flushed the ctx params into the ms's copy */
    ZSTD_v1_5_2_assertEqualCParams(zc->appliedParams.cParams, ms->cParams);
    if (srcSize < MIN_CBLOCK_SIZE+ZSTD_v1_5_2_blockHeaderSize+1) {
        if (zc->appliedParams.cParams.strategy >= ZSTD_v1_5_2_btopt) {
            ZSTD_v1_5_2_ldm_skipRawSeqStoreBytes(&zc->externSeqStore, srcSize);
        } else {
            ZSTD_v1_5_2_ldm_skipSequences(&zc->externSeqStore, srcSize, zc->appliedParams.cParams.minMatch);
        }
        return ZSTDbss_noCompress; /* don't even attempt compression below a certain srcSize */
    }
    ZSTD_v1_5_2_resetSeqStore(&(zc->seqStore));
    /* required for optimal parser to read stats from dictionary */
    ms->opt.symbolCosts = &zc->blockState.prevCBlock->entropy;
    /* tell the optimal parser how we expect to compress literals */
    ms->opt.literalCompressionMode = zc->appliedParams.literalCompressionMode;
    /* a gap between an attached dict and the current window is not safe,
     * they must remain adjacent,
     * and when that stops being the case, the dict must be unset */
    assert(ms->dictMatchState == NULL || ms->loadedDictEnd == ms->window.dictLimit);

    /* limited update after a very long match */
    {   const BYTE* const base = ms->window.base;
        const BYTE* const istart = (const BYTE*)src;
        const U32 curr = (U32)(istart-base);
        if (sizeof(ptrdiff_t)==8) assert(istart - base < (ptrdiff_t)(U32)(-1));   /* ensure no overflow */
        if (curr > ms->nextToUpdate + 384)
            ms->nextToUpdate = curr - MIN(192, (U32)(curr - ms->nextToUpdate - 384));
    }

    /* select and store sequences */
    {   ZSTD_v1_5_2_dictMode_e const dictMode = ZSTD_v1_5_2_matchState_dictMode(ms);
        size_t lastLLSize;
        {   int i;
            for (i = 0; i < ZSTD_v1_5_2_REP_NUM; ++i)
                zc->blockState.nextCBlock->rep[i] = zc->blockState.prevCBlock->rep[i];
        }
        if (zc->externSeqStore.pos < zc->externSeqStore.size) {
            assert(zc->appliedParams.ldmParams.enableLdm == ZSTD_v1_5_2_ps_disable);
            /* Updates ldmSeqStore.pos */
            lastLLSize =
                ZSTD_v1_5_2_ldm_blockCompress(&zc->externSeqStore,
                                       ms, &zc->seqStore,
                                       zc->blockState.nextCBlock->rep,
                                       zc->appliedParams.useRowMatchFinder,
                                       src, srcSize);
            assert(zc->externSeqStore.pos <= zc->externSeqStore.size);
        } else if (zc->appliedParams.ldmParams.enableLdm == ZSTD_v1_5_2_ps_enable) {
            rawSeqStore_t ldmSeqStore = kNullRawSeqStore;

            ldmSeqStore.seq = zc->ldmSequences;
            ldmSeqStore.capacity = zc->maxNbLdmSequences;
            /* Updates ldmSeqStore.size */
            FORWARD_IF_ERROR(ZSTD_v1_5_2_ldm_generateSequences(&zc->ldmState, &ldmSeqStore,
                                               &zc->appliedParams.ldmParams,
                                               src, srcSize), "");
            /* Updates ldmSeqStore.pos */
            lastLLSize =
                ZSTD_v1_5_2_ldm_blockCompress(&ldmSeqStore,
                                       ms, &zc->seqStore,
                                       zc->blockState.nextCBlock->rep,
                                       zc->appliedParams.useRowMatchFinder,
                                       src, srcSize);
            assert(ldmSeqStore.pos == ldmSeqStore.size);
        } else {   /* not long range mode */
            ZSTD_v1_5_2_blockCompressor const blockCompressor = ZSTD_v1_5_2_selectBlockCompressor(zc->appliedParams.cParams.strategy,
                                                                                    zc->appliedParams.useRowMatchFinder,
                                                                                    dictMode);
            ms->ldmSeqStore = NULL;
            lastLLSize = blockCompressor(ms, &zc->seqStore, zc->blockState.nextCBlock->rep, src, srcSize);
        }
        {   const BYTE* const lastLiterals = (const BYTE*)src + srcSize - lastLLSize;
            ZSTD_v1_5_2_storeLastLiterals(&zc->seqStore, lastLiterals, lastLLSize);
    }   }
    return ZSTDbss_compress;
}

static void ZSTD_v1_5_2_copyBlockSequences(ZSTD_v1_5_2_CCtx* zc)
{
    const seqStore_t* seqStore = ZSTD_v1_5_2_getSeqStore(zc);
    const seqDef* seqStoreSeqs = seqStore->sequencesStart;
    size_t seqStoreSeqSize = seqStore->sequences - seqStoreSeqs;
    size_t seqStoreLiteralsSize = (size_t)(seqStore->lit - seqStore->litStart);
    size_t literalsRead = 0;
    size_t lastLLSize;

    ZSTD_v1_5_2_Sequence* outSeqs = &zc->seqCollector.seqStart[zc->seqCollector.seqIndex];
    size_t i;
    repcodes_t updatedRepcodes;

    assert(zc->seqCollector.seqIndex + 1 < zc->seqCollector.maxSequences);
    /* Ensure we have enough space for last literals "sequence" */
    assert(zc->seqCollector.maxSequences >= seqStoreSeqSize + 1);
    ZSTD_v1_5_2_memcpy(updatedRepcodes.rep, zc->blockState.prevCBlock->rep, sizeof(repcodes_t));
    for (i = 0; i < seqStoreSeqSize; ++i) {
        U32 rawOffset = seqStoreSeqs[i].offBase - ZSTD_v1_5_2_REP_NUM;
        outSeqs[i].litLength = seqStoreSeqs[i].litLength;
        outSeqs[i].matchLength = seqStoreSeqs[i].mlBase + MINMATCH;
        outSeqs[i].rep = 0;

        if (i == seqStore->longLengthPos) {
            if (seqStore->longLengthType == ZSTD_v1_5_2_llt_literalLength) {
                outSeqs[i].litLength += 0x10000;
            } else if (seqStore->longLengthType == ZSTD_v1_5_2_llt_matchLength) {
                outSeqs[i].matchLength += 0x10000;
            }
        }

        if (seqStoreSeqs[i].offBase <= ZSTD_v1_5_2_REP_NUM) {
            /* Derive the correct offset corresponding to a repcode */
            outSeqs[i].rep = seqStoreSeqs[i].offBase;
            if (outSeqs[i].litLength != 0) {
                rawOffset = updatedRepcodes.rep[outSeqs[i].rep - 1];
            } else {
                if (outSeqs[i].rep == 3) {
                    rawOffset = updatedRepcodes.rep[0] - 1;
                } else {
                    rawOffset = updatedRepcodes.rep[outSeqs[i].rep];
                }
            }
        }
        outSeqs[i].offset = rawOffset;
        /* seqStoreSeqs[i].offset == offCode+1, and ZSTD_v1_5_2_updateRep() expects offCode
           so we provide seqStoreSeqs[i].offset - 1 */
        ZSTD_v1_5_2_updateRep(updatedRepcodes.rep,
                       seqStoreSeqs[i].offBase - 1,
                       seqStoreSeqs[i].litLength == 0);
        literalsRead += outSeqs[i].litLength;
    }
    /* Insert last literals (if any exist) in the block as a sequence with ml == off == 0.
     * If there are no last literals, then we'll emit (of: 0, ml: 0, ll: 0), which is a marker
     * for the block boundary, according to the API.
     */
    assert(seqStoreLiteralsSize >= literalsRead);
    lastLLSize = seqStoreLiteralsSize - literalsRead;
    outSeqs[i].litLength = (U32)lastLLSize;
    outSeqs[i].matchLength = outSeqs[i].offset = outSeqs[i].rep = 0;
    seqStoreSeqSize++;
    zc->seqCollector.seqIndex += seqStoreSeqSize;
}

size_t ZSTD_v1_5_2_generateSequences(ZSTD_v1_5_2_CCtx* zc, ZSTD_v1_5_2_Sequence* outSeqs,
                              size_t outSeqsSize, const void* src, size_t srcSize)
{
    const size_t dstCapacity = ZSTD_v1_5_2_compressBound(srcSize);
    void* dst = ZSTD_v1_5_2_customMalloc(dstCapacity, ZSTD_v1_5_2_defaultCMem);
    SeqCollector seqCollector;

    RETURN_ERROR_IF(dst == NULL, memory_allocation, "NULL pointer!");

    seqCollector.collectSequences = 1;
    seqCollector.seqStart = outSeqs;
    seqCollector.seqIndex = 0;
    seqCollector.maxSequences = outSeqsSize;
    zc->seqCollector = seqCollector;

    ZSTD_v1_5_2_compress2(zc, dst, dstCapacity, src, srcSize);
    ZSTD_v1_5_2_customFree(dst, ZSTD_v1_5_2_defaultCMem);
    return zc->seqCollector.seqIndex;
}

size_t ZSTD_v1_5_2_mergeBlockDelimiters(ZSTD_v1_5_2_Sequence* sequences, size_t seqsSize) {
    size_t in = 0;
    size_t out = 0;
    for (; in < seqsSize; ++in) {
        if (sequences[in].offset == 0 && sequences[in].matchLength == 0) {
            if (in != seqsSize - 1) {
                sequences[in+1].litLength += sequences[in].litLength;
            }
        } else {
            sequences[out] = sequences[in];
            ++out;
        }
    }
    return out;
}

/* Unrolled loop to read four size_ts of input at a time. Returns 1 if is RLE, 0 if not. */
static int ZSTD_v1_5_2_isRLE(const BYTE* src, size_t length) {
    const BYTE* ip = src;
    const BYTE value = ip[0];
    const size_t valueST = (size_t)((U64)value * 0x0101010101010101ULL);
    const size_t unrollSize = sizeof(size_t) * 4;
    const size_t unrollMask = unrollSize - 1;
    const size_t prefixLength = length & unrollMask;
    size_t i;
    size_t u;
    if (length == 1) return 1;
    /* Check if prefix is RLE first before using unrolled loop */
    if (prefixLength && ZSTD_v1_5_2_count(ip+1, ip, ip+prefixLength) != prefixLength-1) {
        return 0;
    }
    for (i = prefixLength; i != length; i += unrollSize) {
        for (u = 0; u < unrollSize; u += sizeof(size_t)) {
            if (MEM_readST(ip + i + u) != valueST) {
                return 0;
            }
        }
    }
    return 1;
}

/* Returns true if the given block may be RLE.
 * This is just a heuristic based on the compressibility.
 * It may return both false positives and false negatives.
 */
static int ZSTD_v1_5_2_maybeRLE(seqStore_t const* seqStore)
{
    size_t const nbSeqs = (size_t)(seqStore->sequences - seqStore->sequencesStart);
    size_t const nbLits = (size_t)(seqStore->lit - seqStore->litStart);

    return nbSeqs < 4 && nbLits < 10;
}

static void ZSTD_v1_5_2_blockState_confirmRepcodesAndEntropyTables(ZSTD_v1_5_2_blockState_t* const bs)
{
    ZSTD_v1_5_2_compressedBlockState_t* const tmp = bs->prevCBlock;
    bs->prevCBlock = bs->nextCBlock;
    bs->nextCBlock = tmp;
}

/* Writes the block header */
static void writeBlockHeader(void* op, size_t cSize, size_t blockSize, U32 lastBlock) {
    U32 const cBlockHeader = cSize == 1 ?
                        lastBlock + (((U32)bt_rle)<<1) + (U32)(blockSize << 3) :
                        lastBlock + (((U32)bt_compressed)<<1) + (U32)(cSize << 3);
    MEM_writeLE24(op, cBlockHeader);
    DEBUGLOG(3, "writeBlockHeader: cSize: %zu blockSize: %zu lastBlock: %u", cSize, blockSize, lastBlock);
}

/** ZSTD_v1_5_2_buildBlockEntropyStats_literals() :
 *  Builds entropy for the literals.
 *  Stores literals block type (raw, rle, compressed, repeat) and
 *  huffman description table to hufMetadata.
 *  Requires ENTROPY_WORKSPACE_SIZE workspace
 *  @return : size of huffman description table or error code */
static size_t ZSTD_v1_5_2_buildBlockEntropyStats_literals(void* const src, size_t srcSize,
                                            const ZSTD_v1_5_2_hufCTables_t* prevHuf,
                                                  ZSTD_v1_5_2_hufCTables_t* nextHuf,
                                                  ZSTD_v1_5_2_hufCTablesMetadata_t* hufMetadata,
                                                  const int literalsCompressionIsDisabled,
                                                  void* workspace, size_t wkspSize)
{
    BYTE* const wkspStart = (BYTE*)workspace;
    BYTE* const wkspEnd = wkspStart + wkspSize;
    BYTE* const countWkspStart = wkspStart;
    unsigned* const countWksp = (unsigned*)workspace;
    const size_t countWkspSize = (HUF_v1_5_2_SYMBOLVALUE_MAX + 1) * sizeof(unsigned);
    BYTE* const nodeWksp = countWkspStart + countWkspSize;
    const size_t nodeWkspSize = wkspEnd-nodeWksp;
    unsigned maxSymbolValue = HUF_v1_5_2_SYMBOLVALUE_MAX;
    unsigned huffLog = HUF_v1_5_2_TABLELOG_DEFAULT;
    HUF_v1_5_2_repeat repeat = prevHuf->repeatMode;
    DEBUGLOG(5, "ZSTD_v1_5_2_buildBlockEntropyStats_literals (srcSize=%zu)", srcSize);

    /* Prepare nextEntropy assuming reusing the existing table */
    ZSTD_v1_5_2_memcpy(nextHuf, prevHuf, sizeof(*prevHuf));

    if (literalsCompressionIsDisabled) {
        DEBUGLOG(5, "set_basic - disabled");
        hufMetadata->hType = set_basic;
        return 0;
    }

    /* small ? don't even attempt compression (speed opt) */
#ifndef COMPRESS_LITERALS_SIZE_MIN
#define COMPRESS_LITERALS_SIZE_MIN 63
#endif
    {   size_t const minLitSize = (prevHuf->repeatMode == HUF_v1_5_2_repeat_valid) ? 6 : COMPRESS_LITERALS_SIZE_MIN;
        if (srcSize <= minLitSize) {
            DEBUGLOG(5, "set_basic - too small");
            hufMetadata->hType = set_basic;
            return 0;
        }
    }

    /* Scan input and build symbol stats */
    {   size_t const largest = HIST_v1_5_2_count_wksp (countWksp, &maxSymbolValue, (const BYTE*)src, srcSize, workspace, wkspSize);
        FORWARD_IF_ERROR(largest, "HIST_v1_5_2_count_wksp failed");
        if (largest == srcSize) {
            DEBUGLOG(5, "set_rle");
            hufMetadata->hType = set_rle;
            return 0;
        }
        if (largest <= (srcSize >> 7)+4) {
            DEBUGLOG(5, "set_basic - no gain");
            hufMetadata->hType = set_basic;
            return 0;
        }
    }

    /* Validate the previous Huffman table */
    if (repeat == HUF_v1_5_2_repeat_check && !HUF_v1_5_2_validateCTable((HUF_v1_5_2_CElt const*)prevHuf->CTable, countWksp, maxSymbolValue)) {
        repeat = HUF_v1_5_2_repeat_none;
    }

    /* Build Huffman Tree */
    ZSTD_v1_5_2_memset(nextHuf->CTable, 0, sizeof(nextHuf->CTable));
    huffLog = HUF_v1_5_2_optimalTableLog(huffLog, srcSize, maxSymbolValue);
    {   size_t const maxBits = HUF_v1_5_2_buildCTable_wksp((HUF_v1_5_2_CElt*)nextHuf->CTable, countWksp,
                                                    maxSymbolValue, huffLog,
                                                    nodeWksp, nodeWkspSize);
        FORWARD_IF_ERROR(maxBits, "HUF_v1_5_2_buildCTable_wksp");
        huffLog = (U32)maxBits;
        {   /* Build and write the CTable */
            size_t const newCSize = HUF_v1_5_2_estimateCompressedSize(
                    (HUF_v1_5_2_CElt*)nextHuf->CTable, countWksp, maxSymbolValue);
            size_t const hSize = HUF_v1_5_2_writeCTable_wksp(
                    hufMetadata->hufDesBuffer, sizeof(hufMetadata->hufDesBuffer),
                    (HUF_v1_5_2_CElt*)nextHuf->CTable, maxSymbolValue, huffLog,
                    nodeWksp, nodeWkspSize);
            /* Check against repeating the previous CTable */
            if (repeat != HUF_v1_5_2_repeat_none) {
                size_t const oldCSize = HUF_v1_5_2_estimateCompressedSize(
                        (HUF_v1_5_2_CElt const*)prevHuf->CTable, countWksp, maxSymbolValue);
                if (oldCSize < srcSize && (oldCSize <= hSize + newCSize || hSize + 12 >= srcSize)) {
                    DEBUGLOG(5, "set_repeat - smaller");
                    ZSTD_v1_5_2_memcpy(nextHuf, prevHuf, sizeof(*prevHuf));
                    hufMetadata->hType = set_repeat;
                    return 0;
                }
            }
            if (newCSize + hSize >= srcSize) {
                DEBUGLOG(5, "set_basic - no gains");
                ZSTD_v1_5_2_memcpy(nextHuf, prevHuf, sizeof(*prevHuf));
                hufMetadata->hType = set_basic;
                return 0;
            }
            DEBUGLOG(5, "set_compressed (hSize=%u)", (U32)hSize);
            hufMetadata->hType = set_compressed;
            nextHuf->repeatMode = HUF_v1_5_2_repeat_check;
            return hSize;
        }
    }
}


/* ZSTD_v1_5_2_buildDummySequencesStatistics():
 * Returns a ZSTD_v1_5_2_symbolEncodingTypeStats_t with all encoding types as set_basic,
 * and updates nextEntropy to the appropriate repeatMode.
 */
static ZSTD_v1_5_2_symbolEncodingTypeStats_t
ZSTD_v1_5_2_buildDummySequencesStatistics(ZSTD_v1_5_2_fseCTables_t* nextEntropy) {
    ZSTD_v1_5_2_symbolEncodingTypeStats_t stats = {set_basic, set_basic, set_basic, 0, 0};
    nextEntropy->litlength_repeatMode = FSE_v1_5_2_repeat_none;
    nextEntropy->offcode_repeatMode = FSE_v1_5_2_repeat_none;
    nextEntropy->matchlength_repeatMode = FSE_v1_5_2_repeat_none;
    return stats;
}

/** ZSTD_v1_5_2_buildBlockEntropyStats_sequences() :
 *  Builds entropy for the sequences.
 *  Stores symbol compression modes and fse table to fseMetadata.
 *  Requires ENTROPY_WORKSPACE_SIZE wksp.
 *  @return : size of fse tables or error code */
static size_t ZSTD_v1_5_2_buildBlockEntropyStats_sequences(seqStore_t* seqStorePtr,
                                              const ZSTD_v1_5_2_fseCTables_t* prevEntropy,
                                                    ZSTD_v1_5_2_fseCTables_t* nextEntropy,
                                              const ZSTD_v1_5_2_CCtx_params* cctxParams,
                                                    ZSTD_v1_5_2_fseCTablesMetadata_t* fseMetadata,
                                                    void* workspace, size_t wkspSize)
{
    ZSTD_v1_5_2_strategy const strategy = cctxParams->cParams.strategy;
    size_t const nbSeq = seqStorePtr->sequences - seqStorePtr->sequencesStart;
    BYTE* const ostart = fseMetadata->fseTablesBuffer;
    BYTE* const oend = ostart + sizeof(fseMetadata->fseTablesBuffer);
    BYTE* op = ostart;
    unsigned* countWorkspace = (unsigned*)workspace;
    unsigned* entropyWorkspace = countWorkspace + (MaxSeq + 1);
    size_t entropyWorkspaceSize = wkspSize - (MaxSeq + 1) * sizeof(*countWorkspace);
    ZSTD_v1_5_2_symbolEncodingTypeStats_t stats;

    DEBUGLOG(5, "ZSTD_v1_5_2_buildBlockEntropyStats_sequences (nbSeq=%zu)", nbSeq);
    stats = nbSeq != 0 ? ZSTD_v1_5_2_buildSequencesStatistics(seqStorePtr, nbSeq,
                                          prevEntropy, nextEntropy, op, oend,
                                          strategy, countWorkspace,
                                          entropyWorkspace, entropyWorkspaceSize)
                       : ZSTD_v1_5_2_buildDummySequencesStatistics(nextEntropy);
    FORWARD_IF_ERROR(stats.size, "ZSTD_v1_5_2_buildSequencesStatistics failed!");
    fseMetadata->llType = (symbolEncodingType_e) stats.LLtype;
    fseMetadata->ofType = (symbolEncodingType_e) stats.Offtype;
    fseMetadata->mlType = (symbolEncodingType_e) stats.MLtype;
    fseMetadata->lastCountSize = stats.lastCountSize;
    return stats.size;
}


/** ZSTD_v1_5_2_buildBlockEntropyStats() :
 *  Builds entropy for the block.
 *  Requires workspace size ENTROPY_WORKSPACE_SIZE
 *
 *  @return : 0 on success or error code
 */
size_t ZSTD_v1_5_2_buildBlockEntropyStats(seqStore_t* seqStorePtr,
                             const ZSTD_v1_5_2_entropyCTables_t* prevEntropy,
                                   ZSTD_v1_5_2_entropyCTables_t* nextEntropy,
                             const ZSTD_v1_5_2_CCtx_params* cctxParams,
                                   ZSTD_v1_5_2_entropyCTablesMetadata_t* entropyMetadata,
                                   void* workspace, size_t wkspSize)
{
    size_t const litSize = seqStorePtr->lit - seqStorePtr->litStart;
    entropyMetadata->hufMetadata.hufDesSize =
        ZSTD_v1_5_2_buildBlockEntropyStats_literals(seqStorePtr->litStart, litSize,
                                            &prevEntropy->huf, &nextEntropy->huf,
                                            &entropyMetadata->hufMetadata,
                                            ZSTD_v1_5_2_literalsCompressionIsDisabled(cctxParams),
                                            workspace, wkspSize);
    FORWARD_IF_ERROR(entropyMetadata->hufMetadata.hufDesSize, "ZSTD_v1_5_2_buildBlockEntropyStats_literals failed");
    entropyMetadata->fseMetadata.fseTablesSize =
        ZSTD_v1_5_2_buildBlockEntropyStats_sequences(seqStorePtr,
                                              &prevEntropy->fse, &nextEntropy->fse,
                                              cctxParams,
                                              &entropyMetadata->fseMetadata,
                                              workspace, wkspSize);
    FORWARD_IF_ERROR(entropyMetadata->fseMetadata.fseTablesSize, "ZSTD_v1_5_2_buildBlockEntropyStats_sequences failed");
    return 0;
}

/* Returns the size estimate for the literals section (header + content) of a block */
static size_t ZSTD_v1_5_2_estimateBlockSize_literal(const BYTE* literals, size_t litSize,
                                                const ZSTD_v1_5_2_hufCTables_t* huf,
                                                const ZSTD_v1_5_2_hufCTablesMetadata_t* hufMetadata,
                                                void* workspace, size_t wkspSize,
                                                int writeEntropy)
{
    unsigned* const countWksp = (unsigned*)workspace;
    unsigned maxSymbolValue = HUF_v1_5_2_SYMBOLVALUE_MAX;
    size_t literalSectionHeaderSize = 3 + (litSize >= 1 KB) + (litSize >= 16 KB);
    U32 singleStream = litSize < 256;

    if (hufMetadata->hType == set_basic) return litSize;
    else if (hufMetadata->hType == set_rle) return 1;
    else if (hufMetadata->hType == set_compressed || hufMetadata->hType == set_repeat) {
        size_t const largest = HIST_v1_5_2_count_wksp (countWksp, &maxSymbolValue, (const BYTE*)literals, litSize, workspace, wkspSize);
        if (ZSTD_v1_5_2_isError(largest)) return litSize;
        {   size_t cLitSizeEstimate = HUF_v1_5_2_estimateCompressedSize((const HUF_v1_5_2_CElt*)huf->CTable, countWksp, maxSymbolValue);
            if (writeEntropy) cLitSizeEstimate += hufMetadata->hufDesSize;
            if (!singleStream) cLitSizeEstimate += 6; /* multi-stream huffman uses 6-byte jump table */
            return cLitSizeEstimate + literalSectionHeaderSize;
    }   }
    assert(0); /* impossible */
    return 0;
}

/* Returns the size estimate for the FSE-compressed symbols (of, ml, ll) of a block */
static size_t ZSTD_v1_5_2_estimateBlockSize_symbolType(symbolEncodingType_e type,
                        const BYTE* codeTable, size_t nbSeq, unsigned maxCode,
                        const FSE_v1_5_2_CTable* fseCTable,
                        const U8* additionalBits,
                        short const* defaultNorm, U32 defaultNormLog, U32 defaultMax,
                        void* workspace, size_t wkspSize)
{
    unsigned* const countWksp = (unsigned*)workspace;
    const BYTE* ctp = codeTable;
    const BYTE* const ctStart = ctp;
    const BYTE* const ctEnd = ctStart + nbSeq;
    size_t cSymbolTypeSizeEstimateInBits = 0;
    unsigned max = maxCode;

    HIST_v1_5_2_countFast_wksp(countWksp, &max, codeTable, nbSeq, workspace, wkspSize);  /* can't fail */
    if (type == set_basic) {
        /* We selected this encoding type, so it must be valid. */
        assert(max <= defaultMax);
        (void)defaultMax;
        cSymbolTypeSizeEstimateInBits = ZSTD_v1_5_2_crossEntropyCost(defaultNorm, defaultNormLog, countWksp, max);
    } else if (type == set_rle) {
        cSymbolTypeSizeEstimateInBits = 0;
    } else if (type == set_compressed || type == set_repeat) {
        cSymbolTypeSizeEstimateInBits = ZSTD_v1_5_2_fseBitCost(fseCTable, countWksp, max);
    }
    if (ZSTD_v1_5_2_isError(cSymbolTypeSizeEstimateInBits)) {
        return nbSeq * 10;
    }
    while (ctp < ctEnd) {
        if (additionalBits) cSymbolTypeSizeEstimateInBits += additionalBits[*ctp];
        else cSymbolTypeSizeEstimateInBits += *ctp; /* for offset, offset code is also the number of additional bits */
        ctp++;
    }
    return cSymbolTypeSizeEstimateInBits >> 3;
}

/* Returns the size estimate for the sequences section (header + content) of a block */
static size_t ZSTD_v1_5_2_estimateBlockSize_sequences(const BYTE* ofCodeTable,
                                                  const BYTE* llCodeTable,
                                                  const BYTE* mlCodeTable,
                                                  size_t nbSeq,
                                                  const ZSTD_v1_5_2_fseCTables_t* fseTables,
                                                  const ZSTD_v1_5_2_fseCTablesMetadata_t* fseMetadata,
                                                  void* workspace, size_t wkspSize,
                                                  int writeEntropy)
{
    size_t sequencesSectionHeaderSize = 1 /* seqHead */ + 1 /* min seqSize size */ + (nbSeq >= 128) + (nbSeq >= LONGNBSEQ);
    size_t cSeqSizeEstimate = 0;
    cSeqSizeEstimate += ZSTD_v1_5_2_estimateBlockSize_symbolType(fseMetadata->ofType, ofCodeTable, nbSeq, MaxOff,
                                         fseTables->offcodeCTable, NULL,
                                         OF_defaultNorm, OF_defaultNormLog, DefaultMaxOff,
                                         workspace, wkspSize);
    cSeqSizeEstimate += ZSTD_v1_5_2_estimateBlockSize_symbolType(fseMetadata->llType, llCodeTable, nbSeq, MaxLL,
                                         fseTables->litlengthCTable, LL_bits,
                                         LL_defaultNorm, LL_defaultNormLog, MaxLL,
                                         workspace, wkspSize);
    cSeqSizeEstimate += ZSTD_v1_5_2_estimateBlockSize_symbolType(fseMetadata->mlType, mlCodeTable, nbSeq, MaxML,
                                         fseTables->matchlengthCTable, ML_bits,
                                         ML_defaultNorm, ML_defaultNormLog, MaxML,
                                         workspace, wkspSize);
    if (writeEntropy) cSeqSizeEstimate += fseMetadata->fseTablesSize;
    return cSeqSizeEstimate + sequencesSectionHeaderSize;
}

/* Returns the size estimate for a given stream of literals, of, ll, ml */
static size_t ZSTD_v1_5_2_estimateBlockSize(const BYTE* literals, size_t litSize,
                                     const BYTE* ofCodeTable,
                                     const BYTE* llCodeTable,
                                     const BYTE* mlCodeTable,
                                     size_t nbSeq,
                                     const ZSTD_v1_5_2_entropyCTables_t* entropy,
                                     const ZSTD_v1_5_2_entropyCTablesMetadata_t* entropyMetadata,
                                     void* workspace, size_t wkspSize,
                                     int writeLitEntropy, int writeSeqEntropy) {
    size_t const literalsSize = ZSTD_v1_5_2_estimateBlockSize_literal(literals, litSize,
                                                         &entropy->huf, &entropyMetadata->hufMetadata,
                                                         workspace, wkspSize, writeLitEntropy);
    size_t const seqSize = ZSTD_v1_5_2_estimateBlockSize_sequences(ofCodeTable, llCodeTable, mlCodeTable,
                                                         nbSeq, &entropy->fse, &entropyMetadata->fseMetadata,
                                                         workspace, wkspSize, writeSeqEntropy);
    return seqSize + literalsSize + ZSTD_v1_5_2_blockHeaderSize;
}

/* Builds entropy statistics and uses them for blocksize estimation.
 *
 * Returns the estimated compressed size of the seqStore, or a zstd error.
 */
static size_t ZSTD_v1_5_2_buildEntropyStatisticsAndEstimateSubBlockSize(seqStore_t* seqStore, ZSTD_v1_5_2_CCtx* zc) {
    ZSTD_v1_5_2_entropyCTablesMetadata_t* entropyMetadata = &zc->blockSplitCtx.entropyMetadata;
    DEBUGLOG(6, "ZSTD_v1_5_2_buildEntropyStatisticsAndEstimateSubBlockSize()");
    FORWARD_IF_ERROR(ZSTD_v1_5_2_buildBlockEntropyStats(seqStore,
                    &zc->blockState.prevCBlock->entropy,
                    &zc->blockState.nextCBlock->entropy,
                    &zc->appliedParams,
                    entropyMetadata,
                    zc->entropyWorkspace, ENTROPY_WORKSPACE_SIZE /* statically allocated in resetCCtx */), "");
    return ZSTD_v1_5_2_estimateBlockSize(seqStore->litStart, (size_t)(seqStore->lit - seqStore->litStart),
                    seqStore->ofCode, seqStore->llCode, seqStore->mlCode,
                    (size_t)(seqStore->sequences - seqStore->sequencesStart),
                    &zc->blockState.nextCBlock->entropy, entropyMetadata, zc->entropyWorkspace, ENTROPY_WORKSPACE_SIZE,
                    (int)(entropyMetadata->hufMetadata.hType == set_compressed), 1);
}

/* Returns literals bytes represented in a seqStore */
static size_t ZSTD_v1_5_2_countSeqStoreLiteralsBytes(const seqStore_t* const seqStore) {
    size_t literalsBytes = 0;
    size_t const nbSeqs = seqStore->sequences - seqStore->sequencesStart;
    size_t i;
    for (i = 0; i < nbSeqs; ++i) {
        seqDef seq = seqStore->sequencesStart[i];
        literalsBytes += seq.litLength;
        if (i == seqStore->longLengthPos && seqStore->longLengthType == ZSTD_v1_5_2_llt_literalLength) {
            literalsBytes += 0x10000;
        }
    }
    return literalsBytes;
}

/* Returns match bytes represented in a seqStore */
static size_t ZSTD_v1_5_2_countSeqStoreMatchBytes(const seqStore_t* const seqStore) {
    size_t matchBytes = 0;
    size_t const nbSeqs = seqStore->sequences - seqStore->sequencesStart;
    size_t i;
    for (i = 0; i < nbSeqs; ++i) {
        seqDef seq = seqStore->sequencesStart[i];
        matchBytes += seq.mlBase + MINMATCH;
        if (i == seqStore->longLengthPos && seqStore->longLengthType == ZSTD_v1_5_2_llt_matchLength) {
            matchBytes += 0x10000;
        }
    }
    return matchBytes;
}

/* Derives the seqStore that is a chunk of the originalSeqStore from [startIdx, endIdx).
 * Stores the result in resultSeqStore.
 */
static void ZSTD_v1_5_2_deriveSeqStoreChunk(seqStore_t* resultSeqStore,
                               const seqStore_t* originalSeqStore,
                                     size_t startIdx, size_t endIdx) {
    BYTE* const litEnd = originalSeqStore->lit;
    size_t literalsBytes;
    size_t literalsBytesPreceding = 0;

    *resultSeqStore = *originalSeqStore;
    if (startIdx > 0) {
        resultSeqStore->sequences = originalSeqStore->sequencesStart + startIdx;
        literalsBytesPreceding = ZSTD_v1_5_2_countSeqStoreLiteralsBytes(resultSeqStore);
    }

    /* Move longLengthPos into the correct position if necessary */
    if (originalSeqStore->longLengthType != ZSTD_v1_5_2_llt_none) {
        if (originalSeqStore->longLengthPos < startIdx || originalSeqStore->longLengthPos > endIdx) {
            resultSeqStore->longLengthType = ZSTD_v1_5_2_llt_none;
        } else {
            resultSeqStore->longLengthPos -= (U32)startIdx;
        }
    }
    resultSeqStore->sequencesStart = originalSeqStore->sequencesStart + startIdx;
    resultSeqStore->sequences = originalSeqStore->sequencesStart + endIdx;
    literalsBytes = ZSTD_v1_5_2_countSeqStoreLiteralsBytes(resultSeqStore);
    resultSeqStore->litStart += literalsBytesPreceding;
    if (endIdx == (size_t)(originalSeqStore->sequences - originalSeqStore->sequencesStart)) {
        /* This accounts for possible last literals if the derived chunk reaches the end of the block */
        resultSeqStore->lit = litEnd;
    } else {
        resultSeqStore->lit = resultSeqStore->litStart+literalsBytes;
    }
    resultSeqStore->llCode += startIdx;
    resultSeqStore->mlCode += startIdx;
    resultSeqStore->ofCode += startIdx;
}

/**
 * Returns the raw offset represented by the combination of offCode, ll0, and repcode history.
 * offCode must represent a repcode in the numeric representation of ZSTD_v1_5_2_storeSeq().
 */
static U32
ZSTD_v1_5_2_resolveRepcodeToRawOffset(const U32 rep[ZSTD_v1_5_2_REP_NUM], const U32 offCode, const U32 ll0)
{
    U32 const adjustedOffCode = STORED_REPCODE(offCode) - 1 + ll0;  /* [ 0 - 3 ] */
    assert(STORED_IS_REPCODE(offCode));
    if (adjustedOffCode == ZSTD_v1_5_2_REP_NUM) {
        /* litlength == 0 and offCode == 2 implies selection of first repcode - 1 */
        assert(rep[0] > 0);
        return rep[0] - 1;
    }
    return rep[adjustedOffCode];
}

/**
 * ZSTD_v1_5_2_seqStore_resolveOffCodes() reconciles any possible divergences in offset history that may arise
 * due to emission of RLE/raw blocks that disturb the offset history,
 * and replaces any repcodes within the seqStore that may be invalid.
 *
 * dRepcodes are updated as would be on the decompression side.
 * cRepcodes are updated exactly in accordance with the seqStore.
 *
 * Note : this function assumes seq->offBase respects the following numbering scheme :
 *        0 : invalid
 *        1-3 : repcode 1-3
 *        4+ : real_offset+3
 */
static void ZSTD_v1_5_2_seqStore_resolveOffCodes(repcodes_t* const dRepcodes, repcodes_t* const cRepcodes,
                                          seqStore_t* const seqStore, U32 const nbSeq) {
    U32 idx = 0;
    for (; idx < nbSeq; ++idx) {
        seqDef* const seq = seqStore->sequencesStart + idx;
        U32 const ll0 = (seq->litLength == 0);
        U32 const offCode = OFFBASE_TO_STORED(seq->offBase);
        assert(seq->offBase > 0);
        if (STORED_IS_REPCODE(offCode)) {
            U32 const dRawOffset = ZSTD_v1_5_2_resolveRepcodeToRawOffset(dRepcodes->rep, offCode, ll0);
            U32 const cRawOffset = ZSTD_v1_5_2_resolveRepcodeToRawOffset(cRepcodes->rep, offCode, ll0);
            /* Adjust simulated decompression repcode history if we come across a mismatch. Replace
             * the repcode with the offset it actually references, determined by the compression
             * repcode history.
             */
            if (dRawOffset != cRawOffset) {
                seq->offBase = cRawOffset + ZSTD_v1_5_2_REP_NUM;
            }
        }
        /* Compression repcode history is always updated with values directly from the unmodified seqStore.
         * Decompression repcode history may use modified seq->offset value taken from compression repcode history.
         */
        ZSTD_v1_5_2_updateRep(dRepcodes->rep, OFFBASE_TO_STORED(seq->offBase), ll0);
        ZSTD_v1_5_2_updateRep(cRepcodes->rep, offCode, ll0);
    }
}

/* ZSTD_v1_5_2_compressSeqStore_singleBlock():
 * Compresses a seqStore into a block with a block header, into the buffer dst.
 *
 * Returns the total size of that block (including header) or a ZSTD error code.
 */
static size_t
ZSTD_v1_5_2_compressSeqStore_singleBlock(ZSTD_v1_5_2_CCtx* zc, seqStore_t* const seqStore,
                                  repcodes_t* const dRep, repcodes_t* const cRep,
                                  void* dst, size_t dstCapacity,
                                  const void* src, size_t srcSize,
                                  U32 lastBlock, U32 isPartition)
{
    const U32 rleMaxLength = 25;
    BYTE* op = (BYTE*)dst;
    const BYTE* ip = (const BYTE*)src;
    size_t cSize;
    size_t cSeqsSize;

    /* In case of an RLE or raw block, the simulated decompression repcode history must be reset */
    repcodes_t const dRepOriginal = *dRep;
    DEBUGLOG(5, "ZSTD_v1_5_2_compressSeqStore_singleBlock");
    if (isPartition)
        ZSTD_v1_5_2_seqStore_resolveOffCodes(dRep, cRep, seqStore, (U32)(seqStore->sequences - seqStore->sequencesStart));

    RETURN_ERROR_IF(dstCapacity < ZSTD_v1_5_2_blockHeaderSize, dstSize_tooSmall, "Block header doesn't fit");
    cSeqsSize = ZSTD_v1_5_2_entropyCompressSeqStore(seqStore,
                &zc->blockState.prevCBlock->entropy, &zc->blockState.nextCBlock->entropy,
                &zc->appliedParams,
                op + ZSTD_v1_5_2_blockHeaderSize, dstCapacity - ZSTD_v1_5_2_blockHeaderSize,
                srcSize,
                zc->entropyWorkspace, ENTROPY_WORKSPACE_SIZE /* statically allocated in resetCCtx */,
                zc->bmi2);
    FORWARD_IF_ERROR(cSeqsSize, "ZSTD_v1_5_2_entropyCompressSeqStore failed!");

    if (!zc->isFirstBlock &&
        cSeqsSize < rleMaxLength &&
        ZSTD_v1_5_2_isRLE((BYTE const*)src, srcSize)) {
        /* We don't want to emit our first block as a RLE even if it qualifies because
        * doing so will cause the decoder (cli only) to throw a "should consume all input error."
        * This is only an issue for zstd <= v1.4.3
        */
        cSeqsSize = 1;
    }

    if (zc->seqCollector.collectSequences) {
        ZSTD_v1_5_2_copyBlockSequences(zc);
        ZSTD_v1_5_2_blockState_confirmRepcodesAndEntropyTables(&zc->blockState);
        return 0;
    }

    if (cSeqsSize == 0) {
        cSize = ZSTD_v1_5_2_noCompressBlock(op, dstCapacity, ip, srcSize, lastBlock);
        FORWARD_IF_ERROR(cSize, "Nocompress block failed");
        DEBUGLOG(4, "Writing out nocompress block, size: %zu", cSize);
        *dRep = dRepOriginal; /* reset simulated decompression repcode history */
    } else if (cSeqsSize == 1) {
        cSize = ZSTD_v1_5_2_rleCompressBlock(op, dstCapacity, *ip, srcSize, lastBlock);
        FORWARD_IF_ERROR(cSize, "RLE compress block failed");
        DEBUGLOG(4, "Writing out RLE block, size: %zu", cSize);
        *dRep = dRepOriginal; /* reset simulated decompression repcode history */
    } else {
        ZSTD_v1_5_2_blockState_confirmRepcodesAndEntropyTables(&zc->blockState);
        writeBlockHeader(op, cSeqsSize, srcSize, lastBlock);
        cSize = ZSTD_v1_5_2_blockHeaderSize + cSeqsSize;
        DEBUGLOG(4, "Writing out compressed block, size: %zu", cSize);
    }

    if (zc->blockState.prevCBlock->entropy.fse.offcode_repeatMode == FSE_v1_5_2_repeat_valid)
        zc->blockState.prevCBlock->entropy.fse.offcode_repeatMode = FSE_v1_5_2_repeat_check;

    return cSize;
}

/* Struct to keep track of where we are in our recursive calls. */
typedef struct {
    U32* splitLocations;    /* Array of split indices */
    size_t idx;             /* The current index within splitLocations being worked on */
} seqStoreSplits;

#define MIN_SEQUENCES_BLOCK_SPLITTING 300

/* Helper function to perform the recursive search for block splits.
 * Estimates the cost of seqStore prior to split, and estimates the cost of splitting the sequences in half.
 * If advantageous to split, then we recurse down the two sub-blocks. If not, or if an error occurred in estimation, then
 * we do not recurse.
 *
 * Note: The recursion depth is capped by a heuristic minimum number of sequences, defined by MIN_SEQUENCES_BLOCK_SPLITTING.
 * In theory, this means the absolute largest recursion depth is 10 == log2(maxNbSeqInBlock/MIN_SEQUENCES_BLOCK_SPLITTING).
 * In practice, recursion depth usually doesn't go beyond 4.
 *
 * Furthermore, the number of splits is capped by ZSTD_v1_5_2_MAX_NB_BLOCK_SPLITS. At ZSTD_v1_5_2_MAX_NB_BLOCK_SPLITS == 196 with the current existing blockSize
 * maximum of 128 KB, this value is actually impossible to reach.
 */
static void
ZSTD_v1_5_2_deriveBlockSplitsHelper(seqStoreSplits* splits, size_t startIdx, size_t endIdx,
                             ZSTD_v1_5_2_CCtx* zc, const seqStore_t* origSeqStore)
{
    seqStore_t* fullSeqStoreChunk = &zc->blockSplitCtx.fullSeqStoreChunk;
    seqStore_t* firstHalfSeqStore = &zc->blockSplitCtx.firstHalfSeqStore;
    seqStore_t* secondHalfSeqStore = &zc->blockSplitCtx.secondHalfSeqStore;
    size_t estimatedOriginalSize;
    size_t estimatedFirstHalfSize;
    size_t estimatedSecondHalfSize;
    size_t midIdx = (startIdx + endIdx)/2;

    if (endIdx - startIdx < MIN_SEQUENCES_BLOCK_SPLITTING || splits->idx >= ZSTD_v1_5_2_MAX_NB_BLOCK_SPLITS) {
        DEBUGLOG(6, "ZSTD_v1_5_2_deriveBlockSplitsHelper: Too few sequences");
        return;
    }
    DEBUGLOG(4, "ZSTD_v1_5_2_deriveBlockSplitsHelper: startIdx=%zu endIdx=%zu", startIdx, endIdx);
    ZSTD_v1_5_2_deriveSeqStoreChunk(fullSeqStoreChunk, origSeqStore, startIdx, endIdx);
    ZSTD_v1_5_2_deriveSeqStoreChunk(firstHalfSeqStore, origSeqStore, startIdx, midIdx);
    ZSTD_v1_5_2_deriveSeqStoreChunk(secondHalfSeqStore, origSeqStore, midIdx, endIdx);
    estimatedOriginalSize = ZSTD_v1_5_2_buildEntropyStatisticsAndEstimateSubBlockSize(fullSeqStoreChunk, zc);
    estimatedFirstHalfSize = ZSTD_v1_5_2_buildEntropyStatisticsAndEstimateSubBlockSize(firstHalfSeqStore, zc);
    estimatedSecondHalfSize = ZSTD_v1_5_2_buildEntropyStatisticsAndEstimateSubBlockSize(secondHalfSeqStore, zc);
    DEBUGLOG(4, "Estimated original block size: %zu -- First half split: %zu -- Second half split: %zu",
             estimatedOriginalSize, estimatedFirstHalfSize, estimatedSecondHalfSize);
    if (ZSTD_v1_5_2_isError(estimatedOriginalSize) || ZSTD_v1_5_2_isError(estimatedFirstHalfSize) || ZSTD_v1_5_2_isError(estimatedSecondHalfSize)) {
        return;
    }
    if (estimatedFirstHalfSize + estimatedSecondHalfSize < estimatedOriginalSize) {
        ZSTD_v1_5_2_deriveBlockSplitsHelper(splits, startIdx, midIdx, zc, origSeqStore);
        splits->splitLocations[splits->idx] = (U32)midIdx;
        splits->idx++;
        ZSTD_v1_5_2_deriveBlockSplitsHelper(splits, midIdx, endIdx, zc, origSeqStore);
    }
}

/* Base recursive function. Populates a table with intra-block partition indices that can improve compression ratio.
 *
 * Returns the number of splits made (which equals the size of the partition table - 1).
 */
static size_t ZSTD_v1_5_2_deriveBlockSplits(ZSTD_v1_5_2_CCtx* zc, U32 partitions[], U32 nbSeq) {
    seqStoreSplits splits = {partitions, 0};
    if (nbSeq <= 4) {
        DEBUGLOG(4, "ZSTD_v1_5_2_deriveBlockSplits: Too few sequences to split");
        /* Refuse to try and split anything with less than 4 sequences */
        return 0;
    }
    ZSTD_v1_5_2_deriveBlockSplitsHelper(&splits, 0, nbSeq, zc, &zc->seqStore);
    splits.splitLocations[splits.idx] = nbSeq;
    DEBUGLOG(5, "ZSTD_v1_5_2_deriveBlockSplits: final nb partitions: %zu", splits.idx+1);
    return splits.idx;
}

/* ZSTD_v1_5_2_compressBlock_splitBlock():
 * Attempts to split a given block into multiple blocks to improve compression ratio.
 *
 * Returns combined size of all blocks (which includes headers), or a ZSTD error code.
 */
static size_t
ZSTD_v1_5_2_compressBlock_splitBlock_internal(ZSTD_v1_5_2_CCtx* zc, void* dst, size_t dstCapacity,
                                       const void* src, size_t blockSize, U32 lastBlock, U32 nbSeq)
{
    size_t cSize = 0;
    const BYTE* ip = (const BYTE*)src;
    BYTE* op = (BYTE*)dst;
    size_t i = 0;
    size_t srcBytesTotal = 0;
    U32* partitions = zc->blockSplitCtx.partitions; /* size == ZSTD_v1_5_2_MAX_NB_BLOCK_SPLITS */
    seqStore_t* nextSeqStore = &zc->blockSplitCtx.nextSeqStore;
    seqStore_t* currSeqStore = &zc->blockSplitCtx.currSeqStore;
    size_t numSplits = ZSTD_v1_5_2_deriveBlockSplits(zc, partitions, nbSeq);

    /* If a block is split and some partitions are emitted as RLE/uncompressed, then repcode history
     * may become invalid. In order to reconcile potentially invalid repcodes, we keep track of two
     * separate repcode histories that simulate repcode history on compression and decompression side,
     * and use the histories to determine whether we must replace a particular repcode with its raw offset.
     *
     * 1) cRep gets updated for each partition, regardless of whether the block was emitted as uncompressed
     *    or RLE. This allows us to retrieve the offset value that an invalid repcode references within
     *    a nocompress/RLE block.
     * 2) dRep gets updated only for compressed partitions, and when a repcode gets replaced, will use
     *    the replacement offset value rather than the original repcode to update the repcode history.
     *    dRep also will be the final repcode history sent to the next block.
     *
     * See ZSTD_v1_5_2_seqStore_resolveOffCodes() for more details.
     */
    repcodes_t dRep;
    repcodes_t cRep;
    ZSTD_v1_5_2_memcpy(dRep.rep, zc->blockState.prevCBlock->rep, sizeof(repcodes_t));
    ZSTD_v1_5_2_memcpy(cRep.rep, zc->blockState.prevCBlock->rep, sizeof(repcodes_t));
    ZSTD_v1_5_2_memset(nextSeqStore, 0, sizeof(seqStore_t));

    DEBUGLOG(4, "ZSTD_v1_5_2_compressBlock_splitBlock_internal (dstCapacity=%u, dictLimit=%u, nextToUpdate=%u)",
                (unsigned)dstCapacity, (unsigned)zc->blockState.matchState.window.dictLimit,
                (unsigned)zc->blockState.matchState.nextToUpdate);

    if (numSplits == 0) {
        size_t cSizeSingleBlock = ZSTD_v1_5_2_compressSeqStore_singleBlock(zc, &zc->seqStore,
                                                                   &dRep, &cRep,
                                                                    op, dstCapacity,
                                                                    ip, blockSize,
                                                                    lastBlock, 0 /* isPartition */);
        FORWARD_IF_ERROR(cSizeSingleBlock, "Compressing single block from splitBlock_internal() failed!");
        DEBUGLOG(5, "ZSTD_v1_5_2_compressBlock_splitBlock_internal: No splits");
        assert(cSizeSingleBlock <= ZSTD_v1_5_2_BLOCKSIZE_MAX + ZSTD_v1_5_2_blockHeaderSize);
        return cSizeSingleBlock;
    }

    ZSTD_v1_5_2_deriveSeqStoreChunk(currSeqStore, &zc->seqStore, 0, partitions[0]);
    for (i = 0; i <= numSplits; ++i) {
        size_t srcBytes;
        size_t cSizeChunk;
        U32 const lastPartition = (i == numSplits);
        U32 lastBlockEntireSrc = 0;

        srcBytes = ZSTD_v1_5_2_countSeqStoreLiteralsBytes(currSeqStore) + ZSTD_v1_5_2_countSeqStoreMatchBytes(currSeqStore);
        srcBytesTotal += srcBytes;
        if (lastPartition) {
            /* This is the final partition, need to account for possible last literals */
            srcBytes += blockSize - srcBytesTotal;
            lastBlockEntireSrc = lastBlock;
        } else {
            ZSTD_v1_5_2_deriveSeqStoreChunk(nextSeqStore, &zc->seqStore, partitions[i], partitions[i+1]);
        }

        cSizeChunk = ZSTD_v1_5_2_compressSeqStore_singleBlock(zc, currSeqStore,
                                                      &dRep, &cRep,
                                                       op, dstCapacity,
                                                       ip, srcBytes,
                                                       lastBlockEntireSrc, 1 /* isPartition */);
        DEBUGLOG(5, "Estimated size: %zu actual size: %zu", ZSTD_v1_5_2_buildEntropyStatisticsAndEstimateSubBlockSize(currSeqStore, zc), cSizeChunk);
        FORWARD_IF_ERROR(cSizeChunk, "Compressing chunk failed!");

        ip += srcBytes;
        op += cSizeChunk;
        dstCapacity -= cSizeChunk;
        cSize += cSizeChunk;
        *currSeqStore = *nextSeqStore;
        assert(cSizeChunk <= ZSTD_v1_5_2_BLOCKSIZE_MAX + ZSTD_v1_5_2_blockHeaderSize);
    }
    /* cRep and dRep may have diverged during the compression. If so, we use the dRep repcodes
     * for the next block.
     */
    ZSTD_v1_5_2_memcpy(zc->blockState.prevCBlock->rep, dRep.rep, sizeof(repcodes_t));
    return cSize;
}

static size_t
ZSTD_v1_5_2_compressBlock_splitBlock(ZSTD_v1_5_2_CCtx* zc,
                              void* dst, size_t dstCapacity,
                              const void* src, size_t srcSize, U32 lastBlock)
{
    const BYTE* ip = (const BYTE*)src;
    BYTE* op = (BYTE*)dst;
    U32 nbSeq;
    size_t cSize;
    DEBUGLOG(4, "ZSTD_v1_5_2_compressBlock_splitBlock");
    assert(zc->appliedParams.useBlockSplitter == ZSTD_v1_5_2_ps_enable);

    {   const size_t bss = ZSTD_v1_5_2_buildSeqStore(zc, src, srcSize);
        FORWARD_IF_ERROR(bss, "ZSTD_v1_5_2_buildSeqStore failed");
        if (bss == ZSTDbss_noCompress) {
            if (zc->blockState.prevCBlock->entropy.fse.offcode_repeatMode == FSE_v1_5_2_repeat_valid)
                zc->blockState.prevCBlock->entropy.fse.offcode_repeatMode = FSE_v1_5_2_repeat_check;
            cSize = ZSTD_v1_5_2_noCompressBlock(op, dstCapacity, ip, srcSize, lastBlock);
            FORWARD_IF_ERROR(cSize, "ZSTD_v1_5_2_noCompressBlock failed");
            DEBUGLOG(4, "ZSTD_v1_5_2_compressBlock_splitBlock: Nocompress block");
            return cSize;
        }
        nbSeq = (U32)(zc->seqStore.sequences - zc->seqStore.sequencesStart);
    }

    cSize = ZSTD_v1_5_2_compressBlock_splitBlock_internal(zc, dst, dstCapacity, src, srcSize, lastBlock, nbSeq);
    FORWARD_IF_ERROR(cSize, "Splitting blocks failed!");
    return cSize;
}

static size_t
ZSTD_v1_5_2_compressBlock_internal(ZSTD_v1_5_2_CCtx* zc,
                            void* dst, size_t dstCapacity,
                            const void* src, size_t srcSize, U32 frame)
{
    /* This the upper bound for the length of an rle block.
     * This isn't the actual upper bound. Finding the real threshold
     * needs further investigation.
     */
    const U32 rleMaxLength = 25;
    size_t cSize;
    const BYTE* ip = (const BYTE*)src;
    BYTE* op = (BYTE*)dst;
    DEBUGLOG(5, "ZSTD_v1_5_2_compressBlock_internal (dstCapacity=%u, dictLimit=%u, nextToUpdate=%u)",
                (unsigned)dstCapacity, (unsigned)zc->blockState.matchState.window.dictLimit,
                (unsigned)zc->blockState.matchState.nextToUpdate);

    {   const size_t bss = ZSTD_v1_5_2_buildSeqStore(zc, src, srcSize);
        FORWARD_IF_ERROR(bss, "ZSTD_v1_5_2_buildSeqStore failed");
        if (bss == ZSTDbss_noCompress) { cSize = 0; goto out; }
    }

    if (zc->seqCollector.collectSequences) {
        ZSTD_v1_5_2_copyBlockSequences(zc);
        ZSTD_v1_5_2_blockState_confirmRepcodesAndEntropyTables(&zc->blockState);
        return 0;
    }

    /* encode sequences and literals */
    cSize = ZSTD_v1_5_2_entropyCompressSeqStore(&zc->seqStore,
            &zc->blockState.prevCBlock->entropy, &zc->blockState.nextCBlock->entropy,
            &zc->appliedParams,
            dst, dstCapacity,
            srcSize,
            zc->entropyWorkspace, ENTROPY_WORKSPACE_SIZE /* statically allocated in resetCCtx */,
            zc->bmi2);

    if (frame &&
        /* We don't want to emit our first block as a RLE even if it qualifies because
         * doing so will cause the decoder (cli only) to throw a "should consume all input error."
         * This is only an issue for zstd <= v1.4.3
         */
        !zc->isFirstBlock &&
        cSize < rleMaxLength &&
        ZSTD_v1_5_2_isRLE(ip, srcSize))
    {
        cSize = 1;
        op[0] = ip[0];
    }

out:
    if (!ZSTD_v1_5_2_isError(cSize) && cSize > 1) {
        ZSTD_v1_5_2_blockState_confirmRepcodesAndEntropyTables(&zc->blockState);
    }
    /* We check that dictionaries have offset codes available for the first
     * block. After the first block, the offcode table might not have large
     * enough codes to represent the offsets in the data.
     */
    if (zc->blockState.prevCBlock->entropy.fse.offcode_repeatMode == FSE_v1_5_2_repeat_valid)
        zc->blockState.prevCBlock->entropy.fse.offcode_repeatMode = FSE_v1_5_2_repeat_check;

    return cSize;
}

static size_t ZSTD_v1_5_2_compressBlock_targetCBlockSize_body(ZSTD_v1_5_2_CCtx* zc,
                               void* dst, size_t dstCapacity,
                               const void* src, size_t srcSize,
                               const size_t bss, U32 lastBlock)
{
    DEBUGLOG(6, "Attempting ZSTD_v1_5_2_compressSuperBlock()");
    if (bss == ZSTDbss_compress) {
        if (/* We don't want to emit our first block as a RLE even if it qualifies because
            * doing so will cause the decoder (cli only) to throw a "should consume all input error."
            * This is only an issue for zstd <= v1.4.3
            */
            !zc->isFirstBlock &&
            ZSTD_v1_5_2_maybeRLE(&zc->seqStore) &&
            ZSTD_v1_5_2_isRLE((BYTE const*)src, srcSize))
        {
            return ZSTD_v1_5_2_rleCompressBlock(dst, dstCapacity, *(BYTE const*)src, srcSize, lastBlock);
        }
        /* Attempt superblock compression.
         *
         * Note that compressed size of ZSTD_v1_5_2_compressSuperBlock() is not bound by the
         * standard ZSTD_v1_5_2_compressBound(). This is a problem, because even if we have
         * space now, taking an extra byte now could cause us to run out of space later
         * and violate ZSTD_v1_5_2_compressBound().
         *
         * Define blockBound(blockSize) = blockSize + ZSTD_v1_5_2_blockHeaderSize.
         *
         * In order to respect ZSTD_v1_5_2_compressBound() we must attempt to emit a raw
         * uncompressed block in these cases:
         *   * cSize == 0: Return code for an uncompressed block.
         *   * cSize == dstSize_tooSmall: We may have expanded beyond blockBound(srcSize).
         *     ZSTD_v1_5_2_noCompressBlock() will return dstSize_tooSmall if we are really out of
         *     output space.
         *   * cSize >= blockBound(srcSize): We have expanded the block too much so
         *     emit an uncompressed block.
         */
        {
            size_t const cSize = ZSTD_v1_5_2_compressSuperBlock(zc, dst, dstCapacity, src, srcSize, lastBlock);
            if (cSize != ERROR(dstSize_tooSmall)) {
                size_t const maxCSize = srcSize - ZSTD_v1_5_2_minGain(srcSize, zc->appliedParams.cParams.strategy);
                FORWARD_IF_ERROR(cSize, "ZSTD_v1_5_2_compressSuperBlock failed");
                if (cSize != 0 && cSize < maxCSize + ZSTD_v1_5_2_blockHeaderSize) {
                    ZSTD_v1_5_2_blockState_confirmRepcodesAndEntropyTables(&zc->blockState);
                    return cSize;
                }
            }
        }
    }

    DEBUGLOG(6, "Resorting to ZSTD_v1_5_2_noCompressBlock()");
    /* Superblock compression failed, attempt to emit a single no compress block.
     * The decoder will be able to stream this block since it is uncompressed.
     */
    return ZSTD_v1_5_2_noCompressBlock(dst, dstCapacity, src, srcSize, lastBlock);
}

static size_t ZSTD_v1_5_2_compressBlock_targetCBlockSize(ZSTD_v1_5_2_CCtx* zc,
                               void* dst, size_t dstCapacity,
                               const void* src, size_t srcSize,
                               U32 lastBlock)
{
    size_t cSize = 0;
    const size_t bss = ZSTD_v1_5_2_buildSeqStore(zc, src, srcSize);
    DEBUGLOG(5, "ZSTD_v1_5_2_compressBlock_targetCBlockSize (dstCapacity=%u, dictLimit=%u, nextToUpdate=%u, srcSize=%zu)",
                (unsigned)dstCapacity, (unsigned)zc->blockState.matchState.window.dictLimit, (unsigned)zc->blockState.matchState.nextToUpdate, srcSize);
    FORWARD_IF_ERROR(bss, "ZSTD_v1_5_2_buildSeqStore failed");

    cSize = ZSTD_v1_5_2_compressBlock_targetCBlockSize_body(zc, dst, dstCapacity, src, srcSize, bss, lastBlock);
    FORWARD_IF_ERROR(cSize, "ZSTD_v1_5_2_compressBlock_targetCBlockSize_body failed");

    if (zc->blockState.prevCBlock->entropy.fse.offcode_repeatMode == FSE_v1_5_2_repeat_valid)
        zc->blockState.prevCBlock->entropy.fse.offcode_repeatMode = FSE_v1_5_2_repeat_check;

    return cSize;
}

static void ZSTD_v1_5_2_overflowCorrectIfNeeded(ZSTD_v1_5_2_matchState_t* ms,
                                         ZSTD_v1_5_2_cwksp* ws,
                                         ZSTD_v1_5_2_CCtx_params const* params,
                                         void const* ip,
                                         void const* iend)
{
    U32 const cycleLog = ZSTD_v1_5_2_cycleLog(params->cParams.chainLog, params->cParams.strategy);
    U32 const maxDist = (U32)1 << params->cParams.windowLog;
    if (ZSTD_v1_5_2_window_needOverflowCorrection(ms->window, cycleLog, maxDist, ms->loadedDictEnd, ip, iend)) {
        U32 const correction = ZSTD_v1_5_2_window_correctOverflow(&ms->window, cycleLog, maxDist, ip);
        ZSTD_v1_5_2_STATIC_ASSERT(ZSTD_v1_5_2_CHAINLOG_MAX <= 30);
        ZSTD_v1_5_2_STATIC_ASSERT(ZSTD_v1_5_2_WINDOWLOG_MAX_32 <= 30);
        ZSTD_v1_5_2_STATIC_ASSERT(ZSTD_v1_5_2_WINDOWLOG_MAX <= 31);
        ZSTD_v1_5_2_cwksp_mark_tables_dirty(ws);
        ZSTD_v1_5_2_reduceIndex(ms, params, correction);
        ZSTD_v1_5_2_cwksp_mark_tables_clean(ws);
        if (ms->nextToUpdate < correction) ms->nextToUpdate = 0;
        else ms->nextToUpdate -= correction;
        /* invalidate dictionaries on overflow correction */
        ms->loadedDictEnd = 0;
        ms->dictMatchState = NULL;
    }
}

/*! ZSTD_v1_5_2_compress_frameChunk() :
*   Compress a chunk of data into one or multiple blocks.
*   All blocks will be terminated, all input will be consumed.
*   Function will issue an error if there is not enough `dstCapacity` to hold the compressed content.
*   Frame is supposed already started (header already produced)
*   @return : compressed size, or an error code
*/
static size_t ZSTD_v1_5_2_compress_frameChunk(ZSTD_v1_5_2_CCtx* cctx,
                                     void* dst, size_t dstCapacity,
                               const void* src, size_t srcSize,
                                     U32 lastFrameChunk)
{
    size_t blockSize = cctx->blockSize;
    size_t remaining = srcSize;
    const BYTE* ip = (const BYTE*)src;
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;
    U32 const maxDist = (U32)1 << cctx->appliedParams.cParams.windowLog;

    assert(cctx->appliedParams.cParams.windowLog <= ZSTD_v1_5_2_WINDOWLOG_MAX);

    DEBUGLOG(4, "ZSTD_v1_5_2_compress_frameChunk (blockSize=%u)", (unsigned)blockSize);
    if (cctx->appliedParams.fParams.checksumFlag && srcSize)
        XXH64_update(&cctx->xxhState, src, srcSize);

    while (remaining) {
        ZSTD_v1_5_2_matchState_t* const ms = &cctx->blockState.matchState;
        U32 const lastBlock = lastFrameChunk & (blockSize >= remaining);

        RETURN_ERROR_IF(dstCapacity < ZSTD_v1_5_2_blockHeaderSize + MIN_CBLOCK_SIZE,
                        dstSize_tooSmall,
                        "not enough space to store compressed block");
        if (remaining < blockSize) blockSize = remaining;

        ZSTD_v1_5_2_overflowCorrectIfNeeded(
            ms, &cctx->workspace, &cctx->appliedParams, ip, ip + blockSize);
        ZSTD_v1_5_2_checkDictValidity(&ms->window, ip + blockSize, maxDist, &ms->loadedDictEnd, &ms->dictMatchState);
        ZSTD_v1_5_2_window_enforceMaxDist(&ms->window, ip, maxDist, &ms->loadedDictEnd, &ms->dictMatchState);

        /* Ensure hash/chain table insertion resumes no sooner than lowlimit */
        if (ms->nextToUpdate < ms->window.lowLimit) ms->nextToUpdate = ms->window.lowLimit;

        {   size_t cSize;
            if (ZSTD_v1_5_2_useTargetCBlockSize(&cctx->appliedParams)) {
                cSize = ZSTD_v1_5_2_compressBlock_targetCBlockSize(cctx, op, dstCapacity, ip, blockSize, lastBlock);
                FORWARD_IF_ERROR(cSize, "ZSTD_v1_5_2_compressBlock_targetCBlockSize failed");
                assert(cSize > 0);
                assert(cSize <= blockSize + ZSTD_v1_5_2_blockHeaderSize);
            } else if (ZSTD_v1_5_2_blockSplitterEnabled(&cctx->appliedParams)) {
                cSize = ZSTD_v1_5_2_compressBlock_splitBlock(cctx, op, dstCapacity, ip, blockSize, lastBlock);
                FORWARD_IF_ERROR(cSize, "ZSTD_v1_5_2_compressBlock_splitBlock failed");
                assert(cSize > 0 || cctx->seqCollector.collectSequences == 1);
            } else {
                cSize = ZSTD_v1_5_2_compressBlock_internal(cctx,
                                        op+ZSTD_v1_5_2_blockHeaderSize, dstCapacity-ZSTD_v1_5_2_blockHeaderSize,
                                        ip, blockSize, 1 /* frame */);
                FORWARD_IF_ERROR(cSize, "ZSTD_v1_5_2_compressBlock_internal failed");

                if (cSize == 0) {  /* block is not compressible */
                    cSize = ZSTD_v1_5_2_noCompressBlock(op, dstCapacity, ip, blockSize, lastBlock);
                    FORWARD_IF_ERROR(cSize, "ZSTD_v1_5_2_noCompressBlock failed");
                } else {
                    U32 const cBlockHeader = cSize == 1 ?
                        lastBlock + (((U32)bt_rle)<<1) + (U32)(blockSize << 3) :
                        lastBlock + (((U32)bt_compressed)<<1) + (U32)(cSize << 3);
                    MEM_writeLE24(op, cBlockHeader);
                    cSize += ZSTD_v1_5_2_blockHeaderSize;
                }
            }


            ip += blockSize;
            assert(remaining >= blockSize);
            remaining -= blockSize;
            op += cSize;
            assert(dstCapacity >= cSize);
            dstCapacity -= cSize;
            cctx->isFirstBlock = 0;
            DEBUGLOG(5, "ZSTD_v1_5_2_compress_frameChunk: adding a block of size %u",
                        (unsigned)cSize);
    }   }

    if (lastFrameChunk && (op>ostart)) cctx->stage = ZSTDcs_ending;
    return (size_t)(op-ostart);
}


static size_t ZSTD_v1_5_2_writeFrameHeader(void* dst, size_t dstCapacity,
                                    const ZSTD_v1_5_2_CCtx_params* params, U64 pledgedSrcSize, U32 dictID)
{   BYTE* const op = (BYTE*)dst;
    U32   const dictIDSizeCodeLength = (dictID>0) + (dictID>=256) + (dictID>=65536);   /* 0-3 */
    U32   const dictIDSizeCode = params->fParams.noDictIDFlag ? 0 : dictIDSizeCodeLength;   /* 0-3 */
    U32   const checksumFlag = params->fParams.checksumFlag>0;
    U32   const windowSize = (U32)1 << params->cParams.windowLog;
    U32   const singleSegment = params->fParams.contentSizeFlag && (windowSize >= pledgedSrcSize);
    BYTE  const windowLogByte = (BYTE)((params->cParams.windowLog - ZSTD_v1_5_2_WINDOWLOG_ABSOLUTEMIN) << 3);
    U32   const fcsCode = params->fParams.contentSizeFlag ?
                     (pledgedSrcSize>=256) + (pledgedSrcSize>=65536+256) + (pledgedSrcSize>=0xFFFFFFFFU) : 0;  /* 0-3 */
    BYTE  const frameHeaderDescriptionByte = (BYTE)(dictIDSizeCode + (checksumFlag<<2) + (singleSegment<<5) + (fcsCode<<6) );
    size_t pos=0;

    assert(!(params->fParams.contentSizeFlag && pledgedSrcSize == ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN));
    RETURN_ERROR_IF(dstCapacity < ZSTD_v1_5_2_FRAMEHEADERSIZE_MAX, dstSize_tooSmall,
                    "dst buf is too small to fit worst-case frame header size.");
    DEBUGLOG(4, "ZSTD_v1_5_2_writeFrameHeader : dictIDFlag : %u ; dictID : %u ; dictIDSizeCode : %u",
                !params->fParams.noDictIDFlag, (unsigned)dictID, (unsigned)dictIDSizeCode);
    if (params->format == ZSTD_v1_5_2_f_zstd1) {
        MEM_writeLE32(dst, ZSTD_v1_5_2_MAGICNUMBER);
        pos = 4;
    }
    op[pos++] = frameHeaderDescriptionByte;
    if (!singleSegment) op[pos++] = windowLogByte;
    switch(dictIDSizeCode)
    {
        default:
            assert(0); /* impossible */
            ZSTD_v1_5_2_FALLTHROUGH;
        case 0 : break;
        case 1 : op[pos] = (BYTE)(dictID); pos++; break;
        case 2 : MEM_writeLE16(op+pos, (U16)dictID); pos+=2; break;
        case 3 : MEM_writeLE32(op+pos, dictID); pos+=4; break;
    }
    switch(fcsCode)
    {
        default:
            assert(0); /* impossible */
            ZSTD_v1_5_2_FALLTHROUGH;
        case 0 : if (singleSegment) op[pos++] = (BYTE)(pledgedSrcSize); break;
        case 1 : MEM_writeLE16(op+pos, (U16)(pledgedSrcSize-256)); pos+=2; break;
        case 2 : MEM_writeLE32(op+pos, (U32)(pledgedSrcSize)); pos+=4; break;
        case 3 : MEM_writeLE64(op+pos, (U64)(pledgedSrcSize)); pos+=8; break;
    }
    return pos;
}

/* ZSTD_v1_5_2_writeSkippableFrame_advanced() :
 * Writes out a skippable frame with the specified magic number variant (16 are supported),
 * from ZSTD_v1_5_2_MAGIC_SKIPPABLE_START to ZSTD_v1_5_2_MAGIC_SKIPPABLE_START+15, and the desired source data.
 *
 * Returns the total number of bytes written, or a ZSTD error code.
 */
size_t ZSTD_v1_5_2_writeSkippableFrame(void* dst, size_t dstCapacity,
                                const void* src, size_t srcSize, unsigned magicVariant) {
    BYTE* op = (BYTE*)dst;
    RETURN_ERROR_IF(dstCapacity < srcSize + ZSTD_v1_5_2_SKIPPABLEHEADERSIZE /* Skippable frame overhead */,
                    dstSize_tooSmall, "Not enough room for skippable frame");
    RETURN_ERROR_IF(srcSize > (unsigned)0xFFFFFFFF, srcSize_wrong, "Src size too large for skippable frame");
    RETURN_ERROR_IF(magicVariant > 15, parameter_outOfBound, "Skippable frame magic number variant not supported");

    MEM_writeLE32(op, (U32)(ZSTD_v1_5_2_MAGIC_SKIPPABLE_START + magicVariant));
    MEM_writeLE32(op+4, (U32)srcSize);
    ZSTD_v1_5_2_memcpy(op+8, src, srcSize);
    return srcSize + ZSTD_v1_5_2_SKIPPABLEHEADERSIZE;
}

/* ZSTD_v1_5_2_writeLastEmptyBlock() :
 * output an empty Block with end-of-frame mark to complete a frame
 * @return : size of data written into `dst` (== ZSTD_v1_5_2_blockHeaderSize (defined in ZSTD_internal.h))
 *           or an error code if `dstCapacity` is too small (<ZSTD_v1_5_2_blockHeaderSize)
 */
size_t ZSTD_v1_5_2_writeLastEmptyBlock(void* dst, size_t dstCapacity)
{
    RETURN_ERROR_IF(dstCapacity < ZSTD_v1_5_2_blockHeaderSize, dstSize_tooSmall,
                    "dst buf is too small to write frame trailer empty block.");
    {   U32 const cBlockHeader24 = 1 /*lastBlock*/ + (((U32)bt_raw)<<1);  /* 0 size */
        MEM_writeLE24(dst, cBlockHeader24);
        return ZSTD_v1_5_2_blockHeaderSize;
    }
}

size_t ZSTD_v1_5_2_referenceExternalSequences(ZSTD_v1_5_2_CCtx* cctx, rawSeq* seq, size_t nbSeq)
{
    RETURN_ERROR_IF(cctx->stage != ZSTDcs_init, stage_wrong,
                    "wrong cctx stage");
    RETURN_ERROR_IF(cctx->appliedParams.ldmParams.enableLdm == ZSTD_v1_5_2_ps_enable,
                    parameter_unsupported,
                    "incompatible with ldm");
    cctx->externSeqStore.seq = seq;
    cctx->externSeqStore.size = nbSeq;
    cctx->externSeqStore.capacity = nbSeq;
    cctx->externSeqStore.pos = 0;
    cctx->externSeqStore.posInSequence = 0;
    return 0;
}


static size_t ZSTD_v1_5_2_compressContinue_internal (ZSTD_v1_5_2_CCtx* cctx,
                              void* dst, size_t dstCapacity,
                        const void* src, size_t srcSize,
                               U32 frame, U32 lastFrameChunk)
{
    ZSTD_v1_5_2_matchState_t* const ms = &cctx->blockState.matchState;
    size_t fhSize = 0;

    DEBUGLOG(5, "ZSTD_v1_5_2_compressContinue_internal, stage: %u, srcSize: %u",
                cctx->stage, (unsigned)srcSize);
    RETURN_ERROR_IF(cctx->stage==ZSTDcs_created, stage_wrong,
                    "missing init (ZSTD_v1_5_2_compressBegin)");

    if (frame && (cctx->stage==ZSTDcs_init)) {
        fhSize = ZSTD_v1_5_2_writeFrameHeader(dst, dstCapacity, &cctx->appliedParams,
                                       cctx->pledgedSrcSizePlusOne-1, cctx->dictID);
        FORWARD_IF_ERROR(fhSize, "ZSTD_v1_5_2_writeFrameHeader failed");
        assert(fhSize <= dstCapacity);
        dstCapacity -= fhSize;
        dst = (char*)dst + fhSize;
        cctx->stage = ZSTDcs_ongoing;
    }

    if (!srcSize) return fhSize;  /* do not generate an empty block if no input */

    if (!ZSTD_v1_5_2_window_update(&ms->window, src, srcSize, ms->forceNonContiguous)) {
        ms->forceNonContiguous = 0;
        ms->nextToUpdate = ms->window.dictLimit;
    }
    if (cctx->appliedParams.ldmParams.enableLdm == ZSTD_v1_5_2_ps_enable) {
        ZSTD_v1_5_2_window_update(&cctx->ldmState.window, src, srcSize, /* forceNonContiguous */ 0);
    }

    if (!frame) {
        /* overflow check and correction for block mode */
        ZSTD_v1_5_2_overflowCorrectIfNeeded(
            ms, &cctx->workspace, &cctx->appliedParams,
            src, (BYTE const*)src + srcSize);
    }

    DEBUGLOG(5, "ZSTD_v1_5_2_compressContinue_internal (blockSize=%u)", (unsigned)cctx->blockSize);
    {   size_t const cSize = frame ?
                             ZSTD_v1_5_2_compress_frameChunk (cctx, dst, dstCapacity, src, srcSize, lastFrameChunk) :
                             ZSTD_v1_5_2_compressBlock_internal (cctx, dst, dstCapacity, src, srcSize, 0 /* frame */);
        FORWARD_IF_ERROR(cSize, "%s", frame ? "ZSTD_v1_5_2_compress_frameChunk failed" : "ZSTD_v1_5_2_compressBlock_internal failed");
        cctx->consumedSrcSize += srcSize;
        cctx->producedCSize += (cSize + fhSize);
        assert(!(cctx->appliedParams.fParams.contentSizeFlag && cctx->pledgedSrcSizePlusOne == 0));
        if (cctx->pledgedSrcSizePlusOne != 0) {  /* control src size */
            ZSTD_v1_5_2_STATIC_ASSERT(ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN == (unsigned long long)-1);
            RETURN_ERROR_IF(
                cctx->consumedSrcSize+1 > cctx->pledgedSrcSizePlusOne,
                srcSize_wrong,
                "error : pledgedSrcSize = %u, while realSrcSize >= %u",
                (unsigned)cctx->pledgedSrcSizePlusOne-1,
                (unsigned)cctx->consumedSrcSize);
        }
        return cSize + fhSize;
    }
}

size_t ZSTD_v1_5_2_compressContinue (ZSTD_v1_5_2_CCtx* cctx,
                              void* dst, size_t dstCapacity,
                        const void* src, size_t srcSize)
{
    DEBUGLOG(5, "ZSTD_v1_5_2_compressContinue (srcSize=%u)", (unsigned)srcSize);
    return ZSTD_v1_5_2_compressContinue_internal(cctx, dst, dstCapacity, src, srcSize, 1 /* frame mode */, 0 /* last chunk */);
}


size_t ZSTD_v1_5_2_getBlockSize(const ZSTD_v1_5_2_CCtx* cctx)
{
    ZSTD_v1_5_2_compressionParameters const cParams = cctx->appliedParams.cParams;
    assert(!ZSTD_v1_5_2_checkCParams(cParams));
    return MIN (ZSTD_v1_5_2_BLOCKSIZE_MAX, (U32)1 << cParams.windowLog);
}

size_t ZSTD_v1_5_2_compressBlock(ZSTD_v1_5_2_CCtx* cctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    DEBUGLOG(5, "ZSTD_v1_5_2_compressBlock: srcSize = %u", (unsigned)srcSize);
    { size_t const blockSizeMax = ZSTD_v1_5_2_getBlockSize(cctx);
      RETURN_ERROR_IF(srcSize > blockSizeMax, srcSize_wrong, "input is larger than a block"); }

    return ZSTD_v1_5_2_compressContinue_internal(cctx, dst, dstCapacity, src, srcSize, 0 /* frame mode */, 0 /* last chunk */);
}

/*! ZSTD_v1_5_2_loadDictionaryContent() :
 *  @return : 0, or an error code
 */
static size_t ZSTD_v1_5_2_loadDictionaryContent(ZSTD_v1_5_2_matchState_t* ms,
                                         ldmState_t* ls,
                                         ZSTD_v1_5_2_cwksp* ws,
                                         ZSTD_v1_5_2_CCtx_params const* params,
                                         const void* src, size_t srcSize,
                                         ZSTD_v1_5_2_dictTableLoadMethod_e dtlm)
{
    const BYTE* ip = (const BYTE*) src;
    const BYTE* const iend = ip + srcSize;
    int const loadLdmDict = params->ldmParams.enableLdm == ZSTD_v1_5_2_ps_enable && ls != NULL;

    /* Assert that we the ms params match the params we're being given */
    ZSTD_v1_5_2_assertEqualCParams(params->cParams, ms->cParams);

    if (srcSize > ZSTD_v1_5_2_CHUNKSIZE_MAX) {
        /* Allow the dictionary to set indices up to exactly ZSTD_v1_5_2_CURRENT_MAX.
         * Dictionaries right at the edge will immediately trigger overflow
         * correction, but I don't want to insert extra constraints here.
         */
        U32 const maxDictSize = ZSTD_v1_5_2_CURRENT_MAX - 1;
        /* We must have cleared our windows when our source is this large. */
        assert(ZSTD_v1_5_2_window_isEmpty(ms->window));
        if (loadLdmDict)
            assert(ZSTD_v1_5_2_window_isEmpty(ls->window));
        /* If the dictionary is too large, only load the suffix of the dictionary. */
        if (srcSize > maxDictSize) {
            ip = iend - maxDictSize;
            src = ip;
            srcSize = maxDictSize;
        }
    }

    DEBUGLOG(4, "ZSTD_v1_5_2_loadDictionaryContent(): useRowMatchFinder=%d", (int)params->useRowMatchFinder);
    ZSTD_v1_5_2_window_update(&ms->window, src, srcSize, /* forceNonContiguous */ 0);
    ms->loadedDictEnd = params->forceWindow ? 0 : (U32)(iend - ms->window.base);
    ms->forceNonContiguous = params->deterministicRefPrefix;

    if (loadLdmDict) {
        ZSTD_v1_5_2_window_update(&ls->window, src, srcSize, /* forceNonContiguous */ 0);
        ls->loadedDictEnd = params->forceWindow ? 0 : (U32)(iend - ls->window.base);
    }

    if (srcSize <= HASH_READ_SIZE) return 0;

    ZSTD_v1_5_2_overflowCorrectIfNeeded(ms, ws, params, ip, iend);

    if (loadLdmDict)
        ZSTD_v1_5_2_ldm_fillHashTable(ls, ip, iend, &params->ldmParams);

    switch(params->cParams.strategy)
    {
    case ZSTD_v1_5_2_fast:
        ZSTD_v1_5_2_fillHashTable(ms, iend, dtlm);
        break;
    case ZSTD_v1_5_2_dfast:
        ZSTD_v1_5_2_fillDoubleHashTable(ms, iend, dtlm);
        break;

    case ZSTD_v1_5_2_greedy:
    case ZSTD_v1_5_2_lazy:
    case ZSTD_v1_5_2_lazy2:
        assert(srcSize >= HASH_READ_SIZE);
        if (ms->dedicatedDictSearch) {
            assert(ms->chainTable != NULL);
            ZSTD_v1_5_2_dedicatedDictSearch_lazy_loadDictionary(ms, iend-HASH_READ_SIZE);
        } else {
            assert(params->useRowMatchFinder != ZSTD_v1_5_2_ps_auto);
            if (params->useRowMatchFinder == ZSTD_v1_5_2_ps_enable) {
                size_t const tagTableSize = ((size_t)1 << params->cParams.hashLog) * sizeof(U16);
                ZSTD_v1_5_2_memset(ms->tagTable, 0, tagTableSize);
                ZSTD_v1_5_2_row_update(ms, iend-HASH_READ_SIZE);
                DEBUGLOG(4, "Using row-based hash table for lazy dict");
            } else {
                ZSTD_v1_5_2_insertAndFindFirstIndex(ms, iend-HASH_READ_SIZE);
                DEBUGLOG(4, "Using chain-based hash table for lazy dict");
            }
        }
        break;

    case ZSTD_v1_5_2_btlazy2:   /* we want the dictionary table fully sorted */
    case ZSTD_v1_5_2_btopt:
    case ZSTD_v1_5_2_btultra:
    case ZSTD_v1_5_2_btultra2:
        assert(srcSize >= HASH_READ_SIZE);
        ZSTD_v1_5_2_updateTree(ms, iend-HASH_READ_SIZE, iend);
        break;

    default:
        assert(0);  /* not possible : not a valid strategy id */
    }

    ms->nextToUpdate = (U32)(iend - ms->window.base);
    return 0;
}


/* Dictionaries that assign zero probability to symbols that show up causes problems
 * when FSE encoding. Mark dictionaries with zero probability symbols as FSE_v1_5_2_repeat_check
 * and only dictionaries with 100% valid symbols can be assumed valid.
 */
static FSE_v1_5_2_repeat ZSTD_v1_5_2_dictNCountRepeat(short* normalizedCounter, unsigned dictMaxSymbolValue, unsigned maxSymbolValue)
{
    U32 s;
    if (dictMaxSymbolValue < maxSymbolValue) {
        return FSE_v1_5_2_repeat_check;
    }
    for (s = 0; s <= maxSymbolValue; ++s) {
        if (normalizedCounter[s] == 0) {
            return FSE_v1_5_2_repeat_check;
        }
    }
    return FSE_v1_5_2_repeat_valid;
}

size_t ZSTD_v1_5_2_loadCEntropy(ZSTD_v1_5_2_compressedBlockState_t* bs, void* workspace,
                         const void* const dict, size_t dictSize)
{
    short offcodeNCount[MaxOff+1];
    unsigned offcodeMaxValue = MaxOff;
    const BYTE* dictPtr = (const BYTE*)dict;    /* skip magic num and dict ID */
    const BYTE* const dictEnd = dictPtr + dictSize;
    dictPtr += 8;
    bs->entropy.huf.repeatMode = HUF_v1_5_2_repeat_check;

    {   unsigned maxSymbolValue = 255;
        unsigned hasZeroWeights = 1;
        size_t const hufHeaderSize = HUF_v1_5_2_readCTable((HUF_v1_5_2_CElt*)bs->entropy.huf.CTable, &maxSymbolValue, dictPtr,
            dictEnd-dictPtr, &hasZeroWeights);

        /* We only set the loaded table as valid if it contains all non-zero
         * weights. Otherwise, we set it to check */
        if (!hasZeroWeights)
            bs->entropy.huf.repeatMode = HUF_v1_5_2_repeat_valid;

        RETURN_ERROR_IF(HUF_v1_5_2_isError(hufHeaderSize), dictionary_corrupted, "");
        RETURN_ERROR_IF(maxSymbolValue < 255, dictionary_corrupted, "");
        dictPtr += hufHeaderSize;
    }

    {   unsigned offcodeLog;
        size_t const offcodeHeaderSize = FSE_v1_5_2_readNCount(offcodeNCount, &offcodeMaxValue, &offcodeLog, dictPtr, dictEnd-dictPtr);
        RETURN_ERROR_IF(FSE_v1_5_2_isError(offcodeHeaderSize), dictionary_corrupted, "");
        RETURN_ERROR_IF(offcodeLog > OffFSELog, dictionary_corrupted, "");
        /* fill all offset symbols to avoid garbage at end of table */
        RETURN_ERROR_IF(FSE_v1_5_2_isError(FSE_v1_5_2_buildCTable_wksp(
                bs->entropy.fse.offcodeCTable,
                offcodeNCount, MaxOff, offcodeLog,
                workspace, HUF_v1_5_2_WORKSPACE_SIZE)),
            dictionary_corrupted, "");
        /* Defer checking offcodeMaxValue because we need to know the size of the dictionary content */
        dictPtr += offcodeHeaderSize;
    }

    {   short matchlengthNCount[MaxML+1];
        unsigned matchlengthMaxValue = MaxML, matchlengthLog;
        size_t const matchlengthHeaderSize = FSE_v1_5_2_readNCount(matchlengthNCount, &matchlengthMaxValue, &matchlengthLog, dictPtr, dictEnd-dictPtr);
        RETURN_ERROR_IF(FSE_v1_5_2_isError(matchlengthHeaderSize), dictionary_corrupted, "");
        RETURN_ERROR_IF(matchlengthLog > MLFSELog, dictionary_corrupted, "");
        RETURN_ERROR_IF(FSE_v1_5_2_isError(FSE_v1_5_2_buildCTable_wksp(
                bs->entropy.fse.matchlengthCTable,
                matchlengthNCount, matchlengthMaxValue, matchlengthLog,
                workspace, HUF_v1_5_2_WORKSPACE_SIZE)),
            dictionary_corrupted, "");
        bs->entropy.fse.matchlength_repeatMode = ZSTD_v1_5_2_dictNCountRepeat(matchlengthNCount, matchlengthMaxValue, MaxML);
        dictPtr += matchlengthHeaderSize;
    }

    {   short litlengthNCount[MaxLL+1];
        unsigned litlengthMaxValue = MaxLL, litlengthLog;
        size_t const litlengthHeaderSize = FSE_v1_5_2_readNCount(litlengthNCount, &litlengthMaxValue, &litlengthLog, dictPtr, dictEnd-dictPtr);
        RETURN_ERROR_IF(FSE_v1_5_2_isError(litlengthHeaderSize), dictionary_corrupted, "");
        RETURN_ERROR_IF(litlengthLog > LLFSELog, dictionary_corrupted, "");
        RETURN_ERROR_IF(FSE_v1_5_2_isError(FSE_v1_5_2_buildCTable_wksp(
                bs->entropy.fse.litlengthCTable,
                litlengthNCount, litlengthMaxValue, litlengthLog,
                workspace, HUF_v1_5_2_WORKSPACE_SIZE)),
            dictionary_corrupted, "");
        bs->entropy.fse.litlength_repeatMode = ZSTD_v1_5_2_dictNCountRepeat(litlengthNCount, litlengthMaxValue, MaxLL);
        dictPtr += litlengthHeaderSize;
    }

    RETURN_ERROR_IF(dictPtr+12 > dictEnd, dictionary_corrupted, "");
    bs->rep[0] = MEM_readLE32(dictPtr+0);
    bs->rep[1] = MEM_readLE32(dictPtr+4);
    bs->rep[2] = MEM_readLE32(dictPtr+8);
    dictPtr += 12;

    {   size_t const dictContentSize = (size_t)(dictEnd - dictPtr);
        U32 offcodeMax = MaxOff;
        if (dictContentSize <= ((U32)-1) - 128 KB) {
            U32 const maxOffset = (U32)dictContentSize + 128 KB; /* The maximum offset that must be supported */
            offcodeMax = ZSTD_v1_5_2_highbit32(maxOffset); /* Calculate minimum offset code required to represent maxOffset */
        }
        /* All offset values <= dictContentSize + 128 KB must be representable for a valid table */
        bs->entropy.fse.offcode_repeatMode = ZSTD_v1_5_2_dictNCountRepeat(offcodeNCount, offcodeMaxValue, MIN(offcodeMax, MaxOff));

        /* All repCodes must be <= dictContentSize and != 0 */
        {   U32 u;
            for (u=0; u<3; u++) {
                RETURN_ERROR_IF(bs->rep[u] == 0, dictionary_corrupted, "");
                RETURN_ERROR_IF(bs->rep[u] > dictContentSize, dictionary_corrupted, "");
    }   }   }

    return dictPtr - (const BYTE*)dict;
}

/* Dictionary format :
 * See :
 * https://github.com/facebook/zstd/blob/release/doc/ZSTD_compression_format.md#dictionary-format
 */
/*! ZSTD_v1_5_2_loadZstdDictionary() :
 * @return : dictID, or an error code
 *  assumptions : magic number supposed already checked
 *                dictSize supposed >= 8
 */
static size_t ZSTD_v1_5_2_loadZstdDictionary(ZSTD_v1_5_2_compressedBlockState_t* bs,
                                      ZSTD_v1_5_2_matchState_t* ms,
                                      ZSTD_v1_5_2_cwksp* ws,
                                      ZSTD_v1_5_2_CCtx_params const* params,
                                      const void* dict, size_t dictSize,
                                      ZSTD_v1_5_2_dictTableLoadMethod_e dtlm,
                                      void* workspace)
{
    const BYTE* dictPtr = (const BYTE*)dict;
    const BYTE* const dictEnd = dictPtr + dictSize;
    size_t dictID;
    size_t eSize;
    ZSTD_v1_5_2_STATIC_ASSERT(HUF_v1_5_2_WORKSPACE_SIZE >= (1<<MAX(MLFSELog,LLFSELog)));
    assert(dictSize >= 8);
    assert(MEM_readLE32(dictPtr) == ZSTD_v1_5_2_MAGIC_DICTIONARY);

    dictID = params->fParams.noDictIDFlag ? 0 :  MEM_readLE32(dictPtr + 4 /* skip magic number */ );
    eSize = ZSTD_v1_5_2_loadCEntropy(bs, workspace, dict, dictSize);
    FORWARD_IF_ERROR(eSize, "ZSTD_v1_5_2_loadCEntropy failed");
    dictPtr += eSize;

    {
        size_t const dictContentSize = (size_t)(dictEnd - dictPtr);
        FORWARD_IF_ERROR(ZSTD_v1_5_2_loadDictionaryContent(
            ms, NULL, ws, params, dictPtr, dictContentSize, dtlm), "");
    }
    return dictID;
}

/** ZSTD_v1_5_2_compress_insertDictionary() :
*   @return : dictID, or an error code */
static size_t
ZSTD_v1_5_2_compress_insertDictionary(ZSTD_v1_5_2_compressedBlockState_t* bs,
                               ZSTD_v1_5_2_matchState_t* ms,
                               ldmState_t* ls,
                               ZSTD_v1_5_2_cwksp* ws,
                         const ZSTD_v1_5_2_CCtx_params* params,
                         const void* dict, size_t dictSize,
                               ZSTD_v1_5_2_dictContentType_e dictContentType,
                               ZSTD_v1_5_2_dictTableLoadMethod_e dtlm,
                               void* workspace)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_compress_insertDictionary (dictSize=%u)", (U32)dictSize);
    if ((dict==NULL) || (dictSize<8)) {
        RETURN_ERROR_IF(dictContentType == ZSTD_v1_5_2_dct_fullDict, dictionary_wrong, "");
        return 0;
    }

    ZSTD_v1_5_2_reset_compressedBlockState(bs);

    /* dict restricted modes */
    if (dictContentType == ZSTD_v1_5_2_dct_rawContent)
        return ZSTD_v1_5_2_loadDictionaryContent(ms, ls, ws, params, dict, dictSize, dtlm);

    if (MEM_readLE32(dict) != ZSTD_v1_5_2_MAGIC_DICTIONARY) {
        if (dictContentType == ZSTD_v1_5_2_dct_auto) {
            DEBUGLOG(4, "raw content dictionary detected");
            return ZSTD_v1_5_2_loadDictionaryContent(
                ms, ls, ws, params, dict, dictSize, dtlm);
        }
        RETURN_ERROR_IF(dictContentType == ZSTD_v1_5_2_dct_fullDict, dictionary_wrong, "");
        assert(0);   /* impossible */
    }

    /* dict as full zstd dictionary */
    return ZSTD_v1_5_2_loadZstdDictionary(
        bs, ms, ws, params, dict, dictSize, dtlm, workspace);
}

#define ZSTD_v1_5_2_USE_CDICT_PARAMS_SRCSIZE_CUTOFF (128 KB)
#define ZSTD_v1_5_2_USE_CDICT_PARAMS_DICTSIZE_MULTIPLIER (6ULL)

/*! ZSTD_v1_5_2_compressBegin_internal() :
 * @return : 0, or an error code */
static size_t ZSTD_v1_5_2_compressBegin_internal(ZSTD_v1_5_2_CCtx* cctx,
                                    const void* dict, size_t dictSize,
                                    ZSTD_v1_5_2_dictContentType_e dictContentType,
                                    ZSTD_v1_5_2_dictTableLoadMethod_e dtlm,
                                    const ZSTD_v1_5_2_CDict* cdict,
                                    const ZSTD_v1_5_2_CCtx_params* params, U64 pledgedSrcSize,
                                    ZSTD_v1_5_2_buffered_policy_e zbuff)
{
    size_t const dictContentSize = cdict ? cdict->dictContentSize : dictSize;
#if ZSTD_v1_5_2_TRACE
    cctx->traceCtx = (ZSTD_v1_5_2_trace_compress_begin != NULL) ? ZSTD_v1_5_2_trace_compress_begin(cctx) : 0;
#endif
    DEBUGLOG(4, "ZSTD_v1_5_2_compressBegin_internal: wlog=%u", params->cParams.windowLog);
    /* params are supposed to be fully validated at this point */
    assert(!ZSTD_v1_5_2_isError(ZSTD_v1_5_2_checkCParams(params->cParams)));
    assert(!((dict) && (cdict)));  /* either dict or cdict, not both */
    if ( (cdict)
      && (cdict->dictContentSize > 0)
      && ( pledgedSrcSize < ZSTD_v1_5_2_USE_CDICT_PARAMS_SRCSIZE_CUTOFF
        || pledgedSrcSize < cdict->dictContentSize * ZSTD_v1_5_2_USE_CDICT_PARAMS_DICTSIZE_MULTIPLIER
        || pledgedSrcSize == ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN
        || cdict->compressionLevel == 0)
      && (params->attachDictPref != ZSTD_v1_5_2_dictForceLoad) ) {
        return ZSTD_v1_5_2_resetCCtx_usingCDict(cctx, cdict, params, pledgedSrcSize, zbuff);
    }

    FORWARD_IF_ERROR( ZSTD_v1_5_2_resetCCtx_internal(cctx, params, pledgedSrcSize,
                                     dictContentSize,
                                     ZSTDcrp_makeClean, zbuff) , "");
    {   size_t const dictID = cdict ?
                ZSTD_v1_5_2_compress_insertDictionary(
                        cctx->blockState.prevCBlock, &cctx->blockState.matchState,
                        &cctx->ldmState, &cctx->workspace, &cctx->appliedParams, cdict->dictContent,
                        cdict->dictContentSize, cdict->dictContentType, dtlm,
                        cctx->entropyWorkspace)
              : ZSTD_v1_5_2_compress_insertDictionary(
                        cctx->blockState.prevCBlock, &cctx->blockState.matchState,
                        &cctx->ldmState, &cctx->workspace, &cctx->appliedParams, dict, dictSize,
                        dictContentType, dtlm, cctx->entropyWorkspace);
        FORWARD_IF_ERROR(dictID, "ZSTD_v1_5_2_compress_insertDictionary failed");
        assert(dictID <= UINT_MAX);
        cctx->dictID = (U32)dictID;
        cctx->dictContentSize = dictContentSize;
    }
    return 0;
}

size_t ZSTD_v1_5_2_compressBegin_advanced_internal(ZSTD_v1_5_2_CCtx* cctx,
                                    const void* dict, size_t dictSize,
                                    ZSTD_v1_5_2_dictContentType_e dictContentType,
                                    ZSTD_v1_5_2_dictTableLoadMethod_e dtlm,
                                    const ZSTD_v1_5_2_CDict* cdict,
                                    const ZSTD_v1_5_2_CCtx_params* params,
                                    unsigned long long pledgedSrcSize)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_compressBegin_advanced_internal: wlog=%u", params->cParams.windowLog);
    /* compression parameters verification and optimization */
    FORWARD_IF_ERROR( ZSTD_v1_5_2_checkCParams(params->cParams) , "");
    return ZSTD_v1_5_2_compressBegin_internal(cctx,
                                       dict, dictSize, dictContentType, dtlm,
                                       cdict,
                                       params, pledgedSrcSize,
                                       ZSTDb_not_buffered);
}

/*! ZSTD_v1_5_2_compressBegin_advanced() :
*   @return : 0, or an error code */
size_t ZSTD_v1_5_2_compressBegin_advanced(ZSTD_v1_5_2_CCtx* cctx,
                             const void* dict, size_t dictSize,
                                   ZSTD_v1_5_2_parameters params, unsigned long long pledgedSrcSize)
{
    ZSTD_v1_5_2_CCtx_params cctxParams;
    ZSTD_v1_5_2_CCtxParams_init_internal(&cctxParams, &params, ZSTD_v1_5_2_NO_CLEVEL);
    return ZSTD_v1_5_2_compressBegin_advanced_internal(cctx,
                                            dict, dictSize, ZSTD_v1_5_2_dct_auto, ZSTD_v1_5_2_dtlm_fast,
                                            NULL /*cdict*/,
                                            &cctxParams, pledgedSrcSize);
}

size_t ZSTD_v1_5_2_compressBegin_usingDict(ZSTD_v1_5_2_CCtx* cctx, const void* dict, size_t dictSize, int compressionLevel)
{
    ZSTD_v1_5_2_CCtx_params cctxParams;
    {
        ZSTD_v1_5_2_parameters const params = ZSTD_v1_5_2_getParams_internal(compressionLevel, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN, dictSize, ZSTD_v1_5_2_cpm_noAttachDict);
        ZSTD_v1_5_2_CCtxParams_init_internal(&cctxParams, &params, (compressionLevel == 0) ? ZSTD_v1_5_2_CLEVEL_DEFAULT : compressionLevel);
    }
    DEBUGLOG(4, "ZSTD_v1_5_2_compressBegin_usingDict (dictSize=%u)", (unsigned)dictSize);
    return ZSTD_v1_5_2_compressBegin_internal(cctx, dict, dictSize, ZSTD_v1_5_2_dct_auto, ZSTD_v1_5_2_dtlm_fast, NULL,
                                       &cctxParams, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN, ZSTDb_not_buffered);
}

size_t ZSTD_v1_5_2_compressBegin(ZSTD_v1_5_2_CCtx* cctx, int compressionLevel)
{
    return ZSTD_v1_5_2_compressBegin_usingDict(cctx, NULL, 0, compressionLevel);
}


/*! ZSTD_v1_5_2_writeEpilogue() :
*   Ends a frame.
*   @return : nb of bytes written into dst (or an error code) */
static size_t ZSTD_v1_5_2_writeEpilogue(ZSTD_v1_5_2_CCtx* cctx, void* dst, size_t dstCapacity)
{
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;
    size_t fhSize = 0;

    DEBUGLOG(4, "ZSTD_v1_5_2_writeEpilogue");
    RETURN_ERROR_IF(cctx->stage == ZSTDcs_created, stage_wrong, "init missing");

    /* special case : empty frame */
    if (cctx->stage == ZSTDcs_init) {
        fhSize = ZSTD_v1_5_2_writeFrameHeader(dst, dstCapacity, &cctx->appliedParams, 0, 0);
        FORWARD_IF_ERROR(fhSize, "ZSTD_v1_5_2_writeFrameHeader failed");
        dstCapacity -= fhSize;
        op += fhSize;
        cctx->stage = ZSTDcs_ongoing;
    }

    if (cctx->stage != ZSTDcs_ending) {
        /* write one last empty block, make it the "last" block */
        U32 const cBlockHeader24 = 1 /* last block */ + (((U32)bt_raw)<<1) + 0;
        RETURN_ERROR_IF(dstCapacity<4, dstSize_tooSmall, "no room for epilogue");
        MEM_writeLE32(op, cBlockHeader24);
        op += ZSTD_v1_5_2_blockHeaderSize;
        dstCapacity -= ZSTD_v1_5_2_blockHeaderSize;
    }

    if (cctx->appliedParams.fParams.checksumFlag) {
        U32 const checksum = (U32) XXH64_digest(&cctx->xxhState);
        RETURN_ERROR_IF(dstCapacity<4, dstSize_tooSmall, "no room for checksum");
        DEBUGLOG(4, "ZSTD_v1_5_2_writeEpilogue: write checksum : %08X", (unsigned)checksum);
        MEM_writeLE32(op, checksum);
        op += 4;
    }

    cctx->stage = ZSTDcs_created;  /* return to "created but no init" status */
    return op-ostart;
}

void ZSTD_v1_5_2_CCtx_trace(ZSTD_v1_5_2_CCtx* cctx, size_t extraCSize)
{
#if ZSTD_v1_5_2_TRACE
    if (cctx->traceCtx && ZSTD_v1_5_2_trace_compress_end != NULL) {
        int const streaming = cctx->inBuffSize > 0 || cctx->outBuffSize > 0 || cctx->appliedParams.nbWorkers > 0;
        ZSTD_v1_5_2_Trace trace;
        ZSTD_v1_5_2_memset(&trace, 0, sizeof(trace));
        trace.version = ZSTD_v1_5_2_VERSION_NUMBER;
        trace.streaming = streaming;
        trace.dictionaryID = cctx->dictID;
        trace.dictionarySize = cctx->dictContentSize;
        trace.uncompressedSize = cctx->consumedSrcSize;
        trace.compressedSize = cctx->producedCSize + extraCSize;
        trace.params = &cctx->appliedParams;
        trace.cctx = cctx;
        ZSTD_v1_5_2_trace_compress_end(cctx->traceCtx, &trace);
    }
    cctx->traceCtx = 0;
#else
    (void)cctx;
    (void)extraCSize;
#endif
}

size_t ZSTD_v1_5_2_compressEnd (ZSTD_v1_5_2_CCtx* cctx,
                         void* dst, size_t dstCapacity,
                   const void* src, size_t srcSize)
{
    size_t endResult;
    size_t const cSize = ZSTD_v1_5_2_compressContinue_internal(cctx,
                                dst, dstCapacity, src, srcSize,
                                1 /* frame mode */, 1 /* last chunk */);
    FORWARD_IF_ERROR(cSize, "ZSTD_v1_5_2_compressContinue_internal failed");
    endResult = ZSTD_v1_5_2_writeEpilogue(cctx, (char*)dst + cSize, dstCapacity-cSize);
    FORWARD_IF_ERROR(endResult, "ZSTD_v1_5_2_writeEpilogue failed");
    assert(!(cctx->appliedParams.fParams.contentSizeFlag && cctx->pledgedSrcSizePlusOne == 0));
    if (cctx->pledgedSrcSizePlusOne != 0) {  /* control src size */
        ZSTD_v1_5_2_STATIC_ASSERT(ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN == (unsigned long long)-1);
        DEBUGLOG(4, "end of frame : controlling src size");
        RETURN_ERROR_IF(
            cctx->pledgedSrcSizePlusOne != cctx->consumedSrcSize+1,
            srcSize_wrong,
             "error : pledgedSrcSize = %u, while realSrcSize = %u",
            (unsigned)cctx->pledgedSrcSizePlusOne-1,
            (unsigned)cctx->consumedSrcSize);
    }
    ZSTD_v1_5_2_CCtx_trace(cctx, endResult);
    return cSize + endResult;
}

size_t ZSTD_v1_5_2_compress_advanced (ZSTD_v1_5_2_CCtx* cctx,
                               void* dst, size_t dstCapacity,
                         const void* src, size_t srcSize,
                         const void* dict,size_t dictSize,
                               ZSTD_v1_5_2_parameters params)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_compress_advanced");
    FORWARD_IF_ERROR(ZSTD_v1_5_2_checkCParams(params.cParams), "");
    ZSTD_v1_5_2_CCtxParams_init_internal(&cctx->simpleApiParams, &params, ZSTD_v1_5_2_NO_CLEVEL);
    return ZSTD_v1_5_2_compress_advanced_internal(cctx,
                                           dst, dstCapacity,
                                           src, srcSize,
                                           dict, dictSize,
                                           &cctx->simpleApiParams);
}

/* Internal */
size_t ZSTD_v1_5_2_compress_advanced_internal(
        ZSTD_v1_5_2_CCtx* cctx,
        void* dst, size_t dstCapacity,
        const void* src, size_t srcSize,
        const void* dict,size_t dictSize,
        const ZSTD_v1_5_2_CCtx_params* params)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_compress_advanced_internal (srcSize:%u)", (unsigned)srcSize);
    FORWARD_IF_ERROR( ZSTD_v1_5_2_compressBegin_internal(cctx,
                         dict, dictSize, ZSTD_v1_5_2_dct_auto, ZSTD_v1_5_2_dtlm_fast, NULL,
                         params, srcSize, ZSTDb_not_buffered) , "");
    return ZSTD_v1_5_2_compressEnd(cctx, dst, dstCapacity, src, srcSize);
}

size_t ZSTD_v1_5_2_compress_usingDict(ZSTD_v1_5_2_CCtx* cctx,
                               void* dst, size_t dstCapacity,
                         const void* src, size_t srcSize,
                         const void* dict, size_t dictSize,
                               int compressionLevel)
{
    {
        ZSTD_v1_5_2_parameters const params = ZSTD_v1_5_2_getParams_internal(compressionLevel, srcSize, dict ? dictSize : 0, ZSTD_v1_5_2_cpm_noAttachDict);
        assert(params.fParams.contentSizeFlag == 1);
        ZSTD_v1_5_2_CCtxParams_init_internal(&cctx->simpleApiParams, &params, (compressionLevel == 0) ? ZSTD_v1_5_2_CLEVEL_DEFAULT: compressionLevel);
    }
    DEBUGLOG(4, "ZSTD_v1_5_2_compress_usingDict (srcSize=%u)", (unsigned)srcSize);
    return ZSTD_v1_5_2_compress_advanced_internal(cctx, dst, dstCapacity, src, srcSize, dict, dictSize, &cctx->simpleApiParams);
}

size_t ZSTD_v1_5_2_compressCCtx(ZSTD_v1_5_2_CCtx* cctx,
                         void* dst, size_t dstCapacity,
                   const void* src, size_t srcSize,
                         int compressionLevel)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_compressCCtx (srcSize=%u)", (unsigned)srcSize);
    assert(cctx != NULL);
    return ZSTD_v1_5_2_compress_usingDict(cctx, dst, dstCapacity, src, srcSize, NULL, 0, compressionLevel);
}

size_t ZSTD_v1_5_2_compress(void* dst, size_t dstCapacity,
               const void* src, size_t srcSize,
                     int compressionLevel)
{
    size_t result;
#if ZSTD_v1_5_2_COMPRESS_HEAPMODE
    ZSTD_v1_5_2_CCtx* cctx = ZSTD_v1_5_2_createCCtx();
    RETURN_ERROR_IF(!cctx, memory_allocation, "ZSTD_v1_5_2_createCCtx failed");
    result = ZSTD_v1_5_2_compressCCtx(cctx, dst, dstCapacity, src, srcSize, compressionLevel);
    ZSTD_v1_5_2_freeCCtx(cctx);
#else
    ZSTD_v1_5_2_CCtx ctxBody;
    ZSTD_v1_5_2_initCCtx(&ctxBody, ZSTD_v1_5_2_defaultCMem);
    result = ZSTD_v1_5_2_compressCCtx(&ctxBody, dst, dstCapacity, src, srcSize, compressionLevel);
    ZSTD_v1_5_2_freeCCtxContent(&ctxBody);   /* can't free ctxBody itself, as it's on stack; free only heap content */
#endif
    return result;
}


/* =====  Dictionary API  ===== */

/*! ZSTD_v1_5_2_estimateCDictSize_advanced() :
 *  Estimate amount of memory that will be needed to create a dictionary with following arguments */
size_t ZSTD_v1_5_2_estimateCDictSize_advanced(
        size_t dictSize, ZSTD_v1_5_2_compressionParameters cParams,
        ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod)
{
    DEBUGLOG(5, "sizeof(ZSTD_v1_5_2_CDict) : %u", (unsigned)sizeof(ZSTD_v1_5_2_CDict));
    return ZSTD_v1_5_2_cwksp_alloc_size(sizeof(ZSTD_v1_5_2_CDict))
         + ZSTD_v1_5_2_cwksp_alloc_size(HUF_v1_5_2_WORKSPACE_SIZE)
         /* enableDedicatedDictSearch == 1 ensures that CDict estimation will not be too small
          * in case we are using DDS with row-hash. */
         + ZSTD_v1_5_2_sizeof_matchState(&cParams, ZSTD_v1_5_2_resolveRowMatchFinderMode(ZSTD_v1_5_2_ps_auto, &cParams),
                                  /* enableDedicatedDictSearch */ 1, /* forCCtx */ 0)
         + (dictLoadMethod == ZSTD_v1_5_2_dlm_byRef ? 0
            : ZSTD_v1_5_2_cwksp_alloc_size(ZSTD_v1_5_2_cwksp_align(dictSize, sizeof(void *))));
}

size_t ZSTD_v1_5_2_estimateCDictSize(size_t dictSize, int compressionLevel)
{
    ZSTD_v1_5_2_compressionParameters const cParams = ZSTD_v1_5_2_getCParams_internal(compressionLevel, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN, dictSize, ZSTD_v1_5_2_cpm_createCDict);
    return ZSTD_v1_5_2_estimateCDictSize_advanced(dictSize, cParams, ZSTD_v1_5_2_dlm_byCopy);
}

size_t ZSTD_v1_5_2_sizeof_CDict(const ZSTD_v1_5_2_CDict* cdict)
{
    if (cdict==NULL) return 0;   /* support sizeof on NULL */
    DEBUGLOG(5, "sizeof(*cdict) : %u", (unsigned)sizeof(*cdict));
    /* cdict may be in the workspace */
    return (cdict->workspace.workspace == cdict ? 0 : sizeof(*cdict))
        + ZSTD_v1_5_2_cwksp_sizeof(&cdict->workspace);
}

static size_t ZSTD_v1_5_2_initCDict_internal(
                    ZSTD_v1_5_2_CDict* cdict,
              const void* dictBuffer, size_t dictSize,
                    ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
                    ZSTD_v1_5_2_dictContentType_e dictContentType,
                    ZSTD_v1_5_2_CCtx_params params)
{
    DEBUGLOG(3, "ZSTD_v1_5_2_initCDict_internal (dictContentType:%u)", (unsigned)dictContentType);
    assert(!ZSTD_v1_5_2_checkCParams(params.cParams));
    cdict->matchState.cParams = params.cParams;
    cdict->matchState.dedicatedDictSearch = params.enableDedicatedDictSearch;
    if ((dictLoadMethod == ZSTD_v1_5_2_dlm_byRef) || (!dictBuffer) || (!dictSize)) {
        cdict->dictContent = dictBuffer;
    } else {
         void *internalBuffer = ZSTD_v1_5_2_cwksp_reserve_object(&cdict->workspace, ZSTD_v1_5_2_cwksp_align(dictSize, sizeof(void*)));
        RETURN_ERROR_IF(!internalBuffer, memory_allocation, "NULL pointer!");
        cdict->dictContent = internalBuffer;
        ZSTD_v1_5_2_memcpy(internalBuffer, dictBuffer, dictSize);
    }
    cdict->dictContentSize = dictSize;
    cdict->dictContentType = dictContentType;

    cdict->entropyWorkspace = (U32*)ZSTD_v1_5_2_cwksp_reserve_object(&cdict->workspace, HUF_v1_5_2_WORKSPACE_SIZE);


    /* Reset the state to no dictionary */
    ZSTD_v1_5_2_reset_compressedBlockState(&cdict->cBlockState);
    FORWARD_IF_ERROR(ZSTD_v1_5_2_reset_matchState(
        &cdict->matchState,
        &cdict->workspace,
        &params.cParams,
        params.useRowMatchFinder,
        ZSTDcrp_makeClean,
        ZSTDirp_reset,
        ZSTD_v1_5_2_resetTarget_CDict), "");
    /* (Maybe) load the dictionary
     * Skips loading the dictionary if it is < 8 bytes.
     */
    {   params.compressionLevel = ZSTD_v1_5_2_CLEVEL_DEFAULT;
        params.fParams.contentSizeFlag = 1;
        {   size_t const dictID = ZSTD_v1_5_2_compress_insertDictionary(
                    &cdict->cBlockState, &cdict->matchState, NULL, &cdict->workspace,
                    &params, cdict->dictContent, cdict->dictContentSize,
                    dictContentType, ZSTD_v1_5_2_dtlm_full, cdict->entropyWorkspace);
            FORWARD_IF_ERROR(dictID, "ZSTD_v1_5_2_compress_insertDictionary failed");
            assert(dictID <= (size_t)(U32)-1);
            cdict->dictID = (U32)dictID;
        }
    }

    return 0;
}

static ZSTD_v1_5_2_CDict* ZSTD_v1_5_2_createCDict_advanced_internal(size_t dictSize,
                                      ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
                                      ZSTD_v1_5_2_compressionParameters cParams,
                                      ZSTD_v1_5_2_paramSwitch_e useRowMatchFinder,
                                      U32 enableDedicatedDictSearch,
                                      ZSTD_v1_5_2_customMem customMem)
{
    if ((!customMem.customAlloc) ^ (!customMem.customFree)) return NULL;

    {   size_t const workspaceSize =
            ZSTD_v1_5_2_cwksp_alloc_size(sizeof(ZSTD_v1_5_2_CDict)) +
            ZSTD_v1_5_2_cwksp_alloc_size(HUF_v1_5_2_WORKSPACE_SIZE) +
            ZSTD_v1_5_2_sizeof_matchState(&cParams, useRowMatchFinder, enableDedicatedDictSearch, /* forCCtx */ 0) +
            (dictLoadMethod == ZSTD_v1_5_2_dlm_byRef ? 0
             : ZSTD_v1_5_2_cwksp_alloc_size(ZSTD_v1_5_2_cwksp_align(dictSize, sizeof(void*))));
        void* const workspace = ZSTD_v1_5_2_customMalloc(workspaceSize, customMem);
        ZSTD_v1_5_2_cwksp ws;
        ZSTD_v1_5_2_CDict* cdict;

        if (!workspace) {
            ZSTD_v1_5_2_customFree(workspace, customMem);
            return NULL;
        }

        ZSTD_v1_5_2_cwksp_init(&ws, workspace, workspaceSize, ZSTD_v1_5_2_cwksp_dynamic_alloc);

        cdict = (ZSTD_v1_5_2_CDict*)ZSTD_v1_5_2_cwksp_reserve_object(&ws, sizeof(ZSTD_v1_5_2_CDict));
        assert(cdict != NULL);
        ZSTD_v1_5_2_cwksp_move(&cdict->workspace, &ws);
        cdict->customMem = customMem;
        cdict->compressionLevel = ZSTD_v1_5_2_NO_CLEVEL; /* signals advanced API usage */
        cdict->useRowMatchFinder = useRowMatchFinder;
        return cdict;
    }
}

ZSTD_v1_5_2_CDict* ZSTD_v1_5_2_createCDict_advanced(const void* dictBuffer, size_t dictSize,
                                      ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
                                      ZSTD_v1_5_2_dictContentType_e dictContentType,
                                      ZSTD_v1_5_2_compressionParameters cParams,
                                      ZSTD_v1_5_2_customMem customMem)
{
    ZSTD_v1_5_2_CCtx_params cctxParams;
    ZSTD_v1_5_2_memset(&cctxParams, 0, sizeof(cctxParams));
    ZSTD_v1_5_2_CCtxParams_init(&cctxParams, 0);
    cctxParams.cParams = cParams;
    cctxParams.customMem = customMem;
    return ZSTD_v1_5_2_createCDict_advanced2(
        dictBuffer, dictSize,
        dictLoadMethod, dictContentType,
        &cctxParams, customMem);
}

ZSTD_v1_5_2_CDict* ZSTD_v1_5_2_createCDict_advanced2(
        const void* dict, size_t dictSize,
        ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
        ZSTD_v1_5_2_dictContentType_e dictContentType,
        const ZSTD_v1_5_2_CCtx_params* originalCctxParams,
        ZSTD_v1_5_2_customMem customMem)
{
    ZSTD_v1_5_2_CCtx_params cctxParams = *originalCctxParams;
    ZSTD_v1_5_2_compressionParameters cParams;
    ZSTD_v1_5_2_CDict* cdict;

    DEBUGLOG(3, "ZSTD_v1_5_2_createCDict_advanced2, mode %u", (unsigned)dictContentType);
    if (!customMem.customAlloc ^ !customMem.customFree) return NULL;

    if (cctxParams.enableDedicatedDictSearch) {
        cParams = ZSTD_v1_5_2_dedicatedDictSearch_getCParams(
            cctxParams.compressionLevel, dictSize);
        ZSTD_v1_5_2_overrideCParams(&cParams, &cctxParams.cParams);
    } else {
        cParams = ZSTD_v1_5_2_getCParamsFromCCtxParams(
            &cctxParams, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN, dictSize, ZSTD_v1_5_2_cpm_createCDict);
    }

    if (!ZSTD_v1_5_2_dedicatedDictSearch_isSupported(&cParams)) {
        /* Fall back to non-DDSS params */
        cctxParams.enableDedicatedDictSearch = 0;
        cParams = ZSTD_v1_5_2_getCParamsFromCCtxParams(
            &cctxParams, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN, dictSize, ZSTD_v1_5_2_cpm_createCDict);
    }

    DEBUGLOG(3, "ZSTD_v1_5_2_createCDict_advanced2: DDS: %u", cctxParams.enableDedicatedDictSearch);
    cctxParams.cParams = cParams;
    cctxParams.useRowMatchFinder = ZSTD_v1_5_2_resolveRowMatchFinderMode(cctxParams.useRowMatchFinder, &cParams);

    cdict = ZSTD_v1_5_2_createCDict_advanced_internal(dictSize,
                        dictLoadMethod, cctxParams.cParams,
                        cctxParams.useRowMatchFinder, cctxParams.enableDedicatedDictSearch,
                        customMem);

    if (ZSTD_v1_5_2_isError( ZSTD_v1_5_2_initCDict_internal(cdict,
                                    dict, dictSize,
                                    dictLoadMethod, dictContentType,
                                    cctxParams) )) {
        ZSTD_v1_5_2_freeCDict(cdict);
        return NULL;
    }

    return cdict;
}

ZSTD_v1_5_2_CDict* ZSTD_v1_5_2_createCDict(const void* dict, size_t dictSize, int compressionLevel)
{
    ZSTD_v1_5_2_compressionParameters cParams = ZSTD_v1_5_2_getCParams_internal(compressionLevel, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN, dictSize, ZSTD_v1_5_2_cpm_createCDict);
    ZSTD_v1_5_2_CDict* const cdict = ZSTD_v1_5_2_createCDict_advanced(dict, dictSize,
                                                  ZSTD_v1_5_2_dlm_byCopy, ZSTD_v1_5_2_dct_auto,
                                                  cParams, ZSTD_v1_5_2_defaultCMem);
    if (cdict)
        cdict->compressionLevel = (compressionLevel == 0) ? ZSTD_v1_5_2_CLEVEL_DEFAULT : compressionLevel;
    return cdict;
}

ZSTD_v1_5_2_CDict* ZSTD_v1_5_2_createCDict_byReference(const void* dict, size_t dictSize, int compressionLevel)
{
    ZSTD_v1_5_2_compressionParameters cParams = ZSTD_v1_5_2_getCParams_internal(compressionLevel, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN, dictSize, ZSTD_v1_5_2_cpm_createCDict);
    ZSTD_v1_5_2_CDict* const cdict = ZSTD_v1_5_2_createCDict_advanced(dict, dictSize,
                                     ZSTD_v1_5_2_dlm_byRef, ZSTD_v1_5_2_dct_auto,
                                     cParams, ZSTD_v1_5_2_defaultCMem);
    if (cdict)
        cdict->compressionLevel = (compressionLevel == 0) ? ZSTD_v1_5_2_CLEVEL_DEFAULT : compressionLevel;
    return cdict;
}

size_t ZSTD_v1_5_2_freeCDict(ZSTD_v1_5_2_CDict* cdict)
{
    if (cdict==NULL) return 0;   /* support free on NULL */
    {   ZSTD_v1_5_2_customMem const cMem = cdict->customMem;
        int cdictInWorkspace = ZSTD_v1_5_2_cwksp_owns_buffer(&cdict->workspace, cdict);
        ZSTD_v1_5_2_cwksp_free(&cdict->workspace, cMem);
        if (!cdictInWorkspace) {
            ZSTD_v1_5_2_customFree(cdict, cMem);
        }
        return 0;
    }
}

/*! ZSTD_v1_5_2_initStaticCDict_advanced() :
 *  Generate a digested dictionary in provided memory area.
 *  workspace: The memory area to emplace the dictionary into.
 *             Provided pointer must 8-bytes aligned.
 *             It must outlive dictionary usage.
 *  workspaceSize: Use ZSTD_v1_5_2_estimateCDictSize()
 *                 to determine how large workspace must be.
 *  cParams : use ZSTD_v1_5_2_getCParams() to transform a compression level
 *            into its relevants cParams.
 * @return : pointer to ZSTD_v1_5_2_CDict*, or NULL if error (size too small)
 *  Note : there is no corresponding "free" function.
 *         Since workspace was allocated externally, it must be freed externally.
 */
const ZSTD_v1_5_2_CDict* ZSTD_v1_5_2_initStaticCDict(
                                 void* workspace, size_t workspaceSize,
                           const void* dict, size_t dictSize,
                                 ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
                                 ZSTD_v1_5_2_dictContentType_e dictContentType,
                                 ZSTD_v1_5_2_compressionParameters cParams)
{
    ZSTD_v1_5_2_paramSwitch_e const useRowMatchFinder = ZSTD_v1_5_2_resolveRowMatchFinderMode(ZSTD_v1_5_2_ps_auto, &cParams);
    /* enableDedicatedDictSearch == 1 ensures matchstate is not too small in case this CDict will be used for DDS + row hash */
    size_t const matchStateSize = ZSTD_v1_5_2_sizeof_matchState(&cParams, useRowMatchFinder, /* enableDedicatedDictSearch */ 1, /* forCCtx */ 0);
    size_t const neededSize = ZSTD_v1_5_2_cwksp_alloc_size(sizeof(ZSTD_v1_5_2_CDict))
                            + (dictLoadMethod == ZSTD_v1_5_2_dlm_byRef ? 0
                               : ZSTD_v1_5_2_cwksp_alloc_size(ZSTD_v1_5_2_cwksp_align(dictSize, sizeof(void*))))
                            + ZSTD_v1_5_2_cwksp_alloc_size(HUF_v1_5_2_WORKSPACE_SIZE)
                            + matchStateSize;
    ZSTD_v1_5_2_CDict* cdict;
    ZSTD_v1_5_2_CCtx_params params;

    if ((size_t)workspace & 7) return NULL;  /* 8-aligned */

    {
        ZSTD_v1_5_2_cwksp ws;
        ZSTD_v1_5_2_cwksp_init(&ws, workspace, workspaceSize, ZSTD_v1_5_2_cwksp_static_alloc);
        cdict = (ZSTD_v1_5_2_CDict*)ZSTD_v1_5_2_cwksp_reserve_object(&ws, sizeof(ZSTD_v1_5_2_CDict));
        if (cdict == NULL) return NULL;
        ZSTD_v1_5_2_cwksp_move(&cdict->workspace, &ws);
    }

    DEBUGLOG(4, "(workspaceSize < neededSize) : (%u < %u) => %u",
        (unsigned)workspaceSize, (unsigned)neededSize, (unsigned)(workspaceSize < neededSize));
    if (workspaceSize < neededSize) return NULL;

    ZSTD_v1_5_2_CCtxParams_init(&params, 0);
    params.cParams = cParams;
    params.useRowMatchFinder = useRowMatchFinder;
    cdict->useRowMatchFinder = useRowMatchFinder;

    if (ZSTD_v1_5_2_isError( ZSTD_v1_5_2_initCDict_internal(cdict,
                                              dict, dictSize,
                                              dictLoadMethod, dictContentType,
                                              params) ))
        return NULL;

    return cdict;
}

ZSTD_v1_5_2_compressionParameters ZSTD_v1_5_2_getCParamsFromCDict(const ZSTD_v1_5_2_CDict* cdict)
{
    assert(cdict != NULL);
    return cdict->matchState.cParams;
}

/*! ZSTD_v1_5_2_getDictID_fromCDict() :
 *  Provides the dictID of the dictionary loaded into `cdict`.
 *  If @return == 0, the dictionary is not conformant to Zstandard specification, or empty.
 *  Non-conformant dictionaries can still be loaded, but as content-only dictionaries. */
unsigned ZSTD_v1_5_2_getDictID_fromCDict(const ZSTD_v1_5_2_CDict* cdict)
{
    if (cdict==NULL) return 0;
    return cdict->dictID;
}

/* ZSTD_v1_5_2_compressBegin_usingCDict_internal() :
 * Implementation of various ZSTD_v1_5_2_compressBegin_usingCDict* functions.
 */
static size_t ZSTD_v1_5_2_compressBegin_usingCDict_internal(
    ZSTD_v1_5_2_CCtx* const cctx, const ZSTD_v1_5_2_CDict* const cdict,
    ZSTD_v1_5_2_frameParameters const fParams, unsigned long long const pledgedSrcSize)
{
    ZSTD_v1_5_2_CCtx_params cctxParams;
    DEBUGLOG(4, "ZSTD_v1_5_2_compressBegin_usingCDict_internal");
    RETURN_ERROR_IF(cdict==NULL, dictionary_wrong, "NULL pointer!");
    /* Initialize the cctxParams from the cdict */
    {
        ZSTD_v1_5_2_parameters params;
        params.fParams = fParams;
        params.cParams = ( pledgedSrcSize < ZSTD_v1_5_2_USE_CDICT_PARAMS_SRCSIZE_CUTOFF
                        || pledgedSrcSize < cdict->dictContentSize * ZSTD_v1_5_2_USE_CDICT_PARAMS_DICTSIZE_MULTIPLIER
                        || pledgedSrcSize == ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN
                        || cdict->compressionLevel == 0 ) ?
                ZSTD_v1_5_2_getCParamsFromCDict(cdict)
              : ZSTD_v1_5_2_getCParams(cdict->compressionLevel,
                                pledgedSrcSize,
                                cdict->dictContentSize);
        ZSTD_v1_5_2_CCtxParams_init_internal(&cctxParams, &params, cdict->compressionLevel);
    }
    /* Increase window log to fit the entire dictionary and source if the
     * source size is known. Limit the increase to 19, which is the
     * window log for compression level 1 with the largest source size.
     */
    if (pledgedSrcSize != ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN) {
        U32 const limitedSrcSize = (U32)MIN(pledgedSrcSize, 1U << 19);
        U32 const limitedSrcLog = limitedSrcSize > 1 ? ZSTD_v1_5_2_highbit32(limitedSrcSize - 1) + 1 : 1;
        cctxParams.cParams.windowLog = MAX(cctxParams.cParams.windowLog, limitedSrcLog);
    }
    return ZSTD_v1_5_2_compressBegin_internal(cctx,
                                        NULL, 0, ZSTD_v1_5_2_dct_auto, ZSTD_v1_5_2_dtlm_fast,
                                        cdict,
                                        &cctxParams, pledgedSrcSize,
                                        ZSTDb_not_buffered);
}


/* ZSTD_v1_5_2_compressBegin_usingCDict_advanced() :
 * This function is DEPRECATED.
 * cdict must be != NULL */
size_t ZSTD_v1_5_2_compressBegin_usingCDict_advanced(
    ZSTD_v1_5_2_CCtx* const cctx, const ZSTD_v1_5_2_CDict* const cdict,
    ZSTD_v1_5_2_frameParameters const fParams, unsigned long long const pledgedSrcSize)
{
    return ZSTD_v1_5_2_compressBegin_usingCDict_internal(cctx, cdict, fParams, pledgedSrcSize);
}

/* ZSTD_v1_5_2_compressBegin_usingCDict() :
 * cdict must be != NULL */
size_t ZSTD_v1_5_2_compressBegin_usingCDict(ZSTD_v1_5_2_CCtx* cctx, const ZSTD_v1_5_2_CDict* cdict)
{
    ZSTD_v1_5_2_frameParameters const fParams = { 0 /*content*/, 0 /*checksum*/, 0 /*noDictID*/ };
    return ZSTD_v1_5_2_compressBegin_usingCDict_internal(cctx, cdict, fParams, ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN);
}

/*! ZSTD_v1_5_2_compress_usingCDict_internal():
 * Implementation of various ZSTD_v1_5_2_compress_usingCDict* functions.
 */
static size_t ZSTD_v1_5_2_compress_usingCDict_internal(ZSTD_v1_5_2_CCtx* cctx,
                                void* dst, size_t dstCapacity,
                                const void* src, size_t srcSize,
                                const ZSTD_v1_5_2_CDict* cdict, ZSTD_v1_5_2_frameParameters fParams)
{
    FORWARD_IF_ERROR(ZSTD_v1_5_2_compressBegin_usingCDict_internal(cctx, cdict, fParams, srcSize), ""); /* will check if cdict != NULL */
    return ZSTD_v1_5_2_compressEnd(cctx, dst, dstCapacity, src, srcSize);
}

/*! ZSTD_v1_5_2_compress_usingCDict_advanced():
 * This function is DEPRECATED.
 */
size_t ZSTD_v1_5_2_compress_usingCDict_advanced(ZSTD_v1_5_2_CCtx* cctx,
                                void* dst, size_t dstCapacity,
                                const void* src, size_t srcSize,
                                const ZSTD_v1_5_2_CDict* cdict, ZSTD_v1_5_2_frameParameters fParams)
{
    return ZSTD_v1_5_2_compress_usingCDict_internal(cctx, dst, dstCapacity, src, srcSize, cdict, fParams);
}

/*! ZSTD_v1_5_2_compress_usingCDict() :
 *  Compression using a digested Dictionary.
 *  Faster startup than ZSTD_v1_5_2_compress_usingDict(), recommended when same dictionary is used multiple times.
 *  Note that compression parameters are decided at CDict creation time
 *  while frame parameters are hardcoded */
size_t ZSTD_v1_5_2_compress_usingCDict(ZSTD_v1_5_2_CCtx* cctx,
                                void* dst, size_t dstCapacity,
                                const void* src, size_t srcSize,
                                const ZSTD_v1_5_2_CDict* cdict)
{
    ZSTD_v1_5_2_frameParameters const fParams = { 1 /*content*/, 0 /*checksum*/, 0 /*noDictID*/ };
    return ZSTD_v1_5_2_compress_usingCDict_internal(cctx, dst, dstCapacity, src, srcSize, cdict, fParams);
}



/* ******************************************************************
*  Streaming
********************************************************************/

ZSTD_v1_5_2_CStream* ZSTD_v1_5_2_createCStream(void)
{
    DEBUGLOG(3, "ZSTD_v1_5_2_createCStream");
    return ZSTD_v1_5_2_createCStream_advanced(ZSTD_v1_5_2_defaultCMem);
}

ZSTD_v1_5_2_CStream* ZSTD_v1_5_2_initStaticCStream(void *workspace, size_t workspaceSize)
{
    return ZSTD_v1_5_2_initStaticCCtx(workspace, workspaceSize);
}

ZSTD_v1_5_2_CStream* ZSTD_v1_5_2_createCStream_advanced(ZSTD_v1_5_2_customMem customMem)
{   /* CStream and CCtx are now same object */
    return ZSTD_v1_5_2_createCCtx_advanced(customMem);
}

size_t ZSTD_v1_5_2_freeCStream(ZSTD_v1_5_2_CStream* zcs)
{
    return ZSTD_v1_5_2_freeCCtx(zcs);   /* same object */
}



/*======   Initialization   ======*/

size_t ZSTD_v1_5_2_CStreamInSize(void)  { return ZSTD_v1_5_2_BLOCKSIZE_MAX; }

size_t ZSTD_v1_5_2_CStreamOutSize(void)
{
    return ZSTD_v1_5_2_compressBound(ZSTD_v1_5_2_BLOCKSIZE_MAX) + ZSTD_v1_5_2_blockHeaderSize + 4 /* 32-bits hash */ ;
}

static ZSTD_v1_5_2_cParamMode_e ZSTD_v1_5_2_getCParamMode(ZSTD_v1_5_2_CDict const* cdict, ZSTD_v1_5_2_CCtx_params const* params, U64 pledgedSrcSize)
{
    if (cdict != NULL && ZSTD_v1_5_2_shouldAttachDict(cdict, params, pledgedSrcSize))
        return ZSTD_v1_5_2_cpm_attachDict;
    else
        return ZSTD_v1_5_2_cpm_noAttachDict;
}

/* ZSTD_v1_5_2_resetCStream():
 * pledgedSrcSize == 0 means "unknown" */
size_t ZSTD_v1_5_2_resetCStream(ZSTD_v1_5_2_CStream* zcs, unsigned long long pss)
{
    /* temporary : 0 interpreted as "unknown" during transition period.
     * Users willing to specify "unknown" **must** use ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN.
     * 0 will be interpreted as "empty" in the future.
     */
    U64 const pledgedSrcSize = (pss==0) ? ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN : pss;
    DEBUGLOG(4, "ZSTD_v1_5_2_resetCStream: pledgedSrcSize = %u", (unsigned)pledgedSrcSize);
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_setPledgedSrcSize(zcs, pledgedSrcSize) , "");
    return 0;
}

/*! ZSTD_v1_5_2_initCStream_internal() :
 *  Note : for lib/compress only. Used by zstdmt_compress.c.
 *  Assumption 1 : params are valid
 *  Assumption 2 : either dict, or cdict, is defined, not both */
size_t ZSTD_v1_5_2_initCStream_internal(ZSTD_v1_5_2_CStream* zcs,
                    const void* dict, size_t dictSize, const ZSTD_v1_5_2_CDict* cdict,
                    const ZSTD_v1_5_2_CCtx_params* params,
                    unsigned long long pledgedSrcSize)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_initCStream_internal");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_setPledgedSrcSize(zcs, pledgedSrcSize) , "");
    assert(!ZSTD_v1_5_2_isError(ZSTD_v1_5_2_checkCParams(params->cParams)));
    zcs->requestedParams = *params;
    assert(!((dict) && (cdict)));  /* either dict or cdict, not both */
    if (dict) {
        FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_loadDictionary(zcs, dict, dictSize) , "");
    } else {
        /* Dictionary is cleared if !cdict */
        FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_refCDict(zcs, cdict) , "");
    }
    return 0;
}

/* ZSTD_v1_5_2_initCStream_usingCDict_advanced() :
 * same as ZSTD_v1_5_2_initCStream_usingCDict(), with control over frame parameters */
size_t ZSTD_v1_5_2_initCStream_usingCDict_advanced(ZSTD_v1_5_2_CStream* zcs,
                                            const ZSTD_v1_5_2_CDict* cdict,
                                            ZSTD_v1_5_2_frameParameters fParams,
                                            unsigned long long pledgedSrcSize)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_initCStream_usingCDict_advanced");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_setPledgedSrcSize(zcs, pledgedSrcSize) , "");
    zcs->requestedParams.fParams = fParams;
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_refCDict(zcs, cdict) , "");
    return 0;
}

/* note : cdict must outlive compression session */
size_t ZSTD_v1_5_2_initCStream_usingCDict(ZSTD_v1_5_2_CStream* zcs, const ZSTD_v1_5_2_CDict* cdict)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_initCStream_usingCDict");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_refCDict(zcs, cdict) , "");
    return 0;
}


/* ZSTD_v1_5_2_initCStream_advanced() :
 * pledgedSrcSize must be exact.
 * if srcSize is not known at init time, use value ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN.
 * dict is loaded with default parameters ZSTD_v1_5_2_dct_auto and ZSTD_v1_5_2_dlm_byCopy. */
size_t ZSTD_v1_5_2_initCStream_advanced(ZSTD_v1_5_2_CStream* zcs,
                                 const void* dict, size_t dictSize,
                                 ZSTD_v1_5_2_parameters params, unsigned long long pss)
{
    /* for compatibility with older programs relying on this behavior.
     * Users should now specify ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN.
     * This line will be removed in the future.
     */
    U64 const pledgedSrcSize = (pss==0 && params.fParams.contentSizeFlag==0) ? ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN : pss;
    DEBUGLOG(4, "ZSTD_v1_5_2_initCStream_advanced");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_setPledgedSrcSize(zcs, pledgedSrcSize) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_checkCParams(params.cParams) , "");
    ZSTD_v1_5_2_CCtxParams_setZstdParams(&zcs->requestedParams, &params);
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_loadDictionary(zcs, dict, dictSize) , "");
    return 0;
}

size_t ZSTD_v1_5_2_initCStream_usingDict(ZSTD_v1_5_2_CStream* zcs, const void* dict, size_t dictSize, int compressionLevel)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_initCStream_usingDict");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_setParameter(zcs, ZSTD_v1_5_2_c_compressionLevel, compressionLevel) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_loadDictionary(zcs, dict, dictSize) , "");
    return 0;
}

size_t ZSTD_v1_5_2_initCStream_srcSize(ZSTD_v1_5_2_CStream* zcs, int compressionLevel, unsigned long long pss)
{
    /* temporary : 0 interpreted as "unknown" during transition period.
     * Users willing to specify "unknown" **must** use ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN.
     * 0 will be interpreted as "empty" in the future.
     */
    U64 const pledgedSrcSize = (pss==0) ? ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN : pss;
    DEBUGLOG(4, "ZSTD_v1_5_2_initCStream_srcSize");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_refCDict(zcs, NULL) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_setParameter(zcs, ZSTD_v1_5_2_c_compressionLevel, compressionLevel) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_setPledgedSrcSize(zcs, pledgedSrcSize) , "");
    return 0;
}

size_t ZSTD_v1_5_2_initCStream(ZSTD_v1_5_2_CStream* zcs, int compressionLevel)
{
    DEBUGLOG(4, "ZSTD_v1_5_2_initCStream");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_refCDict(zcs, NULL) , "");
    FORWARD_IF_ERROR( ZSTD_v1_5_2_CCtx_setParameter(zcs, ZSTD_v1_5_2_c_compressionLevel, compressionLevel) , "");
    return 0;
}

/*======   Compression   ======*/

static size_t ZSTD_v1_5_2_nextInputSizeHint(const ZSTD_v1_5_2_CCtx* cctx)
{
    size_t hintInSize = cctx->inBuffTarget - cctx->inBuffPos;
    if (hintInSize==0) hintInSize = cctx->blockSize;
    return hintInSize;
}

/** ZSTD_v1_5_2_compressStream_generic():
 *  internal function for all *compressStream*() variants
 *  non-static, because can be called from zstdmt_compress.c
 * @return : hint size for next input */
static size_t ZSTD_v1_5_2_compressStream_generic(ZSTD_v1_5_2_CStream* zcs,
                                          ZSTD_v1_5_2_outBuffer* output,
                                          ZSTD_v1_5_2_inBuffer* input,
                                          ZSTD_v1_5_2_EndDirective const flushMode)
{
    const char* const istart = (const char*)input->src;
    const char* const iend = input->size != 0 ? istart + input->size : istart;
    const char* ip = input->pos != 0 ? istart + input->pos : istart;
    char* const ostart = (char*)output->dst;
    char* const oend = output->size != 0 ? ostart + output->size : ostart;
    char* op = output->pos != 0 ? ostart + output->pos : ostart;
    U32 someMoreWork = 1;

    /* check expectations */
    DEBUGLOG(5, "ZSTD_v1_5_2_compressStream_generic, flush=%u", (unsigned)flushMode);
    if (zcs->appliedParams.inBufferMode == ZSTD_v1_5_2_bm_buffered) {
        assert(zcs->inBuff != NULL);
        assert(zcs->inBuffSize > 0);
    }
    if (zcs->appliedParams.outBufferMode == ZSTD_v1_5_2_bm_buffered) {
        assert(zcs->outBuff !=  NULL);
        assert(zcs->outBuffSize > 0);
    }
    assert(output->pos <= output->size);
    assert(input->pos <= input->size);
    assert((U32)flushMode <= (U32)ZSTD_v1_5_2_e_end);

    while (someMoreWork) {
        switch(zcs->streamStage)
        {
        case zcss_init:
            RETURN_ERROR(init_missing, "call ZSTD_v1_5_2_initCStream() first!");

        case zcss_load:
            if ( (flushMode == ZSTD_v1_5_2_e_end)
              && ( (size_t)(oend-op) >= ZSTD_v1_5_2_compressBound(iend-ip)     /* Enough output space */
                || zcs->appliedParams.outBufferMode == ZSTD_v1_5_2_bm_stable)  /* OR we are allowed to return dstSizeTooSmall */
              && (zcs->inBuffPos == 0) ) {
                /* shortcut to compression pass directly into output buffer */
                size_t const cSize = ZSTD_v1_5_2_compressEnd(zcs,
                                                op, oend-op, ip, iend-ip);
                DEBUGLOG(4, "ZSTD_v1_5_2_compressEnd : cSize=%u", (unsigned)cSize);
                FORWARD_IF_ERROR(cSize, "ZSTD_v1_5_2_compressEnd failed");
                ip = iend;
                op += cSize;
                zcs->frameEnded = 1;
                ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only);
                someMoreWork = 0; break;
            }
            /* complete loading into inBuffer in buffered mode */
            if (zcs->appliedParams.inBufferMode == ZSTD_v1_5_2_bm_buffered) {
                size_t const toLoad = zcs->inBuffTarget - zcs->inBuffPos;
                size_t const loaded = ZSTD_v1_5_2_limitCopy(
                                        zcs->inBuff + zcs->inBuffPos, toLoad,
                                        ip, iend-ip);
                zcs->inBuffPos += loaded;
                if (loaded != 0)
                    ip += loaded;
                if ( (flushMode == ZSTD_v1_5_2_e_continue)
                  && (zcs->inBuffPos < zcs->inBuffTarget) ) {
                    /* not enough input to fill full block : stop here */
                    someMoreWork = 0; break;
                }
                if ( (flushMode == ZSTD_v1_5_2_e_flush)
                  && (zcs->inBuffPos == zcs->inToCompress) ) {
                    /* empty */
                    someMoreWork = 0; break;
                }
            }
            /* compress current block (note : this stage cannot be stopped in the middle) */
            DEBUGLOG(5, "stream compression stage (flushMode==%u)", flushMode);
            {   int const inputBuffered = (zcs->appliedParams.inBufferMode == ZSTD_v1_5_2_bm_buffered);
                void* cDst;
                size_t cSize;
                size_t oSize = oend-op;
                size_t const iSize = inputBuffered
                    ? zcs->inBuffPos - zcs->inToCompress
                    : MIN((size_t)(iend - ip), zcs->blockSize);
                if (oSize >= ZSTD_v1_5_2_compressBound(iSize) || zcs->appliedParams.outBufferMode == ZSTD_v1_5_2_bm_stable)
                    cDst = op;   /* compress into output buffer, to skip flush stage */
                else
                    cDst = zcs->outBuff, oSize = zcs->outBuffSize;
                if (inputBuffered) {
                    unsigned const lastBlock = (flushMode == ZSTD_v1_5_2_e_end) && (ip==iend);
                    cSize = lastBlock ?
                            ZSTD_v1_5_2_compressEnd(zcs, cDst, oSize,
                                        zcs->inBuff + zcs->inToCompress, iSize) :
                            ZSTD_v1_5_2_compressContinue(zcs, cDst, oSize,
                                        zcs->inBuff + zcs->inToCompress, iSize);
                    FORWARD_IF_ERROR(cSize, "%s", lastBlock ? "ZSTD_v1_5_2_compressEnd failed" : "ZSTD_v1_5_2_compressContinue failed");
                    zcs->frameEnded = lastBlock;
                    /* prepare next block */
                    zcs->inBuffTarget = zcs->inBuffPos + zcs->blockSize;
                    if (zcs->inBuffTarget > zcs->inBuffSize)
                        zcs->inBuffPos = 0, zcs->inBuffTarget = zcs->blockSize;
                    DEBUGLOG(5, "inBuffTarget:%u / inBuffSize:%u",
                            (unsigned)zcs->inBuffTarget, (unsigned)zcs->inBuffSize);
                    if (!lastBlock)
                        assert(zcs->inBuffTarget <= zcs->inBuffSize);
                    zcs->inToCompress = zcs->inBuffPos;
                } else {
                    unsigned const lastBlock = (ip + iSize == iend);
                    assert(flushMode == ZSTD_v1_5_2_e_end /* Already validated */);
                    cSize = lastBlock ?
                            ZSTD_v1_5_2_compressEnd(zcs, cDst, oSize, ip, iSize) :
                            ZSTD_v1_5_2_compressContinue(zcs, cDst, oSize, ip, iSize);
                    /* Consume the input prior to error checking to mirror buffered mode. */
                    if (iSize > 0)
                        ip += iSize;
                    FORWARD_IF_ERROR(cSize, "%s", lastBlock ? "ZSTD_v1_5_2_compressEnd failed" : "ZSTD_v1_5_2_compressContinue failed");
                    zcs->frameEnded = lastBlock;
                    if (lastBlock)
                        assert(ip == iend);
                }
                if (cDst == op) {  /* no need to flush */
                    op += cSize;
                    if (zcs->frameEnded) {
                        DEBUGLOG(5, "Frame completed directly in outBuffer");
                        someMoreWork = 0;
                        ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only);
                    }
                    break;
                }
                zcs->outBuffContentSize = cSize;
                zcs->outBuffFlushedSize = 0;
                zcs->streamStage = zcss_flush; /* pass-through to flush stage */
            }
	    ZSTD_v1_5_2_FALLTHROUGH;
        case zcss_flush:
            DEBUGLOG(5, "flush stage");
            assert(zcs->appliedParams.outBufferMode == ZSTD_v1_5_2_bm_buffered);
            {   size_t const toFlush = zcs->outBuffContentSize - zcs->outBuffFlushedSize;
                size_t const flushed = ZSTD_v1_5_2_limitCopy(op, (size_t)(oend-op),
                            zcs->outBuff + zcs->outBuffFlushedSize, toFlush);
                DEBUGLOG(5, "toFlush: %u into %u ==> flushed: %u",
                            (unsigned)toFlush, (unsigned)(oend-op), (unsigned)flushed);
                if (flushed)
                    op += flushed;
                zcs->outBuffFlushedSize += flushed;
                if (toFlush!=flushed) {
                    /* flush not fully completed, presumably because dst is too small */
                    assert(op==oend);
                    someMoreWork = 0;
                    break;
                }
                zcs->outBuffContentSize = zcs->outBuffFlushedSize = 0;
                if (zcs->frameEnded) {
                    DEBUGLOG(5, "Frame completed on flush");
                    someMoreWork = 0;
                    ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only);
                    break;
                }
                zcs->streamStage = zcss_load;
                break;
            }

        default: /* impossible */
            assert(0);
        }
    }

    input->pos = ip - istart;
    output->pos = op - ostart;
    if (zcs->frameEnded) return 0;
    return ZSTD_v1_5_2_nextInputSizeHint(zcs);
}

static size_t ZSTD_v1_5_2_nextInputSizeHint_MTorST(const ZSTD_v1_5_2_CCtx* cctx)
{
#ifdef ZSTD_v1_5_2_MULTITHREAD
    if (cctx->appliedParams.nbWorkers >= 1) {
        assert(cctx->mtctx != NULL);
        return ZSTDMT_nextInputSizeHint(cctx->mtctx);
    }
#endif
    return ZSTD_v1_5_2_nextInputSizeHint(cctx);

}

size_t ZSTD_v1_5_2_compressStream(ZSTD_v1_5_2_CStream* zcs, ZSTD_v1_5_2_outBuffer* output, ZSTD_v1_5_2_inBuffer* input)
{
    FORWARD_IF_ERROR( ZSTD_v1_5_2_compressStream2(zcs, output, input, ZSTD_v1_5_2_e_continue) , "");
    return ZSTD_v1_5_2_nextInputSizeHint_MTorST(zcs);
}

/* After a compression call set the expected input/output buffer.
 * This is validated at the start of the next compression call.
 */
static void ZSTD_v1_5_2_setBufferExpectations(ZSTD_v1_5_2_CCtx* cctx, ZSTD_v1_5_2_outBuffer const* output, ZSTD_v1_5_2_inBuffer const* input)
{
    if (cctx->appliedParams.inBufferMode == ZSTD_v1_5_2_bm_stable) {
        cctx->expectedInBuffer = *input;
    }
    if (cctx->appliedParams.outBufferMode == ZSTD_v1_5_2_bm_stable) {
        cctx->expectedOutBufferSize = output->size - output->pos;
    }
}

/* Validate that the input/output buffers match the expectations set by
 * ZSTD_v1_5_2_setBufferExpectations.
 */
static size_t ZSTD_v1_5_2_checkBufferStability(ZSTD_v1_5_2_CCtx const* cctx,
                                        ZSTD_v1_5_2_outBuffer const* output,
                                        ZSTD_v1_5_2_inBuffer const* input,
                                        ZSTD_v1_5_2_EndDirective endOp)
{
    if (cctx->appliedParams.inBufferMode == ZSTD_v1_5_2_bm_stable) {
        ZSTD_v1_5_2_inBuffer const expect = cctx->expectedInBuffer;
        if (expect.src != input->src || expect.pos != input->pos || expect.size != input->size)
            RETURN_ERROR(srcBuffer_wrong, "ZSTD_v1_5_2_c_stableInBuffer enabled but input differs!");
        if (endOp != ZSTD_v1_5_2_e_end)
            RETURN_ERROR(srcBuffer_wrong, "ZSTD_v1_5_2_c_stableInBuffer can only be used with ZSTD_v1_5_2_e_end!");
    }
    if (cctx->appliedParams.outBufferMode == ZSTD_v1_5_2_bm_stable) {
        size_t const outBufferSize = output->size - output->pos;
        if (cctx->expectedOutBufferSize != outBufferSize)
            RETURN_ERROR(dstBuffer_wrong, "ZSTD_v1_5_2_c_stableOutBuffer enabled but output size differs!");
    }
    return 0;
}

static size_t ZSTD_v1_5_2_CCtx_init_compressStream2(ZSTD_v1_5_2_CCtx* cctx,
                                             ZSTD_v1_5_2_EndDirective endOp,
                                             size_t inSize) {
    ZSTD_v1_5_2_CCtx_params params = cctx->requestedParams;
    ZSTD_v1_5_2_prefixDict const prefixDict = cctx->prefixDict;
    FORWARD_IF_ERROR( ZSTD_v1_5_2_initLocalDict(cctx) , ""); /* Init the local dict if present. */
    ZSTD_v1_5_2_memset(&cctx->prefixDict, 0, sizeof(cctx->prefixDict));   /* single usage */
    assert(prefixDict.dict==NULL || cctx->cdict==NULL);    /* only one can be set */
    if (cctx->cdict && !cctx->localDict.cdict) {
        /* Let the cdict's compression level take priority over the requested params.
         * But do not take the cdict's compression level if the "cdict" is actually a localDict
         * generated from ZSTD_v1_5_2_initLocalDict().
         */
        params.compressionLevel = cctx->cdict->compressionLevel;
    }
    DEBUGLOG(4, "ZSTD_v1_5_2_compressStream2 : transparent init stage");
    if (endOp == ZSTD_v1_5_2_e_end) cctx->pledgedSrcSizePlusOne = inSize + 1;  /* auto-fix pledgedSrcSize */
    {
        size_t const dictSize = prefixDict.dict
                ? prefixDict.dictSize
                : (cctx->cdict ? cctx->cdict->dictContentSize : 0);
        ZSTD_v1_5_2_cParamMode_e const mode = ZSTD_v1_5_2_getCParamMode(cctx->cdict, &params, cctx->pledgedSrcSizePlusOne - 1);
        params.cParams = ZSTD_v1_5_2_getCParamsFromCCtxParams(
                &params, cctx->pledgedSrcSizePlusOne-1,
                dictSize, mode);
    }

    params.useBlockSplitter = ZSTD_v1_5_2_resolveBlockSplitterMode(params.useBlockSplitter, &params.cParams);
    params.ldmParams.enableLdm = ZSTD_v1_5_2_resolveEnableLdm(params.ldmParams.enableLdm, &params.cParams);
    params.useRowMatchFinder = ZSTD_v1_5_2_resolveRowMatchFinderMode(params.useRowMatchFinder, &params.cParams);

#ifdef ZSTD_v1_5_2_MULTITHREAD
    if ((cctx->pledgedSrcSizePlusOne-1) <= ZSTDMT_JOBSIZE_MIN) {
        params.nbWorkers = 0; /* do not invoke multi-threading when src size is too small */
    }
    if (params.nbWorkers > 0) {
#if ZSTD_v1_5_2_TRACE
        cctx->traceCtx = (ZSTD_v1_5_2_trace_compress_begin != NULL) ? ZSTD_v1_5_2_trace_compress_begin(cctx) : 0;
#endif
        /* mt context creation */
        if (cctx->mtctx == NULL) {
            DEBUGLOG(4, "ZSTD_v1_5_2_compressStream2: creating new mtctx for nbWorkers=%u",
                        params.nbWorkers);
            cctx->mtctx = ZSTDMT_createCCtx_advanced((U32)params.nbWorkers, cctx->customMem, cctx->pool);
            RETURN_ERROR_IF(cctx->mtctx == NULL, memory_allocation, "NULL pointer!");
        }
        /* mt compression */
        DEBUGLOG(4, "call ZSTDMT_initCStream_internal as nbWorkers=%u", params.nbWorkers);
        FORWARD_IF_ERROR( ZSTDMT_initCStream_internal(
                    cctx->mtctx,
                    prefixDict.dict, prefixDict.dictSize, prefixDict.dictContentType,
                    cctx->cdict, params, cctx->pledgedSrcSizePlusOne-1) , "");
        cctx->dictID = cctx->cdict ? cctx->cdict->dictID : 0;
        cctx->dictContentSize = cctx->cdict ? cctx->cdict->dictContentSize : prefixDict.dictSize;
        cctx->consumedSrcSize = 0;
        cctx->producedCSize = 0;
        cctx->streamStage = zcss_load;
        cctx->appliedParams = params;
    } else
#endif
    {   U64 const pledgedSrcSize = cctx->pledgedSrcSizePlusOne - 1;
        assert(!ZSTD_v1_5_2_isError(ZSTD_v1_5_2_checkCParams(params.cParams)));
        FORWARD_IF_ERROR( ZSTD_v1_5_2_compressBegin_internal(cctx,
                prefixDict.dict, prefixDict.dictSize, prefixDict.dictContentType, ZSTD_v1_5_2_dtlm_fast,
                cctx->cdict,
                &params, pledgedSrcSize,
                ZSTDb_buffered) , "");
        assert(cctx->appliedParams.nbWorkers == 0);
        cctx->inToCompress = 0;
        cctx->inBuffPos = 0;
        if (cctx->appliedParams.inBufferMode == ZSTD_v1_5_2_bm_buffered) {
            /* for small input: avoid automatic flush on reaching end of block, since
            * it would require to add a 3-bytes null block to end frame
            */
            cctx->inBuffTarget = cctx->blockSize + (cctx->blockSize == pledgedSrcSize);
        } else {
            cctx->inBuffTarget = 0;
        }
        cctx->outBuffContentSize = cctx->outBuffFlushedSize = 0;
        cctx->streamStage = zcss_load;
        cctx->frameEnded = 0;
    }
    return 0;
}

size_t ZSTD_v1_5_2_compressStream2( ZSTD_v1_5_2_CCtx* cctx,
                             ZSTD_v1_5_2_outBuffer* output,
                             ZSTD_v1_5_2_inBuffer* input,
                             ZSTD_v1_5_2_EndDirective endOp)
{
    DEBUGLOG(5, "ZSTD_v1_5_2_compressStream2, endOp=%u ", (unsigned)endOp);
    /* check conditions */
    RETURN_ERROR_IF(output->pos > output->size, dstSize_tooSmall, "invalid output buffer");
    RETURN_ERROR_IF(input->pos  > input->size, srcSize_wrong, "invalid input buffer");
    RETURN_ERROR_IF((U32)endOp > (U32)ZSTD_v1_5_2_e_end, parameter_outOfBound, "invalid endDirective");
    assert(cctx != NULL);

    /* transparent initialization stage */
    if (cctx->streamStage == zcss_init) {
        FORWARD_IF_ERROR(ZSTD_v1_5_2_CCtx_init_compressStream2(cctx, endOp, input->size), "CompressStream2 initialization failed");
        ZSTD_v1_5_2_setBufferExpectations(cctx, output, input);    /* Set initial buffer expectations now that we've initialized */
    }
    /* end of transparent initialization stage */

    FORWARD_IF_ERROR(ZSTD_v1_5_2_checkBufferStability(cctx, output, input, endOp), "invalid buffers");
    /* compression stage */
#ifdef ZSTD_v1_5_2_MULTITHREAD
    if (cctx->appliedParams.nbWorkers > 0) {
        size_t flushMin;
        if (cctx->cParamsChanged) {
            ZSTDMT_updateCParams_whileCompressing(cctx->mtctx, &cctx->requestedParams);
            cctx->cParamsChanged = 0;
        }
        for (;;) {
            size_t const ipos = input->pos;
            size_t const opos = output->pos;
            flushMin = ZSTDMT_compressStream_generic(cctx->mtctx, output, input, endOp);
            cctx->consumedSrcSize += (U64)(input->pos - ipos);
            cctx->producedCSize += (U64)(output->pos - opos);
            if ( ZSTD_v1_5_2_isError(flushMin)
              || (endOp == ZSTD_v1_5_2_e_end && flushMin == 0) ) { /* compression completed */
                if (flushMin == 0)
                    ZSTD_v1_5_2_CCtx_trace(cctx, 0);
                ZSTD_v1_5_2_CCtx_reset(cctx, ZSTD_v1_5_2_reset_session_only);
            }
            FORWARD_IF_ERROR(flushMin, "ZSTDMT_compressStream_generic failed");

            if (endOp == ZSTD_v1_5_2_e_continue) {
                /* We only require some progress with ZSTD_v1_5_2_e_continue, not maximal progress.
                 * We're done if we've consumed or produced any bytes, or either buffer is
                 * full.
                 */
                if (input->pos != ipos || output->pos != opos || input->pos == input->size || output->pos == output->size)
                    break;
            } else {
                assert(endOp == ZSTD_v1_5_2_e_flush || endOp == ZSTD_v1_5_2_e_end);
                /* We require maximal progress. We're done when the flush is complete or the
                 * output buffer is full.
                 */
                if (flushMin == 0 || output->pos == output->size)
                    break;
            }
        }
        DEBUGLOG(5, "completed ZSTD_v1_5_2_compressStream2 delegating to ZSTDMT_compressStream_generic");
        /* Either we don't require maximum forward progress, we've finished the
         * flush, or we are out of output space.
         */
        assert(endOp == ZSTD_v1_5_2_e_continue || flushMin == 0 || output->pos == output->size);
        ZSTD_v1_5_2_setBufferExpectations(cctx, output, input);
        return flushMin;
    }
#endif
    FORWARD_IF_ERROR( ZSTD_v1_5_2_compressStream_generic(cctx, output, input, endOp) , "");
    DEBUGLOG(5, "completed ZSTD_v1_5_2_compressStream2");
    ZSTD_v1_5_2_setBufferExpectations(cctx, output, input);
    return cctx->outBuffContentSize - cctx->outBuffFlushedSize; /* remaining to flush */
}

size_t ZSTD_v1_5_2_compressStream2_simpleArgs (
                            ZSTD_v1_5_2_CCtx* cctx,
                            void* dst, size_t dstCapacity, size_t* dstPos,
                      const void* src, size_t srcSize, size_t* srcPos,
                            ZSTD_v1_5_2_EndDirective endOp)
{
    ZSTD_v1_5_2_outBuffer output = { dst, dstCapacity, *dstPos };
    ZSTD_v1_5_2_inBuffer  input  = { src, srcSize, *srcPos };
    /* ZSTD_v1_5_2_compressStream2() will check validity of dstPos and srcPos */
    size_t const cErr = ZSTD_v1_5_2_compressStream2(cctx, &output, &input, endOp);
    *dstPos = output.pos;
    *srcPos = input.pos;
    return cErr;
}

size_t ZSTD_v1_5_2_compress2(ZSTD_v1_5_2_CCtx* cctx,
                      void* dst, size_t dstCapacity,
                      const void* src, size_t srcSize)
{
    ZSTD_v1_5_2_bufferMode_e const originalInBufferMode = cctx->requestedParams.inBufferMode;
    ZSTD_v1_5_2_bufferMode_e const originalOutBufferMode = cctx->requestedParams.outBufferMode;
    DEBUGLOG(4, "ZSTD_v1_5_2_compress2 (srcSize=%u)", (unsigned)srcSize);
    ZSTD_v1_5_2_CCtx_reset(cctx, ZSTD_v1_5_2_reset_session_only);
    /* Enable stable input/output buffers. */
    cctx->requestedParams.inBufferMode = ZSTD_v1_5_2_bm_stable;
    cctx->requestedParams.outBufferMode = ZSTD_v1_5_2_bm_stable;
    {   size_t oPos = 0;
        size_t iPos = 0;
        size_t const result = ZSTD_v1_5_2_compressStream2_simpleArgs(cctx,
                                        dst, dstCapacity, &oPos,
                                        src, srcSize, &iPos,
                                        ZSTD_v1_5_2_e_end);
        /* Reset to the original values. */
        cctx->requestedParams.inBufferMode = originalInBufferMode;
        cctx->requestedParams.outBufferMode = originalOutBufferMode;
        FORWARD_IF_ERROR(result, "ZSTD_v1_5_2_compressStream2_simpleArgs failed");
        if (result != 0) {  /* compression not completed, due to lack of output space */
            assert(oPos == dstCapacity);
            RETURN_ERROR(dstSize_tooSmall, "");
        }
        assert(iPos == srcSize);   /* all input is expected consumed */
        return oPos;
    }
}

typedef struct {
    U32 idx;             /* Index in array of ZSTD_v1_5_2_Sequence */
    U32 posInSequence;   /* Position within sequence at idx */
    size_t posInSrc;        /* Number of bytes given by sequences provided so far */
} ZSTD_v1_5_2_sequencePosition;

/* ZSTD_v1_5_2_validateSequence() :
 * @offCode : is presumed to follow format required by ZSTD_v1_5_2_storeSeq()
 * @returns a ZSTD error code if sequence is not valid
 */
static size_t
ZSTD_v1_5_2_validateSequence(U32 offCode, U32 matchLength,
                      size_t posInSrc, U32 windowLog, size_t dictSize)
{
    U32 const windowSize = 1 << windowLog;
    /* posInSrc represents the amount of data the the decoder would decode up to this point.
     * As long as the amount of data decoded is less than or equal to window size, offsets may be
     * larger than the total length of output decoded in order to reference the dict, even larger than
     * window size. After output surpasses windowSize, we're limited to windowSize offsets again.
     */
    size_t const offsetBound = posInSrc > windowSize ? (size_t)windowSize : posInSrc + (size_t)dictSize;
    RETURN_ERROR_IF(offCode > STORE_OFFSET(offsetBound), corruption_detected, "Offset too large!");
    RETURN_ERROR_IF(matchLength < MINMATCH, corruption_detected, "Matchlength too small");
    return 0;
}

/* Returns an offset code, given a sequence's raw offset, the ongoing repcode array, and whether litLength == 0 */
static U32 ZSTD_v1_5_2_finalizeOffCode(U32 rawOffset, const U32 rep[ZSTD_v1_5_2_REP_NUM], U32 ll0)
{
    U32 offCode = STORE_OFFSET(rawOffset);

    if (!ll0 && rawOffset == rep[0]) {
        offCode = STORE_REPCODE_1;
    } else if (rawOffset == rep[1]) {
        offCode = STORE_REPCODE(2 - ll0);
    } else if (rawOffset == rep[2]) {
        offCode = STORE_REPCODE(3 - ll0);
    } else if (ll0 && rawOffset == rep[0] - 1) {
        offCode = STORE_REPCODE_3;
    }
    return offCode;
}

/* Returns 0 on success, and a ZSTD_v1_5_2_error otherwise. This function scans through an array of
 * ZSTD_v1_5_2_Sequence, storing the sequences it finds, until it reaches a block delimiter.
 */
static size_t
ZSTD_v1_5_2_copySequencesToSeqStoreExplicitBlockDelim(ZSTD_v1_5_2_CCtx* cctx,
                                              ZSTD_v1_5_2_sequencePosition* seqPos,
                                        const ZSTD_v1_5_2_Sequence* const inSeqs, size_t inSeqsSize,
                                        const void* src, size_t blockSize)
{
    U32 idx = seqPos->idx;
    BYTE const* ip = (BYTE const*)(src);
    const BYTE* const iend = ip + blockSize;
    repcodes_t updatedRepcodes;
    U32 dictSize;

    if (cctx->cdict) {
        dictSize = (U32)cctx->cdict->dictContentSize;
    } else if (cctx->prefixDict.dict) {
        dictSize = (U32)cctx->prefixDict.dictSize;
    } else {
        dictSize = 0;
    }
    ZSTD_v1_5_2_memcpy(updatedRepcodes.rep, cctx->blockState.prevCBlock->rep, sizeof(repcodes_t));
    for (; (inSeqs[idx].matchLength != 0 || inSeqs[idx].offset != 0) && idx < inSeqsSize; ++idx) {
        U32 const litLength = inSeqs[idx].litLength;
        U32 const ll0 = (litLength == 0);
        U32 const matchLength = inSeqs[idx].matchLength;
        U32 const offCode = ZSTD_v1_5_2_finalizeOffCode(inSeqs[idx].offset, updatedRepcodes.rep, ll0);
        ZSTD_v1_5_2_updateRep(updatedRepcodes.rep, offCode, ll0);

        DEBUGLOG(6, "Storing sequence: (of: %u, ml: %u, ll: %u)", offCode, matchLength, litLength);
        if (cctx->appliedParams.validateSequences) {
            seqPos->posInSrc += litLength + matchLength;
            FORWARD_IF_ERROR(ZSTD_v1_5_2_validateSequence(offCode, matchLength, seqPos->posInSrc,
                                                cctx->appliedParams.cParams.windowLog, dictSize),
                                                "Sequence validation failed");
        }
        RETURN_ERROR_IF(idx - seqPos->idx > cctx->seqStore.maxNbSeq, memory_allocation,
                        "Not enough memory allocated. Try adjusting ZSTD_v1_5_2_c_minMatch.");
        ZSTD_v1_5_2_storeSeq(&cctx->seqStore, litLength, ip, iend, offCode, matchLength);
        ip += matchLength + litLength;
    }
    ZSTD_v1_5_2_memcpy(cctx->blockState.nextCBlock->rep, updatedRepcodes.rep, sizeof(repcodes_t));

    if (inSeqs[idx].litLength) {
        DEBUGLOG(6, "Storing last literals of size: %u", inSeqs[idx].litLength);
        ZSTD_v1_5_2_storeLastLiterals(&cctx->seqStore, ip, inSeqs[idx].litLength);
        ip += inSeqs[idx].litLength;
        seqPos->posInSrc += inSeqs[idx].litLength;
    }
    RETURN_ERROR_IF(ip != iend, corruption_detected, "Blocksize doesn't agree with block delimiter!");
    seqPos->idx = idx+1;
    return 0;
}

/* Returns the number of bytes to move the current read position back by. Only non-zero
 * if we ended up splitting a sequence. Otherwise, it may return a ZSTD error if something
 * went wrong.
 *
 * This function will attempt to scan through blockSize bytes represented by the sequences
 * in inSeqs, storing any (partial) sequences.
 *
 * Occasionally, we may want to change the actual number of bytes we consumed from inSeqs to
 * avoid splitting a match, or to avoid splitting a match such that it would produce a match
 * smaller than MINMATCH. In this case, we return the number of bytes that we didn't read from this block.
 */
static size_t
ZSTD_v1_5_2_copySequencesToSeqStoreNoBlockDelim(ZSTD_v1_5_2_CCtx* cctx, ZSTD_v1_5_2_sequencePosition* seqPos,
                                   const ZSTD_v1_5_2_Sequence* const inSeqs, size_t inSeqsSize,
                                   const void* src, size_t blockSize)
{
    U32 idx = seqPos->idx;
    U32 startPosInSequence = seqPos->posInSequence;
    U32 endPosInSequence = seqPos->posInSequence + (U32)blockSize;
    size_t dictSize;
    BYTE const* ip = (BYTE const*)(src);
    BYTE const* iend = ip + blockSize;  /* May be adjusted if we decide to process fewer than blockSize bytes */
    repcodes_t updatedRepcodes;
    U32 bytesAdjustment = 0;
    U32 finalMatchSplit = 0;

    if (cctx->cdict) {
        dictSize = cctx->cdict->dictContentSize;
    } else if (cctx->prefixDict.dict) {
        dictSize = cctx->prefixDict.dictSize;
    } else {
        dictSize = 0;
    }
    DEBUGLOG(5, "ZSTD_v1_5_2_copySequencesToSeqStore: idx: %u PIS: %u blockSize: %zu", idx, startPosInSequence, blockSize);
    DEBUGLOG(5, "Start seq: idx: %u (of: %u ml: %u ll: %u)", idx, inSeqs[idx].offset, inSeqs[idx].matchLength, inSeqs[idx].litLength);
    ZSTD_v1_5_2_memcpy(updatedRepcodes.rep, cctx->blockState.prevCBlock->rep, sizeof(repcodes_t));
    while (endPosInSequence && idx < inSeqsSize && !finalMatchSplit) {
        const ZSTD_v1_5_2_Sequence currSeq = inSeqs[idx];
        U32 litLength = currSeq.litLength;
        U32 matchLength = currSeq.matchLength;
        U32 const rawOffset = currSeq.offset;
        U32 offCode;

        /* Modify the sequence depending on where endPosInSequence lies */
        if (endPosInSequence >= currSeq.litLength + currSeq.matchLength) {
            if (startPosInSequence >= litLength) {
                startPosInSequence -= litLength;
                litLength = 0;
                matchLength -= startPosInSequence;
            } else {
                litLength -= startPosInSequence;
            }
            /* Move to the next sequence */
            endPosInSequence -= currSeq.litLength + currSeq.matchLength;
            startPosInSequence = 0;
            idx++;
        } else {
            /* This is the final (partial) sequence we're adding from inSeqs, and endPosInSequence
               does not reach the end of the match. So, we have to split the sequence */
            DEBUGLOG(6, "Require a split: diff: %u, idx: %u PIS: %u",
                     currSeq.litLength + currSeq.matchLength - endPosInSequence, idx, endPosInSequence);
            if (endPosInSequence > litLength) {
                U32 firstHalfMatchLength;
                litLength = startPosInSequence >= litLength ? 0 : litLength - startPosInSequence;
                firstHalfMatchLength = endPosInSequence - startPosInSequence - litLength;
                if (matchLength > blockSize && firstHalfMatchLength >= cctx->appliedParams.cParams.minMatch) {
                    /* Only ever split the match if it is larger than the block size */
                    U32 secondHalfMatchLength = currSeq.matchLength + currSeq.litLength - endPosInSequence;
                    if (secondHalfMatchLength < cctx->appliedParams.cParams.minMatch) {
                        /* Move the endPosInSequence backward so that it creates match of minMatch length */
                        endPosInSequence -= cctx->appliedParams.cParams.minMatch - secondHalfMatchLength;
                        bytesAdjustment = cctx->appliedParams.cParams.minMatch - secondHalfMatchLength;
                        firstHalfMatchLength -= bytesAdjustment;
                    }
                    matchLength = firstHalfMatchLength;
                    /* Flag that we split the last match - after storing the sequence, exit the loop,
                       but keep the value of endPosInSequence */
                    finalMatchSplit = 1;
                } else {
                    /* Move the position in sequence backwards so that we don't split match, and break to store
                     * the last literals. We use the original currSeq.litLength as a marker for where endPosInSequence
                     * should go. We prefer to do this whenever it is not necessary to split the match, or if doing so
                     * would cause the first half of the match to be too small
                     */
                    bytesAdjustment = endPosInSequence - currSeq.litLength;
                    endPosInSequence = currSeq.litLength;
                    break;
                }
            } else {
                /* This sequence ends inside the literals, break to store the last literals */
                break;
            }
        }
        /* Check if this offset can be represented with a repcode */
        {   U32 const ll0 = (litLength == 0);
            offCode = ZSTD_v1_5_2_finalizeOffCode(rawOffset, updatedRepcodes.rep, ll0);
            ZSTD_v1_5_2_updateRep(updatedRepcodes.rep, offCode, ll0);
        }

        if (cctx->appliedParams.validateSequences) {
            seqPos->posInSrc += litLength + matchLength;
            FORWARD_IF_ERROR(ZSTD_v1_5_2_validateSequence(offCode, matchLength, seqPos->posInSrc,
                                                   cctx->appliedParams.cParams.windowLog, dictSize),
                                                   "Sequence validation failed");
        }
        DEBUGLOG(6, "Storing sequence: (of: %u, ml: %u, ll: %u)", offCode, matchLength, litLength);
        RETURN_ERROR_IF(idx - seqPos->idx > cctx->seqStore.maxNbSeq, memory_allocation,
                        "Not enough memory allocated. Try adjusting ZSTD_v1_5_2_c_minMatch.");
        ZSTD_v1_5_2_storeSeq(&cctx->seqStore, litLength, ip, iend, offCode, matchLength);
        ip += matchLength + litLength;
    }
    DEBUGLOG(5, "Ending seq: idx: %u (of: %u ml: %u ll: %u)", idx, inSeqs[idx].offset, inSeqs[idx].matchLength, inSeqs[idx].litLength);
    assert(idx == inSeqsSize || endPosInSequence <= inSeqs[idx].litLength + inSeqs[idx].matchLength);
    seqPos->idx = idx;
    seqPos->posInSequence = endPosInSequence;
    ZSTD_v1_5_2_memcpy(cctx->blockState.nextCBlock->rep, updatedRepcodes.rep, sizeof(repcodes_t));

    iend -= bytesAdjustment;
    if (ip != iend) {
        /* Store any last literals */
        U32 lastLLSize = (U32)(iend - ip);
        assert(ip <= iend);
        DEBUGLOG(6, "Storing last literals of size: %u", lastLLSize);
        ZSTD_v1_5_2_storeLastLiterals(&cctx->seqStore, ip, lastLLSize);
        seqPos->posInSrc += lastLLSize;
    }

    return bytesAdjustment;
}

typedef size_t (*ZSTD_v1_5_2_sequenceCopier) (ZSTD_v1_5_2_CCtx* cctx, ZSTD_v1_5_2_sequencePosition* seqPos,
                                       const ZSTD_v1_5_2_Sequence* const inSeqs, size_t inSeqsSize,
                                       const void* src, size_t blockSize);
static ZSTD_v1_5_2_sequenceCopier ZSTD_v1_5_2_selectSequenceCopier(ZSTD_v1_5_2_sequenceFormat_e mode)
{
    ZSTD_v1_5_2_sequenceCopier sequenceCopier = NULL;
    assert(ZSTD_v1_5_2_cParam_withinBounds(ZSTD_v1_5_2_c_blockDelimiters, mode));
    if (mode == ZSTD_v1_5_2_sf_explicitBlockDelimiters) {
        return ZSTD_v1_5_2_copySequencesToSeqStoreExplicitBlockDelim;
    } else if (mode == ZSTD_v1_5_2_sf_noBlockDelimiters) {
        return ZSTD_v1_5_2_copySequencesToSeqStoreNoBlockDelim;
    }
    assert(sequenceCopier != NULL);
    return sequenceCopier;
}

/* Compress, block-by-block, all of the sequences given.
 *
 * Returns the cumulative size of all compressed blocks (including their headers),
 * otherwise a ZSTD error.
 */
static size_t
ZSTD_v1_5_2_compressSequences_internal(ZSTD_v1_5_2_CCtx* cctx,
                                void* dst, size_t dstCapacity,
                          const ZSTD_v1_5_2_Sequence* inSeqs, size_t inSeqsSize,
                          const void* src, size_t srcSize)
{
    size_t cSize = 0;
    U32 lastBlock;
    size_t blockSize;
    size_t compressedSeqsSize;
    size_t remaining = srcSize;
    ZSTD_v1_5_2_sequencePosition seqPos = {0, 0, 0};

    BYTE const* ip = (BYTE const*)src;
    BYTE* op = (BYTE*)dst;
    ZSTD_v1_5_2_sequenceCopier const sequenceCopier = ZSTD_v1_5_2_selectSequenceCopier(cctx->appliedParams.blockDelimiters);

    DEBUGLOG(4, "ZSTD_v1_5_2_compressSequences_internal srcSize: %zu, inSeqsSize: %zu", srcSize, inSeqsSize);
    /* Special case: empty frame */
    if (remaining == 0) {
        U32 const cBlockHeader24 = 1 /* last block */ + (((U32)bt_raw)<<1);
        RETURN_ERROR_IF(dstCapacity<4, dstSize_tooSmall, "No room for empty frame block header");
        MEM_writeLE32(op, cBlockHeader24);
        op += ZSTD_v1_5_2_blockHeaderSize;
        dstCapacity -= ZSTD_v1_5_2_blockHeaderSize;
        cSize += ZSTD_v1_5_2_blockHeaderSize;
    }

    while (remaining) {
        size_t cBlockSize;
        size_t additionalByteAdjustment;
        lastBlock = remaining <= cctx->blockSize;
        blockSize = lastBlock ? (U32)remaining : (U32)cctx->blockSize;
        ZSTD_v1_5_2_resetSeqStore(&cctx->seqStore);
        DEBUGLOG(4, "Working on new block. Blocksize: %zu", blockSize);

        additionalByteAdjustment = sequenceCopier(cctx, &seqPos, inSeqs, inSeqsSize, ip, blockSize);
        FORWARD_IF_ERROR(additionalByteAdjustment, "Bad sequence copy");
        blockSize -= additionalByteAdjustment;

        /* If blocks are too small, emit as a nocompress block */
        if (blockSize < MIN_CBLOCK_SIZE+ZSTD_v1_5_2_blockHeaderSize+1) {
            cBlockSize = ZSTD_v1_5_2_noCompressBlock(op, dstCapacity, ip, blockSize, lastBlock);
            FORWARD_IF_ERROR(cBlockSize, "Nocompress block failed");
            DEBUGLOG(4, "Block too small, writing out nocompress block: cSize: %zu", cBlockSize);
            cSize += cBlockSize;
            ip += blockSize;
            op += cBlockSize;
            remaining -= blockSize;
            dstCapacity -= cBlockSize;
            continue;
        }

        compressedSeqsSize = ZSTD_v1_5_2_entropyCompressSeqStore(&cctx->seqStore,
                                &cctx->blockState.prevCBlock->entropy, &cctx->blockState.nextCBlock->entropy,
                                &cctx->appliedParams,
                                op + ZSTD_v1_5_2_blockHeaderSize /* Leave space for block header */, dstCapacity - ZSTD_v1_5_2_blockHeaderSize,
                                blockSize,
                                cctx->entropyWorkspace, ENTROPY_WORKSPACE_SIZE /* statically allocated in resetCCtx */,
                                cctx->bmi2);
        FORWARD_IF_ERROR(compressedSeqsSize, "Compressing sequences of block failed");
        DEBUGLOG(4, "Compressed sequences size: %zu", compressedSeqsSize);

        if (!cctx->isFirstBlock &&
            ZSTD_v1_5_2_maybeRLE(&cctx->seqStore) &&
            ZSTD_v1_5_2_isRLE((BYTE const*)src, srcSize)) {
            /* We don't want to emit our first block as a RLE even if it qualifies because
            * doing so will cause the decoder (cli only) to throw a "should consume all input error."
            * This is only an issue for zstd <= v1.4.3
            */
            compressedSeqsSize = 1;
        }

        if (compressedSeqsSize == 0) {
            /* ZSTD_v1_5_2_noCompressBlock writes the block header as well */
            cBlockSize = ZSTD_v1_5_2_noCompressBlock(op, dstCapacity, ip, blockSize, lastBlock);
            FORWARD_IF_ERROR(cBlockSize, "Nocompress block failed");
            DEBUGLOG(4, "Writing out nocompress block, size: %zu", cBlockSize);
        } else if (compressedSeqsSize == 1) {
            cBlockSize = ZSTD_v1_5_2_rleCompressBlock(op, dstCapacity, *ip, blockSize, lastBlock);
            FORWARD_IF_ERROR(cBlockSize, "RLE compress block failed");
            DEBUGLOG(4, "Writing out RLE block, size: %zu", cBlockSize);
        } else {
            U32 cBlockHeader;
            /* Error checking and repcodes update */
            ZSTD_v1_5_2_blockState_confirmRepcodesAndEntropyTables(&cctx->blockState);
            if (cctx->blockState.prevCBlock->entropy.fse.offcode_repeatMode == FSE_v1_5_2_repeat_valid)
                cctx->blockState.prevCBlock->entropy.fse.offcode_repeatMode = FSE_v1_5_2_repeat_check;

            /* Write block header into beginning of block*/
            cBlockHeader = lastBlock + (((U32)bt_compressed)<<1) + (U32)(compressedSeqsSize << 3);
            MEM_writeLE24(op, cBlockHeader);
            cBlockSize = ZSTD_v1_5_2_blockHeaderSize + compressedSeqsSize;
            DEBUGLOG(4, "Writing out compressed block, size: %zu", cBlockSize);
        }

        cSize += cBlockSize;
        DEBUGLOG(4, "cSize running total: %zu", cSize);

        if (lastBlock) {
            break;
        } else {
            ip += blockSize;
            op += cBlockSize;
            remaining -= blockSize;
            dstCapacity -= cBlockSize;
            cctx->isFirstBlock = 0;
        }
    }

    return cSize;
}

size_t ZSTD_v1_5_2_compressSequences(ZSTD_v1_5_2_CCtx* const cctx, void* dst, size_t dstCapacity,
                              const ZSTD_v1_5_2_Sequence* inSeqs, size_t inSeqsSize,
                              const void* src, size_t srcSize)
{
    BYTE* op = (BYTE*)dst;
    size_t cSize = 0;
    size_t compressedBlocksSize = 0;
    size_t frameHeaderSize = 0;

    /* Transparent initialization stage, same as compressStream2() */
    DEBUGLOG(3, "ZSTD_v1_5_2_compressSequences()");
    assert(cctx != NULL);
    FORWARD_IF_ERROR(ZSTD_v1_5_2_CCtx_init_compressStream2(cctx, ZSTD_v1_5_2_e_end, srcSize), "CCtx initialization failed");
    /* Begin writing output, starting with frame header */
    frameHeaderSize = ZSTD_v1_5_2_writeFrameHeader(op, dstCapacity, &cctx->appliedParams, srcSize, cctx->dictID);
    op += frameHeaderSize;
    dstCapacity -= frameHeaderSize;
    cSize += frameHeaderSize;
    if (cctx->appliedParams.fParams.checksumFlag && srcSize) {
        XXH64_update(&cctx->xxhState, src, srcSize);
    }
    /* cSize includes block header size and compressed sequences size */
    compressedBlocksSize = ZSTD_v1_5_2_compressSequences_internal(cctx,
                                                           op, dstCapacity,
                                                           inSeqs, inSeqsSize,
                                                           src, srcSize);
    FORWARD_IF_ERROR(compressedBlocksSize, "Compressing blocks failed!");
    cSize += compressedBlocksSize;
    dstCapacity -= compressedBlocksSize;

    if (cctx->appliedParams.fParams.checksumFlag) {
        U32 const checksum = (U32) XXH64_digest(&cctx->xxhState);
        RETURN_ERROR_IF(dstCapacity<4, dstSize_tooSmall, "no room for checksum");
        DEBUGLOG(4, "Write checksum : %08X", (unsigned)checksum);
        MEM_writeLE32((char*)dst + cSize, checksum);
        cSize += 4;
    }

    DEBUGLOG(3, "Final compressed size: %zu", cSize);
    return cSize;
}

/*======   Finalize   ======*/

/*! ZSTD_v1_5_2_flushStream() :
 * @return : amount of data remaining to flush */
size_t ZSTD_v1_5_2_flushStream(ZSTD_v1_5_2_CStream* zcs, ZSTD_v1_5_2_outBuffer* output)
{
    ZSTD_v1_5_2_inBuffer input = { NULL, 0, 0 };
    return ZSTD_v1_5_2_compressStream2(zcs, output, &input, ZSTD_v1_5_2_e_flush);
}


size_t ZSTD_v1_5_2_endStream(ZSTD_v1_5_2_CStream* zcs, ZSTD_v1_5_2_outBuffer* output)
{
    ZSTD_v1_5_2_inBuffer input = { NULL, 0, 0 };
    size_t const remainingToFlush = ZSTD_v1_5_2_compressStream2(zcs, output, &input, ZSTD_v1_5_2_e_end);
    FORWARD_IF_ERROR( remainingToFlush , "ZSTD_v1_5_2_compressStream2 failed");
    if (zcs->appliedParams.nbWorkers > 0) return remainingToFlush;   /* minimal estimation */
    /* single thread mode : attempt to calculate remaining to flush more precisely */
    {   size_t const lastBlockSize = zcs->frameEnded ? 0 : ZSTD_v1_5_2_BLOCKHEADERSIZE;
        size_t const checksumSize = (size_t)(zcs->frameEnded ? 0 : zcs->appliedParams.fParams.checksumFlag * 4);
        size_t const toFlush = remainingToFlush + lastBlockSize + checksumSize;
        DEBUGLOG(4, "ZSTD_v1_5_2_endStream : remaining to flush : %u", (unsigned)toFlush);
        return toFlush;
    }
}


/*-=====  Pre-defined compression levels  =====-*/
#include "clevels.h"

int ZSTD_v1_5_2_maxCLevel(void) { return ZSTD_v1_5_2_MAX_CLEVEL; }
int ZSTD_v1_5_2_minCLevel(void) { return (int)-ZSTD_v1_5_2_TARGETLENGTH_MAX; }
int ZSTD_v1_5_2_defaultCLevel(void) { return ZSTD_v1_5_2_CLEVEL_DEFAULT; }

static ZSTD_v1_5_2_compressionParameters ZSTD_v1_5_2_dedicatedDictSearch_getCParams(int const compressionLevel, size_t const dictSize)
{
    ZSTD_v1_5_2_compressionParameters cParams = ZSTD_v1_5_2_getCParams_internal(compressionLevel, 0, dictSize, ZSTD_v1_5_2_cpm_createCDict);
    switch (cParams.strategy) {
        case ZSTD_v1_5_2_fast:
        case ZSTD_v1_5_2_dfast:
            break;
        case ZSTD_v1_5_2_greedy:
        case ZSTD_v1_5_2_lazy:
        case ZSTD_v1_5_2_lazy2:
            cParams.hashLog += ZSTD_v1_5_2_LAZY_DDSS_BUCKET_LOG;
            break;
        case ZSTD_v1_5_2_btlazy2:
        case ZSTD_v1_5_2_btopt:
        case ZSTD_v1_5_2_btultra:
        case ZSTD_v1_5_2_btultra2:
            break;
    }
    return cParams;
}

static int ZSTD_v1_5_2_dedicatedDictSearch_isSupported(
        ZSTD_v1_5_2_compressionParameters const* cParams)
{
    return (cParams->strategy >= ZSTD_v1_5_2_greedy)
        && (cParams->strategy <= ZSTD_v1_5_2_lazy2)
        && (cParams->hashLog > cParams->chainLog)
        && (cParams->chainLog <= 24);
}

/**
 * Reverses the adjustment applied to cparams when enabling dedicated dict
 * search. This is used to recover the params set to be used in the working
 * context. (Otherwise, those tables would also grow.)
 */
static void ZSTD_v1_5_2_dedicatedDictSearch_revertCParams(
        ZSTD_v1_5_2_compressionParameters* cParams) {
    switch (cParams->strategy) {
        case ZSTD_v1_5_2_fast:
        case ZSTD_v1_5_2_dfast:
            break;
        case ZSTD_v1_5_2_greedy:
        case ZSTD_v1_5_2_lazy:
        case ZSTD_v1_5_2_lazy2:
            cParams->hashLog -= ZSTD_v1_5_2_LAZY_DDSS_BUCKET_LOG;
            if (cParams->hashLog < ZSTD_v1_5_2_HASHLOG_MIN) {
                cParams->hashLog = ZSTD_v1_5_2_HASHLOG_MIN;
            }
            break;
        case ZSTD_v1_5_2_btlazy2:
        case ZSTD_v1_5_2_btopt:
        case ZSTD_v1_5_2_btultra:
        case ZSTD_v1_5_2_btultra2:
            break;
    }
}

static U64 ZSTD_v1_5_2_getCParamRowSize(U64 srcSizeHint, size_t dictSize, ZSTD_v1_5_2_cParamMode_e mode)
{
    switch (mode) {
    case ZSTD_v1_5_2_cpm_unknown:
    case ZSTD_v1_5_2_cpm_noAttachDict:
    case ZSTD_v1_5_2_cpm_createCDict:
        break;
    case ZSTD_v1_5_2_cpm_attachDict:
        dictSize = 0;
        break;
    default:
        assert(0);
        break;
    }
    {   int const unknown = srcSizeHint == ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN;
        size_t const addedSize = unknown && dictSize > 0 ? 500 : 0;
        return unknown && dictSize == 0 ? ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN : srcSizeHint+dictSize+addedSize;
    }
}

/*! ZSTD_v1_5_2_getCParams_internal() :
 * @return ZSTD_v1_5_2_compressionParameters structure for a selected compression level, srcSize and dictSize.
 *  Note: srcSizeHint 0 means 0, use ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN for unknown.
 *        Use dictSize == 0 for unknown or unused.
 *  Note: `mode` controls how we treat the `dictSize`. See docs for `ZSTD_v1_5_2_cParamMode_e`. */
static ZSTD_v1_5_2_compressionParameters ZSTD_v1_5_2_getCParams_internal(int compressionLevel, unsigned long long srcSizeHint, size_t dictSize, ZSTD_v1_5_2_cParamMode_e mode)
{
    U64 const rSize = ZSTD_v1_5_2_getCParamRowSize(srcSizeHint, dictSize, mode);
    U32 const tableID = (rSize <= 256 KB) + (rSize <= 128 KB) + (rSize <= 16 KB);
    int row;
    DEBUGLOG(5, "ZSTD_v1_5_2_getCParams_internal (cLevel=%i)", compressionLevel);

    /* row */
    if (compressionLevel == 0) row = ZSTD_v1_5_2_CLEVEL_DEFAULT;   /* 0 == default */
    else if (compressionLevel < 0) row = 0;   /* entry 0 is baseline for fast mode */
    else if (compressionLevel > ZSTD_v1_5_2_MAX_CLEVEL) row = ZSTD_v1_5_2_MAX_CLEVEL;
    else row = compressionLevel;

    {   ZSTD_v1_5_2_compressionParameters cp = ZSTD_v1_5_2_defaultCParameters[tableID][row];
        DEBUGLOG(5, "ZSTD_v1_5_2_getCParams_internal selected tableID: %u row: %u strat: %u", tableID, row, (U32)cp.strategy);
        /* acceleration factor */
        if (compressionLevel < 0) {
            int const clampedCompressionLevel = MAX(ZSTD_v1_5_2_minCLevel(), compressionLevel);
            cp.targetLength = (unsigned)(-clampedCompressionLevel);
        }
        /* refine parameters based on srcSize & dictSize */
        return ZSTD_v1_5_2_adjustCParams_internal(cp, srcSizeHint, dictSize, mode);
    }
}

/*! ZSTD_v1_5_2_getCParams() :
 * @return ZSTD_v1_5_2_compressionParameters structure for a selected compression level, srcSize and dictSize.
 *  Size values are optional, provide 0 if not known or unused */
ZSTD_v1_5_2_compressionParameters ZSTD_v1_5_2_getCParams(int compressionLevel, unsigned long long srcSizeHint, size_t dictSize)
{
    if (srcSizeHint == 0) srcSizeHint = ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN;
    return ZSTD_v1_5_2_getCParams_internal(compressionLevel, srcSizeHint, dictSize, ZSTD_v1_5_2_cpm_unknown);
}

/*! ZSTD_v1_5_2_getParams() :
 *  same idea as ZSTD_v1_5_2_getCParams()
 * @return a `ZSTD_v1_5_2_parameters` structure (instead of `ZSTD_v1_5_2_compressionParameters`).
 *  Fields of `ZSTD_v1_5_2_frameParameters` are set to default values */
static ZSTD_v1_5_2_parameters ZSTD_v1_5_2_getParams_internal(int compressionLevel, unsigned long long srcSizeHint, size_t dictSize, ZSTD_v1_5_2_cParamMode_e mode) {
    ZSTD_v1_5_2_parameters params;
    ZSTD_v1_5_2_compressionParameters const cParams = ZSTD_v1_5_2_getCParams_internal(compressionLevel, srcSizeHint, dictSize, mode);
    DEBUGLOG(5, "ZSTD_v1_5_2_getParams (cLevel=%i)", compressionLevel);
    ZSTD_v1_5_2_memset(&params, 0, sizeof(params));
    params.cParams = cParams;
    params.fParams.contentSizeFlag = 1;
    return params;
}

/*! ZSTD_v1_5_2_getParams() :
 *  same idea as ZSTD_v1_5_2_getCParams()
 * @return a `ZSTD_v1_5_2_parameters` structure (instead of `ZSTD_v1_5_2_compressionParameters`).
 *  Fields of `ZSTD_v1_5_2_frameParameters` are set to default values */
ZSTD_v1_5_2_parameters ZSTD_v1_5_2_getParams(int compressionLevel, unsigned long long srcSizeHint, size_t dictSize) {
    if (srcSizeHint == 0) srcSizeHint = ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN;
    return ZSTD_v1_5_2_getParams_internal(compressionLevel, srcSizeHint, dictSize, ZSTD_v1_5_2_cpm_unknown);
}
