/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */
#if defined (__cplusplus)
extern "C" {
#endif

#ifndef ZSTD_v1_5_2_H_235446
#define ZSTD_v1_5_2_H_235446

/* ======   Dependency   ======*/
#include <limits.h>   /* INT_MAX */
#include <stddef.h>   /* size_t */


/* =====   ZSTDLIB_API : control library symbols visibility   ===== */
#ifndef ZSTDLIB_VISIBLE
#  if defined(__GNUC__) && (__GNUC__ >= 4) && !defined(__MINGW32__)
#    define ZSTDLIB_VISIBLE __attribute__ ((visibility ("default")))
#    define ZSTDLIB_HIDDEN __attribute__ ((visibility ("hidden")))
#  else
#    define ZSTDLIB_VISIBLE
#    define ZSTDLIB_HIDDEN
#  endif
#endif
#if defined(ZSTD_v1_5_2_DLL_EXPORT) && (ZSTD_v1_5_2_DLL_EXPORT==1)
#  define ZSTDLIB_API __declspec(dllexport) ZSTDLIB_VISIBLE
#elif defined(ZSTD_v1_5_2_DLL_IMPORT) && (ZSTD_v1_5_2_DLL_IMPORT==1)
#  define ZSTDLIB_API __declspec(dllimport) ZSTDLIB_VISIBLE /* It isn't required but allows to generate better code, saving a function pointer load from the IAT and an indirect jump.*/
#else
#  define ZSTDLIB_API ZSTDLIB_VISIBLE
#endif


/*******************************************************************************
  Introduction

  zstd, short for Zstandard, is a fast lossless compression algorithm, targeting
  real-time compression scenarios at zlib-level and better compression ratios.
  The zstd compression library provides in-memory compression and decompression
  functions.

  The library supports regular compression levels from 1 up to ZSTD_v1_5_2_maxCLevel(),
  which is currently 22. Levels >= 20, labeled `--ultra`, should be used with
  caution, as they require more memory. The library also offers negative
  compression levels, which extend the range of speed vs. ratio preferences.
  The lower the level, the faster the speed (at the cost of compression).

  Compression can be done in:
    - a single step (described as Simple API)
    - a single step, reusing a context (described as Explicit context)
    - unbounded multiple steps (described as Streaming compression)

  The compression ratio achievable on small data can be highly improved using
  a dictionary. Dictionary compression can be performed in:
    - a single step (described as Simple dictionary API)
    - a single step, reusing a dictionary (described as Bulk-processing
      dictionary API)

  Advanced experimental functions can be accessed using
  `#define ZSTD_v1_5_2_STATIC_LINKING_ONLY` before including zstd.h.

  Advanced experimental APIs should never be used with a dynamically-linked
  library. They are not "stable"; their definitions or signatures may change in
  the future. Only static linking is allowed.
*******************************************************************************/

/*------   Version   ------*/
#define ZSTD_v1_5_2_VERSION_MAJOR    1
#define ZSTD_v1_5_2_VERSION_MINOR    5
#define ZSTD_v1_5_2_VERSION_RELEASE  2
#define ZSTD_v1_5_2_VERSION_NUMBER  (ZSTD_v1_5_2_VERSION_MAJOR *100*100 + ZSTD_v1_5_2_VERSION_MINOR *100 + ZSTD_v1_5_2_VERSION_RELEASE)

/*! ZSTD_v1_5_2_versionNumber() :
 *  Return runtime library version, the value is (MAJOR*100*100 + MINOR*100 + RELEASE). */
ZSTDLIB_API unsigned ZSTD_v1_5_2_versionNumber(void);

#define ZSTD_v1_5_2_LIB_VERSION ZSTD_VERSION_MAJOR.ZSTD_VERSION_MINOR.ZSTD_v1_5_2_VERSION_RELEASE
#define ZSTD_v1_5_2_QUOTE(str) #str
#define ZSTD_v1_5_2_EXPAND_AND_QUOTE(str) ZSTD_v1_5_2_QUOTE(str)
#define ZSTD_v1_5_2_VERSION_STRING ZSTD_v1_5_2_EXPAND_AND_QUOTE(ZSTD_v1_5_2_LIB_VERSION)

/*! ZSTD_v1_5_2_versionString() :
 *  Return runtime library version, like "1.4.5". Requires v1.3.0+. */
ZSTDLIB_API const char* ZSTD_v1_5_2_versionString(void);

/* *************************************
 *  Default constant
 ***************************************/
#ifndef ZSTD_v1_5_2_CLEVEL_DEFAULT
#  define ZSTD_v1_5_2_CLEVEL_DEFAULT 3
#endif

/* *************************************
 *  Constants
 ***************************************/

/* All magic numbers are supposed read/written to/from files/memory using little-endian convention */
#define ZSTD_v1_5_2_MAGICNUMBER            0xFD2FB528    /* valid since v0.8.0 */
#define ZSTD_v1_5_2_MAGIC_DICTIONARY       0xEC30A437    /* valid since v0.7.0 */
#define ZSTD_v1_5_2_MAGIC_SKIPPABLE_START  0x184D2A50    /* all 16 values, from 0x184D2A50 to 0x184D2A5F, signal the beginning of a skippable frame */
#define ZSTD_v1_5_2_MAGIC_SKIPPABLE_MASK   0xFFFFFFF0

#define ZSTD_v1_5_2_BLOCKSIZELOG_MAX  17
#define ZSTD_v1_5_2_BLOCKSIZE_MAX     (1<<ZSTD_v1_5_2_BLOCKSIZELOG_MAX)


/***************************************
*  Simple API
***************************************/
/*! ZSTD_v1_5_2_compress() :
 *  Compresses `src` content as a single zstd compressed frame into already allocated `dst`.
 *  Hint : compression runs faster if `dstCapacity` >=  `ZSTD_v1_5_2_compressBound(srcSize)`.
 *  @return : compressed size written into `dst` (<= `dstCapacity),
 *            or an error code if it fails (which can be tested using ZSTD_v1_5_2_isError()). */
ZSTDLIB_API size_t ZSTD_v1_5_2_compress( void* dst, size_t dstCapacity,
                            const void* src, size_t srcSize,
                                  int compressionLevel);

/*! ZSTD_v1_5_2_decompress() :
 *  `compressedSize` : must be the _exact_ size of some number of compressed and/or skippable frames.
 *  `dstCapacity` is an upper bound of originalSize to regenerate.
 *  If user cannot imply a maximum upper bound, it's better to use streaming mode to decompress data.
 *  @return : the number of bytes decompressed into `dst` (<= `dstCapacity`),
 *            or an errorCode if it fails (which can be tested using ZSTD_v1_5_2_isError()). */
ZSTDLIB_API size_t ZSTD_v1_5_2_decompress( void* dst, size_t dstCapacity,
                              const void* src, size_t compressedSize);

/*! ZSTD_v1_5_2_getFrameContentSize() : requires v1.3.0+
 *  `src` should point to the start of a ZSTD encoded frame.
 *  `srcSize` must be at least as large as the frame header.
 *            hint : any size >= `ZSTD_v1_5_2_frameHeaderSize_max` is large enough.
 *  @return : - decompressed size of `src` frame content, if known
 *            - ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN if the size cannot be determined
 *            - ZSTD_v1_5_2_CONTENTSIZE_ERROR if an error occurred (e.g. invalid magic number, srcSize too small)
 *   note 1 : a 0 return value means the frame is valid but "empty".
 *   note 2 : decompressed size is an optional field, it may not be present, typically in streaming mode.
 *            When `return==ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN`, data to decompress could be any size.
 *            In which case, it's necessary to use streaming mode to decompress data.
 *            Optionally, application can rely on some implicit limit,
 *            as ZSTD_v1_5_2_decompress() only needs an upper bound of decompressed size.
 *            (For example, data could be necessarily cut into blocks <= 16 KB).
 *   note 3 : decompressed size is always present when compression is completed using single-pass functions,
 *            such as ZSTD_v1_5_2_compress(), ZSTD_v1_5_2_compressCCtx() ZSTD_v1_5_2_compress_usingDict() or ZSTD_v1_5_2_compress_usingCDict().
 *   note 4 : decompressed size can be very large (64-bits value),
 *            potentially larger than what local system can handle as a single memory segment.
 *            In which case, it's necessary to use streaming mode to decompress data.
 *   note 5 : If source is untrusted, decompressed size could be wrong or intentionally modified.
 *            Always ensure return value fits within application's authorized limits.
 *            Each application can set its own limits.
 *   note 6 : This function replaces ZSTD_v1_5_2_getDecompressedSize() */
#define ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN (0ULL - 1)
#define ZSTD_v1_5_2_CONTENTSIZE_ERROR   (0ULL - 2)
ZSTDLIB_API unsigned long long ZSTD_v1_5_2_getFrameContentSize(const void *src, size_t srcSize);

/*! ZSTD_v1_5_2_getDecompressedSize() :
 *  NOTE: This function is now obsolete, in favor of ZSTD_v1_5_2_getFrameContentSize().
 *  Both functions work the same way, but ZSTD_v1_5_2_getDecompressedSize() blends
 *  "empty", "unknown" and "error" results to the same return value (0),
 *  while ZSTD_v1_5_2_getFrameContentSize() gives them separate return values.
 * @return : decompressed size of `src` frame content _if known and not empty_, 0 otherwise. */
ZSTDLIB_API unsigned long long ZSTD_v1_5_2_getDecompressedSize(const void* src, size_t srcSize);

/*! ZSTD_v1_5_2_findFrameCompressedSize() : Requires v1.4.0+
 * `src` should point to the start of a ZSTD frame or skippable frame.
 * `srcSize` must be >= first frame size
 * @return : the compressed size of the first frame starting at `src`,
 *           suitable to pass as `srcSize` to `ZSTD_v1_5_2_decompress` or similar,
 *        or an error code if input is invalid */
ZSTDLIB_API size_t ZSTD_v1_5_2_findFrameCompressedSize(const void* src, size_t srcSize);


/*======  Helper functions  ======*/
#define ZSTD_v1_5_2_COMPRESSBOUND(srcSize)   ((srcSize) + ((srcSize)>>8) + (((srcSize) < (128<<10)) ? (((128<<10) - (srcSize)) >> 11) /* margin, from 64 to 0 */ : 0))  /* this formula ensures that bound(A) + bound(B) <= bound(A+B) as long as A and B >= 128 KB */
ZSTDLIB_API size_t      ZSTD_v1_5_2_compressBound(size_t srcSize); /*!< maximum compressed size in worst case single-pass scenario */
ZSTDLIB_API unsigned    ZSTD_v1_5_2_isError(size_t code);          /*!< tells if a `size_t` function result is an error code */
ZSTDLIB_API const char* ZSTD_v1_5_2_getErrorName(size_t code);     /*!< provides readable string from an error code */
ZSTDLIB_API int         ZSTD_v1_5_2_minCLevel(void);               /*!< minimum negative compression level allowed, requires v1.4.0+ */
ZSTDLIB_API int         ZSTD_v1_5_2_maxCLevel(void);               /*!< maximum compression level available */
ZSTDLIB_API int         ZSTD_v1_5_2_defaultCLevel(void);           /*!< default compression level, specified by ZSTD_v1_5_2_CLEVEL_DEFAULT, requires v1.5.0+ */


/***************************************
*  Explicit context
***************************************/
/*= Compression context
 *  When compressing many times,
 *  it is recommended to allocate a context just once,
 *  and re-use it for each successive compression operation.
 *  This will make workload friendlier for system's memory.
 *  Note : re-using context is just a speed / resource optimization.
 *         It doesn't change the compression ratio, which remains identical.
 *  Note 2 : In multi-threaded environments,
 *         use one different context per thread for parallel execution.
 */
typedef struct ZSTD_v1_5_2_CCtx_s ZSTD_v1_5_2_CCtx;
ZSTDLIB_API ZSTD_v1_5_2_CCtx* ZSTD_v1_5_2_createCCtx(void);
ZSTDLIB_API size_t     ZSTD_v1_5_2_freeCCtx(ZSTD_v1_5_2_CCtx* cctx);  /* accept NULL pointer */

/*! ZSTD_v1_5_2_compressCCtx() :
 *  Same as ZSTD_v1_5_2_compress(), using an explicit ZSTD_v1_5_2_CCtx.
 *  Important : in order to behave similarly to `ZSTD_v1_5_2_compress()`,
 *  this function compresses at requested compression level,
 *  __ignoring any other parameter__ .
 *  If any advanced parameter was set using the advanced API,
 *  they will all be reset. Only `compressionLevel` remains.
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_compressCCtx(ZSTD_v1_5_2_CCtx* cctx,
                                     void* dst, size_t dstCapacity,
                               const void* src, size_t srcSize,
                                     int compressionLevel);

/*= Decompression context
 *  When decompressing many times,
 *  it is recommended to allocate a context only once,
 *  and re-use it for each successive compression operation.
 *  This will make workload friendlier for system's memory.
 *  Use one context per thread for parallel execution. */
typedef struct ZSTD_v1_5_2_DCtx_s ZSTD_v1_5_2_DCtx;
ZSTDLIB_API ZSTD_v1_5_2_DCtx* ZSTD_v1_5_2_createDCtx(void);
ZSTDLIB_API size_t     ZSTD_v1_5_2_freeDCtx(ZSTD_v1_5_2_DCtx* dctx);  /* accept NULL pointer */

/*! ZSTD_v1_5_2_decompressDCtx() :
 *  Same as ZSTD_v1_5_2_decompress(),
 *  requires an allocated ZSTD_v1_5_2_DCtx.
 *  Compatible with sticky parameters.
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_decompressDCtx(ZSTD_v1_5_2_DCtx* dctx,
                                       void* dst, size_t dstCapacity,
                                 const void* src, size_t srcSize);


/*********************************************
*  Advanced compression API (Requires v1.4.0+)
**********************************************/

/* API design :
 *   Parameters are pushed one by one into an existing context,
 *   using ZSTD_v1_5_2_CCtx_set*() functions.
 *   Pushed parameters are sticky : they are valid for next compressed frame, and any subsequent frame.
 *   "sticky" parameters are applicable to `ZSTD_v1_5_2_compress2()` and `ZSTD_v1_5_2_compressStream*()` !
 *   __They do not apply to "simple" one-shot variants such as ZSTD_v1_5_2_compressCCtx()__ .
 *
 *   It's possible to reset all parameters to "default" using ZSTD_v1_5_2_CCtx_reset().
 *
 *   This API supersedes all other "advanced" API entry points in the experimental section.
 *   In the future, we expect to remove from experimental API entry points which are redundant with this API.
 */


/* Compression strategies, listed from fastest to strongest */
typedef enum { ZSTD_v1_5_2_fast=1,
               ZSTD_v1_5_2_dfast=2,
               ZSTD_v1_5_2_greedy=3,
               ZSTD_v1_5_2_lazy=4,
               ZSTD_v1_5_2_lazy2=5,
               ZSTD_v1_5_2_btlazy2=6,
               ZSTD_v1_5_2_btopt=7,
               ZSTD_v1_5_2_btultra=8,
               ZSTD_v1_5_2_btultra2=9
               /* note : new strategies _might_ be added in the future.
                         Only the order (from fast to strong) is guaranteed */
} ZSTD_v1_5_2_strategy;

typedef enum {

    /* compression parameters
     * Note: When compressing with a ZSTD_v1_5_2_CDict these parameters are superseded
     * by the parameters used to construct the ZSTD_v1_5_2_CDict.
     * See ZSTD_v1_5_2_CCtx_refCDict() for more info (superseded-by-cdict). */
    ZSTD_v1_5_2_c_compressionLevel=100, /* Set compression parameters according to pre-defined cLevel table.
                              * Note that exact compression parameters are dynamically determined,
                              * depending on both compression level and srcSize (when known).
                              * Default level is ZSTD_v1_5_2_CLEVEL_DEFAULT==3.
                              * Special: value 0 means default, which is controlled by ZSTD_v1_5_2_CLEVEL_DEFAULT.
                              * Note 1 : it's possible to pass a negative compression level.
                              * Note 2 : setting a level does not automatically set all other compression parameters
                              *   to default. Setting this will however eventually dynamically impact the compression
                              *   parameters which have not been manually set. The manually set
                              *   ones will 'stick'. */
    /* Advanced compression parameters :
     * It's possible to pin down compression parameters to some specific values.
     * In which case, these values are no longer dynamically selected by the compressor */
    ZSTD_v1_5_2_c_windowLog=101,    /* Maximum allowed back-reference distance, expressed as power of 2.
                              * This will set a memory budget for streaming decompression,
                              * with larger values requiring more memory
                              * and typically compressing more.
                              * Must be clamped between ZSTD_v1_5_2_WINDOWLOG_MIN and ZSTD_v1_5_2_WINDOWLOG_MAX.
                              * Special: value 0 means "use default windowLog".
                              * Note: Using a windowLog greater than ZSTD_v1_5_2_WINDOWLOG_LIMIT_DEFAULT
                              *       requires explicitly allowing such size at streaming decompression stage. */
    ZSTD_v1_5_2_c_hashLog=102,      /* Size of the initial probe table, as a power of 2.
                              * Resulting memory usage is (1 << (hashLog+2)).
                              * Must be clamped between ZSTD_v1_5_2_HASHLOG_MIN and ZSTD_v1_5_2_HASHLOG_MAX.
                              * Larger tables improve compression ratio of strategies <= dFast,
                              * and improve speed of strategies > dFast.
                              * Special: value 0 means "use default hashLog". */
    ZSTD_v1_5_2_c_chainLog=103,     /* Size of the multi-probe search table, as a power of 2.
                              * Resulting memory usage is (1 << (chainLog+2)).
                              * Must be clamped between ZSTD_v1_5_2_CHAINLOG_MIN and ZSTD_v1_5_2_CHAINLOG_MAX.
                              * Larger tables result in better and slower compression.
                              * This parameter is useless for "fast" strategy.
                              * It's still useful when using "dfast" strategy,
                              * in which case it defines a secondary probe table.
                              * Special: value 0 means "use default chainLog". */
    ZSTD_v1_5_2_c_searchLog=104,    /* Number of search attempts, as a power of 2.
                              * More attempts result in better and slower compression.
                              * This parameter is useless for "fast" and "dFast" strategies.
                              * Special: value 0 means "use default searchLog". */
    ZSTD_v1_5_2_c_minMatch=105,     /* Minimum size of searched matches.
                              * Note that Zstandard can still find matches of smaller size,
                              * it just tweaks its search algorithm to look for this size and larger.
                              * Larger values increase compression and decompression speed, but decrease ratio.
                              * Must be clamped between ZSTD_v1_5_2_MINMATCH_MIN and ZSTD_v1_5_2_MINMATCH_MAX.
                              * Note that currently, for all strategies < btopt, effective minimum is 4.
                              *                    , for all strategies > fast, effective maximum is 6.
                              * Special: value 0 means "use default minMatchLength". */
    ZSTD_v1_5_2_c_targetLength=106, /* Impact of this field depends on strategy.
                              * For strategies btopt, btultra & btultra2:
                              *     Length of Match considered "good enough" to stop search.
                              *     Larger values make compression stronger, and slower.
                              * For strategy fast:
                              *     Distance between match sampling.
                              *     Larger values make compression faster, and weaker.
                              * Special: value 0 means "use default targetLength". */
    ZSTD_v1_5_2_c_strategy=107,     /* See ZSTD_v1_5_2_strategy enum definition.
                              * The higher the value of selected strategy, the more complex it is,
                              * resulting in stronger and slower compression.
                              * Special: value 0 means "use default strategy". */
    /* LDM mode parameters */
    ZSTD_v1_5_2_c_enableLongDistanceMatching=160, /* Enable long distance matching.
                                     * This parameter is designed to improve compression ratio
                                     * for large inputs, by finding large matches at long distance.
                                     * It increases memory usage and window size.
                                     * Note: enabling this parameter increases default ZSTD_v1_5_2_c_windowLog to 128 MB
                                     * except when expressly set to a different value.
                                     * Note: will be enabled by default if ZSTD_v1_5_2_c_windowLog >= 128 MB and
                                     * compression strategy >= ZSTD_v1_5_2_btopt (== compression level 16+) */
    ZSTD_v1_5_2_c_ldmHashLog=161,   /* Size of the table for long distance matching, as a power of 2.
                              * Larger values increase memory usage and compression ratio,
                              * but decrease compression speed.
                              * Must be clamped between ZSTD_v1_5_2_HASHLOG_MIN and ZSTD_v1_5_2_HASHLOG_MAX
                              * default: windowlog - 7.
                              * Special: value 0 means "automatically determine hashlog". */
    ZSTD_v1_5_2_c_ldmMinMatch=162,  /* Minimum match size for long distance matcher.
                              * Larger/too small values usually decrease compression ratio.
                              * Must be clamped between ZSTD_v1_5_2_LDM_MINMATCH_MIN and ZSTD_v1_5_2_LDM_MINMATCH_MAX.
                              * Special: value 0 means "use default value" (default: 64). */
    ZSTD_v1_5_2_c_ldmBucketSizeLog=163, /* Log size of each bucket in the LDM hash table for collision resolution.
                              * Larger values improve collision resolution but decrease compression speed.
                              * The maximum value is ZSTD_v1_5_2_LDM_BUCKETSIZELOG_MAX.
                              * Special: value 0 means "use default value" (default: 3). */
    ZSTD_v1_5_2_c_ldmHashRateLog=164, /* Frequency of inserting/looking up entries into the LDM hash table.
                              * Must be clamped between 0 and (ZSTD_v1_5_2_WINDOWLOG_MAX - ZSTD_v1_5_2_HASHLOG_MIN).
                              * Default is MAX(0, (windowLog - ldmHashLog)), optimizing hash table usage.
                              * Larger values improve compression speed.
                              * Deviating far from default value will likely result in a compression ratio decrease.
                              * Special: value 0 means "automatically determine hashRateLog". */

    /* frame parameters */
    ZSTD_v1_5_2_c_contentSizeFlag=200, /* Content size will be written into frame header _whenever known_ (default:1)
                              * Content size must be known at the beginning of compression.
                              * This is automatically the case when using ZSTD_v1_5_2_compress2(),
                              * For streaming scenarios, content size must be provided with ZSTD_v1_5_2_CCtx_setPledgedSrcSize() */
    ZSTD_v1_5_2_c_checksumFlag=201, /* A 32-bits checksum of content is written at end of frame (default:0) */
    ZSTD_v1_5_2_c_dictIDFlag=202,   /* When applicable, dictionary's ID is written into frame header (default:1) */

    /* multi-threading parameters */
    /* These parameters are only active if multi-threading is enabled (compiled with build macro ZSTD_v1_5_2_MULTITHREAD).
     * Otherwise, trying to set any other value than default (0) will be a no-op and return an error.
     * In a situation where it's unknown if the linked library supports multi-threading or not,
     * setting ZSTD_v1_5_2_c_nbWorkers to any value >= 1 and consulting the return value provides a quick way to check this property.
     */
    ZSTD_v1_5_2_c_nbWorkers=400,    /* Select how many threads will be spawned to compress in parallel.
                              * When nbWorkers >= 1, triggers asynchronous mode when invoking ZSTD_v1_5_2_compressStream*() :
                              * ZSTD_v1_5_2_compressStream*() consumes input and flush output if possible, but immediately gives back control to caller,
                              * while compression is performed in parallel, within worker thread(s).
                              * (note : a strong exception to this rule is when first invocation of ZSTD_v1_5_2_compressStream2() sets ZSTD_v1_5_2_e_end :
                              *  in which case, ZSTD_v1_5_2_compressStream2() delegates to ZSTD_v1_5_2_compress2(), which is always a blocking call).
                              * More workers improve speed, but also increase memory usage.
                              * Default value is `0`, aka "single-threaded mode" : no worker is spawned,
                              * compression is performed inside Caller's thread, and all invocations are blocking */
    ZSTD_v1_5_2_c_jobSize=401,      /* Size of a compression job. This value is enforced only when nbWorkers >= 1.
                              * Each compression job is completed in parallel, so this value can indirectly impact the nb of active threads.
                              * 0 means default, which is dynamically determined based on compression parameters.
                              * Job size must be a minimum of overlap size, or ZSTDMT_JOBSIZE_MIN (= 512 KB), whichever is largest.
                              * The minimum size is automatically and transparently enforced. */
    ZSTD_v1_5_2_c_overlapLog=402,   /* Control the overlap size, as a fraction of window size.
                              * The overlap size is an amount of data reloaded from previous job at the beginning of a new job.
                              * It helps preserve compression ratio, while each job is compressed in parallel.
                              * This value is enforced only when nbWorkers >= 1.
                              * Larger values increase compression ratio, but decrease speed.
                              * Possible values range from 0 to 9 :
                              * - 0 means "default" : value will be determined by the library, depending on strategy
                              * - 1 means "no overlap"
                              * - 9 means "full overlap", using a full window size.
                              * Each intermediate rank increases/decreases load size by a factor 2 :
                              * 9: full window;  8: w/2;  7: w/4;  6: w/8;  5:w/16;  4: w/32;  3:w/64;  2:w/128;  1:no overlap;  0:default
                              * default value varies between 6 and 9, depending on strategy */

    /* note : additional experimental parameters are also available
     * within the experimental section of the API.
     * At the time of this writing, they include :
     * ZSTD_v1_5_2_c_rsyncable
     * ZSTD_v1_5_2_c_format
     * ZSTD_v1_5_2_c_forceMaxWindow
     * ZSTD_v1_5_2_c_forceAttachDict
     * ZSTD_v1_5_2_c_literalCompressionMode
     * ZSTD_v1_5_2_c_targetCBlockSize
     * ZSTD_v1_5_2_c_srcSizeHint
     * ZSTD_v1_5_2_c_enableDedicatedDictSearch
     * ZSTD_v1_5_2_c_stableInBuffer
     * ZSTD_v1_5_2_c_stableOutBuffer
     * ZSTD_v1_5_2_c_blockDelimiters
     * ZSTD_v1_5_2_c_validateSequences
     * ZSTD_v1_5_2_c_useBlockSplitter
     * ZSTD_v1_5_2_c_useRowMatchFinder
     * Because they are not stable, it's necessary to define ZSTD_v1_5_2_STATIC_LINKING_ONLY to access them.
     * note : never ever use experimentalParam? names directly;
     *        also, the enums values themselves are unstable and can still change.
     */
     ZSTD_v1_5_2_c_experimentalParam1=500,
     ZSTD_v1_5_2_c_experimentalParam2=10,
     ZSTD_v1_5_2_c_experimentalParam3=1000,
     ZSTD_v1_5_2_c_experimentalParam4=1001,
     ZSTD_v1_5_2_c_experimentalParam5=1002,
     ZSTD_v1_5_2_c_experimentalParam6=1003,
     ZSTD_v1_5_2_c_experimentalParam7=1004,
     ZSTD_v1_5_2_c_experimentalParam8=1005,
     ZSTD_v1_5_2_c_experimentalParam9=1006,
     ZSTD_v1_5_2_c_experimentalParam10=1007,
     ZSTD_v1_5_2_c_experimentalParam11=1008,
     ZSTD_v1_5_2_c_experimentalParam12=1009,
     ZSTD_v1_5_2_c_experimentalParam13=1010,
     ZSTD_v1_5_2_c_experimentalParam14=1011,
     ZSTD_v1_5_2_c_experimentalParam15=1012
} ZSTD_v1_5_2_cParameter;

typedef struct {
    size_t error;
    int lowerBound;
    int upperBound;
} ZSTD_v1_5_2_bounds;

/*! ZSTD_v1_5_2_cParam_getBounds() :
 *  All parameters must belong to an interval with lower and upper bounds,
 *  otherwise they will either trigger an error or be automatically clamped.
 * @return : a structure, ZSTD_v1_5_2_bounds, which contains
 *         - an error status field, which must be tested using ZSTD_v1_5_2_isError()
 *         - lower and upper bounds, both inclusive
 */
ZSTDLIB_API ZSTD_v1_5_2_bounds ZSTD_v1_5_2_cParam_getBounds(ZSTD_v1_5_2_cParameter cParam);

/*! ZSTD_v1_5_2_CCtx_setParameter() :
 *  Set one compression parameter, selected by enum ZSTD_v1_5_2_cParameter.
 *  All parameters have valid bounds. Bounds can be queried using ZSTD_v1_5_2_cParam_getBounds().
 *  Providing a value beyond bound will either clamp it, or trigger an error (depending on parameter).
 *  Setting a parameter is generally only possible during frame initialization (before starting compression).
 *  Exception : when using multi-threading mode (nbWorkers >= 1),
 *              the following parameters can be updated _during_ compression (within same frame):
 *              => compressionLevel, hashLog, chainLog, searchLog, minMatch, targetLength and strategy.
 *              new parameters will be active for next job only (after a flush()).
 * @return : an error code (which can be tested using ZSTD_v1_5_2_isError()).
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_CCtx_setParameter(ZSTD_v1_5_2_CCtx* cctx, ZSTD_v1_5_2_cParameter param, int value);

/*! ZSTD_v1_5_2_CCtx_setPledgedSrcSize() :
 *  Total input data size to be compressed as a single frame.
 *  Value will be written in frame header, unless if explicitly forbidden using ZSTD_v1_5_2_c_contentSizeFlag.
 *  This value will also be controlled at end of frame, and trigger an error if not respected.
 * @result : 0, or an error code (which can be tested with ZSTD_v1_5_2_isError()).
 *  Note 1 : pledgedSrcSize==0 actually means zero, aka an empty frame.
 *           In order to mean "unknown content size", pass constant ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN.
 *           ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN is default value for any new frame.
 *  Note 2 : pledgedSrcSize is only valid once, for the next frame.
 *           It's discarded at the end of the frame, and replaced by ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN.
 *  Note 3 : Whenever all input data is provided and consumed in a single round,
 *           for example with ZSTD_v1_5_2_compress2(),
 *           or invoking immediately ZSTD_v1_5_2_compressStream2(,,,ZSTD_v1_5_2_e_end),
 *           this value is automatically overridden by srcSize instead.
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_CCtx_setPledgedSrcSize(ZSTD_v1_5_2_CCtx* cctx, unsigned long long pledgedSrcSize);

typedef enum {
    ZSTD_v1_5_2_reset_session_only = 1,
    ZSTD_v1_5_2_reset_parameters = 2,
    ZSTD_v1_5_2_reset_session_and_parameters = 3
} ZSTD_v1_5_2_ResetDirective;

/*! ZSTD_v1_5_2_CCtx_reset() :
 *  There are 2 different things that can be reset, independently or jointly :
 *  - The session : will stop compressing current frame, and make CCtx ready to start a new one.
 *                  Useful after an error, or to interrupt any ongoing compression.
 *                  Any internal data not yet flushed is cancelled.
 *                  Compression parameters and dictionary remain unchanged.
 *                  They will be used to compress next frame.
 *                  Resetting session never fails.
 *  - The parameters : changes all parameters back to "default".
 *                  This removes any reference to any dictionary too.
 *                  Parameters can only be changed between 2 sessions (i.e. no compression is currently ongoing)
 *                  otherwise the reset fails, and function returns an error value (which can be tested using ZSTD_v1_5_2_isError())
 *  - Both : similar to resetting the session, followed by resetting parameters.
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_CCtx_reset(ZSTD_v1_5_2_CCtx* cctx, ZSTD_v1_5_2_ResetDirective reset);

/*! ZSTD_v1_5_2_compress2() :
 *  Behave the same as ZSTD_v1_5_2_compressCCtx(), but compression parameters are set using the advanced API.
 *  ZSTD_v1_5_2_compress2() always starts a new frame.
 *  Should cctx hold data from a previously unfinished frame, everything about it is forgotten.
 *  - Compression parameters are pushed into CCtx before starting compression, using ZSTD_v1_5_2_CCtx_set*()
 *  - The function is always blocking, returns when compression is completed.
 *  Hint : compression runs faster if `dstCapacity` >=  `ZSTD_v1_5_2_compressBound(srcSize)`.
 * @return : compressed size written into `dst` (<= `dstCapacity),
 *           or an error code if it fails (which can be tested using ZSTD_v1_5_2_isError()).
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_compress2( ZSTD_v1_5_2_CCtx* cctx,
                                   void* dst, size_t dstCapacity,
                             const void* src, size_t srcSize);


/***********************************************
*  Advanced decompression API (Requires v1.4.0+)
************************************************/

/* The advanced API pushes parameters one by one into an existing DCtx context.
 * Parameters are sticky, and remain valid for all following frames
 * using the same DCtx context.
 * It's possible to reset parameters to default values using ZSTD_v1_5_2_DCtx_reset().
 * Note : This API is compatible with existing ZSTD_v1_5_2_decompressDCtx() and ZSTD_v1_5_2_decompressStream().
 *        Therefore, no new decompression function is necessary.
 */

typedef enum {

    ZSTD_v1_5_2_d_windowLogMax=100, /* Select a size limit (in power of 2) beyond which
                              * the streaming API will refuse to allocate memory buffer
                              * in order to protect the host from unreasonable memory requirements.
                              * This parameter is only useful in streaming mode, since no internal buffer is allocated in single-pass mode.
                              * By default, a decompression context accepts window sizes <= (1 << ZSTD_v1_5_2_WINDOWLOG_LIMIT_DEFAULT).
                              * Special: value 0 means "use default maximum windowLog". */

    /* note : additional experimental parameters are also available
     * within the experimental section of the API.
     * At the time of this writing, they include :
     * ZSTD_v1_5_2_d_format
     * ZSTD_v1_5_2_d_stableOutBuffer
     * ZSTD_v1_5_2_d_forceIgnoreChecksum
     * ZSTD_v1_5_2_d_refMultipleDDicts
     * Because they are not stable, it's necessary to define ZSTD_v1_5_2_STATIC_LINKING_ONLY to access them.
     * note : never ever use experimentalParam? names directly
     */
     ZSTD_v1_5_2_d_experimentalParam1=1000,
     ZSTD_v1_5_2_d_experimentalParam2=1001,
     ZSTD_v1_5_2_d_experimentalParam3=1002,
     ZSTD_v1_5_2_d_experimentalParam4=1003

} ZSTD_v1_5_2_dParameter;

/*! ZSTD_v1_5_2_dParam_getBounds() :
 *  All parameters must belong to an interval with lower and upper bounds,
 *  otherwise they will either trigger an error or be automatically clamped.
 * @return : a structure, ZSTD_v1_5_2_bounds, which contains
 *         - an error status field, which must be tested using ZSTD_v1_5_2_isError()
 *         - both lower and upper bounds, inclusive
 */
ZSTDLIB_API ZSTD_v1_5_2_bounds ZSTD_v1_5_2_dParam_getBounds(ZSTD_v1_5_2_dParameter dParam);

/*! ZSTD_v1_5_2_DCtx_setParameter() :
 *  Set one compression parameter, selected by enum ZSTD_v1_5_2_dParameter.
 *  All parameters have valid bounds. Bounds can be queried using ZSTD_v1_5_2_dParam_getBounds().
 *  Providing a value beyond bound will either clamp it, or trigger an error (depending on parameter).
 *  Setting a parameter is only possible during frame initialization (before starting decompression).
 * @return : 0, or an error code (which can be tested using ZSTD_v1_5_2_isError()).
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_DCtx_setParameter(ZSTD_v1_5_2_DCtx* dctx, ZSTD_v1_5_2_dParameter param, int value);

/*! ZSTD_v1_5_2_DCtx_reset() :
 *  Return a DCtx to clean state.
 *  Session and parameters can be reset jointly or separately.
 *  Parameters can only be reset when no active frame is being decompressed.
 * @return : 0, or an error code, which can be tested with ZSTD_v1_5_2_isError()
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_DCtx_reset(ZSTD_v1_5_2_DCtx* dctx, ZSTD_v1_5_2_ResetDirective reset);


/****************************
*  Streaming
****************************/

typedef struct ZSTD_v1_5_2_inBuffer_s {
  const void* src;    /**< start of input buffer */
  size_t size;        /**< size of input buffer */
  size_t pos;         /**< position where reading stopped. Will be updated. Necessarily 0 <= pos <= size */
} ZSTD_v1_5_2_inBuffer;

typedef struct ZSTD_v1_5_2_outBuffer_s {
  void*  dst;         /**< start of output buffer */
  size_t size;        /**< size of output buffer */
  size_t pos;         /**< position where writing stopped. Will be updated. Necessarily 0 <= pos <= size */
} ZSTD_v1_5_2_outBuffer;



/*-***********************************************************************
*  Streaming compression - HowTo
*
*  A ZSTD_v1_5_2_CStream object is required to track streaming operation.
*  Use ZSTD_v1_5_2_createCStream() and ZSTD_v1_5_2_freeCStream() to create/release resources.
*  ZSTD_v1_5_2_CStream objects can be reused multiple times on consecutive compression operations.
*  It is recommended to re-use ZSTD_v1_5_2_CStream since it will play nicer with system's memory, by re-using already allocated memory.
*
*  For parallel execution, use one separate ZSTD_v1_5_2_CStream per thread.
*
*  note : since v1.3.0, ZSTD_v1_5_2_CStream and ZSTD_v1_5_2_CCtx are the same thing.
*
*  Parameters are sticky : when starting a new compression on the same context,
*  it will re-use the same sticky parameters as previous compression session.
*  When in doubt, it's recommended to fully initialize the context before usage.
*  Use ZSTD_v1_5_2_CCtx_reset() to reset the context and ZSTD_v1_5_2_CCtx_setParameter(),
*  ZSTD_v1_5_2_CCtx_setPledgedSrcSize(), or ZSTD_v1_5_2_CCtx_loadDictionary() and friends to
*  set more specific parameters, the pledged source size, or load a dictionary.
*
*  Use ZSTD_v1_5_2_compressStream2() with ZSTD_v1_5_2_e_continue as many times as necessary to
*  consume input stream. The function will automatically update both `pos`
*  fields within `input` and `output`.
*  Note that the function may not consume the entire input, for example, because
*  the output buffer is already full, in which case `input.pos < input.size`.
*  The caller must check if input has been entirely consumed.
*  If not, the caller must make some room to receive more compressed data,
*  and then present again remaining input data.
*  note: ZSTD_v1_5_2_e_continue is guaranteed to make some forward progress when called,
*        but doesn't guarantee maximal forward progress. This is especially relevant
*        when compressing with multiple threads. The call won't block if it can
*        consume some input, but if it can't it will wait for some, but not all,
*        output to be flushed.
* @return : provides a minimum amount of data remaining to be flushed from internal buffers
*           or an error code, which can be tested using ZSTD_v1_5_2_isError().
*
*  At any moment, it's possible to flush whatever data might remain stuck within internal buffer,
*  using ZSTD_v1_5_2_compressStream2() with ZSTD_v1_5_2_e_flush. `output->pos` will be updated.
*  Note that, if `output->size` is too small, a single invocation with ZSTD_v1_5_2_e_flush might not be enough (return code > 0).
*  In which case, make some room to receive more compressed data, and call again ZSTD_v1_5_2_compressStream2() with ZSTD_v1_5_2_e_flush.
*  You must continue calling ZSTD_v1_5_2_compressStream2() with ZSTD_v1_5_2_e_flush until it returns 0, at which point you can change the
*  operation.
*  note: ZSTD_v1_5_2_e_flush will flush as much output as possible, meaning when compressing with multiple threads, it will
*        block until the flush is complete or the output buffer is full.
*  @return : 0 if internal buffers are entirely flushed,
*            >0 if some data still present within internal buffer (the value is minimal estimation of remaining size),
*            or an error code, which can be tested using ZSTD_v1_5_2_isError().
*
*  Calling ZSTD_v1_5_2_compressStream2() with ZSTD_v1_5_2_e_end instructs to finish a frame.
*  It will perform a flush and write frame epilogue.
*  The epilogue is required for decoders to consider a frame completed.
*  flush operation is the same, and follows same rules as calling ZSTD_v1_5_2_compressStream2() with ZSTD_v1_5_2_e_flush.
*  You must continue calling ZSTD_v1_5_2_compressStream2() with ZSTD_v1_5_2_e_end until it returns 0, at which point you are free to
*  start a new frame.
*  note: ZSTD_v1_5_2_e_end will flush as much output as possible, meaning when compressing with multiple threads, it will
*        block until the flush is complete or the output buffer is full.
*  @return : 0 if frame fully completed and fully flushed,
*            >0 if some data still present within internal buffer (the value is minimal estimation of remaining size),
*            or an error code, which can be tested using ZSTD_v1_5_2_isError().
*
* *******************************************************************/

typedef ZSTD_v1_5_2_CCtx ZSTD_v1_5_2_CStream;  /**< CCtx and CStream are now effectively same object (>= v1.3.0) */
                                 /* Continue to distinguish them for compatibility with older versions <= v1.2.0 */
/*===== ZSTD_v1_5_2_CStream management functions =====*/
ZSTDLIB_API ZSTD_v1_5_2_CStream* ZSTD_v1_5_2_createCStream(void);
ZSTDLIB_API size_t ZSTD_v1_5_2_freeCStream(ZSTD_v1_5_2_CStream* zcs);  /* accept NULL pointer */

/*===== Streaming compression functions =====*/
typedef enum {
    ZSTD_v1_5_2_e_continue=0, /* collect more data, encoder decides when to output compressed result, for optimal compression ratio */
    ZSTD_v1_5_2_e_flush=1,    /* flush any data provided so far,
                        * it creates (at least) one new block, that can be decoded immediately on reception;
                        * frame will continue: any future data can still reference previously compressed data, improving compression.
                        * note : multithreaded compression will block to flush as much output as possible. */
    ZSTD_v1_5_2_e_end=2       /* flush any remaining data _and_ close current frame.
                        * note that frame is only closed after compressed data is fully flushed (return value == 0).
                        * After that point, any additional data starts a new frame.
                        * note : each frame is independent (does not reference any content from previous frame).
                        : note : multithreaded compression will block to flush as much output as possible. */
} ZSTD_v1_5_2_EndDirective;

/*! ZSTD_v1_5_2_compressStream2() : Requires v1.4.0+
 *  Behaves about the same as ZSTD_v1_5_2_compressStream, with additional control on end directive.
 *  - Compression parameters are pushed into CCtx before starting compression, using ZSTD_v1_5_2_CCtx_set*()
 *  - Compression parameters cannot be changed once compression is started (save a list of exceptions in multi-threading mode)
 *  - output->pos must be <= dstCapacity, input->pos must be <= srcSize
 *  - output->pos and input->pos will be updated. They are guaranteed to remain below their respective limit.
 *  - endOp must be a valid directive
 *  - When nbWorkers==0 (default), function is blocking : it completes its job before returning to caller.
 *  - When nbWorkers>=1, function is non-blocking : it copies a portion of input, distributes jobs to internal worker threads, flush to output whatever is available,
 *                                                  and then immediately returns, just indicating that there is some data remaining to be flushed.
 *                                                  The function nonetheless guarantees forward progress : it will return only after it reads or write at least 1+ byte.
 *  - Exception : if the first call requests a ZSTD_v1_5_2_e_end directive and provides enough dstCapacity, the function delegates to ZSTD_v1_5_2_compress2() which is always blocking.
 *  - @return provides a minimum amount of data remaining to be flushed from internal buffers
 *            or an error code, which can be tested using ZSTD_v1_5_2_isError().
 *            if @return != 0, flush is not fully completed, there is still some data left within internal buffers.
 *            This is useful for ZSTD_v1_5_2_e_flush, since in this case more flushes are necessary to empty all buffers.
 *            For ZSTD_v1_5_2_e_end, @return == 0 when internal buffers are fully flushed and frame is completed.
 *  - after a ZSTD_v1_5_2_e_end directive, if internal buffer is not fully flushed (@return != 0),
 *            only ZSTD_v1_5_2_e_end or ZSTD_v1_5_2_e_flush operations are allowed.
 *            Before starting a new compression job, or changing compression parameters,
 *            it is required to fully flush internal buffers.
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_compressStream2( ZSTD_v1_5_2_CCtx* cctx,
                                         ZSTD_v1_5_2_outBuffer* output,
                                         ZSTD_v1_5_2_inBuffer* input,
                                         ZSTD_v1_5_2_EndDirective endOp);


/* These buffer sizes are softly recommended.
 * They are not required : ZSTD_v1_5_2_compressStream*() happily accepts any buffer size, for both input and output.
 * Respecting the recommended size just makes it a bit easier for ZSTD_v1_5_2_compressStream*(),
 * reducing the amount of memory shuffling and buffering, resulting in minor performance savings.
 *
 * However, note that these recommendations are from the perspective of a C caller program.
 * If the streaming interface is invoked from some other language,
 * especially managed ones such as Java or Go, through a foreign function interface such as jni or cgo,
 * a major performance rule is to reduce crossing such interface to an absolute minimum.
 * It's not rare that performance ends being spent more into the interface, rather than compression itself.
 * In which cases, prefer using large buffers, as large as practical,
 * for both input and output, to reduce the nb of roundtrips.
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_CStreamInSize(void);    /**< recommended size for input buffer */
ZSTDLIB_API size_t ZSTD_v1_5_2_CStreamOutSize(void);   /**< recommended size for output buffer. Guarantee to successfully flush at least one complete compressed block. */


/* *****************************************************************************
 * This following is a legacy streaming API, available since v1.0+ .
 * It can be replaced by ZSTD_v1_5_2_CCtx_reset() and ZSTD_v1_5_2_compressStream2().
 * It is redundant, but remains fully supported.
 * Streaming in combination with advanced parameters and dictionary compression
 * can only be used through the new API.
 ******************************************************************************/

/*!
 * Equivalent to:
 *
 *     ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only);
 *     ZSTD_v1_5_2_CCtx_refCDict(zcs, NULL); // clear the dictionary (if any)
 *     ZSTD_v1_5_2_CCtx_setParameter(zcs, ZSTD_v1_5_2_c_compressionLevel, compressionLevel);
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_initCStream(ZSTD_v1_5_2_CStream* zcs, int compressionLevel);
/*!
 * Alternative for ZSTD_v1_5_2_compressStream2(zcs, output, input, ZSTD_v1_5_2_e_continue).
 * NOTE: The return value is different. ZSTD_v1_5_2_compressStream() returns a hint for
 * the next read size (if non-zero and not an error). ZSTD_v1_5_2_compressStream2()
 * returns the minimum nb of bytes left to flush (if non-zero and not an error).
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_compressStream(ZSTD_v1_5_2_CStream* zcs, ZSTD_v1_5_2_outBuffer* output, ZSTD_v1_5_2_inBuffer* input);
/*! Equivalent to ZSTD_v1_5_2_compressStream2(zcs, output, &emptyInput, ZSTD_v1_5_2_e_flush). */
ZSTDLIB_API size_t ZSTD_v1_5_2_flushStream(ZSTD_v1_5_2_CStream* zcs, ZSTD_v1_5_2_outBuffer* output);
/*! Equivalent to ZSTD_v1_5_2_compressStream2(zcs, output, &emptyInput, ZSTD_v1_5_2_e_end). */
ZSTDLIB_API size_t ZSTD_v1_5_2_endStream(ZSTD_v1_5_2_CStream* zcs, ZSTD_v1_5_2_outBuffer* output);


/*-***************************************************************************
*  Streaming decompression - HowTo
*
*  A ZSTD_v1_5_2_DStream object is required to track streaming operations.
*  Use ZSTD_v1_5_2_createDStream() and ZSTD_v1_5_2_freeDStream() to create/release resources.
*  ZSTD_v1_5_2_DStream objects can be re-used multiple times.
*
*  Use ZSTD_v1_5_2_initDStream() to start a new decompression operation.
* @return : recommended first input size
*  Alternatively, use advanced API to set specific properties.
*
*  Use ZSTD_v1_5_2_decompressStream() repetitively to consume your input.
*  The function will update both `pos` fields.
*  If `input.pos < input.size`, some input has not been consumed.
*  It's up to the caller to present again remaining data.
*  The function tries to flush all data decoded immediately, respecting output buffer size.
*  If `output.pos < output.size`, decoder has flushed everything it could.
*  But if `output.pos == output.size`, there might be some data left within internal buffers.,
*  In which case, call ZSTD_v1_5_2_decompressStream() again to flush whatever remains in the buffer.
*  Note : with no additional input provided, amount of data flushed is necessarily <= ZSTD_v1_5_2_BLOCKSIZE_MAX.
* @return : 0 when a frame is completely decoded and fully flushed,
*        or an error code, which can be tested using ZSTD_v1_5_2_isError(),
*        or any other value > 0, which means there is still some decoding or flushing to do to complete current frame :
*                                the return value is a suggested next input size (just a hint for better latency)
*                                that will never request more than the remaining frame size.
* *******************************************************************************/

typedef ZSTD_v1_5_2_DCtx ZSTD_v1_5_2_DStream;  /**< DCtx and DStream are now effectively same object (>= v1.3.0) */
                                 /* For compatibility with versions <= v1.2.0, prefer differentiating them. */
/*===== ZSTD_v1_5_2_DStream management functions =====*/
ZSTDLIB_API ZSTD_v1_5_2_DStream* ZSTD_v1_5_2_createDStream(void);
ZSTDLIB_API size_t ZSTD_v1_5_2_freeDStream(ZSTD_v1_5_2_DStream* zds);  /* accept NULL pointer */

/*===== Streaming decompression functions =====*/

/* This function is redundant with the advanced API and equivalent to:
 *
 *     ZSTD_v1_5_2_DCtx_reset(zds, ZSTD_v1_5_2_reset_session_only);
 *     ZSTD_v1_5_2_DCtx_refDDict(zds, NULL);
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_initDStream(ZSTD_v1_5_2_DStream* zds);

ZSTDLIB_API size_t ZSTD_v1_5_2_decompressStream(ZSTD_v1_5_2_DStream* zds, ZSTD_v1_5_2_outBuffer* output, ZSTD_v1_5_2_inBuffer* input);

ZSTDLIB_API size_t ZSTD_v1_5_2_DStreamInSize(void);    /*!< recommended size for input buffer */
ZSTDLIB_API size_t ZSTD_v1_5_2_DStreamOutSize(void);   /*!< recommended size for output buffer. Guarantee to successfully flush at least one complete block in all circumstances. */


/**************************
*  Simple dictionary API
***************************/
/*! ZSTD_v1_5_2_compress_usingDict() :
 *  Compression at an explicit compression level using a Dictionary.
 *  A dictionary can be any arbitrary data segment (also called a prefix),
 *  or a buffer with specified information (see zdict.h).
 *  Note : This function loads the dictionary, resulting in significant startup delay.
 *         It's intended for a dictionary used only once.
 *  Note 2 : When `dict == NULL || dictSize < 8` no dictionary is used. */
ZSTDLIB_API size_t ZSTD_v1_5_2_compress_usingDict(ZSTD_v1_5_2_CCtx* ctx,
                                           void* dst, size_t dstCapacity,
                                     const void* src, size_t srcSize,
                                     const void* dict,size_t dictSize,
                                           int compressionLevel);

/*! ZSTD_v1_5_2_decompress_usingDict() :
 *  Decompression using a known Dictionary.
 *  Dictionary must be identical to the one used during compression.
 *  Note : This function loads the dictionary, resulting in significant startup delay.
 *         It's intended for a dictionary used only once.
 *  Note : When `dict == NULL || dictSize < 8` no dictionary is used. */
ZSTDLIB_API size_t ZSTD_v1_5_2_decompress_usingDict(ZSTD_v1_5_2_DCtx* dctx,
                                             void* dst, size_t dstCapacity,
                                       const void* src, size_t srcSize,
                                       const void* dict,size_t dictSize);


/***********************************
 *  Bulk processing dictionary API
 **********************************/
typedef struct ZSTD_v1_5_2_CDict_s ZSTD_v1_5_2_CDict;

/*! ZSTD_v1_5_2_createCDict() :
 *  When compressing multiple messages or blocks using the same dictionary,
 *  it's recommended to digest the dictionary only once, since it's a costly operation.
 *  ZSTD_v1_5_2_createCDict() will create a state from digesting a dictionary.
 *  The resulting state can be used for future compression operations with very limited startup cost.
 *  ZSTD_v1_5_2_CDict can be created once and shared by multiple threads concurrently, since its usage is read-only.
 * @dictBuffer can be released after ZSTD_v1_5_2_CDict creation, because its content is copied within CDict.
 *  Note 1 : Consider experimental function `ZSTD_v1_5_2_createCDict_byReference()` if you prefer to not duplicate @dictBuffer content.
 *  Note 2 : A ZSTD_v1_5_2_CDict can be created from an empty @dictBuffer,
 *      in which case the only thing that it transports is the @compressionLevel.
 *      This can be useful in a pipeline featuring ZSTD_v1_5_2_compress_usingCDict() exclusively,
 *      expecting a ZSTD_v1_5_2_CDict parameter with any data, including those without a known dictionary. */
ZSTDLIB_API ZSTD_v1_5_2_CDict* ZSTD_v1_5_2_createCDict(const void* dictBuffer, size_t dictSize,
                                         int compressionLevel);

/*! ZSTD_v1_5_2_freeCDict() :
 *  Function frees memory allocated by ZSTD_v1_5_2_createCDict().
 *  If a NULL pointer is passed, no operation is performed. */
ZSTDLIB_API size_t      ZSTD_v1_5_2_freeCDict(ZSTD_v1_5_2_CDict* CDict);

/*! ZSTD_v1_5_2_compress_usingCDict() :
 *  Compression using a digested Dictionary.
 *  Recommended when same dictionary is used multiple times.
 *  Note : compression level is _decided at dictionary creation time_,
 *     and frame parameters are hardcoded (dictID=yes, contentSize=yes, checksum=no) */
ZSTDLIB_API size_t ZSTD_v1_5_2_compress_usingCDict(ZSTD_v1_5_2_CCtx* cctx,
                                            void* dst, size_t dstCapacity,
                                      const void* src, size_t srcSize,
                                      const ZSTD_v1_5_2_CDict* cdict);


typedef struct ZSTD_v1_5_2_DDict_s ZSTD_v1_5_2_DDict;

/*! ZSTD_v1_5_2_createDDict() :
 *  Create a digested dictionary, ready to start decompression operation without startup delay.
 *  dictBuffer can be released after DDict creation, as its content is copied inside DDict. */
ZSTDLIB_API ZSTD_v1_5_2_DDict* ZSTD_v1_5_2_createDDict(const void* dictBuffer, size_t dictSize);

/*! ZSTD_v1_5_2_freeDDict() :
 *  Function frees memory allocated with ZSTD_v1_5_2_createDDict()
 *  If a NULL pointer is passed, no operation is performed. */
ZSTDLIB_API size_t      ZSTD_v1_5_2_freeDDict(ZSTD_v1_5_2_DDict* ddict);

/*! ZSTD_v1_5_2_decompress_usingDDict() :
 *  Decompression using a digested Dictionary.
 *  Recommended when same dictionary is used multiple times. */
ZSTDLIB_API size_t ZSTD_v1_5_2_decompress_usingDDict(ZSTD_v1_5_2_DCtx* dctx,
                                              void* dst, size_t dstCapacity,
                                        const void* src, size_t srcSize,
                                        const ZSTD_v1_5_2_DDict* ddict);


/********************************
 *  Dictionary helper functions
 *******************************/

/*! ZSTD_v1_5_2_getDictID_fromDict() : Requires v1.4.0+
 *  Provides the dictID stored within dictionary.
 *  if @return == 0, the dictionary is not conformant with Zstandard specification.
 *  It can still be loaded, but as a content-only dictionary. */
ZSTDLIB_API unsigned ZSTD_v1_5_2_getDictID_fromDict(const void* dict, size_t dictSize);

/*! ZSTD_v1_5_2_getDictID_fromCDict() : Requires v1.5.0+
 *  Provides the dictID of the dictionary loaded into `cdict`.
 *  If @return == 0, the dictionary is not conformant to Zstandard specification, or empty.
 *  Non-conformant dictionaries can still be loaded, but as content-only dictionaries. */
ZSTDLIB_API unsigned ZSTD_v1_5_2_getDictID_fromCDict(const ZSTD_v1_5_2_CDict* cdict);

/*! ZSTD_v1_5_2_getDictID_fromDDict() : Requires v1.4.0+
 *  Provides the dictID of the dictionary loaded into `ddict`.
 *  If @return == 0, the dictionary is not conformant to Zstandard specification, or empty.
 *  Non-conformant dictionaries can still be loaded, but as content-only dictionaries. */
ZSTDLIB_API unsigned ZSTD_v1_5_2_getDictID_fromDDict(const ZSTD_v1_5_2_DDict* ddict);

/*! ZSTD_v1_5_2_getDictID_fromFrame() : Requires v1.4.0+
 *  Provides the dictID required to decompressed the frame stored within `src`.
 *  If @return == 0, the dictID could not be decoded.
 *  This could for one of the following reasons :
 *  - The frame does not require a dictionary to be decoded (most common case).
 *  - The frame was built with dictID intentionally removed. Whatever dictionary is necessary is a hidden information.
 *    Note : this use case also happens when using a non-conformant dictionary.
 *  - `srcSize` is too small, and as a result, the frame header could not be decoded (only possible if `srcSize < ZSTD_v1_5_2_FRAMEHEADERSIZE_MAX`).
 *  - This is not a Zstandard frame.
 *  When identifying the exact failure cause, it's possible to use ZSTD_v1_5_2_getFrameHeader(), which will provide a more precise error code. */
ZSTDLIB_API unsigned ZSTD_v1_5_2_getDictID_fromFrame(const void* src, size_t srcSize);


/*******************************************************************************
 * Advanced dictionary and prefix API (Requires v1.4.0+)
 *
 * This API allows dictionaries to be used with ZSTD_v1_5_2_compress2(),
 * ZSTD_v1_5_2_compressStream2(), and ZSTD_v1_5_2_decompressDCtx(). Dictionaries are sticky, and
 * only reset with the context is reset with ZSTD_v1_5_2_reset_parameters or
 * ZSTD_v1_5_2_reset_session_and_parameters. Prefixes are single-use.
 ******************************************************************************/


/*! ZSTD_v1_5_2_CCtx_loadDictionary() : Requires v1.4.0+
 *  Create an internal CDict from `dict` buffer.
 *  Decompression will have to use same dictionary.
 * @result : 0, or an error code (which can be tested with ZSTD_v1_5_2_isError()).
 *  Special: Loading a NULL (or 0-size) dictionary invalidates previous dictionary,
 *           meaning "return to no-dictionary mode".
 *  Note 1 : Dictionary is sticky, it will be used for all future compressed frames.
 *           To return to "no-dictionary" situation, load a NULL dictionary (or reset parameters).
 *  Note 2 : Loading a dictionary involves building tables.
 *           It's also a CPU consuming operation, with non-negligible impact on latency.
 *           Tables are dependent on compression parameters, and for this reason,
 *           compression parameters can no longer be changed after loading a dictionary.
 *  Note 3 :`dict` content will be copied internally.
 *           Use experimental ZSTD_v1_5_2_CCtx_loadDictionary_byReference() to reference content instead.
 *           In such a case, dictionary buffer must outlive its users.
 *  Note 4 : Use ZSTD_v1_5_2_CCtx_loadDictionary_advanced()
 *           to precisely select how dictionary content must be interpreted. */
ZSTDLIB_API size_t ZSTD_v1_5_2_CCtx_loadDictionary(ZSTD_v1_5_2_CCtx* cctx, const void* dict, size_t dictSize);

/*! ZSTD_v1_5_2_CCtx_refCDict() : Requires v1.4.0+
 *  Reference a prepared dictionary, to be used for all next compressed frames.
 *  Note that compression parameters are enforced from within CDict,
 *  and supersede any compression parameter previously set within CCtx.
 *  The parameters ignored are labelled as "superseded-by-cdict" in the ZSTD_v1_5_2_cParameter enum docs.
 *  The ignored parameters will be used again if the CCtx is returned to no-dictionary mode.
 *  The dictionary will remain valid for future compressed frames using same CCtx.
 * @result : 0, or an error code (which can be tested with ZSTD_v1_5_2_isError()).
 *  Special : Referencing a NULL CDict means "return to no-dictionary mode".
 *  Note 1 : Currently, only one dictionary can be managed.
 *           Referencing a new dictionary effectively "discards" any previous one.
 *  Note 2 : CDict is just referenced, its lifetime must outlive its usage within CCtx. */
ZSTDLIB_API size_t ZSTD_v1_5_2_CCtx_refCDict(ZSTD_v1_5_2_CCtx* cctx, const ZSTD_v1_5_2_CDict* cdict);

/*! ZSTD_v1_5_2_CCtx_refPrefix() : Requires v1.4.0+
 *  Reference a prefix (single-usage dictionary) for next compressed frame.
 *  A prefix is **only used once**. Tables are discarded at end of frame (ZSTD_v1_5_2_e_end).
 *  Decompression will need same prefix to properly regenerate data.
 *  Compressing with a prefix is similar in outcome as performing a diff and compressing it,
 *  but performs much faster, especially during decompression (compression speed is tunable with compression level).
 * @result : 0, or an error code (which can be tested with ZSTD_v1_5_2_isError()).
 *  Special: Adding any prefix (including NULL) invalidates any previous prefix or dictionary
 *  Note 1 : Prefix buffer is referenced. It **must** outlive compression.
 *           Its content must remain unmodified during compression.
 *  Note 2 : If the intention is to diff some large src data blob with some prior version of itself,
 *           ensure that the window size is large enough to contain the entire source.
 *           See ZSTD_v1_5_2_c_windowLog.
 *  Note 3 : Referencing a prefix involves building tables, which are dependent on compression parameters.
 *           It's a CPU consuming operation, with non-negligible impact on latency.
 *           If there is a need to use the same prefix multiple times, consider loadDictionary instead.
 *  Note 4 : By default, the prefix is interpreted as raw content (ZSTD_v1_5_2_dct_rawContent).
 *           Use experimental ZSTD_v1_5_2_CCtx_refPrefix_advanced() to alter dictionary interpretation. */
ZSTDLIB_API size_t ZSTD_v1_5_2_CCtx_refPrefix(ZSTD_v1_5_2_CCtx* cctx,
                                 const void* prefix, size_t prefixSize);

/*! ZSTD_v1_5_2_DCtx_loadDictionary() : Requires v1.4.0+
 *  Create an internal DDict from dict buffer,
 *  to be used to decompress next frames.
 *  The dictionary remains valid for all future frames, until explicitly invalidated.
 * @result : 0, or an error code (which can be tested with ZSTD_v1_5_2_isError()).
 *  Special : Adding a NULL (or 0-size) dictionary invalidates any previous dictionary,
 *            meaning "return to no-dictionary mode".
 *  Note 1 : Loading a dictionary involves building tables,
 *           which has a non-negligible impact on CPU usage and latency.
 *           It's recommended to "load once, use many times", to amortize the cost
 *  Note 2 :`dict` content will be copied internally, so `dict` can be released after loading.
 *           Use ZSTD_v1_5_2_DCtx_loadDictionary_byReference() to reference dictionary content instead.
 *  Note 3 : Use ZSTD_v1_5_2_DCtx_loadDictionary_advanced() to take control of
 *           how dictionary content is loaded and interpreted.
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_DCtx_loadDictionary(ZSTD_v1_5_2_DCtx* dctx, const void* dict, size_t dictSize);

/*! ZSTD_v1_5_2_DCtx_refDDict() : Requires v1.4.0+
 *  Reference a prepared dictionary, to be used to decompress next frames.
 *  The dictionary remains active for decompression of future frames using same DCtx.
 *
 *  If called with ZSTD_v1_5_2_d_refMultipleDDicts enabled, repeated calls of this function
 *  will store the DDict references in a table, and the DDict used for decompression
 *  will be determined at decompression time, as per the dict ID in the frame.
 *  The memory for the table is allocated on the first call to refDDict, and can be
 *  freed with ZSTD_v1_5_2_freeDCtx().
 *
 * @result : 0, or an error code (which can be tested with ZSTD_v1_5_2_isError()).
 *  Note 1 : Currently, only one dictionary can be managed.
 *           Referencing a new dictionary effectively "discards" any previous one.
 *  Special: referencing a NULL DDict means "return to no-dictionary mode".
 *  Note 2 : DDict is just referenced, its lifetime must outlive its usage from DCtx.
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_DCtx_refDDict(ZSTD_v1_5_2_DCtx* dctx, const ZSTD_v1_5_2_DDict* ddict);

/*! ZSTD_v1_5_2_DCtx_refPrefix() : Requires v1.4.0+
 *  Reference a prefix (single-usage dictionary) to decompress next frame.
 *  This is the reverse operation of ZSTD_v1_5_2_CCtx_refPrefix(),
 *  and must use the same prefix as the one used during compression.
 *  Prefix is **only used once**. Reference is discarded at end of frame.
 *  End of frame is reached when ZSTD_v1_5_2_decompressStream() returns 0.
 * @result : 0, or an error code (which can be tested with ZSTD_v1_5_2_isError()).
 *  Note 1 : Adding any prefix (including NULL) invalidates any previously set prefix or dictionary
 *  Note 2 : Prefix buffer is referenced. It **must** outlive decompression.
 *           Prefix buffer must remain unmodified up to the end of frame,
 *           reached when ZSTD_v1_5_2_decompressStream() returns 0.
 *  Note 3 : By default, the prefix is treated as raw content (ZSTD_v1_5_2_dct_rawContent).
 *           Use ZSTD_v1_5_2_CCtx_refPrefix_advanced() to alter dictMode (Experimental section)
 *  Note 4 : Referencing a raw content prefix has almost no cpu nor memory cost.
 *           A full dictionary is more costly, as it requires building tables.
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_DCtx_refPrefix(ZSTD_v1_5_2_DCtx* dctx,
                                 const void* prefix, size_t prefixSize);

/* ===   Memory management   === */

/*! ZSTD_v1_5_2_sizeof_*() : Requires v1.4.0+
 *  These functions give the _current_ memory usage of selected object.
 *  Note that object memory usage can evolve (increase or decrease) over time. */
ZSTDLIB_API size_t ZSTD_v1_5_2_sizeof_CCtx(const ZSTD_v1_5_2_CCtx* cctx);
ZSTDLIB_API size_t ZSTD_v1_5_2_sizeof_DCtx(const ZSTD_v1_5_2_DCtx* dctx);
ZSTDLIB_API size_t ZSTD_v1_5_2_sizeof_CStream(const ZSTD_v1_5_2_CStream* zcs);
ZSTDLIB_API size_t ZSTD_v1_5_2_sizeof_DStream(const ZSTD_v1_5_2_DStream* zds);
ZSTDLIB_API size_t ZSTD_v1_5_2_sizeof_CDict(const ZSTD_v1_5_2_CDict* cdict);
ZSTDLIB_API size_t ZSTD_v1_5_2_sizeof_DDict(const ZSTD_v1_5_2_DDict* ddict);

#endif  /* ZSTD_v1_5_2_H_235446 */


/* **************************************************************************************
 *   ADVANCED AND EXPERIMENTAL FUNCTIONS
 ****************************************************************************************
 * The definitions in the following section are considered experimental.
 * They are provided for advanced scenarios.
 * They should never be used with a dynamic library, as prototypes may change in the future.
 * Use them only in association with static linking.
 * ***************************************************************************************/

#if defined(ZSTD_v1_5_2_STATIC_LINKING_ONLY) && !defined(ZSTD_v1_5_2_H_ZSTD_v1_5_2_STATIC_LINKING_ONLY)
#define ZSTD_v1_5_2_H_ZSTD_v1_5_2_STATIC_LINKING_ONLY

/* This can be overridden externally to hide static symbols. */
#ifndef ZSTDLIB_STATIC_API
#  if defined(ZSTD_v1_5_2_DLL_EXPORT) && (ZSTD_v1_5_2_DLL_EXPORT==1)
#    define ZSTDLIB_STATIC_API __declspec(dllexport) ZSTDLIB_VISIBLE
#  elif defined(ZSTD_v1_5_2_DLL_IMPORT) && (ZSTD_v1_5_2_DLL_IMPORT==1)
#    define ZSTDLIB_STATIC_API __declspec(dllimport) ZSTDLIB_VISIBLE
#  else
#    define ZSTDLIB_STATIC_API ZSTDLIB_VISIBLE
#  endif
#endif

/* Deprecation warnings :
 * Should these warnings be a problem, it is generally possible to disable them,
 * typically with -Wno-deprecated-declarations for gcc or _CRT_SECURE_NO_WARNINGS in Visual.
 * Otherwise, it's also possible to define ZSTD_v1_5_2_DISABLE_DEPRECATE_WARNINGS.
 */
#ifdef ZSTD_v1_5_2_DISABLE_DEPRECATE_WARNINGS
#  define ZSTD_v1_5_2_DEPRECATED(message) ZSTDLIB_STATIC_API  /* disable deprecation warnings */
#else
#  if defined (__cplusplus) && (__cplusplus >= 201402) /* C++14 or greater */
#    define ZSTD_v1_5_2_DEPRECATED(message) [[deprecated(message)]] ZSTDLIB_STATIC_API
#  elif (defined(GNUC) && (GNUC > 4 || (GNUC == 4 && GNUC_MINOR >= 5))) || defined(__clang__)
#    define ZSTD_v1_5_2_DEPRECATED(message) ZSTDLIB_STATIC_API __attribute__((deprecated(message)))
#  elif defined(__GNUC__) && (__GNUC__ >= 3)
#    define ZSTD_v1_5_2_DEPRECATED(message) ZSTDLIB_STATIC_API __attribute__((deprecated))
#  elif defined(_MSC_VER)
#    define ZSTD_v1_5_2_DEPRECATED(message) ZSTDLIB_STATIC_API __declspec(deprecated(message))
#  else
#    pragma message("WARNING: You need to implement ZSTD_v1_5_2_DEPRECATED for this compiler")
#    define ZSTD_v1_5_2_DEPRECATED(message) ZSTDLIB_STATIC_API
#  endif
#endif /* ZSTD_v1_5_2_DISABLE_DEPRECATE_WARNINGS */

/****************************************************************************************
 *   experimental API (static linking only)
 ****************************************************************************************
 * The following symbols and constants
 * are not planned to join "stable API" status in the near future.
 * They can still change in future versions.
 * Some of them are planned to remain in the static_only section indefinitely.
 * Some of them might be removed in the future (especially when redundant with existing stable functions)
 * ***************************************************************************************/

#define ZSTD_v1_5_2_FRAMEHEADERSIZE_PREFIX(format) ((format) == ZSTD_v1_5_2_f_zstd1 ? 5 : 1)   /* minimum input size required to query frame header size */
#define ZSTD_v1_5_2_FRAMEHEADERSIZE_MIN(format)    ((format) == ZSTD_v1_5_2_f_zstd1 ? 6 : 2)
#define ZSTD_v1_5_2_FRAMEHEADERSIZE_MAX   18   /* can be useful for static allocation */
#define ZSTD_v1_5_2_SKIPPABLEHEADERSIZE    8

/* compression parameter bounds */
#define ZSTD_v1_5_2_WINDOWLOG_MAX_32    30
#define ZSTD_v1_5_2_WINDOWLOG_MAX_64    31
#define ZSTD_v1_5_2_WINDOWLOG_MAX     ((int)(sizeof(size_t) == 4 ? ZSTD_v1_5_2_WINDOWLOG_MAX_32 : ZSTD_v1_5_2_WINDOWLOG_MAX_64))
#define ZSTD_v1_5_2_WINDOWLOG_MIN       10
#define ZSTD_v1_5_2_HASHLOG_MAX       ((ZSTD_v1_5_2_WINDOWLOG_MAX < 30) ? ZSTD_v1_5_2_WINDOWLOG_MAX : 30)
#define ZSTD_v1_5_2_HASHLOG_MIN          6
#define ZSTD_v1_5_2_CHAINLOG_MAX_32     29
#define ZSTD_v1_5_2_CHAINLOG_MAX_64     30
#define ZSTD_v1_5_2_CHAINLOG_MAX      ((int)(sizeof(size_t) == 4 ? ZSTD_v1_5_2_CHAINLOG_MAX_32 : ZSTD_v1_5_2_CHAINLOG_MAX_64))
#define ZSTD_v1_5_2_CHAINLOG_MIN        ZSTD_v1_5_2_HASHLOG_MIN
#define ZSTD_v1_5_2_SEARCHLOG_MAX      (ZSTD_v1_5_2_WINDOWLOG_MAX-1)
#define ZSTD_v1_5_2_SEARCHLOG_MIN        1
#define ZSTD_v1_5_2_MINMATCH_MAX         7   /* only for ZSTD_v1_5_2_fast, other strategies are limited to 6 */
#define ZSTD_v1_5_2_MINMATCH_MIN         3   /* only for ZSTD_v1_5_2_btopt+, faster strategies are limited to 4 */
#define ZSTD_v1_5_2_TARGETLENGTH_MAX    ZSTD_v1_5_2_BLOCKSIZE_MAX
#define ZSTD_v1_5_2_TARGETLENGTH_MIN     0   /* note : comparing this constant to an unsigned results in a tautological test */
#define ZSTD_v1_5_2_STRATEGY_MIN        ZSTD_v1_5_2_fast
#define ZSTD_v1_5_2_STRATEGY_MAX        ZSTD_v1_5_2_btultra2


#define ZSTD_v1_5_2_OVERLAPLOG_MIN       0
#define ZSTD_v1_5_2_OVERLAPLOG_MAX       9

#define ZSTD_v1_5_2_WINDOWLOG_LIMIT_DEFAULT 27   /* by default, the streaming decoder will refuse any frame
                                           * requiring larger than (1<<ZSTD_v1_5_2_WINDOWLOG_LIMIT_DEFAULT) window size,
                                           * to preserve host's memory from unreasonable requirements.
                                           * This limit can be overridden using ZSTD_v1_5_2_DCtx_setParameter(,ZSTD_v1_5_2_d_windowLogMax,).
                                           * The limit does not apply for one-pass decoders (such as ZSTD_v1_5_2_decompress()), since no additional memory is allocated */


/* LDM parameter bounds */
#define ZSTD_v1_5_2_LDM_HASHLOG_MIN      ZSTD_v1_5_2_HASHLOG_MIN
#define ZSTD_v1_5_2_LDM_HASHLOG_MAX      ZSTD_v1_5_2_HASHLOG_MAX
#define ZSTD_v1_5_2_LDM_MINMATCH_MIN        4
#define ZSTD_v1_5_2_LDM_MINMATCH_MAX     4096
#define ZSTD_v1_5_2_LDM_BUCKETSIZELOG_MIN   1
#define ZSTD_v1_5_2_LDM_BUCKETSIZELOG_MAX   8
#define ZSTD_v1_5_2_LDM_HASHRATELOG_MIN     0
#define ZSTD_v1_5_2_LDM_HASHRATELOG_MAX (ZSTD_v1_5_2_WINDOWLOG_MAX - ZSTD_v1_5_2_HASHLOG_MIN)

/* Advanced parameter bounds */
#define ZSTD_v1_5_2_TARGETCBLOCKSIZE_MIN   64
#define ZSTD_v1_5_2_TARGETCBLOCKSIZE_MAX   ZSTD_v1_5_2_BLOCKSIZE_MAX
#define ZSTD_v1_5_2_SRCSIZEHINT_MIN        0
#define ZSTD_v1_5_2_SRCSIZEHINT_MAX        INT_MAX


/* ---  Advanced types  --- */

typedef struct ZSTD_v1_5_2_CCtx_params_s ZSTD_v1_5_2_CCtx_params;

typedef struct {
    unsigned int offset;      /* The offset of the match. (NOT the same as the offset code)
                               * If offset == 0 and matchLength == 0, this sequence represents the last
                               * literals in the block of litLength size.
                               */

    unsigned int litLength;   /* Literal length of the sequence. */
    unsigned int matchLength; /* Match length of the sequence. */

                              /* Note: Users of this API may provide a sequence with matchLength == litLength == offset == 0.
                               * In this case, we will treat the sequence as a marker for a block boundary.
                               */

    unsigned int rep;         /* Represents which repeat offset is represented by the field 'offset'.
                               * Ranges from [0, 3].
                               *
                               * Repeat offsets are essentially previous offsets from previous sequences sorted in
                               * recency order. For more detail, see doc/ZSTD_compression_format.md
                               *
                               * If rep == 0, then 'offset' does not contain a repeat offset.
                               * If rep > 0:
                               *  If litLength != 0:
                               *      rep == 1 --> offset == repeat_offset_1
                               *      rep == 2 --> offset == repeat_offset_2
                               *      rep == 3 --> offset == repeat_offset_3
                               *  If litLength == 0:
                               *      rep == 1 --> offset == repeat_offset_2
                               *      rep == 2 --> offset == repeat_offset_3
                               *      rep == 3 --> offset == repeat_offset_1 - 1
                               *
                               * Note: This field is optional. ZSTD_v1_5_2_generateSequences() will calculate the value of
                               * 'rep', but repeat offsets do not necessarily need to be calculated from an external
                               * sequence provider's perspective. For example, ZSTD_v1_5_2_compressSequences() does not
                               * use this 'rep' field at all (as of now).
                               */
} ZSTD_v1_5_2_Sequence;

typedef struct {
    unsigned windowLog;       /**< largest match distance : larger == more compression, more memory needed during decompression */
    unsigned chainLog;        /**< fully searched segment : larger == more compression, slower, more memory (useless for fast) */
    unsigned hashLog;         /**< dispatch table : larger == faster, more memory */
    unsigned searchLog;       /**< nb of searches : larger == more compression, slower */
    unsigned minMatch;        /**< match length searched : larger == faster decompression, sometimes less compression */
    unsigned targetLength;    /**< acceptable match size for optimal parser (only) : larger == more compression, slower */
    ZSTD_v1_5_2_strategy strategy;   /**< see ZSTD_v1_5_2_strategy definition above */
} ZSTD_v1_5_2_compressionParameters;

typedef struct {
    int contentSizeFlag; /**< 1: content size will be in frame header (when known) */
    int checksumFlag;    /**< 1: generate a 32-bits checksum using XXH64 algorithm at end of frame, for error detection */
    int noDictIDFlag;    /**< 1: no dictID will be saved into frame header (dictID is only useful for dictionary compression) */
} ZSTD_v1_5_2_frameParameters;

typedef struct {
    ZSTD_v1_5_2_compressionParameters cParams;
    ZSTD_v1_5_2_frameParameters fParams;
} ZSTD_v1_5_2_parameters;

typedef enum {
    ZSTD_v1_5_2_dct_auto = 0,       /* dictionary is "full" when starting with ZSTD_v1_5_2_MAGIC_DICTIONARY, otherwise it is "rawContent" */
    ZSTD_v1_5_2_dct_rawContent = 1, /* ensures dictionary is always loaded as rawContent, even if it starts with ZSTD_v1_5_2_MAGIC_DICTIONARY */
    ZSTD_v1_5_2_dct_fullDict = 2    /* refuses to load a dictionary if it does not respect Zstandard's specification, starting with ZSTD_v1_5_2_MAGIC_DICTIONARY */
} ZSTD_v1_5_2_dictContentType_e;

typedef enum {
    ZSTD_v1_5_2_dlm_byCopy = 0,  /**< Copy dictionary content internally */
    ZSTD_v1_5_2_dlm_byRef = 1    /**< Reference dictionary content -- the dictionary buffer must outlive its users. */
} ZSTD_v1_5_2_dictLoadMethod_e;

typedef enum {
    ZSTD_v1_5_2_f_zstd1 = 0,           /* zstd frame format, specified in ZSTD_compression_format.md (default) */
    ZSTD_v1_5_2_f_zstd1_magicless = 1  /* Variant of zstd frame format, without initial 4-bytes magic number.
                                 * Useful to save 4 bytes per generated frame.
                                 * Decoder cannot recognise automatically this format, requiring this instruction. */
} ZSTD_v1_5_2_format_e;

typedef enum {
    /* Note: this enum controls ZSTD_v1_5_2_d_forceIgnoreChecksum */
    ZSTD_v1_5_2_d_validateChecksum = 0,
    ZSTD_v1_5_2_d_ignoreChecksum = 1
} ZSTD_v1_5_2_forceIgnoreChecksum_e;

typedef enum {
    /* Note: this enum controls ZSTD_v1_5_2_d_refMultipleDDicts */
    ZSTD_v1_5_2_rmd_refSingleDDict = 0,
    ZSTD_v1_5_2_rmd_refMultipleDDicts = 1
} ZSTD_v1_5_2_refMultipleDDicts_e;

typedef enum {
    /* Note: this enum and the behavior it controls are effectively internal
     * implementation details of the compressor. They are expected to continue
     * to evolve and should be considered only in the context of extremely
     * advanced performance tuning.
     *
     * Zstd currently supports the use of a CDict in three ways:
     *
     * - The contents of the CDict can be copied into the working context. This
     *   means that the compression can search both the dictionary and input
     *   while operating on a single set of internal tables. This makes
     *   the compression faster per-byte of input. However, the initial copy of
     *   the CDict's tables incurs a fixed cost at the beginning of the
     *   compression. For small compressions (< 8 KB), that copy can dominate
     *   the cost of the compression.
     *
     * - The CDict's tables can be used in-place. In this model, compression is
     *   slower per input byte, because the compressor has to search two sets of
     *   tables. However, this model incurs no start-up cost (as long as the
     *   working context's tables can be reused). For small inputs, this can be
     *   faster than copying the CDict's tables.
     *
     * - The CDict's tables are not used at all, and instead we use the working
     *   context alone to reload the dictionary and use params based on the source
     *   size. See ZSTD_v1_5_2_compress_insertDictionary() and ZSTD_v1_5_2_compress_usingDict().
     *   This method is effective when the dictionary sizes are very small relative
     *   to the input size, and the input size is fairly large to begin with.
     *
     * Zstd has a simple internal heuristic that selects which strategy to use
     * at the beginning of a compression. However, if experimentation shows that
     * Zstd is making poor choices, it is possible to override that choice with
     * this enum.
     */
    ZSTD_v1_5_2_dictDefaultAttach = 0, /* Use the default heuristic. */
    ZSTD_v1_5_2_dictForceAttach   = 1, /* Never copy the dictionary. */
    ZSTD_v1_5_2_dictForceCopy     = 2, /* Always copy the dictionary. */
    ZSTD_v1_5_2_dictForceLoad     = 3  /* Always reload the dictionary */
} ZSTD_v1_5_2_dictAttachPref_e;

typedef enum {
  ZSTD_v1_5_2_lcm_auto = 0,          /**< Automatically determine the compression mode based on the compression level.
                               *   Negative compression levels will be uncompressed, and positive compression
                               *   levels will be compressed. */
  ZSTD_v1_5_2_lcm_huffman = 1,       /**< Always attempt Huffman compression. Uncompressed literals will still be
                               *   emitted if Huffman compression is not profitable. */
  ZSTD_v1_5_2_lcm_uncompressed = 2   /**< Always emit uncompressed literals. */
} ZSTD_v1_5_2_literalCompressionMode_e;

typedef enum {
  /* Note: This enum controls features which are conditionally beneficial. Zstd typically will make a final
   * decision on whether or not to enable the feature (ZSTD_v1_5_2_ps_auto), but setting the switch to ZSTD_v1_5_2_ps_enable
   * or ZSTD_v1_5_2_ps_disable allow for a force enable/disable the feature.
   */
  ZSTD_v1_5_2_ps_auto = 0,         /* Let the library automatically determine whether the feature shall be enabled */
  ZSTD_v1_5_2_ps_enable = 1,       /* Force-enable the feature */
  ZSTD_v1_5_2_ps_disable = 2       /* Do not use the feature */
} ZSTD_v1_5_2_paramSwitch_e;

/***************************************
*  Frame size functions
***************************************/

/*! ZSTD_v1_5_2_findDecompressedSize() :
 *  `src` should point to the start of a series of ZSTD encoded and/or skippable frames
 *  `srcSize` must be the _exact_ size of this series
 *       (i.e. there should be a frame boundary at `src + srcSize`)
 *  @return : - decompressed size of all data in all successive frames
 *            - if the decompressed size cannot be determined: ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN
 *            - if an error occurred: ZSTD_v1_5_2_CONTENTSIZE_ERROR
 *
 *   note 1 : decompressed size is an optional field, that may not be present, especially in streaming mode.
 *            When `return==ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN`, data to decompress could be any size.
 *            In which case, it's necessary to use streaming mode to decompress data.
 *   note 2 : decompressed size is always present when compression is done with ZSTD_v1_5_2_compress()
 *   note 3 : decompressed size can be very large (64-bits value),
 *            potentially larger than what local system can handle as a single memory segment.
 *            In which case, it's necessary to use streaming mode to decompress data.
 *   note 4 : If source is untrusted, decompressed size could be wrong or intentionally modified.
 *            Always ensure result fits within application's authorized limits.
 *            Each application can set its own limits.
 *   note 5 : ZSTD_v1_5_2_findDecompressedSize handles multiple frames, and so it must traverse the input to
 *            read each contained frame header.  This is fast as most of the data is skipped,
 *            however it does mean that all frame data must be present and valid. */
ZSTDLIB_STATIC_API unsigned long long ZSTD_v1_5_2_findDecompressedSize(const void* src, size_t srcSize);

/*! ZSTD_v1_5_2_decompressBound() :
 *  `src` should point to the start of a series of ZSTD encoded and/or skippable frames
 *  `srcSize` must be the _exact_ size of this series
 *       (i.e. there should be a frame boundary at `src + srcSize`)
 *  @return : - upper-bound for the decompressed size of all data in all successive frames
 *            - if an error occurred: ZSTD_v1_5_2_CONTENTSIZE_ERROR
 *
 *  note 1  : an error can occur if `src` contains an invalid or incorrectly formatted frame.
 *  note 2  : the upper-bound is exact when the decompressed size field is available in every ZSTD encoded frame of `src`.
 *            in this case, `ZSTD_v1_5_2_findDecompressedSize` and `ZSTD_v1_5_2_decompressBound` return the same value.
 *  note 3  : when the decompressed size field isn't available, the upper-bound for that frame is calculated by:
 *              upper-bound = # blocks * min(128 KB, Window_Size)
 */
ZSTDLIB_STATIC_API unsigned long long ZSTD_v1_5_2_decompressBound(const void* src, size_t srcSize);

/*! ZSTD_v1_5_2_frameHeaderSize() :
 *  srcSize must be >= ZSTD_v1_5_2_FRAMEHEADERSIZE_PREFIX.
 * @return : size of the Frame Header,
 *           or an error code (if srcSize is too small) */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_frameHeaderSize(const void* src, size_t srcSize);

typedef enum {
  ZSTD_v1_5_2_sf_noBlockDelimiters = 0,         /* Representation of ZSTD_v1_5_2_Sequence has no block delimiters, sequences only */
  ZSTD_v1_5_2_sf_explicitBlockDelimiters = 1    /* Representation of ZSTD_v1_5_2_Sequence contains explicit block delimiters */
} ZSTD_v1_5_2_sequenceFormat_e;

/*! ZSTD_v1_5_2_generateSequences() :
 * Generate sequences using ZSTD_v1_5_2_compress2, given a source buffer.
 *
 * Each block will end with a dummy sequence
 * with offset == 0, matchLength == 0, and litLength == length of last literals.
 * litLength may be == 0, and if so, then the sequence of (of: 0 ml: 0 ll: 0)
 * simply acts as a block delimiter.
 *
 * zc can be used to insert custom compression params.
 * This function invokes ZSTD_v1_5_2_compress2
 *
 * The output of this function can be fed into ZSTD_v1_5_2_compressSequences() with CCtx
 * setting of ZSTD_v1_5_2_c_blockDelimiters as ZSTD_v1_5_2_sf_explicitBlockDelimiters
 * @return : number of sequences generated
 */

ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_generateSequences(ZSTD_v1_5_2_CCtx* zc, ZSTD_v1_5_2_Sequence* outSeqs,
                                          size_t outSeqsSize, const void* src, size_t srcSize);

/*! ZSTD_v1_5_2_mergeBlockDelimiters() :
 * Given an array of ZSTD_v1_5_2_Sequence, remove all sequences that represent block delimiters/last literals
 * by merging them into into the literals of the next sequence.
 *
 * As such, the final generated result has no explicit representation of block boundaries,
 * and the final last literals segment is not represented in the sequences.
 *
 * The output of this function can be fed into ZSTD_v1_5_2_compressSequences() with CCtx
 * setting of ZSTD_v1_5_2_c_blockDelimiters as ZSTD_v1_5_2_sf_noBlockDelimiters
 * @return : number of sequences left after merging
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_mergeBlockDelimiters(ZSTD_v1_5_2_Sequence* sequences, size_t seqsSize);

/*! ZSTD_v1_5_2_compressSequences() :
 * Compress an array of ZSTD_v1_5_2_Sequence, generated from the original source buffer, into dst.
 * If a dictionary is included, then the cctx should reference the dict. (see: ZSTD_v1_5_2_CCtx_refCDict(), ZSTD_v1_5_2_CCtx_loadDictionary(), etc.)
 * The entire source is compressed into a single frame.
 *
 * The compression behavior changes based on cctx params. In particular:
 *    If ZSTD_v1_5_2_c_blockDelimiters == ZSTD_v1_5_2_sf_noBlockDelimiters, the array of ZSTD_v1_5_2_Sequence is expected to contain
 *    no block delimiters (defined in ZSTD_v1_5_2_Sequence). Block boundaries are roughly determined based on
 *    the block size derived from the cctx, and sequences may be split. This is the default setting.
 *
 *    If ZSTD_v1_5_2_c_blockDelimiters == ZSTD_v1_5_2_sf_explicitBlockDelimiters, the array of ZSTD_v1_5_2_Sequence is expected to contain
 *    block delimiters (defined in ZSTD_v1_5_2_Sequence). Behavior is undefined if no block delimiters are provided.
 *
 *    If ZSTD_v1_5_2_c_validateSequences == 0, this function will blindly accept the sequences provided. Invalid sequences cause undefined
 *    behavior. If ZSTD_v1_5_2_c_validateSequences == 1, then if sequence is invalid (see doc/ZSTD_compression_format.md for
 *    specifics regarding offset/matchlength requirements) then the function will bail out and return an error.
 *
 *    In addition to the two adjustable experimental params, there are other important cctx params.
 *    - ZSTD_v1_5_2_c_minMatch MUST be set as less than or equal to the smallest match generated by the match finder. It has a minimum value of ZSTD_v1_5_2_MINMATCH_MIN.
 *    - ZSTD_v1_5_2_c_compressionLevel accordingly adjusts the strength of the entropy coder, as it would in typical compression.
 *    - ZSTD_v1_5_2_c_windowLog affects offset validation: this function will return an error at higher debug levels if a provided offset
 *      is larger than what the spec allows for a given window log and dictionary (if present). See: doc/ZSTD_compression_format.md
 *
 * Note: Repcodes are, as of now, always re-calculated within this function, so ZSTD_v1_5_2_Sequence::rep is unused.
 * Note 2: Once we integrate ability to ingest repcodes, the explicit block delims mode must respect those repcodes exactly,
 *         and cannot emit an RLE block that disagrees with the repcode history
 * @return : final compressed size or a ZSTD error.
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_compressSequences(ZSTD_v1_5_2_CCtx* const cctx, void* dst, size_t dstSize,
                                  const ZSTD_v1_5_2_Sequence* inSeqs, size_t inSeqsSize,
                                  const void* src, size_t srcSize);


/*! ZSTD_v1_5_2_writeSkippableFrame() :
 * Generates a zstd skippable frame containing data given by src, and writes it to dst buffer.
 *
 * Skippable frames begin with a a 4-byte magic number. There are 16 possible choices of magic number,
 * ranging from ZSTD_v1_5_2_MAGIC_SKIPPABLE_START to ZSTD_v1_5_2_MAGIC_SKIPPABLE_START+15.
 * As such, the parameter magicVariant controls the exact skippable frame magic number variant used, so
 * the magic number used will be ZSTD_v1_5_2_MAGIC_SKIPPABLE_START + magicVariant.
 *
 * Returns an error if destination buffer is not large enough, if the source size is not representable
 * with a 4-byte unsigned int, or if the parameter magicVariant is greater than 15 (and therefore invalid).
 *
 * @return : number of bytes written or a ZSTD error.
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_writeSkippableFrame(void* dst, size_t dstCapacity,
                                            const void* src, size_t srcSize, unsigned magicVariant);

/*! ZSTD_v1_5_2_readSkippableFrame() :
 * Retrieves a zstd skippable frame containing data given by src, and writes it to dst buffer.
 *
 * The parameter magicVariant will receive the magicVariant that was supplied when the frame was written,
 * i.e. magicNumber - ZSTD_v1_5_2_MAGIC_SKIPPABLE_START.  This can be NULL if the caller is not interested
 * in the magicVariant.
 *
 * Returns an error if destination buffer is not large enough, or if the frame is not skippable.
 *
 * @return : number of bytes written or a ZSTD error.
 */
ZSTDLIB_API size_t ZSTD_v1_5_2_readSkippableFrame(void* dst, size_t dstCapacity, unsigned* magicVariant,
                                            const void* src, size_t srcSize);

/*! ZSTD_v1_5_2_isSkippableFrame() :
 *  Tells if the content of `buffer` starts with a valid Frame Identifier for a skippable frame.
 */
ZSTDLIB_API unsigned ZSTD_v1_5_2_isSkippableFrame(const void* buffer, size_t size);



/***************************************
*  Memory management
***************************************/

/*! ZSTD_v1_5_2_estimate*() :
 *  These functions make it possible to estimate memory usage
 *  of a future {D,C}Ctx, before its creation.
 *
 *  ZSTD_v1_5_2_estimateCCtxSize() will provide a memory budget large enough
 *  for any compression level up to selected one.
 *  Note : Unlike ZSTD_v1_5_2_estimateCStreamSize*(), this estimate
 *         does not include space for a window buffer.
 *         Therefore, the estimation is only guaranteed for single-shot compressions, not streaming.
 *  The estimate will assume the input may be arbitrarily large,
 *  which is the worst case.
 *
 *  When srcSize can be bound by a known and rather "small" value,
 *  this fact can be used to provide a tighter estimation
 *  because the CCtx compression context will need less memory.
 *  This tighter estimation can be provided by more advanced functions
 *  ZSTD_v1_5_2_estimateCCtxSize_usingCParams(), which can be used in tandem with ZSTD_v1_5_2_getCParams(),
 *  and ZSTD_v1_5_2_estimateCCtxSize_usingCCtxParams(), which can be used in tandem with ZSTD_v1_5_2_CCtxParams_setParameter().
 *  Both can be used to estimate memory using custom compression parameters and arbitrary srcSize limits.
 *
 *  Note 2 : only single-threaded compression is supported.
 *  ZSTD_v1_5_2_estimateCCtxSize_usingCCtxParams() will return an error code if ZSTD_v1_5_2_c_nbWorkers is >= 1.
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_estimateCCtxSize(int compressionLevel);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_estimateCCtxSize_usingCParams(ZSTD_v1_5_2_compressionParameters cParams);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_estimateCCtxSize_usingCCtxParams(const ZSTD_v1_5_2_CCtx_params* params);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_estimateDCtxSize(void);

/*! ZSTD_v1_5_2_estimateCStreamSize() :
 *  ZSTD_v1_5_2_estimateCStreamSize() will provide a budget large enough for any compression level up to selected one.
 *  It will also consider src size to be arbitrarily "large", which is worst case.
 *  If srcSize is known to always be small, ZSTD_v1_5_2_estimateCStreamSize_usingCParams() can provide a tighter estimation.
 *  ZSTD_v1_5_2_estimateCStreamSize_usingCParams() can be used in tandem with ZSTD_v1_5_2_getCParams() to create cParams from compressionLevel.
 *  ZSTD_v1_5_2_estimateCStreamSize_usingCCtxParams() can be used in tandem with ZSTD_v1_5_2_CCtxParams_setParameter(). Only single-threaded compression is supported. This function will return an error code if ZSTD_v1_5_2_c_nbWorkers is >= 1.
 *  Note : CStream size estimation is only correct for single-threaded compression.
 *  ZSTD_v1_5_2_DStream memory budget depends on window Size.
 *  This information can be passed manually, using ZSTD_v1_5_2_estimateDStreamSize,
 *  or deducted from a valid frame Header, using ZSTD_v1_5_2_estimateDStreamSize_fromFrame();
 *  Note : if streaming is init with function ZSTD_v1_5_2_init?Stream_usingDict(),
 *         an internal ?Dict will be created, which additional size is not estimated here.
 *         In this case, get total size by adding ZSTD_v1_5_2_estimate?DictSize */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_estimateCStreamSize(int compressionLevel);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_estimateCStreamSize_usingCParams(ZSTD_v1_5_2_compressionParameters cParams);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_estimateCStreamSize_usingCCtxParams(const ZSTD_v1_5_2_CCtx_params* params);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_estimateDStreamSize(size_t windowSize);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_estimateDStreamSize_fromFrame(const void* src, size_t srcSize);

/*! ZSTD_v1_5_2_estimate?DictSize() :
 *  ZSTD_v1_5_2_estimateCDictSize() will bet that src size is relatively "small", and content is copied, like ZSTD_v1_5_2_createCDict().
 *  ZSTD_v1_5_2_estimateCDictSize_advanced() makes it possible to control compression parameters precisely, like ZSTD_v1_5_2_createCDict_advanced().
 *  Note : dictionaries created by reference (`ZSTD_v1_5_2_dlm_byRef`) are logically smaller.
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_estimateCDictSize(size_t dictSize, int compressionLevel);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_estimateCDictSize_advanced(size_t dictSize, ZSTD_v1_5_2_compressionParameters cParams, ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_estimateDDictSize(size_t dictSize, ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod);

/*! ZSTD_v1_5_2_initStatic*() :
 *  Initialize an object using a pre-allocated fixed-size buffer.
 *  workspace: The memory area to emplace the object into.
 *             Provided pointer *must be 8-bytes aligned*.
 *             Buffer must outlive object.
 *  workspaceSize: Use ZSTD_v1_5_2_estimate*Size() to determine
 *                 how large workspace must be to support target scenario.
 * @return : pointer to object (same address as workspace, just different type),
 *           or NULL if error (size too small, incorrect alignment, etc.)
 *  Note : zstd will never resize nor malloc() when using a static buffer.
 *         If the object requires more memory than available,
 *         zstd will just error out (typically ZSTD_v1_5_2_error_memory_allocation).
 *  Note 2 : there is no corresponding "free" function.
 *           Since workspace is allocated externally, it must be freed externally too.
 *  Note 3 : cParams : use ZSTD_v1_5_2_getCParams() to convert a compression level
 *           into its associated cParams.
 *  Limitation 1 : currently not compatible with internal dictionary creation, triggered by
 *                 ZSTD_v1_5_2_CCtx_loadDictionary(), ZSTD_v1_5_2_initCStream_usingDict() or ZSTD_v1_5_2_initDStream_usingDict().
 *  Limitation 2 : static cctx currently not compatible with multi-threading.
 *  Limitation 3 : static dctx is incompatible with legacy support.
 */
ZSTDLIB_STATIC_API ZSTD_v1_5_2_CCtx*    ZSTD_v1_5_2_initStaticCCtx(void* workspace, size_t workspaceSize);
ZSTDLIB_STATIC_API ZSTD_v1_5_2_CStream* ZSTD_v1_5_2_initStaticCStream(void* workspace, size_t workspaceSize);    /**< same as ZSTD_v1_5_2_initStaticCCtx() */

ZSTDLIB_STATIC_API ZSTD_v1_5_2_DCtx*    ZSTD_v1_5_2_initStaticDCtx(void* workspace, size_t workspaceSize);
ZSTDLIB_STATIC_API ZSTD_v1_5_2_DStream* ZSTD_v1_5_2_initStaticDStream(void* workspace, size_t workspaceSize);    /**< same as ZSTD_v1_5_2_initStaticDCtx() */

ZSTDLIB_STATIC_API const ZSTD_v1_5_2_CDict* ZSTD_v1_5_2_initStaticCDict(
                                        void* workspace, size_t workspaceSize,
                                        const void* dict, size_t dictSize,
                                        ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
                                        ZSTD_v1_5_2_dictContentType_e dictContentType,
                                        ZSTD_v1_5_2_compressionParameters cParams);

ZSTDLIB_STATIC_API const ZSTD_v1_5_2_DDict* ZSTD_v1_5_2_initStaticDDict(
                                        void* workspace, size_t workspaceSize,
                                        const void* dict, size_t dictSize,
                                        ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
                                        ZSTD_v1_5_2_dictContentType_e dictContentType);


/*! Custom memory allocation :
 *  These prototypes make it possible to pass your own allocation/free functions.
 *  ZSTD_v1_5_2_customMem is provided at creation time, using ZSTD_v1_5_2_create*_advanced() variants listed below.
 *  All allocation/free operations will be completed using these custom variants instead of regular <stdlib.h> ones.
 */
typedef void* (*ZSTD_v1_5_2_allocFunction) (void* opaque, size_t size);
typedef void  (*ZSTD_v1_5_2_freeFunction) (void* opaque, void* address);
typedef struct { ZSTD_v1_5_2_allocFunction customAlloc; ZSTD_v1_5_2_freeFunction customFree; void* opaque; } ZSTD_v1_5_2_customMem;
static
#ifdef __GNUC__
__attribute__((__unused__))
#endif
ZSTD_v1_5_2_customMem const ZSTD_v1_5_2_defaultCMem = { NULL, NULL, NULL };  /**< this constant defers to stdlib's functions */

ZSTDLIB_STATIC_API ZSTD_v1_5_2_CCtx*    ZSTD_v1_5_2_createCCtx_advanced(ZSTD_v1_5_2_customMem customMem);
ZSTDLIB_STATIC_API ZSTD_v1_5_2_CStream* ZSTD_v1_5_2_createCStream_advanced(ZSTD_v1_5_2_customMem customMem);
ZSTDLIB_STATIC_API ZSTD_v1_5_2_DCtx*    ZSTD_v1_5_2_createDCtx_advanced(ZSTD_v1_5_2_customMem customMem);
ZSTDLIB_STATIC_API ZSTD_v1_5_2_DStream* ZSTD_v1_5_2_createDStream_advanced(ZSTD_v1_5_2_customMem customMem);

ZSTDLIB_STATIC_API ZSTD_v1_5_2_CDict* ZSTD_v1_5_2_createCDict_advanced(const void* dict, size_t dictSize,
                                                  ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
                                                  ZSTD_v1_5_2_dictContentType_e dictContentType,
                                                  ZSTD_v1_5_2_compressionParameters cParams,
                                                  ZSTD_v1_5_2_customMem customMem);

/*! Thread pool :
 *  These prototypes make it possible to share a thread pool among multiple compression contexts.
 *  This can limit resources for applications with multiple threads where each one uses
 *  a threaded compression mode (via ZSTD_v1_5_2_c_nbWorkers parameter).
 *  ZSTD_v1_5_2_createThreadPool creates a new thread pool with a given number of threads.
 *  Note that the lifetime of such pool must exist while being used.
 *  ZSTD_v1_5_2_CCtx_refThreadPool assigns a thread pool to a context (use NULL argument value
 *  to use an internal thread pool).
 *  ZSTD_v1_5_2_freeThreadPool frees a thread pool, accepts NULL pointer.
 */
typedef struct POOL_ctx_s ZSTD_v1_5_2_threadPool;
ZSTDLIB_STATIC_API ZSTD_v1_5_2_threadPool* ZSTD_v1_5_2_createThreadPool(size_t numThreads);
ZSTDLIB_STATIC_API void ZSTD_v1_5_2_freeThreadPool (ZSTD_v1_5_2_threadPool* pool);  /* accept NULL pointer */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_CCtx_refThreadPool(ZSTD_v1_5_2_CCtx* cctx, ZSTD_v1_5_2_threadPool* pool);


/*
 * This API is temporary and is expected to change or disappear in the future!
 */
ZSTDLIB_STATIC_API ZSTD_v1_5_2_CDict* ZSTD_v1_5_2_createCDict_advanced2(
    const void* dict, size_t dictSize,
    ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
    ZSTD_v1_5_2_dictContentType_e dictContentType,
    const ZSTD_v1_5_2_CCtx_params* cctxParams,
    ZSTD_v1_5_2_customMem customMem);

ZSTDLIB_STATIC_API ZSTD_v1_5_2_DDict* ZSTD_v1_5_2_createDDict_advanced(
    const void* dict, size_t dictSize,
    ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod,
    ZSTD_v1_5_2_dictContentType_e dictContentType,
    ZSTD_v1_5_2_customMem customMem);


/***************************************
*  Advanced compression functions
***************************************/

/*! ZSTD_v1_5_2_createCDict_byReference() :
 *  Create a digested dictionary for compression
 *  Dictionary content is just referenced, not duplicated.
 *  As a consequence, `dictBuffer` **must** outlive CDict,
 *  and its content must remain unmodified throughout the lifetime of CDict.
 *  note: equivalent to ZSTD_v1_5_2_createCDict_advanced(), with dictLoadMethod==ZSTD_v1_5_2_dlm_byRef */
ZSTDLIB_STATIC_API ZSTD_v1_5_2_CDict* ZSTD_v1_5_2_createCDict_byReference(const void* dictBuffer, size_t dictSize, int compressionLevel);

/*! ZSTD_v1_5_2_getCParams() :
 * @return ZSTD_v1_5_2_compressionParameters structure for a selected compression level and estimated srcSize.
 * `estimatedSrcSize` value is optional, select 0 if not known */
ZSTDLIB_STATIC_API ZSTD_v1_5_2_compressionParameters ZSTD_v1_5_2_getCParams(int compressionLevel, unsigned long long estimatedSrcSize, size_t dictSize);

/*! ZSTD_v1_5_2_getParams() :
 *  same as ZSTD_v1_5_2_getCParams(), but @return a full `ZSTD_v1_5_2_parameters` object instead of sub-component `ZSTD_v1_5_2_compressionParameters`.
 *  All fields of `ZSTD_v1_5_2_frameParameters` are set to default : contentSize=1, checksum=0, noDictID=0 */
ZSTDLIB_STATIC_API ZSTD_v1_5_2_parameters ZSTD_v1_5_2_getParams(int compressionLevel, unsigned long long estimatedSrcSize, size_t dictSize);

/*! ZSTD_v1_5_2_checkCParams() :
 *  Ensure param values remain within authorized range.
 * @return 0 on success, or an error code (can be checked with ZSTD_v1_5_2_isError()) */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_checkCParams(ZSTD_v1_5_2_compressionParameters params);

/*! ZSTD_v1_5_2_adjustCParams() :
 *  optimize params for a given `srcSize` and `dictSize`.
 * `srcSize` can be unknown, in which case use ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN.
 * `dictSize` must be `0` when there is no dictionary.
 *  cPar can be invalid : all parameters will be clamped within valid range in the @return struct.
 *  This function never fails (wide contract) */
ZSTDLIB_STATIC_API ZSTD_v1_5_2_compressionParameters ZSTD_v1_5_2_adjustCParams(ZSTD_v1_5_2_compressionParameters cPar, unsigned long long srcSize, size_t dictSize);

/*! ZSTD_v1_5_2_compress_advanced() :
 *  Note : this function is now DEPRECATED.
 *         It can be replaced by ZSTD_v1_5_2_compress2(), in combination with ZSTD_v1_5_2_CCtx_setParameter() and other parameter setters.
 *  This prototype will generate compilation warnings. */
ZSTD_v1_5_2_DEPRECATED("use ZSTD_v1_5_2_compress2")
size_t ZSTD_v1_5_2_compress_advanced(ZSTD_v1_5_2_CCtx* cctx,
                                          void* dst, size_t dstCapacity,
                                    const void* src, size_t srcSize,
                                    const void* dict,size_t dictSize,
                                          ZSTD_v1_5_2_parameters params);

/*! ZSTD_v1_5_2_compress_usingCDict_advanced() :
 *  Note : this function is now DEPRECATED.
 *         It can be replaced by ZSTD_v1_5_2_compress2(), in combination with ZSTD_v1_5_2_CCtx_loadDictionary() and other parameter setters.
 *  This prototype will generate compilation warnings. */
ZSTD_v1_5_2_DEPRECATED("use ZSTD_v1_5_2_compress2 with ZSTD_v1_5_2_CCtx_loadDictionary")
size_t ZSTD_v1_5_2_compress_usingCDict_advanced(ZSTD_v1_5_2_CCtx* cctx,
                                              void* dst, size_t dstCapacity,
                                        const void* src, size_t srcSize,
                                        const ZSTD_v1_5_2_CDict* cdict,
                                              ZSTD_v1_5_2_frameParameters fParams);


/*! ZSTD_v1_5_2_CCtx_loadDictionary_byReference() :
 *  Same as ZSTD_v1_5_2_CCtx_loadDictionary(), but dictionary content is referenced, instead of being copied into CCtx.
 *  It saves some memory, but also requires that `dict` outlives its usage within `cctx` */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_CCtx_loadDictionary_byReference(ZSTD_v1_5_2_CCtx* cctx, const void* dict, size_t dictSize);

/*! ZSTD_v1_5_2_CCtx_loadDictionary_advanced() :
 *  Same as ZSTD_v1_5_2_CCtx_loadDictionary(), but gives finer control over
 *  how to load the dictionary (by copy ? by reference ?)
 *  and how to interpret it (automatic ? force raw mode ? full mode only ?) */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_CCtx_loadDictionary_advanced(ZSTD_v1_5_2_CCtx* cctx, const void* dict, size_t dictSize, ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod, ZSTD_v1_5_2_dictContentType_e dictContentType);

/*! ZSTD_v1_5_2_CCtx_refPrefix_advanced() :
 *  Same as ZSTD_v1_5_2_CCtx_refPrefix(), but gives finer control over
 *  how to interpret prefix content (automatic ? force raw mode (default) ? full mode only ?) */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_CCtx_refPrefix_advanced(ZSTD_v1_5_2_CCtx* cctx, const void* prefix, size_t prefixSize, ZSTD_v1_5_2_dictContentType_e dictContentType);

/* ===   experimental parameters   === */
/* these parameters can be used with ZSTD_v1_5_2_setParameter()
 * they are not guaranteed to remain supported in the future */

 /* Enables rsyncable mode,
  * which makes compressed files more rsync friendly
  * by adding periodic synchronization points to the compressed data.
  * The target average block size is ZSTD_v1_5_2_c_jobSize / 2.
  * It's possible to modify the job size to increase or decrease
  * the granularity of the synchronization point.
  * Once the jobSize is smaller than the window size,
  * it will result in compression ratio degradation.
  * NOTE 1: rsyncable mode only works when multithreading is enabled.
  * NOTE 2: rsyncable performs poorly in combination with long range mode,
  * since it will decrease the effectiveness of synchronization points,
  * though mileage may vary.
  * NOTE 3: Rsyncable mode limits maximum compression speed to ~400 MB/s.
  * If the selected compression level is already running significantly slower,
  * the overall speed won't be significantly impacted.
  */
 #define ZSTD_v1_5_2_c_rsyncable ZSTD_v1_5_2_c_experimentalParam1

/* Select a compression format.
 * The value must be of type ZSTD_v1_5_2_format_e.
 * See ZSTD_v1_5_2_format_e enum definition for details */
#define ZSTD_v1_5_2_c_format ZSTD_v1_5_2_c_experimentalParam2

/* Force back-reference distances to remain < windowSize,
 * even when referencing into Dictionary content (default:0) */
#define ZSTD_v1_5_2_c_forceMaxWindow ZSTD_v1_5_2_c_experimentalParam3

/* Controls whether the contents of a CDict
 * are used in place, or copied into the working context.
 * Accepts values from the ZSTD_v1_5_2_dictAttachPref_e enum.
 * See the comments on that enum for an explanation of the feature. */
#define ZSTD_v1_5_2_c_forceAttachDict ZSTD_v1_5_2_c_experimentalParam4

/* Controlled with ZSTD_v1_5_2_paramSwitch_e enum.
 * Default is ZSTD_v1_5_2_ps_auto.
 * Set to ZSTD_v1_5_2_ps_disable to never compress literals.
 * Set to ZSTD_v1_5_2_ps_enable to always compress literals. (Note: uncompressed literals
 * may still be emitted if huffman is not beneficial to use.)
 *
 * By default, in ZSTD_v1_5_2_ps_auto, the library will decide at runtime whether to use
 * literals compression based on the compression parameters - specifically,
 * negative compression levels do not use literal compression.
 */
#define ZSTD_v1_5_2_c_literalCompressionMode ZSTD_v1_5_2_c_experimentalParam5

/* Tries to fit compressed block size to be around targetCBlockSize.
 * No target when targetCBlockSize == 0.
 * There is no guarantee on compressed block size (default:0) */
#define ZSTD_v1_5_2_c_targetCBlockSize ZSTD_v1_5_2_c_experimentalParam6

/* User's best guess of source size.
 * Hint is not valid when srcSizeHint == 0.
 * There is no guarantee that hint is close to actual source size,
 * but compression ratio may regress significantly if guess considerably underestimates */
#define ZSTD_v1_5_2_c_srcSizeHint ZSTD_v1_5_2_c_experimentalParam7

/* Controls whether the new and experimental "dedicated dictionary search
 * structure" can be used. This feature is still rough around the edges, be
 * prepared for surprising behavior!
 *
 * How to use it:
 *
 * When using a CDict, whether to use this feature or not is controlled at
 * CDict creation, and it must be set in a CCtxParams set passed into that
 * construction (via ZSTD_v1_5_2_createCDict_advanced2()). A compression will then
 * use the feature or not based on how the CDict was constructed; the value of
 * this param, set in the CCtx, will have no effect.
 *
 * However, when a dictionary buffer is passed into a CCtx, such as via
 * ZSTD_v1_5_2_CCtx_loadDictionary(), this param can be set on the CCtx to control
 * whether the CDict that is created internally can use the feature or not.
 *
 * What it does:
 *
 * Normally, the internal data structures of the CDict are analogous to what
 * would be stored in a CCtx after compressing the contents of a dictionary.
 * To an approximation, a compression using a dictionary can then use those
 * data structures to simply continue what is effectively a streaming
 * compression where the simulated compression of the dictionary left off.
 * Which is to say, the search structures in the CDict are normally the same
 * format as in the CCtx.
 *
 * It is possible to do better, since the CDict is not like a CCtx: the search
 * structures are written once during CDict creation, and then are only read
 * after that, while the search structures in the CCtx are both read and
 * written as the compression goes along. This means we can choose a search
 * structure for the dictionary that is read-optimized.
 *
 * This feature enables the use of that different structure.
 *
 * Note that some of the members of the ZSTD_v1_5_2_compressionParameters struct have
 * different semantics and constraints in the dedicated search structure. It is
 * highly recommended that you simply set a compression level in the CCtxParams
 * you pass into the CDict creation call, and avoid messing with the cParams
 * directly.
 *
 * Effects:
 *
 * This will only have any effect when the selected ZSTD_v1_5_2_strategy
 * implementation supports this feature. Currently, that's limited to
 * ZSTD_v1_5_2_greedy, ZSTD_v1_5_2_lazy, and ZSTD_v1_5_2_lazy2.
 *
 * Note that this means that the CDict tables can no longer be copied into the
 * CCtx, so the dict attachment mode ZSTD_v1_5_2_dictForceCopy will no longer be
 * usable. The dictionary can only be attached or reloaded.
 *
 * In general, you should expect compression to be faster--sometimes very much
 * so--and CDict creation to be slightly slower. Eventually, we will probably
 * make this mode the default.
 */
#define ZSTD_v1_5_2_c_enableDedicatedDictSearch ZSTD_v1_5_2_c_experimentalParam8

/* ZSTD_v1_5_2_c_stableInBuffer
 * Experimental parameter.
 * Default is 0 == disabled. Set to 1 to enable.
 *
 * Tells the compressor that the ZSTD_v1_5_2_inBuffer will ALWAYS be the same
 * between calls, except for the modifications that zstd makes to pos (the
 * caller must not modify pos). This is checked by the compressor, and
 * compression will fail if it ever changes. This means the only flush
 * mode that makes sense is ZSTD_v1_5_2_e_end, so zstd will error if ZSTD_v1_5_2_e_end
 * is not used. The data in the ZSTD_v1_5_2_inBuffer in the range [src, src + pos)
 * MUST not be modified during compression or you will get data corruption.
 *
 * When this flag is enabled zstd won't allocate an input window buffer,
 * because the user guarantees it can reference the ZSTD_v1_5_2_inBuffer until
 * the frame is complete. But, it will still allocate an output buffer
 * large enough to fit a block (see ZSTD_v1_5_2_c_stableOutBuffer). This will also
 * avoid the memcpy() from the input buffer to the input window buffer.
 *
 * NOTE: ZSTD_v1_5_2_compressStream2() will error if ZSTD_v1_5_2_e_end is not used.
 * That means this flag cannot be used with ZSTD_v1_5_2_compressStream().
 *
 * NOTE: So long as the ZSTD_v1_5_2_inBuffer always points to valid memory, using
 * this flag is ALWAYS memory safe, and will never access out-of-bounds
 * memory. However, compression WILL fail if you violate the preconditions.
 *
 * WARNING: The data in the ZSTD_v1_5_2_inBuffer in the range [dst, dst + pos) MUST
 * not be modified during compression or you will get data corruption. This
 * is because zstd needs to reference data in the ZSTD_v1_5_2_inBuffer to find
 * matches. Normally zstd maintains its own window buffer for this purpose,
 * but passing this flag tells zstd to use the user provided buffer.
 */
#define ZSTD_v1_5_2_c_stableInBuffer ZSTD_v1_5_2_c_experimentalParam9

/* ZSTD_v1_5_2_c_stableOutBuffer
 * Experimental parameter.
 * Default is 0 == disabled. Set to 1 to enable.
 *
 * Tells he compressor that the ZSTD_v1_5_2_outBuffer will not be resized between
 * calls. Specifically: (out.size - out.pos) will never grow. This gives the
 * compressor the freedom to say: If the compressed data doesn't fit in the
 * output buffer then return ZSTD_v1_5_2_error_dstSizeTooSmall. This allows us to
 * always decompress directly into the output buffer, instead of decompressing
 * into an internal buffer and copying to the output buffer.
 *
 * When this flag is enabled zstd won't allocate an output buffer, because
 * it can write directly to the ZSTD_v1_5_2_outBuffer. It will still allocate the
 * input window buffer (see ZSTD_v1_5_2_c_stableInBuffer).
 *
 * Zstd will check that (out.size - out.pos) never grows and return an error
 * if it does. While not strictly necessary, this should prevent surprises.
 */
#define ZSTD_v1_5_2_c_stableOutBuffer ZSTD_v1_5_2_c_experimentalParam10

/* ZSTD_v1_5_2_c_blockDelimiters
 * Default is 0 == ZSTD_v1_5_2_sf_noBlockDelimiters.
 *
 * For use with sequence compression API: ZSTD_v1_5_2_compressSequences().
 *
 * Designates whether or not the given array of ZSTD_v1_5_2_Sequence contains block delimiters
 * and last literals, which are defined as sequences with offset == 0 and matchLength == 0.
 * See the definition of ZSTD_v1_5_2_Sequence for more specifics.
 */
#define ZSTD_v1_5_2_c_blockDelimiters ZSTD_v1_5_2_c_experimentalParam11

/* ZSTD_v1_5_2_c_validateSequences
 * Default is 0 == disabled. Set to 1 to enable sequence validation.
 *
 * For use with sequence compression API: ZSTD_v1_5_2_compressSequences().
 * Designates whether or not we validate sequences provided to ZSTD_v1_5_2_compressSequences()
 * during function execution.
 *
 * Without validation, providing a sequence that does not conform to the zstd spec will cause
 * undefined behavior, and may produce a corrupted block.
 *
 * With validation enabled, a if sequence is invalid (see doc/ZSTD_compression_format.md for
 * specifics regarding offset/matchlength requirements) then the function will bail out and
 * return an error.
 *
 */
#define ZSTD_v1_5_2_c_validateSequences ZSTD_v1_5_2_c_experimentalParam12

/* ZSTD_v1_5_2_c_useBlockSplitter
 * Controlled with ZSTD_v1_5_2_paramSwitch_e enum.
 * Default is ZSTD_v1_5_2_ps_auto.
 * Set to ZSTD_v1_5_2_ps_disable to never use block splitter.
 * Set to ZSTD_v1_5_2_ps_enable to always use block splitter.
 *
 * By default, in ZSTD_v1_5_2_ps_auto, the library will decide at runtime whether to use
 * block splitting based on the compression parameters.
 */
#define ZSTD_v1_5_2_c_useBlockSplitter ZSTD_v1_5_2_c_experimentalParam13

/* ZSTD_v1_5_2_c_useRowMatchFinder
 * Controlled with ZSTD_v1_5_2_paramSwitch_e enum.
 * Default is ZSTD_v1_5_2_ps_auto.
 * Set to ZSTD_v1_5_2_ps_disable to never use row-based matchfinder.
 * Set to ZSTD_v1_5_2_ps_enable to force usage of row-based matchfinder.
 *
 * By default, in ZSTD_v1_5_2_ps_auto, the library will decide at runtime whether to use
 * the row-based matchfinder based on support for SIMD instructions and the window log.
 * Note that this only pertains to compression strategies: greedy, lazy, and lazy2
 */
#define ZSTD_v1_5_2_c_useRowMatchFinder ZSTD_v1_5_2_c_experimentalParam14

/* ZSTD_v1_5_2_c_deterministicRefPrefix
 * Default is 0 == disabled. Set to 1 to enable.
 *
 * Zstd produces different results for prefix compression when the prefix is
 * directly adjacent to the data about to be compressed vs. when it isn't.
 * This is because zstd detects that the two buffers are contiguous and it can
 * use a more efficient match finding algorithm. However, this produces different
 * results than when the two buffers are non-contiguous. This flag forces zstd
 * to always load the prefix in non-contiguous mode, even if it happens to be
 * adjacent to the data, to guarantee determinism.
 *
 * If you really care about determinism when using a dictionary or prefix,
 * like when doing delta compression, you should select this option. It comes
 * at a speed penalty of about ~2.5% if the dictionary and data happened to be
 * contiguous, and is free if they weren't contiguous. We don't expect that
 * intentionally making the dictionary and data contiguous will be worth the
 * cost to memcpy() the data.
 */
#define ZSTD_v1_5_2_c_deterministicRefPrefix ZSTD_v1_5_2_c_experimentalParam15

/*! ZSTD_v1_5_2_CCtx_getParameter() :
 *  Get the requested compression parameter value, selected by enum ZSTD_v1_5_2_cParameter,
 *  and store it into int* value.
 * @return : 0, or an error code (which can be tested with ZSTD_v1_5_2_isError()).
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_CCtx_getParameter(const ZSTD_v1_5_2_CCtx* cctx, ZSTD_v1_5_2_cParameter param, int* value);


/*! ZSTD_v1_5_2_CCtx_params :
 *  Quick howto :
 *  - ZSTD_v1_5_2_createCCtxParams() : Create a ZSTD_v1_5_2_CCtx_params structure
 *  - ZSTD_v1_5_2_CCtxParams_setParameter() : Push parameters one by one into
 *                                     an existing ZSTD_v1_5_2_CCtx_params structure.
 *                                     This is similar to
 *                                     ZSTD_v1_5_2_CCtx_setParameter().
 *  - ZSTD_v1_5_2_CCtx_setParametersUsingCCtxParams() : Apply parameters to
 *                                    an existing CCtx.
 *                                    These parameters will be applied to
 *                                    all subsequent frames.
 *  - ZSTD_v1_5_2_compressStream2() : Do compression using the CCtx.
 *  - ZSTD_v1_5_2_freeCCtxParams() : Free the memory, accept NULL pointer.
 *
 *  This can be used with ZSTD_v1_5_2_estimateCCtxSize_advanced_usingCCtxParams()
 *  for static allocation of CCtx for single-threaded compression.
 */
ZSTDLIB_STATIC_API ZSTD_v1_5_2_CCtx_params* ZSTD_v1_5_2_createCCtxParams(void);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_freeCCtxParams(ZSTD_v1_5_2_CCtx_params* params);  /* accept NULL pointer */

/*! ZSTD_v1_5_2_CCtxParams_reset() :
 *  Reset params to default values.
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_CCtxParams_reset(ZSTD_v1_5_2_CCtx_params* params);

/*! ZSTD_v1_5_2_CCtxParams_init() :
 *  Initializes the compression parameters of cctxParams according to
 *  compression level. All other parameters are reset to their default values.
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_CCtxParams_init(ZSTD_v1_5_2_CCtx_params* cctxParams, int compressionLevel);

/*! ZSTD_v1_5_2_CCtxParams_init_advanced() :
 *  Initializes the compression and frame parameters of cctxParams according to
 *  params. All other parameters are reset to their default values.
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_CCtxParams_init_advanced(ZSTD_v1_5_2_CCtx_params* cctxParams, ZSTD_v1_5_2_parameters params);

/*! ZSTD_v1_5_2_CCtxParams_setParameter() : Requires v1.4.0+
 *  Similar to ZSTD_v1_5_2_CCtx_setParameter.
 *  Set one compression parameter, selected by enum ZSTD_v1_5_2_cParameter.
 *  Parameters must be applied to a ZSTD_v1_5_2_CCtx using
 *  ZSTD_v1_5_2_CCtx_setParametersUsingCCtxParams().
 * @result : a code representing success or failure (which can be tested with
 *           ZSTD_v1_5_2_isError()).
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_CCtxParams_setParameter(ZSTD_v1_5_2_CCtx_params* params, ZSTD_v1_5_2_cParameter param, int value);

/*! ZSTD_v1_5_2_CCtxParams_getParameter() :
 * Similar to ZSTD_v1_5_2_CCtx_getParameter.
 * Get the requested value of one compression parameter, selected by enum ZSTD_v1_5_2_cParameter.
 * @result : 0, or an error code (which can be tested with ZSTD_v1_5_2_isError()).
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_CCtxParams_getParameter(const ZSTD_v1_5_2_CCtx_params* params, ZSTD_v1_5_2_cParameter param, int* value);

/*! ZSTD_v1_5_2_CCtx_setParametersUsingCCtxParams() :
 *  Apply a set of ZSTD_v1_5_2_CCtx_params to the compression context.
 *  This can be done even after compression is started,
 *    if nbWorkers==0, this will have no impact until a new compression is started.
 *    if nbWorkers>=1, new parameters will be picked up at next job,
 *       with a few restrictions (windowLog, pledgedSrcSize, nbWorkers, jobSize, and overlapLog are not updated).
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_CCtx_setParametersUsingCCtxParams(
        ZSTD_v1_5_2_CCtx* cctx, const ZSTD_v1_5_2_CCtx_params* params);

/*! ZSTD_v1_5_2_compressStream2_simpleArgs() :
 *  Same as ZSTD_v1_5_2_compressStream2(),
 *  but using only integral types as arguments.
 *  This variant might be helpful for binders from dynamic languages
 *  which have troubles handling structures containing memory pointers.
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_compressStream2_simpleArgs (
                            ZSTD_v1_5_2_CCtx* cctx,
                            void* dst, size_t dstCapacity, size_t* dstPos,
                      const void* src, size_t srcSize, size_t* srcPos,
                            ZSTD_v1_5_2_EndDirective endOp);


/***************************************
*  Advanced decompression functions
***************************************/

/*! ZSTD_v1_5_2_isFrame() :
 *  Tells if the content of `buffer` starts with a valid Frame Identifier.
 *  Note : Frame Identifier is 4 bytes. If `size < 4`, @return will always be 0.
 *  Note 2 : Legacy Frame Identifiers are considered valid only if Legacy Support is enabled.
 *  Note 3 : Skippable Frame Identifiers are considered valid. */
ZSTDLIB_STATIC_API unsigned ZSTD_v1_5_2_isFrame(const void* buffer, size_t size);

/*! ZSTD_v1_5_2_createDDict_byReference() :
 *  Create a digested dictionary, ready to start decompression operation without startup delay.
 *  Dictionary content is referenced, and therefore stays in dictBuffer.
 *  It is important that dictBuffer outlives DDict,
 *  it must remain read accessible throughout the lifetime of DDict */
ZSTDLIB_STATIC_API ZSTD_v1_5_2_DDict* ZSTD_v1_5_2_createDDict_byReference(const void* dictBuffer, size_t dictSize);

/*! ZSTD_v1_5_2_DCtx_loadDictionary_byReference() :
 *  Same as ZSTD_v1_5_2_DCtx_loadDictionary(),
 *  but references `dict` content instead of copying it into `dctx`.
 *  This saves memory if `dict` remains around.,
 *  However, it's imperative that `dict` remains accessible (and unmodified) while being used, so it must outlive decompression. */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_DCtx_loadDictionary_byReference(ZSTD_v1_5_2_DCtx* dctx, const void* dict, size_t dictSize);

/*! ZSTD_v1_5_2_DCtx_loadDictionary_advanced() :
 *  Same as ZSTD_v1_5_2_DCtx_loadDictionary(),
 *  but gives direct control over
 *  how to load the dictionary (by copy ? by reference ?)
 *  and how to interpret it (automatic ? force raw mode ? full mode only ?). */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_DCtx_loadDictionary_advanced(ZSTD_v1_5_2_DCtx* dctx, const void* dict, size_t dictSize, ZSTD_v1_5_2_dictLoadMethod_e dictLoadMethod, ZSTD_v1_5_2_dictContentType_e dictContentType);

/*! ZSTD_v1_5_2_DCtx_refPrefix_advanced() :
 *  Same as ZSTD_v1_5_2_DCtx_refPrefix(), but gives finer control over
 *  how to interpret prefix content (automatic ? force raw mode (default) ? full mode only ?) */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_DCtx_refPrefix_advanced(ZSTD_v1_5_2_DCtx* dctx, const void* prefix, size_t prefixSize, ZSTD_v1_5_2_dictContentType_e dictContentType);

/*! ZSTD_v1_5_2_DCtx_setMaxWindowSize() :
 *  Refuses allocating internal buffers for frames requiring a window size larger than provided limit.
 *  This protects a decoder context from reserving too much memory for itself (potential attack scenario).
 *  This parameter is only useful in streaming mode, since no internal buffer is allocated in single-pass mode.
 *  By default, a decompression context accepts all window sizes <= (1 << ZSTD_v1_5_2_WINDOWLOG_LIMIT_DEFAULT)
 * @return : 0, or an error code (which can be tested using ZSTD_v1_5_2_isError()).
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_DCtx_setMaxWindowSize(ZSTD_v1_5_2_DCtx* dctx, size_t maxWindowSize);

/*! ZSTD_v1_5_2_DCtx_getParameter() :
 *  Get the requested decompression parameter value, selected by enum ZSTD_v1_5_2_dParameter,
 *  and store it into int* value.
 * @return : 0, or an error code (which can be tested with ZSTD_v1_5_2_isError()).
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_DCtx_getParameter(ZSTD_v1_5_2_DCtx* dctx, ZSTD_v1_5_2_dParameter param, int* value);

/* ZSTD_v1_5_2_d_format
 * experimental parameter,
 * allowing selection between ZSTD_v1_5_2_format_e input compression formats
 */
#define ZSTD_v1_5_2_d_format ZSTD_v1_5_2_d_experimentalParam1
/* ZSTD_v1_5_2_d_stableOutBuffer
 * Experimental parameter.
 * Default is 0 == disabled. Set to 1 to enable.
 *
 * Tells the decompressor that the ZSTD_v1_5_2_outBuffer will ALWAYS be the same
 * between calls, except for the modifications that zstd makes to pos (the
 * caller must not modify pos). This is checked by the decompressor, and
 * decompression will fail if it ever changes. Therefore the ZSTD_v1_5_2_outBuffer
 * MUST be large enough to fit the entire decompressed frame. This will be
 * checked when the frame content size is known. The data in the ZSTD_v1_5_2_outBuffer
 * in the range [dst, dst + pos) MUST not be modified during decompression
 * or you will get data corruption.
 *
 * When this flags is enabled zstd won't allocate an output buffer, because
 * it can write directly to the ZSTD_v1_5_2_outBuffer, but it will still allocate
 * an input buffer large enough to fit any compressed block. This will also
 * avoid the memcpy() from the internal output buffer to the ZSTD_v1_5_2_outBuffer.
 * If you need to avoid the input buffer allocation use the buffer-less
 * streaming API.
 *
 * NOTE: So long as the ZSTD_v1_5_2_outBuffer always points to valid memory, using
 * this flag is ALWAYS memory safe, and will never access out-of-bounds
 * memory. However, decompression WILL fail if you violate the preconditions.
 *
 * WARNING: The data in the ZSTD_v1_5_2_outBuffer in the range [dst, dst + pos) MUST
 * not be modified during decompression or you will get data corruption. This
 * is because zstd needs to reference data in the ZSTD_v1_5_2_outBuffer to regenerate
 * matches. Normally zstd maintains its own buffer for this purpose, but passing
 * this flag tells zstd to use the user provided buffer.
 */
#define ZSTD_v1_5_2_d_stableOutBuffer ZSTD_v1_5_2_d_experimentalParam2

/* ZSTD_v1_5_2_d_forceIgnoreChecksum
 * Experimental parameter.
 * Default is 0 == disabled. Set to 1 to enable
 *
 * Tells the decompressor to skip checksum validation during decompression, regardless
 * of whether checksumming was specified during compression. This offers some
 * slight performance benefits, and may be useful for debugging.
 * Param has values of type ZSTD_v1_5_2_forceIgnoreChecksum_e
 */
#define ZSTD_v1_5_2_d_forceIgnoreChecksum ZSTD_v1_5_2_d_experimentalParam3

/* ZSTD_v1_5_2_d_refMultipleDDicts
 * Experimental parameter.
 * Default is 0 == disabled. Set to 1 to enable
 *
 * If enabled and dctx is allocated on the heap, then additional memory will be allocated
 * to store references to multiple ZSTD_v1_5_2_DDict. That is, multiple calls of ZSTD_v1_5_2_refDDict()
 * using a given ZSTD_v1_5_2_DCtx, rather than overwriting the previous DDict reference, will instead
 * store all references. At decompression time, the appropriate dictID is selected
 * from the set of DDicts based on the dictID in the frame.
 *
 * Usage is simply calling ZSTD_v1_5_2_refDDict() on multiple dict buffers.
 *
 * Param has values of byte ZSTD_v1_5_2_refMultipleDDicts_e
 *
 * WARNING: Enabling this parameter and calling ZSTD_v1_5_2_DCtx_refDDict(), will trigger memory
 * allocation for the hash table. ZSTD_v1_5_2_freeDCtx() also frees this memory.
 * Memory is allocated as per ZSTD_v1_5_2_DCtx::customMem.
 *
 * Although this function allocates memory for the table, the user is still responsible for
 * memory management of the underlying ZSTD_v1_5_2_DDict* themselves.
 */
#define ZSTD_v1_5_2_d_refMultipleDDicts ZSTD_v1_5_2_d_experimentalParam4


/*! ZSTD_v1_5_2_DCtx_setFormat() :
 *  This function is REDUNDANT. Prefer ZSTD_v1_5_2_DCtx_setParameter().
 *  Instruct the decoder context about what kind of data to decode next.
 *  This instruction is mandatory to decode data without a fully-formed header,
 *  such ZSTD_v1_5_2_f_zstd1_magicless for example.
 * @return : 0, or an error code (which can be tested using ZSTD_v1_5_2_isError()). */
ZSTD_v1_5_2_DEPRECATED("use ZSTD_v1_5_2_DCtx_setParameter() instead")
size_t ZSTD_v1_5_2_DCtx_setFormat(ZSTD_v1_5_2_DCtx* dctx, ZSTD_v1_5_2_format_e format);

/*! ZSTD_v1_5_2_decompressStream_simpleArgs() :
 *  Same as ZSTD_v1_5_2_decompressStream(),
 *  but using only integral types as arguments.
 *  This can be helpful for binders from dynamic languages
 *  which have troubles handling structures containing memory pointers.
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_decompressStream_simpleArgs (
                            ZSTD_v1_5_2_DCtx* dctx,
                            void* dst, size_t dstCapacity, size_t* dstPos,
                      const void* src, size_t srcSize, size_t* srcPos);


/********************************************************************
*  Advanced streaming functions
*  Warning : most of these functions are now redundant with the Advanced API.
*  Once Advanced API reaches "stable" status,
*  redundant functions will be deprecated, and then at some point removed.
********************************************************************/

/*=====   Advanced Streaming compression functions  =====*/

/*! ZSTD_v1_5_2_initCStream_srcSize() :
 * This function is DEPRECATED, and equivalent to:
 *     ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only);
 *     ZSTD_v1_5_2_CCtx_refCDict(zcs, NULL); // clear the dictionary (if any)
 *     ZSTD_v1_5_2_CCtx_setParameter(zcs, ZSTD_v1_5_2_c_compressionLevel, compressionLevel);
 *     ZSTD_v1_5_2_CCtx_setPledgedSrcSize(zcs, pledgedSrcSize);
 *
 * pledgedSrcSize must be correct. If it is not known at init time, use
 * ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN. Note that, for compatibility with older programs,
 * "0" also disables frame content size field. It may be enabled in the future.
 * This prototype will generate compilation warnings.
 */
ZSTD_v1_5_2_DEPRECATED("use ZSTD_v1_5_2_CCtx_reset, see zstd.h for detailed instructions")
size_t ZSTD_v1_5_2_initCStream_srcSize(ZSTD_v1_5_2_CStream* zcs,
                         int compressionLevel,
                         unsigned long long pledgedSrcSize);

/*! ZSTD_v1_5_2_initCStream_usingDict() :
 * This function is DEPRECATED, and is equivalent to:
 *     ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only);
 *     ZSTD_v1_5_2_CCtx_setParameter(zcs, ZSTD_v1_5_2_c_compressionLevel, compressionLevel);
 *     ZSTD_v1_5_2_CCtx_loadDictionary(zcs, dict, dictSize);
 *
 * Creates of an internal CDict (incompatible with static CCtx), except if
 * dict == NULL or dictSize < 8, in which case no dict is used.
 * Note: dict is loaded with ZSTD_v1_5_2_dct_auto (treated as a full zstd dictionary if
 * it begins with ZSTD_v1_5_2_MAGIC_DICTIONARY, else as raw content) and ZSTD_v1_5_2_dlm_byCopy.
 * This prototype will generate compilation warnings.
 */
ZSTD_v1_5_2_DEPRECATED("use ZSTD_v1_5_2_CCtx_reset, see zstd.h for detailed instructions")
size_t ZSTD_v1_5_2_initCStream_usingDict(ZSTD_v1_5_2_CStream* zcs,
                     const void* dict, size_t dictSize,
                           int compressionLevel);

/*! ZSTD_v1_5_2_initCStream_advanced() :
 * This function is DEPRECATED, and is approximately equivalent to:
 *     ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only);
 *     // Pseudocode: Set each zstd parameter and leave the rest as-is.
 *     for ((param, value) : params) {
 *         ZSTD_v1_5_2_CCtx_setParameter(zcs, param, value);
 *     }
 *     ZSTD_v1_5_2_CCtx_setPledgedSrcSize(zcs, pledgedSrcSize);
 *     ZSTD_v1_5_2_CCtx_loadDictionary(zcs, dict, dictSize);
 *
 * dict is loaded with ZSTD_v1_5_2_dct_auto and ZSTD_v1_5_2_dlm_byCopy.
 * pledgedSrcSize must be correct.
 * If srcSize is not known at init time, use value ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN.
 * This prototype will generate compilation warnings.
 */
ZSTD_v1_5_2_DEPRECATED("use ZSTD_v1_5_2_CCtx_reset, see zstd.h for detailed instructions")
size_t ZSTD_v1_5_2_initCStream_advanced(ZSTD_v1_5_2_CStream* zcs,
                    const void* dict, size_t dictSize,
                          ZSTD_v1_5_2_parameters params,
                          unsigned long long pledgedSrcSize);

/*! ZSTD_v1_5_2_initCStream_usingCDict() :
 * This function is DEPRECATED, and equivalent to:
 *     ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only);
 *     ZSTD_v1_5_2_CCtx_refCDict(zcs, cdict);
 *
 * note : cdict will just be referenced, and must outlive compression session
 * This prototype will generate compilation warnings.
 */
ZSTD_v1_5_2_DEPRECATED("use ZSTD_v1_5_2_CCtx_reset and ZSTD_v1_5_2_CCtx_refCDict, see zstd.h for detailed instructions")
size_t ZSTD_v1_5_2_initCStream_usingCDict(ZSTD_v1_5_2_CStream* zcs, const ZSTD_v1_5_2_CDict* cdict);

/*! ZSTD_v1_5_2_initCStream_usingCDict_advanced() :
 *   This function is DEPRECATED, and is approximately equivalent to:
 *     ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only);
 *     // Pseudocode: Set each zstd frame parameter and leave the rest as-is.
 *     for ((fParam, value) : fParams) {
 *         ZSTD_v1_5_2_CCtx_setParameter(zcs, fParam, value);
 *     }
 *     ZSTD_v1_5_2_CCtx_setPledgedSrcSize(zcs, pledgedSrcSize);
 *     ZSTD_v1_5_2_CCtx_refCDict(zcs, cdict);
 *
 * same as ZSTD_v1_5_2_initCStream_usingCDict(), with control over frame parameters.
 * pledgedSrcSize must be correct. If srcSize is not known at init time, use
 * value ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN.
 * This prototype will generate compilation warnings.
 */
ZSTD_v1_5_2_DEPRECATED("use ZSTD_v1_5_2_CCtx_reset and ZSTD_v1_5_2_CCtx_refCDict, see zstd.h for detailed instructions")
size_t ZSTD_v1_5_2_initCStream_usingCDict_advanced(ZSTD_v1_5_2_CStream* zcs,
                               const ZSTD_v1_5_2_CDict* cdict,
                                     ZSTD_v1_5_2_frameParameters fParams,
                                     unsigned long long pledgedSrcSize);

/*! ZSTD_v1_5_2_resetCStream() :
 * This function is DEPRECATED, and is equivalent to:
 *     ZSTD_v1_5_2_CCtx_reset(zcs, ZSTD_v1_5_2_reset_session_only);
 *     ZSTD_v1_5_2_CCtx_setPledgedSrcSize(zcs, pledgedSrcSize);
 * Note: ZSTD_v1_5_2_resetCStream() interprets pledgedSrcSize == 0 as ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN, but
 *       ZSTD_v1_5_2_CCtx_setPledgedSrcSize() does not do the same, so ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN must be
 *       explicitly specified.
 *
 *  start a new frame, using same parameters from previous frame.
 *  This is typically useful to skip dictionary loading stage, since it will re-use it in-place.
 *  Note that zcs must be init at least once before using ZSTD_v1_5_2_resetCStream().
 *  If pledgedSrcSize is not known at reset time, use macro ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN.
 *  If pledgedSrcSize > 0, its value must be correct, as it will be written in header, and controlled at the end.
 *  For the time being, pledgedSrcSize==0 is interpreted as "srcSize unknown" for compatibility with older programs,
 *  but it will change to mean "empty" in future version, so use macro ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN instead.
 * @return : 0, or an error code (which can be tested using ZSTD_v1_5_2_isError())
 *  This prototype will generate compilation warnings.
 */
ZSTD_v1_5_2_DEPRECATED("use ZSTD_v1_5_2_CCtx_reset, see zstd.h for detailed instructions")
size_t ZSTD_v1_5_2_resetCStream(ZSTD_v1_5_2_CStream* zcs, unsigned long long pledgedSrcSize);


typedef struct {
    unsigned long long ingested;   /* nb input bytes read and buffered */
    unsigned long long consumed;   /* nb input bytes actually compressed */
    unsigned long long produced;   /* nb of compressed bytes generated and buffered */
    unsigned long long flushed;    /* nb of compressed bytes flushed : not provided; can be tracked from caller side */
    unsigned currentJobID;         /* MT only : latest started job nb */
    unsigned nbActiveWorkers;      /* MT only : nb of workers actively compressing at probe time */
} ZSTD_v1_5_2_frameProgression;

/* ZSTD_v1_5_2_getFrameProgression() :
 * tells how much data has been ingested (read from input)
 * consumed (input actually compressed) and produced (output) for current frame.
 * Note : (ingested - consumed) is amount of input data buffered internally, not yet compressed.
 * Aggregates progression inside active worker threads.
 */
ZSTDLIB_STATIC_API ZSTD_v1_5_2_frameProgression ZSTD_v1_5_2_getFrameProgression(const ZSTD_v1_5_2_CCtx* cctx);

/*! ZSTD_v1_5_2_toFlushNow() :
 *  Tell how many bytes are ready to be flushed immediately.
 *  Useful for multithreading scenarios (nbWorkers >= 1).
 *  Probe the oldest active job, defined as oldest job not yet entirely flushed,
 *  and check its output buffer.
 * @return : amount of data stored in oldest job and ready to be flushed immediately.
 *  if @return == 0, it means either :
 *  + there is no active job (could be checked with ZSTD_v1_5_2_frameProgression()), or
 *  + oldest job is still actively compressing data,
 *    but everything it has produced has also been flushed so far,
 *    therefore flush speed is limited by production speed of oldest job
 *    irrespective of the speed of concurrent (and newer) jobs.
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_toFlushNow(ZSTD_v1_5_2_CCtx* cctx);


/*=====   Advanced Streaming decompression functions  =====*/

/*!
 * This function is deprecated, and is equivalent to:
 *
 *     ZSTD_v1_5_2_DCtx_reset(zds, ZSTD_v1_5_2_reset_session_only);
 *     ZSTD_v1_5_2_DCtx_loadDictionary(zds, dict, dictSize);
 *
 * note: no dictionary will be used if dict == NULL or dictSize < 8
 * Note : this prototype will be marked as deprecated and generate compilation warnings on reaching v1.5.x
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_initDStream_usingDict(ZSTD_v1_5_2_DStream* zds, const void* dict, size_t dictSize);

/*!
 * This function is deprecated, and is equivalent to:
 *
 *     ZSTD_v1_5_2_DCtx_reset(zds, ZSTD_v1_5_2_reset_session_only);
 *     ZSTD_v1_5_2_DCtx_refDDict(zds, ddict);
 *
 * note : ddict is referenced, it must outlive decompression session
 * Note : this prototype will be marked as deprecated and generate compilation warnings on reaching v1.5.x
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_initDStream_usingDDict(ZSTD_v1_5_2_DStream* zds, const ZSTD_v1_5_2_DDict* ddict);

/*!
 * This function is deprecated, and is equivalent to:
 *
 *     ZSTD_v1_5_2_DCtx_reset(zds, ZSTD_v1_5_2_reset_session_only);
 *
 * re-use decompression parameters from previous init; saves dictionary loading
 * Note : this prototype will be marked as deprecated and generate compilation warnings on reaching v1.5.x
 */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_resetDStream(ZSTD_v1_5_2_DStream* zds);


/*********************************************************************
*  Buffer-less and synchronous inner streaming functions
*
*  This is an advanced API, giving full control over buffer management, for users which need direct control over memory.
*  But it's also a complex one, with several restrictions, documented below.
*  Prefer normal streaming API for an easier experience.
********************************************************************* */

/**
  Buffer-less streaming compression (synchronous mode)

  A ZSTD_v1_5_2_CCtx object is required to track streaming operations.
  Use ZSTD_v1_5_2_createCCtx() / ZSTD_v1_5_2_freeCCtx() to manage resource.
  ZSTD_v1_5_2_CCtx object can be re-used multiple times within successive compression operations.

  Start by initializing a context.
  Use ZSTD_v1_5_2_compressBegin(), or ZSTD_v1_5_2_compressBegin_usingDict() for dictionary compression.
  It's also possible to duplicate a reference context which has already been initialized, using ZSTD_v1_5_2_copyCCtx()

  Then, consume your input using ZSTD_v1_5_2_compressContinue().
  There are some important considerations to keep in mind when using this advanced function :
  - ZSTD_v1_5_2_compressContinue() has no internal buffer. It uses externally provided buffers only.
  - Interface is synchronous : input is consumed entirely and produces 1+ compressed blocks.
  - Caller must ensure there is enough space in `dst` to store compressed data under worst case scenario.
    Worst case evaluation is provided by ZSTD_v1_5_2_compressBound().
    ZSTD_v1_5_2_compressContinue() doesn't guarantee recover after a failed compression.
  - ZSTD_v1_5_2_compressContinue() presumes prior input ***is still accessible and unmodified*** (up to maximum distance size, see WindowLog).
    It remembers all previous contiguous blocks, plus one separated memory segment (which can itself consists of multiple contiguous blocks)
  - ZSTD_v1_5_2_compressContinue() detects that prior input has been overwritten when `src` buffer overlaps.
    In which case, it will "discard" the relevant memory section from its history.

  Finish a frame with ZSTD_v1_5_2_compressEnd(), which will write the last block(s) and optional checksum.
  It's possible to use srcSize==0, in which case, it will write a final empty block to end the frame.
  Without last block mark, frames are considered unfinished (hence corrupted) by compliant decoders.

  `ZSTD_v1_5_2_CCtx` object can be re-used (ZSTD_v1_5_2_compressBegin()) to compress again.
*/

/*=====   Buffer-less streaming compression functions  =====*/
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_compressBegin(ZSTD_v1_5_2_CCtx* cctx, int compressionLevel);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_compressBegin_usingDict(ZSTD_v1_5_2_CCtx* cctx, const void* dict, size_t dictSize, int compressionLevel);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_compressBegin_usingCDict(ZSTD_v1_5_2_CCtx* cctx, const ZSTD_v1_5_2_CDict* cdict); /**< note: fails if cdict==NULL */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_copyCCtx(ZSTD_v1_5_2_CCtx* cctx, const ZSTD_v1_5_2_CCtx* preparedCCtx, unsigned long long pledgedSrcSize); /**<  note: if pledgedSrcSize is not known, use ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN */

ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_compressContinue(ZSTD_v1_5_2_CCtx* cctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_compressEnd(ZSTD_v1_5_2_CCtx* cctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);

/* The ZSTD_v1_5_2_compressBegin_advanced() and ZSTD_v1_5_2_compressBegin_usingCDict_advanced() are now DEPRECATED and will generate a compiler warning */
ZSTD_v1_5_2_DEPRECATED("use advanced API to access custom parameters")
size_t ZSTD_v1_5_2_compressBegin_advanced(ZSTD_v1_5_2_CCtx* cctx, const void* dict, size_t dictSize, ZSTD_v1_5_2_parameters params, unsigned long long pledgedSrcSize); /**< pledgedSrcSize : If srcSize is not known at init time, use ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN */
ZSTD_v1_5_2_DEPRECATED("use advanced API to access custom parameters")
size_t ZSTD_v1_5_2_compressBegin_usingCDict_advanced(ZSTD_v1_5_2_CCtx* const cctx, const ZSTD_v1_5_2_CDict* const cdict, ZSTD_v1_5_2_frameParameters const fParams, unsigned long long const pledgedSrcSize);   /* compression parameters are already set within cdict. pledgedSrcSize must be correct. If srcSize is not known, use macro ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN */
/**
  Buffer-less streaming decompression (synchronous mode)

  A ZSTD_v1_5_2_DCtx object is required to track streaming operations.
  Use ZSTD_v1_5_2_createDCtx() / ZSTD_v1_5_2_freeDCtx() to manage it.
  A ZSTD_v1_5_2_DCtx object can be re-used multiple times.

  First typical operation is to retrieve frame parameters, using ZSTD_v1_5_2_getFrameHeader().
  Frame header is extracted from the beginning of compressed frame, so providing only the frame's beginning is enough.
  Data fragment must be large enough to ensure successful decoding.
 `ZSTD_v1_5_2_frameHeaderSize_max` bytes is guaranteed to always be large enough.
  @result : 0 : successful decoding, the `ZSTD_v1_5_2_frameHeader` structure is correctly filled.
           >0 : `srcSize` is too small, please provide at least @result bytes on next attempt.
           errorCode, which can be tested using ZSTD_v1_5_2_isError().

  It fills a ZSTD_v1_5_2_frameHeader structure with important information to correctly decode the frame,
  such as the dictionary ID, content size, or maximum back-reference distance (`windowSize`).
  Note that these values could be wrong, either because of data corruption, or because a 3rd party deliberately spoofs false information.
  As a consequence, check that values remain within valid application range.
  For example, do not allocate memory blindly, check that `windowSize` is within expectation.
  Each application can set its own limits, depending on local restrictions.
  For extended interoperability, it is recommended to support `windowSize` of at least 8 MB.

  ZSTD_v1_5_2_decompressContinue() needs previous data blocks during decompression, up to `windowSize` bytes.
  ZSTD_v1_5_2_decompressContinue() is very sensitive to contiguity,
  if 2 blocks don't follow each other, make sure that either the compressor breaks contiguity at the same place,
  or that previous contiguous segment is large enough to properly handle maximum back-reference distance.
  There are multiple ways to guarantee this condition.

  The most memory efficient way is to use a round buffer of sufficient size.
  Sufficient size is determined by invoking ZSTD_v1_5_2_decodingBufferSize_min(),
  which can @return an error code if required value is too large for current system (in 32-bits mode).
  In a round buffer methodology, ZSTD_v1_5_2_decompressContinue() decompresses each block next to previous one,
  up to the moment there is not enough room left in the buffer to guarantee decoding another full block,
  which maximum size is provided in `ZSTD_v1_5_2_frameHeader` structure, field `blockSizeMax`.
  At which point, decoding can resume from the beginning of the buffer.
  Note that already decoded data stored in the buffer should be flushed before being overwritten.

  There are alternatives possible, for example using two or more buffers of size `windowSize` each, though they consume more memory.

  Finally, if you control the compression process, you can also ignore all buffer size rules,
  as long as the encoder and decoder progress in "lock-step",
  aka use exactly the same buffer sizes, break contiguity at the same place, etc.

  Once buffers are setup, start decompression, with ZSTD_v1_5_2_decompressBegin().
  If decompression requires a dictionary, use ZSTD_v1_5_2_decompressBegin_usingDict() or ZSTD_v1_5_2_decompressBegin_usingDDict().

  Then use ZSTD_v1_5_2_nextSrcSizeToDecompress() and ZSTD_v1_5_2_decompressContinue() alternatively.
  ZSTD_v1_5_2_nextSrcSizeToDecompress() tells how many bytes to provide as 'srcSize' to ZSTD_v1_5_2_decompressContinue().
  ZSTD_v1_5_2_decompressContinue() requires this _exact_ amount of bytes, or it will fail.

 @result of ZSTD_v1_5_2_decompressContinue() is the number of bytes regenerated within 'dst' (necessarily <= dstCapacity).
  It can be zero : it just means ZSTD_v1_5_2_decompressContinue() has decoded some metadata item.
  It can also be an error code, which can be tested with ZSTD_v1_5_2_isError().

  A frame is fully decoded when ZSTD_v1_5_2_nextSrcSizeToDecompress() returns zero.
  Context can then be reset to start a new decompression.

  Note : it's possible to know if next input to present is a header or a block, using ZSTD_v1_5_2_nextInputType().
  This information is not required to properly decode a frame.

  == Special case : skippable frames ==

  Skippable frames allow integration of user-defined data into a flow of concatenated frames.
  Skippable frames will be ignored (skipped) by decompressor.
  The format of skippable frames is as follows :
  a) Skippable frame ID - 4 Bytes, Little endian format, any value from 0x184D2A50 to 0x184D2A5F
  b) Frame Size - 4 Bytes, Little endian format, unsigned 32-bits
  c) Frame Content - any content (User Data) of length equal to Frame Size
  For skippable frames ZSTD_v1_5_2_getFrameHeader() returns zfhPtr->frameType==ZSTD_v1_5_2_skippableFrame.
  For skippable frames ZSTD_v1_5_2_decompressContinue() always returns 0 : it only skips the content.
*/

/*=====   Buffer-less streaming decompression functions  =====*/
typedef enum { ZSTD_v1_5_2_frame, ZSTD_v1_5_2_skippableFrame } ZSTD_v1_5_2_frameType_e;
typedef struct {
    unsigned long long frameContentSize; /* if == ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN, it means this field is not available. 0 means "empty" */
    unsigned long long windowSize;       /* can be very large, up to <= frameContentSize */
    unsigned blockSizeMax;
    ZSTD_v1_5_2_frameType_e frameType;          /* if == ZSTD_v1_5_2_skippableFrame, frameContentSize is the size of skippable content */
    unsigned headerSize;
    unsigned dictID;
    unsigned checksumFlag;
} ZSTD_v1_5_2_frameHeader;

/*! ZSTD_v1_5_2_getFrameHeader() :
 *  decode Frame Header, or requires larger `srcSize`.
 * @return : 0, `zfhPtr` is correctly filled,
 *          >0, `srcSize` is too small, value is wanted `srcSize` amount,
 *           or an error code, which can be tested using ZSTD_v1_5_2_isError() */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_getFrameHeader(ZSTD_v1_5_2_frameHeader* zfhPtr, const void* src, size_t srcSize);   /**< doesn't consume input */
/*! ZSTD_v1_5_2_getFrameHeader_advanced() :
 *  same as ZSTD_v1_5_2_getFrameHeader(),
 *  with added capability to select a format (like ZSTD_v1_5_2_f_zstd1_magicless) */
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_getFrameHeader_advanced(ZSTD_v1_5_2_frameHeader* zfhPtr, const void* src, size_t srcSize, ZSTD_v1_5_2_format_e format);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_decodingBufferSize_min(unsigned long long windowSize, unsigned long long frameContentSize);  /**< when frame content size is not known, pass in frameContentSize == ZSTD_v1_5_2_CONTENTSIZE_UNKNOWN */

ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_decompressBegin(ZSTD_v1_5_2_DCtx* dctx);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_decompressBegin_usingDict(ZSTD_v1_5_2_DCtx* dctx, const void* dict, size_t dictSize);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_decompressBegin_usingDDict(ZSTD_v1_5_2_DCtx* dctx, const ZSTD_v1_5_2_DDict* ddict);

ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_nextSrcSizeToDecompress(ZSTD_v1_5_2_DCtx* dctx);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_decompressContinue(ZSTD_v1_5_2_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);

/* misc */
ZSTDLIB_STATIC_API void   ZSTD_v1_5_2_copyDCtx(ZSTD_v1_5_2_DCtx* dctx, const ZSTD_v1_5_2_DCtx* preparedDCtx);
typedef enum { ZSTDnit_frameHeader, ZSTDnit_blockHeader, ZSTDnit_block, ZSTDnit_lastBlock, ZSTDnit_checksum, ZSTDnit_skippableFrame } ZSTD_v1_5_2_nextInputType_e;
ZSTDLIB_STATIC_API ZSTD_v1_5_2_nextInputType_e ZSTD_v1_5_2_nextInputType(ZSTD_v1_5_2_DCtx* dctx);




/* ============================ */
/**       Block level API       */
/* ============================ */

/*!
    Block functions produce and decode raw zstd blocks, without frame metadata.
    Frame metadata cost is typically ~12 bytes, which can be non-negligible for very small blocks (< 100 bytes).
    But users will have to take in charge needed metadata to regenerate data, such as compressed and content sizes.

    A few rules to respect :
    - Compressing and decompressing require a context structure
      + Use ZSTD_v1_5_2_createCCtx() and ZSTD_v1_5_2_createDCtx()
    - It is necessary to init context before starting
      + compression : any ZSTD_v1_5_2_compressBegin*() variant, including with dictionary
      + decompression : any ZSTD_v1_5_2_decompressBegin*() variant, including with dictionary
      + copyCCtx() and copyDCtx() can be used too
    - Block size is limited, it must be <= ZSTD_v1_5_2_getBlockSize() <= ZSTD_v1_5_2_BLOCKSIZE_MAX == 128 KB
      + If input is larger than a block size, it's necessary to split input data into multiple blocks
      + For inputs larger than a single block, consider using regular ZSTD_v1_5_2_compress() instead.
        Frame metadata is not that costly, and quickly becomes negligible as source size grows larger than a block.
    - When a block is considered not compressible enough, ZSTD_v1_5_2_compressBlock() result will be 0 (zero) !
      ===> In which case, nothing is produced into `dst` !
      + User __must__ test for such outcome and deal directly with uncompressed data
      + A block cannot be declared incompressible if ZSTD_v1_5_2_compressBlock() return value was != 0.
        Doing so would mess up with statistics history, leading to potential data corruption.
      + ZSTD_v1_5_2_decompressBlock() _doesn't accept uncompressed data as input_ !!
      + In case of multiple successive blocks, should some of them be uncompressed,
        decoder must be informed of their existence in order to follow proper history.
        Use ZSTD_v1_5_2_insertBlock() for such a case.
*/

/*=====   Raw zstd block functions  =====*/
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_getBlockSize   (const ZSTD_v1_5_2_CCtx* cctx);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_compressBlock  (ZSTD_v1_5_2_CCtx* cctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_decompressBlock(ZSTD_v1_5_2_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);
ZSTDLIB_STATIC_API size_t ZSTD_v1_5_2_insertBlock    (ZSTD_v1_5_2_DCtx* dctx, const void* blockStart, size_t blockSize);  /**< insert uncompressed block into `dctx` history. Useful for multi-blocks decompression. */


#endif   /* ZSTD_v1_5_2_H_ZSTD_v1_5_2_STATIC_LINKING_ONLY */

#if defined (__cplusplus)
}
#endif
