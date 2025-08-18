/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_v1_5_2_COMPRESS_SEQUENCES_H
#define ZSTD_v1_5_2_COMPRESS_SEQUENCES_H

#include "../common/fse.h" /* FSE_v1_5_2_repeat, FSE_v1_5_2_CTable */
#include "../common/zstd_internal.h" /* symbolEncodingType_e, ZSTD_v1_5_2_strategy */

typedef enum {
    ZSTD_v1_5_2_defaultDisallowed = 0,
    ZSTD_v1_5_2_defaultAllowed = 1
} ZSTD_v1_5_2_defaultPolicy_e;

symbolEncodingType_e
ZSTD_v1_5_2_selectEncodingType(
        FSE_v1_5_2_repeat* repeatMode, unsigned const* count, unsigned const max,
        size_t const mostFrequent, size_t nbSeq, unsigned const FSELog,
        FSE_v1_5_2_CTable const* prevCTable,
        short const* defaultNorm, U32 defaultNormLog,
        ZSTD_v1_5_2_defaultPolicy_e const isDefaultAllowed,
        ZSTD_v1_5_2_strategy const strategy);

size_t
ZSTD_v1_5_2_buildCTable(void* dst, size_t dstCapacity,
                FSE_v1_5_2_CTable* nextCTable, U32 FSELog, symbolEncodingType_e type,
                unsigned* count, U32 max,
                const BYTE* codeTable, size_t nbSeq,
                const S16* defaultNorm, U32 defaultNormLog, U32 defaultMax,
                const FSE_v1_5_2_CTable* prevCTable, size_t prevCTableSize,
                void* entropyWorkspace, size_t entropyWorkspaceSize);

size_t ZSTD_v1_5_2_encodeSequences(
            void* dst, size_t dstCapacity,
            FSE_v1_5_2_CTable const* CTable_MatchLength, BYTE const* mlCodeTable,
            FSE_v1_5_2_CTable const* CTable_OffsetBits, BYTE const* ofCodeTable,
            FSE_v1_5_2_CTable const* CTable_LitLength, BYTE const* llCodeTable,
            seqDef const* sequences, size_t nbSeq, int longOffsets, int bmi2);

size_t ZSTD_v1_5_2_fseBitCost(
    FSE_v1_5_2_CTable const* ctable,
    unsigned const* count,
    unsigned const max);

size_t ZSTD_v1_5_2_crossEntropyCost(short const* norm, unsigned accuracyLog,
                             unsigned const* count, unsigned const max);
#endif /* ZSTD_v1_5_2_COMPRESS_SEQUENCES_H */
