#ifndef SEEKABLE_V1_5_7_H
#define SEEKABLE_V1_5_7_H

#include <stdio.h>
#include "zstd.h"   /* ZSTDLIB_API */

#if defined (__cplusplus)
extern "C" {
#endif


#define ZSTD_seekTableFooterSize 9

#define ZSTD_v1_5_7_seekable_MAGICNUMBER 0x8F92EAB1

#define ZSTD_v1_5_7_seekable_MAXFRAMES 0x8000000U

/* Limit maximum size to avoid potential issues storing the compressed size */
#define ZSTD_v1_5_7_seekable_MAX_FRAME_DECOMPRESSED_SIZE 0x40000000U

/*-****************************************************************************
*  Seekable Format
*
*  The seekable format splits the compressed data into a series of "frames",
*  each compressed individually so that decompression of a section in the
*  middle of an archive only requires zstd to decompress at most a frame's
*  worth of extra data, instead of the entire archive.
******************************************************************************/

typedef struct ZSTD_v1_5_7_seekable_CStream_s ZSTD_v1_5_7_seekable_CStream;
typedef struct ZSTD_v1_5_7_seekable_s ZSTD_v1_5_7_seekable;
typedef struct ZSTD_v1_5_7_seekTable_s ZSTD_v1_5_7_seekTable;

/*-****************************************************************************
*  Seekable compression - HowTo
*  A ZSTD_v1_5_7_seekable_CStream object is required to tracking streaming operation.
*  Use ZSTD_v1_5_7_seekable_createCStream() and ZSTD_v1_5_7_seekable_freeCStream() to create/
*  release resources.
*
*  Streaming objects are reusable to avoid allocation and deallocation,
*  to start a new compression operation call ZSTD_v1_5_7_seekable_initCStream() on the
*  compressor.
*
*  Data streamed to the seekable compressor will automatically be split into
*  frames of size `maxFrameSize` (provided in ZSTD_v1_5_7_seekable_initCStream()),
*  or if none is provided, will be cut off whenever ZSTD_v1_5_7_seekable_endFrame() is
*  called or when the default maximum frame size (2GB) is reached.
*
*  Use ZSTD_v1_5_7_seekable_initCStream() to initialize a ZSTD_v1_5_7_seekable_CStream object
*  for a new compression operation.
*  - `maxFrameSize` indicates the size at which to automatically start a new
*            seekable frame.
*            `maxFrameSize == 0` implies the default maximum size.
*            Smaller frame sizes allow faster decompression of small segments,
*            since retrieving a single byte requires decompression of
*            the full frame where the byte belongs.
*            In general, size the frames to roughly correspond to
*            the access granularity (when it's known).
*            But small sizes also reduce compression ratio.
*            Avoid really tiny frame sizes (< 1 KB),
*            that would hurt compression ratio considerably.
*  - `checksumFlag` indicates whether or not the seek table should include frame
*            checksums on the uncompressed data for verification.
*  @return : a size hint for input to provide for compression, or an error code
*            checkable with ZSTD_isError()
*
*  Use ZSTD_v1_5_7_seekable_compressStream() repetitively to consume input stream.
*  The function will automatically update both `pos` fields.
*  Note that it may not consume the entire input, in which case `pos < size`,
*  and it's up to the caller to present again remaining data.
*  @return : a size hint, preferred nb of bytes to use as input for next
*            function call or an error code, which can be tested using
*            ZSTD_isError().
*            Note 1 : it's just a hint, to help latency a little, any other
*                     value will work fine.
*
*  At any time, call ZSTD_v1_5_7_seekable_endFrame() to end the current frame and
*  start a new one.
*
*  ZSTD_v1_5_7_seekable_endStream() will end the current frame, and then write the seek
*  table so that decompressors can efficiently find compressed frames.
*  ZSTD_v1_5_7_seekable_endStream() may return a number > 0 if it was unable to flush
*  all the necessary data to `output`.  In this case, it should be called again
*  until all remaining data is flushed out and 0 is returned.
******************************************************************************/

/*===== Seekable compressor management =====*/
ZSTDLIB_API ZSTD_v1_5_7_seekable_CStream* ZSTD_v1_5_7_seekable_createCStream(void);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_freeCStream(ZSTD_v1_5_7_seekable_CStream* zcs);

/*===== Seekable compression functions =====*/
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_initCStream(ZSTD_v1_5_7_seekable_CStream* zcs, int compressionLevel, int checksumFlag, unsigned maxFrameSize);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_compressStream(ZSTD_v1_5_7_seekable_CStream* zcs, ZSTD_outBuffer* output, ZSTD_inBuffer* input);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_endFrame(ZSTD_v1_5_7_seekable_CStream* zcs, ZSTD_outBuffer* output);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_endStream(ZSTD_v1_5_7_seekable_CStream* zcs, ZSTD_outBuffer* output);

/*= Raw seek table API
 *  These functions allow for the seek table to be constructed directly.
 *  This table can then be appended to a file of concatenated frames.
 *  This allows the frames to be compressed independently, even in parallel,
 *  and compiled together afterward into a seekable archive.
 *
 *  Use ZSTD_v1_5_7_seekable_createFrameLog() to allocate and initialize a tracking
 *  structure.
 *
 *  Call ZSTD_v1_5_7_seekable_logFrame() once for each frame in the archive.
 *  checksum is optional, and will not be used if checksumFlag was 0 when the
 *  frame log was created.  If present, it should be the least significant 32
 *  bits of the XXH64 hash of the uncompressed data.
 *
 *  Call ZSTD_v1_5_7_seekable_writeSeekTable to serialize the data into a seek table.
 *  If the entire table was written, the return value will be 0.  Otherwise,
 *  it will be equal to the number of bytes left to write. */
typedef struct ZSTD_frameLog_s ZSTD_v1_5_7_frameLog;
ZSTDLIB_API ZSTD_v1_5_7_frameLog* ZSTD_v1_5_7_seekable_createFrameLog(int checksumFlag);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_freeFrameLog(ZSTD_v1_5_7_frameLog* fl);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_logFrame(ZSTD_v1_5_7_frameLog* fl, unsigned compressedSize, unsigned decompressedSize, unsigned checksum);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_writeSeekTable(ZSTD_v1_5_7_frameLog* fl, ZSTD_outBuffer* output);


/*-****************************************************************************
*  Seekable decompression - HowTo
*  A ZSTD_v1_5_7_seekable object is required to tracking the seekTable.
*
*  Call ZSTD_v1_5_7_seekable_init* to initialize a ZSTD_v1_5_7_seekable object with the
*  the seek table provided in the input.
*  There are three modes for ZSTD_v1_5_7_seekable_init:
*    - ZSTD_v1_5_7_seekable_initBuff() : An in-memory API.  The data contained in
*      `src` should be the entire seekable file, including the seek table.
*      `src` should be kept alive and unmodified until the ZSTD_v1_5_7_seekable object
*      is freed or reset.
*    - ZSTD_v1_5_7_seekable_initFile() : A simplified file API using stdio.  fread and
*      fseek will be used to access the required data for building the seek
*      table and doing decompression operations.  `src` should not be closed
*      or modified until the ZSTD_v1_5_7_seekable object is freed or reset.
*    - ZSTD_v1_5_7_seekable_initAdvanced() : A general API allowing the client to
*      provide its own read and seek callbacks.
*        + ZSTD_v1_5_7_seekable_read() : read exactly `n` bytes into `buffer`.
*                                 Premature EOF should be treated as an error.
*        + ZSTD_v1_5_7_seekable_seek() : seek the read head to `offset` from `origin`,
*                                 where origin is either SEEK_SET (beginning of
*                                 file), or SEEK_END (end of file).
*  Both functions should return a non-negative value in case of success, and a
*  negative value in case of failure.  If implementing using this API and
*  stdio, be careful with files larger than 4GB and fseek.  All of these
*  functions return an error code checkable with ZSTD_isError().
*
*  Call ZSTD_v1_5_7_seekable_decompress to decompress `dstSize` bytes at decompressed
*  offset `offset`.  ZSTD_v1_5_7_seekable_decompress may have to decompress the entire
*  prefix of the frame before the desired data if it has not already processed
*  this section. If ZSTD_v1_5_7_seekable_decompress is called multiple times for a
*  consecutive range of data, it will efficiently retain the decompressor object
*  and avoid redecompressing frame prefixes.  The return value is the number of
*  bytes decompressed, or an error code checkable with ZSTD_isError().
*
*  The seek table access functions can be used to obtain the data contained
*  in the seek table.  If frameIndex is larger than the value returned by
*  ZSTD_v1_5_7_seekable_getNumFrames(), they will return error codes checkable with
*  ZSTD_isError().  Note that since the offset access functions return
*  unsigned long long instead of size_t, in this case they will instead return
*  the value ZSTD_v1_5_7_seekable_FRAMEINDEX_TOOLARGE.
******************************************************************************/

/*===== Seekable decompressor management =====*/
ZSTDLIB_API ZSTD_v1_5_7_seekable* ZSTD_v1_5_7_seekable_create(void);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_free(ZSTD_v1_5_7_seekable* zs);

/*===== Seekable decompression functions =====*/
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_initBuff(ZSTD_v1_5_7_seekable* zs, const void* src, size_t srcSize);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_initFile(ZSTD_v1_5_7_seekable* zs, FILE* src);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_decompress(ZSTD_v1_5_7_seekable* zs, void* dst, size_t dstSize, unsigned long long offset);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_decompressFrame(ZSTD_v1_5_7_seekable* zs, void* dst, size_t dstSize, unsigned frameIndex);

#define ZSTD_v1_5_7_seekable_FRAMEINDEX_TOOLARGE (0ULL-2)
/*===== Seekable seek table access functions =====*/
ZSTDLIB_API unsigned ZSTD_v1_5_7_seekable_getNumFrames(const ZSTD_v1_5_7_seekable* zs);
ZSTDLIB_API unsigned long long ZSTD_v1_5_7_seekable_getFrameCompressedOffset(const ZSTD_v1_5_7_seekable* zs, unsigned frameIndex);
ZSTDLIB_API unsigned long long ZSTD_v1_5_7_seekable_getFrameDecompressedOffset(const ZSTD_v1_5_7_seekable* zs, unsigned frameIndex);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_getFrameCompressedSize(const ZSTD_v1_5_7_seekable* zs, unsigned frameIndex);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_getFrameDecompressedSize(const ZSTD_v1_5_7_seekable* zs, unsigned frameIndex);
ZSTDLIB_API unsigned ZSTD_v1_5_7_seekable_offsetToFrameIndex(const ZSTD_v1_5_7_seekable* zs, unsigned long long offset);


/*-****************************************************************************
*  Direct exploitation of the seekTable
*
*  Memory constrained use cases that manage multiple archives
*  benefit from retaining multiple archive seek tables
*  without retaining a ZSTD_v1_5_7_seekable instance for each.
*
*  Below API allow the above-mentioned use cases
*  to initialize a ZSTD_v1_5_7_seekable, extract its (smaller) ZSTD_v1_5_7_seekTable,
*  then throw the ZSTD_v1_5_7_seekable away to save memory.
*
*  Standard ZSTD operations can then be used
*  to decompress frames based on seek table offsets.
******************************************************************************/

/*===== Independent seek table management =====*/
ZSTDLIB_API ZSTD_v1_5_7_seekTable* ZSTD_v1_5_7_seekTable_create_fromSeekable(const ZSTD_v1_5_7_seekable* zs);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekTable_free(ZSTD_v1_5_7_seekTable* st);

/*===== Direct seek table access functions =====*/
ZSTDLIB_API unsigned ZSTD_v1_5_7_seekTable_getNumFrames(const ZSTD_v1_5_7_seekTable* st);
ZSTDLIB_API unsigned long long ZSTD_v1_5_7_seekTable_getFrameCompressedOffset(const ZSTD_v1_5_7_seekTable* st, unsigned frameIndex);
ZSTDLIB_API unsigned long long ZSTD_v1_5_7_seekTable_getFrameDecompressedOffset(const ZSTD_v1_5_7_seekTable* st, unsigned frameIndex);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekTable_getFrameCompressedSize(const ZSTD_v1_5_7_seekTable* st, unsigned frameIndex);
ZSTDLIB_API size_t ZSTD_v1_5_7_seekTable_getFrameDecompressedSize(const ZSTD_v1_5_7_seekTable* st, unsigned frameIndex);
ZSTDLIB_API unsigned ZSTD_v1_5_7_seekTable_offsetToFrameIndex(const ZSTD_v1_5_7_seekTable* st, unsigned long long offset);


/*===== Seekable advanced I/O API =====*/
typedef int(ZSTD_v1_5_7_seekable_read)(void* opaque, void* buffer, size_t n);
typedef int(ZSTD_v1_5_7_seekable_seek)(void* opaque, long long offset, int origin);
typedef struct {
    void* opaque;
    ZSTD_v1_5_7_seekable_read* read;
    ZSTD_v1_5_7_seekable_seek* seek;
} ZSTD_v1_5_7_seekable_customFile;
ZSTDLIB_API size_t ZSTD_v1_5_7_seekable_initAdvanced(ZSTD_v1_5_7_seekable* zs, ZSTD_v1_5_7_seekable_customFile src);

#if defined (__cplusplus)
}
#endif

#endif
