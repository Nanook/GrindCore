/*
 * Copyright (c) 2017-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* *********************************************************
*  Turn on Large Files support (>4GB) for 32-bit Linux/Unix
***********************************************************/
#if !defined(__64BIT__) || defined(__MINGW32__)       /* No point defining Large file for 64 bit but MinGW-w64 requires it */
#  if !defined(_FILE_OFFSET_BITS)
#    define _FILE_OFFSET_BITS 64                      /* turn off_t into a 64-bit type for ftello, fseeko */
#  endif
#  if !defined(_LARGEFILE_SOURCE)                     /* obsolete macro, replaced with _FILE_OFFSET_BITS */
#    define _LARGEFILE_SOURCE 1                       /* Large File Support extension (LFS) - fseeko, ftello */
#  endif
#  if defined(_AIX) || defined(__hpux)
#    define _LARGE_FILES                              /* Large file support on 32-bits AIX and HP-UX */
#  endif
#endif

/* ************************************************************
* Avoid fseek()'s 2GiB barrier with MSVC, macOS, *BSD, MinGW
***************************************************************/
#if defined(_MSC_VER) && _MSC_VER >= 1400
#   define LONG_SEEK _fseeki64
#elif !defined(__64BIT__) && (PLATFORM_POSIX_VERSION >= 200112L) /* No point defining Large file for 64 bit */
#  define LONG_SEEK fseeko
#elif defined(__MINGW32__) && !defined(__STRICT_ANSI__) && !defined(__NO_MINGW_LFS) && defined(__MSVCRT__)
#   define LONG_SEEK fseeko64
#elif defined(_WIN32) && !defined(__DJGPP__)
#   include <windows.h>
    static int LONG_SEEK(FILE* file, __int64 offset, int origin) {
        LARGE_INTEGER off;
        DWORD method;
        off.QuadPart = offset;
        if (origin == SEEK_END)
            method = FILE_END;
        else if (origin == SEEK_CUR)
            method = FILE_CURRENT;
        else
            method = FILE_BEGIN;

        if (SetFilePointerEx((HANDLE) _get_osfhandle(_fileno(file)), off, NULL, method))
            return 0;
        else
            return -1;
    }
#else
#   define LONG_SEEK fseek
#endif

#include <stdlib.h>  /* malloc, free */
#include <stdio.h>   /* FILE* */
#include <limits.h>  /* UNIT_MAX */
#include <assert.h>

#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"

#define ZSTD_STATIC_LINKING_ONLY
#define ZSTD_v1_5_2_STATIC_LINKING_ONLY
#include "zstd.h"
#include "zstd_errors.h"
#include "mem.h"
#include "zstd_seekable.h"

#undef ERROR
#define ERROR(name) ((size_t)-ZSTD_v1_5_2_error_##name)

#define CHECK_IO(f) { int const errcod = (f); if (errcod < 0) return ERROR(seekableIO); }

#undef MIN
#undef MAX
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ZSTD_v1_5_2_seekable_NO_OUTPUT_PROGRESS_MAX 16

/* Special-case callbacks for FILE* and in-memory modes, so that we can treat
 * them the same way as the advanced API */
static int ZSTD_v1_5_2_seekable_read_FILE(void* opaque, void* buffer, size_t n)
{
    size_t const result = fread(buffer, 1, n, (FILE*)opaque);
    if (result != n) {
        return -1;
    }
    return 0;
}

static int ZSTD_v1_5_2_seekable_seek_FILE(void* opaque, long long offset, int origin)
{
    int const ret = LONG_SEEK((FILE*)opaque, offset, origin);
    if (ret) return ret;
    return fflush((FILE*)opaque);
}

typedef struct {
    const void *ptr;
    size_t size;
    size_t pos;
} buffWrapper_t;

static int ZSTD_v1_5_2_seekable_read_buff(void* opaque, void* buffer, size_t n)
{
    buffWrapper_t* const buff = (buffWrapper_t*)opaque;
    assert(buff != NULL);
    if (buff->pos + n > buff->size) return -1;
    memcpy(buffer, (const BYTE*)buff->ptr + buff->pos, n);
    buff->pos += n;
    return 0;
}

static int ZSTD_v1_5_2_seekable_seek_buff(void* opaque, long long offset, int origin)
{
    buffWrapper_t* const buff = (buffWrapper_t*) opaque;
    unsigned long long newOffset = 0;
    assert(buff != NULL);
    switch (origin) {
    case SEEK_SET:
        assert(offset >= 0);
        newOffset = (unsigned long long)offset;
        break;
    case SEEK_CUR:
        newOffset = (unsigned long long)((long long)buff->pos + offset);
        break;
    case SEEK_END:
        newOffset = (unsigned long long)((long long)buff->size + offset);
        break;
    default:
        assert(0);  /* not possible */
    }
    if (newOffset > buff->size) {
        return -1;
    }
    buff->pos = newOffset;
    return 0;
}

