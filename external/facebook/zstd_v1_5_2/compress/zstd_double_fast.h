/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_v1_5_2_DOUBLE_FAST_H
#define ZSTD_v1_5_2_DOUBLE_FAST_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "../common/mem.h"      /* U32 */
#include "ZSTD_compress_internal.h"     /* ZSTD_v1_5_2_CCtx, size_t */

void ZSTD_v1_5_2_fillDoubleHashTable(ZSTD_v1_5_2_matchState_t* ms,
                              void const* end, ZSTD_v1_5_2_dictTableLoadMethod_e dtlm);
size_t ZSTD_v1_5_2_compressBlock_doubleFast(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_doubleFast_dictMatchState(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_doubleFast_extDict(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);


#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_v1_5_2_DOUBLE_FAST_H */
