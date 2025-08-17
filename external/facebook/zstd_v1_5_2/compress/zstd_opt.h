/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_v1_5_2_OPT_H
#define ZSTD_v1_5_2_OPT_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "ZSTD_compress_internal.h"

/* used in ZSTD_v1_5_2_loadDictionaryContent() */
void ZSTD_v1_5_2_updateTree(ZSTD_v1_5_2_matchState_t* ms, const BYTE* ip, const BYTE* iend);

size_t ZSTD_v1_5_2_compressBlock_btopt(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_btultra(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_btultra2(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);


size_t ZSTD_v1_5_2_compressBlock_btopt_dictMatchState(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_btultra_dictMatchState(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);

size_t ZSTD_v1_5_2_compressBlock_btopt_extDict(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_btultra_extDict(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);

        /* note : no btultra2 variant for extDict nor dictMatchState,
         * because btultra2 is not meant to work with dictionaries
         * and is only specific for the first block (no prefix) */

#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_v1_5_2_OPT_H */