typedef struct {
    U64 cOffset;
    U64 dOffset;
    U32 checksum;
} seekEntry_t;

struct ZSTD_v1_5_2_seekTable_s {
    seekEntry_t* entries;
    size_t tableLen;

    int checksumFlag;
};

#define SEEKABLE_BUFF_SIZE ZSTD_v1_5_2_BLOCKSIZE_MAX

struct ZSTD_v1_5_2_seekable_s {
    ZSTD_v1_5_2_DStream* dstream;
    ZSTD_v1_5_2_seekTable seekTable;
    ZSTD_v1_5_2_seekable_customFile src;

    U64 decompressedOffset;
    U32 curFrame;

    BYTE inBuff[SEEKABLE_BUFF_SIZE]; /* need to do our own input buffering */
    BYTE outBuff[SEEKABLE_BUFF_SIZE]; /* so we can efficiently decompress the
                                         starts of chunks before we get to the
                                         desired section */
    ZSTD_v1_5_2_inBuffer in; /* maintain continuity across ZSTD_v1_5_2_seekable_decompress operations */
    buffWrapper_t buffWrapper; /* for `src.opaque` in in-memory mode */

    XXH64_state_t xxhState;
};

ZSTD_v1_5_2_seekable* ZSTD_v1_5_2_seekable_create(void)
{
    ZSTD_v1_5_2_seekable* const zs = (ZSTD_v1_5_2_seekable*)malloc(sizeof(ZSTD_v1_5_2_seekable));
    if (zs == NULL) return NULL;

    /* also initializes stage to zsds_init */
    memset(zs, 0, sizeof(*zs));

    zs->dstream = ZSTD_v1_5_2_createDStream();
    if (zs->dstream == NULL) {
        free(zs);
        return NULL;
    }

    return zs;
}

size_t ZSTD_v1_5_2_seekable_free(ZSTD_v1_5_2_seekable* zs)
{
    if (zs == NULL) return 0; /* support free on null */
    ZSTD_v1_5_2_freeDStream(zs->dstream);
    free(zs->seekTable.entries);
    free(zs);
    return 0;
}

ZSTD_v1_5_2_seekTable* ZSTD_v1_5_2_seekTable_create_fromSeekable(const ZSTD_v1_5_2_seekable* zs)
{
    ZSTD_v1_5_2_seekTable* const st = (ZSTD_v1_5_2_seekTable*)malloc(sizeof(ZSTD_v1_5_2_seekTable));
    if (st==NULL) return NULL;

    st->checksumFlag = zs->seekTable.checksumFlag;
    st->tableLen = zs->seekTable.tableLen;

    /* Allocate an extra entry at the end to match logic of initial allocation */
    size_t const entriesSize = sizeof(seekEntry_t) * (zs->seekTable.tableLen + 1);
    seekEntry_t* const entries = (seekEntry_t*)malloc(entriesSize);
    if (entries==NULL) {
        free(st);
        return NULL;
    }

    memcpy(entries, zs->seekTable.entries, entriesSize);
    st->entries = entries;
    return st;
}

size_t ZSTD_v1_5_2_seekTable_free(ZSTD_v1_5_2_seekTable* st)
{
    if (st == NULL) return 0; /* support free on null */
    free(st->entries);
    free(st);
    return 0;
}

/** ZSTD_v1_5_2_seekable_offsetToFrameIndex() :
 *  Performs a binary search to find the last frame with a decompressed offset
 *  <= pos
 *  @return : the frame's index */
unsigned ZSTD_v1_5_2_seekable_offsetToFrameIndex(const ZSTD_v1_5_2_seekable* zs, unsigned long long pos)
{
    return ZSTD_v1_5_2_seekTable_offsetToFrameIndex(&zs->seekTable, pos);
}

