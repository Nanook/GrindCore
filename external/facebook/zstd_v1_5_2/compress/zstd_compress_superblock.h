/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_v1_5_2_COMPRESS_ADVANCED_H
#define ZSTD_v1_5_2_COMPRESS_ADVANCED_H

/*-*************************************
*  Dependencies
***************************************/

#include "../zstd.h" /* ZSTD_v1_5_2_CCtx */

/*-*************************************
*  Target Compressed Block Size
***************************************/

/* ZSTD_v1_5_2_compressSuperBlock() :
 * Used to compress a super block when targetCBlockSize is being used.
 * The given block will be compressed into multiple sub blocks that are around targetCBlockSize. */
size_t ZSTD_v1_5_2_compressSuperBlock(ZSTD_v1_5_2_CCtx* zc,
                               void* dst, size_t dstCapacity,
                               void const* src, size_t srcSize,
                               unsigned lastBlock);

#endif /* ZSTD_v1_5_2_COMPRESS_ADVANCED_H */
