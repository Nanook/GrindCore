/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_v1_5_2_LAZY_H
#define ZSTD_v1_5_2_LAZY_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "ZSTD_compress_internal.h"

/**
 * Dedicated Dictionary Search Structure bucket log. In the
 * ZSTD_v1_5_2_dedicatedDictSearch mode, the hashTable has
 * 2 ** ZSTD_v1_5_2_LAZY_DDSS_BUCKET_LOG entries in each bucket, rather than just
 * one.
 */
#define ZSTD_v1_5_2_LAZY_DDSS_BUCKET_LOG 2

U32 ZSTD_v1_5_2_insertAndFindFirstIndex(ZSTD_v1_5_2_matchState_t* ms, const BYTE* ip);
void ZSTD_v1_5_2_row_update(ZSTD_v1_5_2_matchState_t* const ms, const BYTE* ip);

void ZSTD_v1_5_2_dedicatedDictSearch_lazy_loadDictionary(ZSTD_v1_5_2_matchState_t* ms, const BYTE* const ip);

void ZSTD_v1_5_2_preserveUnsortedMark (U32* const table, U32 const size, U32 const reducerValue);  /*! used in ZSTD_v1_5_2_reduceIndex(). preemptively increase value of ZSTD_v1_5_2_DUBT_UNSORTED_MARK */

size_t ZSTD_v1_5_2_compressBlock_btlazy2(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy2(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_greedy(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy2_row(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy_row(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_greedy_row(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);

size_t ZSTD_v1_5_2_compressBlock_btlazy2_dictMatchState(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy2_dictMatchState(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy_dictMatchState(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_greedy_dictMatchState(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy2_dictMatchState_row(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy_dictMatchState_row(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_greedy_dictMatchState_row(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);

size_t ZSTD_v1_5_2_compressBlock_lazy2_dedicatedDictSearch(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy_dedicatedDictSearch(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_greedy_dedicatedDictSearch(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy2_dedicatedDictSearch_row(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy_dedicatedDictSearch_row(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_greedy_dedicatedDictSearch_row(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);

size_t ZSTD_v1_5_2_compressBlock_greedy_extDict(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy_extDict(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy2_extDict(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_greedy_extDict_row(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy_extDict_row(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_lazy2_extDict_row(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
size_t ZSTD_v1_5_2_compressBlock_btlazy2_extDict(
        ZSTD_v1_5_2_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_v1_5_2_REP_NUM],
        void const* src, size_t srcSize);
        

#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_v1_5_2_LAZY_H */