unsigned ZSTD_v1_5_2_seekTable_offsetToFrameIndex(const ZSTD_v1_5_2_seekTable* st, unsigned long long pos)
{
    U32 lo = 0;
    U32 hi = (U32)st->tableLen;
    assert(st->tableLen <= UINT_MAX);

    if (pos >= st->entries[st->tableLen].dOffset) {
        return (unsigned)st->tableLen;
    }

    while (lo + 1 < hi) {
        U32 const mid = lo + ((hi - lo) >> 1);
        if (st->entries[mid].dOffset <= pos) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

unsigned ZSTD_v1_5_2_seekable_getNumFrames(const ZSTD_v1_5_2_seekable* zs)
{
    return ZSTD_v1_5_2_seekTable_getNumFrames(&zs->seekTable);
}

unsigned ZSTD_v1_5_2_seekTable_getNumFrames(const ZSTD_v1_5_2_seekTable* st)
{
    assert(st->tableLen <= UINT_MAX);
    return (unsigned)st->tableLen;
}

unsigned long long ZSTD_v1_5_2_seekable_getFrameCompressedOffset(const ZSTD_v1_5_2_seekable* zs, unsigned frameIndex)
{
    return ZSTD_v1_5_2_seekTable_getFrameCompressedOffset(&zs->seekTable, frameIndex);
}

unsigned long long ZSTD_v1_5_2_seekTable_getFrameCompressedOffset(const ZSTD_v1_5_2_seekTable* st, unsigned frameIndex)
{
    if (frameIndex >= st->tableLen) return ZSTD_v1_5_2_seekable_FRAMEINDEX_TOOLARGE;
    return st->entries[frameIndex].cOffset;
}

unsigned long long ZSTD_v1_5_2_seekable_getFrameDecompressedOffset(const ZSTD_v1_5_2_seekable* zs, unsigned frameIndex)
{
    return ZSTD_v1_5_2_seekTable_getFrameDecompressedOffset(&zs->seekTable, frameIndex);
}

unsigned long long ZSTD_v1_5_2_seekTable_getFrameDecompressedOffset(const ZSTD_v1_5_2_seekTable* st, unsigned frameIndex)
{
    if (frameIndex >= st->tableLen) return ZSTD_v1_5_2_seekable_FRAMEINDEX_TOOLARGE;
    return st->entries[frameIndex].dOffset;
}

size_t ZSTD_v1_5_2_seekable_getFrameCompressedSize(const ZSTD_v1_5_2_seekable* zs, unsigned frameIndex)
{
    return ZSTD_v1_5_2_seekTable_getFrameCompressedSize(&zs->seekTable, frameIndex);
}

size_t ZSTD_v1_5_2_seekTable_getFrameCompressedSize(const ZSTD_v1_5_2_seekTable* st, unsigned frameIndex)
{
    if (frameIndex >= st->tableLen) return ERROR(frameIndex_tooLarge);
    return st->entries[frameIndex + 1].cOffset -
           st->entries[frameIndex].cOffset;
}

size_t ZSTD_v1_5_2_seekable_getFrameDecompressedSize(const ZSTD_v1_5_2_seekable* zs, unsigned frameIndex)
{
    return ZSTD_v1_5_2_seekTable_getFrameDecompressedSize(&zs->seekTable, frameIndex);
}

size_t ZSTD_v1_5_2_seekTable_getFrameDecompressedSize(const ZSTD_v1_5_2_seekTable* st, unsigned frameIndex)
{
    if (frameIndex > st->tableLen) return ERROR(frameIndex_tooLarge);
    return st->entries[frameIndex + 1].dOffset -
           st->entries[frameIndex].dOffset;
}

static size_t ZSTD_v1_5_2_seekable_loadSeekTable(ZSTD_v1_5_2_seekable* zs)
{
    int checksumFlag;
    ZSTD_v1_5_2_seekable_customFile src = zs->src;
    /* read the footer, fixed size */
    CHECK_IO(src.seek(src.opaque, -(int)ZSTD_seekTableFooterSize, SEEK_END));
    CHECK_IO(src.read(src.opaque, zs->inBuff, ZSTD_seekTableFooterSize));

    if (MEM_readLE32(zs->inBuff + 5) != ZSTD_v1_5_2_seekable_MAGICNUMBER) {
        return ERROR(prefix_unknown);
    }

    {   BYTE const sfd = zs->inBuff[4];
        checksumFlag = sfd >> 7;

        /* check reserved bits */
        if ((sfd >> 2) & 0x1f) {
            return ERROR(corruption_detected);
    }   }

    {   U32 const numFrames = MEM_readLE32(zs->inBuff);
        U32 const sizePerEntry = 8 + (checksumFlag?4:0);
        U32 const tableSize = sizePerEntry * numFrames;
        U32 const frameSize = tableSize + ZSTD_seekTableFooterSize + ZSTD_v1_5_2_SKIPPABLEHEADERSIZE;

        U32 remaining = frameSize - ZSTD_seekTableFooterSize; /* don't need to re-read footer */
        {   U32 const toRead = MIN(remaining, SEEKABLE_BUFF_SIZE);
            CHECK_IO(src.seek(src.opaque, -(S64)frameSize, SEEK_END));
            CHECK_IO(src.read(src.opaque, zs->inBuff, toRead));
            remaining -= toRead;
        }

        if (MEM_readLE32(zs->inBuff) != (ZSTD_v1_5_2_MAGIC_SKIPPABLE_START | 0xE)) {
            return ERROR(prefix_unknown);
        }
        if (MEM_readLE32(zs->inBuff+4) + ZSTD_v1_5_2_SKIPPABLEHEADERSIZE != frameSize) {
            return ERROR(prefix_unknown);
        }

        {   /* Allocate an extra entry at the end so that we can do size
             * computations on the last element without special case */
            seekEntry_t* const entries = (seekEntry_t*)malloc(sizeof(seekEntry_t) * (numFrames + 1));

            U32 idx = 0;
            U32 pos = 8;

            U64 cOffset = 0;
            U64 dOffset = 0;

            if (entries == NULL) return ERROR(memory_allocation);

            /* compute cumulative positions */
            for (; idx < numFrames; idx++) {
                if (pos + sizePerEntry > SEEKABLE_BUFF_SIZE) {
                    U32 const offset = SEEKABLE_BUFF_SIZE - pos;
                    U32 const toRead = MIN(remaining, SEEKABLE_BUFF_SIZE - offset);
                    memmove(zs->inBuff, zs->inBuff + pos, offset); /* move any data we haven't read yet */
                    CHECK_IO(src.read(src.opaque, zs->inBuff+offset, toRead));
                    remaining -= toRead;
                    pos = 0;
                }
                entries[idx].cOffset = cOffset;
                entries[idx].dOffset = dOffset;

                cOffset += MEM_readLE32(zs->inBuff + pos);
                pos += 4;
                dOffset += MEM_readLE32(zs->inBuff + pos);
                pos += 4;
                if (checksumFlag) {
                    entries[idx].checksum = MEM_readLE32(zs->inBuff + pos);
                    pos += 4;
                }
            }
            entries[numFrames].cOffset = cOffset;
            entries[numFrames].dOffset = dOffset;

            zs->seekTable.entries = entries;
            zs->seekTable.tableLen = numFrames;
            zs->seekTable.checksumFlag = checksumFlag;
            return 0;
        }
    }
}

size_t ZSTD_v1_5_2_seekable_initBuff(ZSTD_v1_5_2_seekable* zs, const void* src, size_t srcSize)
{
    zs->buffWrapper = (buffWrapper_t){src, srcSize, 0};
    {   ZSTD_v1_5_2_seekable_customFile srcFile = {&zs->buffWrapper,
                                            &ZSTD_v1_5_2_seekable_read_buff,
                                            &ZSTD_v1_5_2_seekable_seek_buff};
        return ZSTD_v1_5_2_seekable_initAdvanced(zs, srcFile); }
}

size_t ZSTD_v1_5_2_seekable_initFile(ZSTD_v1_5_2_seekable* zs, FILE* src)
{
    ZSTD_v1_5_2_seekable_customFile srcFile = {src, &ZSTD_v1_5_2_seekable_read_FILE,
                                        &ZSTD_v1_5_2_seekable_seek_FILE};
    return ZSTD_v1_5_2_seekable_initAdvanced(zs, srcFile);
}

size_t ZSTD_v1_5_2_seekable_initAdvanced(ZSTD_v1_5_2_seekable* zs, ZSTD_v1_5_2_seekable_customFile src)
{
    zs->src = src;

    {   const size_t seekTableInit = ZSTD_v1_5_2_seekable_loadSeekTable(zs);
        if (ZSTD_v1_5_2_isError(seekTableInit)) return seekTableInit; }

    zs->decompressedOffset = (U64)-1;
    zs->curFrame = (U32)-1;

    {   const size_t dstreamInit = ZSTD_v1_5_2_initDStream(zs->dstream);
        if (ZSTD_v1_5_2_isError(dstreamInit)) return dstreamInit; }
    return 0;
}

size_t ZSTD_v1_5_2_seekable_decompress(ZSTD_v1_5_2_seekable* zs, void* dst, size_t len, unsigned long long offset)
{
    unsigned long long const eos = zs->seekTable.entries[zs->seekTable.tableLen].dOffset;
    if (offset + len > eos) {
        len = eos - offset;
    }

    U32 targetFrame = ZSTD_v1_5_2_seekable_offsetToFrameIndex(zs, offset);
    U32 noOutputProgressCount = 0;
    size_t srcBytesRead = 0;
    do {
        /* check if we can continue from a previous decompress job */
        if (targetFrame != zs->curFrame || offset != zs->decompressedOffset) {
            zs->decompressedOffset = zs->seekTable.entries[targetFrame].dOffset;
            zs->curFrame = targetFrame;

            assert(zs->seekTable.entries[targetFrame].cOffset < LLONG_MAX);
            CHECK_IO(zs->src.seek(zs->src.opaque,
                                  (long long)zs->seekTable.entries[targetFrame].cOffset,
                                  SEEK_SET));
            zs->in = (ZSTD_v1_5_2_inBuffer){zs->inBuff, 0, 0};
            XXH64_reset(&zs->xxhState, 0);
            ZSTD_v1_5_2_DCtx_reset(zs->dstream, ZSTD_v1_5_2_reset_session_only);
            if (zs->buffWrapper.size && srcBytesRead > zs->buffWrapper.size) {
                return ERROR(seekableIO);
            }
        }

        while (zs->decompressedOffset < offset + len) {
            size_t toRead;
            ZSTD_v1_5_2_outBuffer outTmp;
            size_t prevOutPos;
            size_t prevInPos;
            size_t forwardProgress;
            if (zs->decompressedOffset < offset) {
                /* dummy decompressions until we get to the target offset */
                outTmp = (ZSTD_v1_5_2_outBuffer){zs->outBuff, MIN(SEEKABLE_BUFF_SIZE, offset - zs->decompressedOffset), 0};
            } else {
                outTmp = (ZSTD_v1_5_2_outBuffer){dst, len, zs->decompressedOffset - offset};
            }

            prevOutPos = outTmp.pos;
            prevInPos = zs->in.pos;
            toRead = ZSTD_v1_5_2_decompressStream(zs->dstream, &outTmp, &zs->in);
            if (ZSTD_v1_5_2_isError(toRead)) {
                return toRead;
            }

            if (zs->seekTable.checksumFlag) {
                XXH64_update(&zs->xxhState, (BYTE*)outTmp.dst + prevOutPos,
                             outTmp.pos - prevOutPos);
            }
            forwardProgress = outTmp.pos - prevOutPos;
            if (forwardProgress == 0) {
                if (noOutputProgressCount++ > ZSTD_v1_5_2_seekable_NO_OUTPUT_PROGRESS_MAX) {
                    return ERROR(seekableIO);
                }
            } else {
                noOutputProgressCount = 0;
            }
            zs->decompressedOffset += forwardProgress;
            srcBytesRead += zs->in.pos - prevInPos;

            if (toRead == 0) {
                /* frame complete */

                /* verify checksum */
                if (zs->seekTable.checksumFlag &&
                    (XXH64_digest(&zs->xxhState) & 0xFFFFFFFFU) !=
                            zs->seekTable.entries[targetFrame].checksum) {
                    return ERROR(corruption_detected);
                }

                if (zs->decompressedOffset < offset + len) {
                    /* go back to the start and force a reset of the stream */
                    targetFrame = ZSTD_v1_5_2_seekable_offsetToFrameIndex(zs, zs->decompressedOffset);
                    /* in this case it will fail later with corruption_detected, since last block does not have checksum */
                    assert(targetFrame != zs->seekTable.tableLen);
                }
                break;
            }

            /* read in more data if we're done with this buffer */
            if (zs->in.pos == zs->in.size) {
                toRead = MIN(toRead, SEEKABLE_BUFF_SIZE);
                CHECK_IO(zs->src.read(zs->src.opaque, zs->inBuff, toRead));
                zs->in.size = toRead;
                zs->in.pos = 0;
            }
        }  /* while (zs->decompressedOffset < offset + len) */
    } while (zs->decompressedOffset != offset + len);

    return len;
}

size_t ZSTD_v1_5_2_seekable_decompressFrame(ZSTD_v1_5_2_seekable* zs, void* dst, size_t dstSize, unsigned frameIndex)
{
    if (frameIndex >= zs->seekTable.tableLen) {
        return ERROR(frameIndex_tooLarge);
    }

    {   size_t const decompressedSize =
                zs->seekTable.entries[frameIndex + 1].dOffset -
                zs->seekTable.entries[frameIndex].dOffset;
        if (dstSize < decompressedSize) {
            return ERROR(dstSize_tooSmall);
        }
        return ZSTD_v1_5_2_seekable_decompress(
                zs, dst, decompressedSize,
                zs->seekTable.entries[frameIndex].dOffset);
    }
}
