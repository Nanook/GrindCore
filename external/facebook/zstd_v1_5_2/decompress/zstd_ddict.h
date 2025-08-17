/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


#ifndef ZSTD_v1_5_2_DDICT_H
#define ZSTD_v1_5_2_DDICT_H

/*-*******************************************************
 *  Dependencies
 *********************************************************/
#include "../common/zstd_deps.h"   /* size_t */
#include "../zstd.h"     /* ZSTD_v1_5_2_DDict, and several public functions */


/*-*******************************************************
 *  Interface
 *********************************************************/

/* note: several prototypes are already published in `zstd.h` :
 * ZSTD_v1_5_2_createDDict()
 * ZSTD_v1_5_2_createDDict_byReference()
 * ZSTD_v1_5_2_createDDict_advanced()
 * ZSTD_v1_5_2_freeDDict()
 * ZSTD_v1_5_2_initStaticDDict()
 * ZSTD_v1_5_2_sizeof_DDict()
 * ZSTD_v1_5_2_estimateDDictSize()
 * ZSTD_v1_5_2_getDictID_fromDict()
 */

const void* ZSTD_v1_5_2_DDict_dictContent(const ZSTD_v1_5_2_DDict* ddict);
size_t ZSTD_v1_5_2_DDict_dictSize(const ZSTD_v1_5_2_DDict* ddict);

void ZSTD_v1_5_2_copyDDictParameters(ZSTD_v1_5_2_DCtx* dctx, const ZSTD_v1_5_2_DDict* ddict);



#endif /* ZSTD_v1_5_2_DDICT_H */
