#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <../../sevenzip/lzma/Alloc.h>

#ifdef __linux__
#include <pthread.h>
#ifndef USE_posix_memalign
#include <unistd.h>
#else
#include <stdlib.h>
#endif
#endif

#include "pal_sevenzip_lzma_v25_01.h"

// Thread-local allocators for better multithreading support on Linux
#ifdef __linux__

#ifndef UNUSED_VAR
#define UNUSED_VAR(x) (void)(x)
#endif

// Direct allocator functions that bypass any potential global allocator issues
static void *ThreadLocal_AlignedAlloc(ISzAllocPtr pp, size_t size)
{
    UNUSED_VAR(pp);
    // Use direct system call to avoid any potential global allocator issues
    #ifndef USE_posix_memalign
    void *p;
    void **p2;
    size_t newSize;
    if (size == 0)
        return NULL;
    newSize = size + sizeof(void *) + (16 - 1);
    if (newSize < size)
        return NULL;
    p = malloc(newSize);
    if (!p)
        return NULL;
    p2 = (void **)(((ptrdiff_t)((char *)p + sizeof(void *) + (16 - 1))) & ~(ptrdiff_t)(16 - 1));
    p2[-1] = p;
    return p2;
    #else
    void *p;
    if (size == 0)
        return NULL;
    if (posix_memalign(&p, 16, size))
        return NULL;
    return p;
    #endif
}

static void ThreadLocal_AlignedFree(ISzAllocPtr pp, void *address)
{
    UNUSED_VAR(pp);
    #ifndef USE_posix_memalign
    if (address)
        free(((void **)address)[-1]);
    #else
    free(address);
    #endif
}

static __thread ISzAlloc t_AlignedAlloc = { 0 };
static __thread ISzAlloc t_BigAlloc = { 0 };
static __thread int t_allocators_initialized = 0;

static void init_thread_allocators(void)
{
    if (!t_allocators_initialized)
    {
        t_AlignedAlloc.Alloc = ThreadLocal_AlignedAlloc;
        t_AlignedAlloc.Free = ThreadLocal_AlignedFree;
        t_BigAlloc.Alloc = ThreadLocal_AlignedAlloc; 
        t_BigAlloc.Free = ThreadLocal_AlignedFree;
        t_allocators_initialized = 1;
    }
}

#define GET_ALIGNED_ALLOC() (init_thread_allocators(), &t_AlignedAlloc)
#define GET_BIG_ALLOC() (init_thread_allocators(), &t_BigAlloc)
#else
#define GET_ALIGNED_ALLOC() (&g_AlignedAlloc)
#define GET_BIG_ALLOC() (&g_BigAlloc)
#endif

SRes LzmaEnc_LzmaCodeMultiCallPrepare(CLzmaEncHandle p, UInt32 *blockSize, UInt32 *dictSize, ISzAllocPtr alloc, ISzAllocPtr allocBig);
SRes LzmaEnc_LzmaCodeMultiCall(CLzmaEncHandle p, Byte *dest, size_t *destLen, ISeqInStreamPtr inStream, UInt32 limit, UInt64 pos, UInt32* availableBytes, BoolInt final);
SRes Lzma2Enc_EncodeMultiCallPrepare(CLzma2EncHandle p);
SRes Lzma2Enc_EncodeMultiCall(CLzma2EncHandle p, Byte *outBuf, size_t *outBufSize, ISeqInStreamPtr inStream, BoolInt init, BoolInt final);
SRes Lzma2Enc_EncodeMultiCallFinalize(CLzma2EncHandle p, Byte *outBuf, size_t *outBufSize);

/* Constructs the LZMA2 decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Dec_Construct(CLzma2Dec *p)
{
    Lzma2Dec_CONSTRUCT(p);
}

/* Frees memory for LZMA2 decoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Dec_FreeProbs(CLzma2Dec *p)
{
    Lzma2Dec_FreeProbs(p, GET_ALIGNED_ALLOC());
}

/* Frees memory for the LZMA2 decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Dec_Free(CLzma2Dec *p)
{
    Lzma2Dec_Free(p, GET_ALIGNED_ALLOC());
}

/* Allocates LZMA2 probabilities. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Dec_AllocateProbs(CLzma2Dec *p, uint8_t prop)
{
    return Lzma2Dec_AllocateProbs(p, prop, GET_ALIGNED_ALLOC());
}

/* Allocates memory for LZMA2 decoder. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Dec_Allocate(CLzma2Dec *p, uint8_t prop)
{
    return Lzma2Dec_Allocate(p, prop, GET_ALIGNED_ALLOC());
}

/* Initializes the LZMA2 decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Dec_Init(CLzma2Dec *p)
{
    Lzma2Dec_Init(p);
}

/* Decodes LZMA2 data to the dictionary. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Dec_DecodeToDic(CLzma2Dec *p, size_t dicLimit,
    const uint8_t *src, size_t *srcLen, int32_t finishMode, int32_t *status)
{
    return Lzma2Dec_DecodeToDic(p, dicLimit, src, srcLen, finishMode, (ELzmaStatus *)status);
}

/* Decodes LZMA2 data to the buffer. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Dec_DecodeToBuf(CLzma2Dec *p, uint8_t *dest, size_t *destLen,
    const uint8_t *src, size_t *srcLen, int32_t finishMode, int32_t *status)
{
    return Lzma2Dec_DecodeToBuf(p, dest, destLen, src, srcLen, finishMode, (ELzmaStatus *)status);
}

/* Parses compressed data stream up to the next independent block or next chunk data. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Dec_Parse(CLzma2Dec *p, size_t outSize, 
    const uint8_t *src, size_t *srcLen, int checkFinishBlock)
{
    return Lzma2Dec_Parse(p, outSize, src, srcLen, checkFinishBlock);
}

/* Decodes LZMA2 data in one call. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Decode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t *srcLen,
    uint8_t prop, int32_t finishMode, int32_t *status)
{
    return Lzma2Decode(dest, destLen, src, srcLen, prop, finishMode, (ELzmaStatus *)status, GET_ALIGNED_ALLOC());
}


/* Constructs the LZMA2 encoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Enc_Construct(CLzma2EncProps *p)
{
    Lzma2EncProps_Init(p);
}

/* Normalizes the LZMA2 encoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Enc_Normalize(CLzma2EncProps *p)
{
    Lzma2EncProps_Normalize(p);
}

/* Creates a new LZMA2 encoder handle. */
FUNCTIONEXPORT CLzma2EncHandle FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Enc_Create(void)
{
    return Lzma2Enc_Create(GET_ALIGNED_ALLOC(), GET_BIG_ALLOC());
}

/* Destroys the LZMA2 encoder handle. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Enc_Destroy(CLzma2EncHandle p)
{
    Lzma2Enc_Destroy(p);
}

/* Sets properties for the LZMA2 encoder. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Enc_SetProps(CLzma2EncHandle p, const CLzma2EncProps *props)
{
    // Lzma2Enc_SetProps assigns the passed CLzma2EncProps to CLzma2Enc.
    // Copy the data as C# will remove the original from the stack, this removes any C# requirement for pinning etc
    CLzma2EncProps newProps;
    memcpy(&newProps, props, sizeof(CLzma2EncProps));
    return Lzma2Enc_SetProps(p, &newProps);
}

/* Sets expected data size for the LZMA2 encoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Enc_SetDataSize(CLzma2EncHandle p, UInt64 expectedDataSize)
{
    Lzma2Enc_SetDataSize(p, expectedDataSize);
}

/* Writes properties of the LZMA2 encoder. */
FUNCTIONEXPORT uint8_t FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Enc_WriteProperties(CLzma2EncHandle p)
{
    return Lzma2Enc_WriteProperties(p);
}

/* Encodes data using the LZMA2 encoder. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Enc_Encode2(CLzma2EncHandle p,
    uint8_t *outBuf,
    size_t *outBufSize,
    const uint8_t *inData,
    size_t inDataSize,
    ICompressProgressPtr progress)
{
    return Lzma2Enc_Encode2(p, 0, outBuf, outBufSize, 0, inData, inDataSize, progress);
}



/* ---------- CLzmaDecHandle Interface ---------- */

/* Constructs the LZMA decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Dec_Construct(CLzmaDec *p)
{
    LzmaDec_Construct(p);
}

/* Initializes the LZMA decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Dec_Init(CLzmaDec *p)
{
    LzmaDec_Init(p);
}

/* Allocates memory for LZMA decoder properties. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Dec_AllocateProbs(CLzmaDec *p, const uint8_t *props, uint32_t propsSize)
{
    return LzmaDec_AllocateProbs(p, props, propsSize, GET_ALIGNED_ALLOC());
}

/* Frees memory for LZMA decoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Dec_FreeProbs(CLzmaDec *p)
{
    LzmaDec_FreeProbs(p, GET_ALIGNED_ALLOC());
}

/* Allocates memory for the LZMA decoder. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Dec_Allocate(CLzmaDec *p, const uint8_t *props, uint32_t propsSize)
{
    return LzmaDec_Allocate(p, props, propsSize, GET_ALIGNED_ALLOC());
}

/* Frees memory for the LZMA decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Dec_Free(CLzmaDec *p)
{
    LzmaDec_Free(p, GET_ALIGNED_ALLOC());
}

/* Decodes data to the internal dictionary buffer. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Dec_DecodeToDic(CLzmaDec *p, size_t dicLimit,
    const uint8_t *src, size_t *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status)
{
    return LzmaDec_DecodeToDic(p, dicLimit, src, srcLen, finishMode, status);
}

/* Decodes data to a buffer. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Dec_DecodeToBuf(CLzmaDec *p, uint8_t *dest, size_t *destLen,
    const uint8_t *src, size_t *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status)
{
    return LzmaDec_DecodeToBuf(p, dest, destLen, src, srcLen, finishMode, status);
}

/* Decodes data in one call. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Dec_LzmaDecode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t *srcLen,
    const uint8_t *propData, unsigned propSize, ELzmaFinishMode finishMode, ELzmaStatus *status)
{
    return LzmaDecode(dest, destLen, src, srcLen, propData, propSize, finishMode, status, GET_ALIGNED_ALLOC());
}



/* ---------- CLzmaEncHandle Interface ---------- */

/* Allocates memory for LZMA encoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_EncProps_Init(CLzmaEncProps *p)
{
    LzmaEncProps_Init(p);
}

/* Normalizes the LZMA encoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_EncProps_Normalize(CLzmaEncProps *p)
{
    LzmaEncProps_Normalize(p);
}

/* Gets the dictionary size from the LZMA encoder properties. */
FUNCTIONEXPORT uint32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_EncProps_GetDictSize(const CLzmaEncProps *props2)
{
    return LzmaEncProps_GetDictSize(props2);
}

/* Creates a new LZMA encoder handle. */
FUNCTIONEXPORT CLzmaEncHandle FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Enc_Create(void)
{
    return LzmaEnc_Create(GET_ALIGNED_ALLOC());
}

/* Destroys the LZMA encoder handle. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Enc_Destroy(CLzmaEncHandle p)
{
    LzmaEnc_Destroy(p, GET_ALIGNED_ALLOC(), GET_BIG_ALLOC());
}

/* Sets properties for the LZMA encoder. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Enc_SetProps(CLzmaEncHandle p, const CLzmaEncProps *props)
{
    return LzmaEnc_SetProps(p, props);
}

/* Sets the expected data size for the LZMA encoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Enc_SetDataSize(CLzmaEncHandle p, uint64_t expectedDataSize)
{
    LzmaEnc_SetDataSize(p, expectedDataSize);
}

/* Writes properties of the LZMA encoder to a buffer. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Enc_WriteProperties(CLzmaEncHandle p, uint8_t *properties, size_t *size)
{
    return LzmaEnc_WriteProperties(p, properties, size);
}

/* Checks if the LZMA encoder should write an end mark. */
FUNCTIONEXPORT unsigned FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Enc_IsWriteEndMark(CLzmaEncHandle p)
{
    return LzmaEnc_IsWriteEndMark(p);
}

/* Encodes data using the LZMA encoder with a streaming interface. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Enc_Encode(CLzmaEncHandle p, ISeqOutStreamPtr outStream, ISeqInStreamPtr inStream,
    ICompressProgressPtr progress)
{
    return LzmaEnc_Encode(p, outStream, inStream, progress, GET_ALIGNED_ALLOC(), GET_BIG_ALLOC());
}

/* Encodes data using the LZMA encoder with a memory interface. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Enc_MemEncode(CLzmaEncHandle p, uint8_t *dest, size_t *destLen, const uint8_t *src, size_t srcLen,
    int writeEndMark, ICompressProgressPtr progress)
{
    return LzmaEnc_MemEncode(p, dest, destLen, src, srcLen, writeEndMark, progress, GET_ALIGNED_ALLOC(), GET_BIG_ALLOC());
}

/* Encodes data using the LZMA encoder in one call. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Enc_LzmaEncode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t srcLen,
    const CLzmaEncProps *props, uint8_t *propsEncoded, size_t *propsSize, int writeEndMark, ICompressProgressPtr progress)
{
    return LzmaEncode(dest, destLen, src, srcLen, props, propsEncoded, propsSize, writeEndMark, progress, GET_ALIGNED_ALLOC(), GET_BIG_ALLOC());
}

// Nanook - circular buffer. Assumes that the buffer is > blocksize (even by just 1 byte)
static SRes BufferInStream_Read(ISeqInStreamPtr pp, void* data, size_t* size)
{
    Z7_CONTAINER_FROM_VTBL_TO_DECL_VAR_pp_vt_p(CBufferInStream)
    p->count++;
    p->lastSize = *size;
    size_t size2 = *size;

    if (p->remaining == 0)
    {
        *size = 0;
        p->finished = 1;
        return SZ_OK;
    }

    if (size2 > p->remaining)
        size2 = (size_t)p->remaining;

    size_t firstChunk = 0;
    if (p->pos + size2 >= p->size)
    {
        firstChunk = (size_t)(p->size - p->pos); // Bytes until the end of the buffer
        memcpy(data, (Byte*)(p->buffer + p->pos), firstChunk);
        p->pos = 0; // reset
    }
    if (size2 - firstChunk != 0)
    {
        memcpy((Byte*)data + firstChunk, (Byte*)(p->buffer + p->pos), size2 - firstChunk);
        p->pos += size2 - firstChunk;
    }

    p->remaining -= size2;
    p->processed += size2;

    *size = size2;

    return SZ_OK;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Enc_LzmaCodeMultiCallPrepare(CLzmaEncHandle p, UInt32 *blockSize, UInt32 *dictSize, uint32_t final)
{
    return LzmaEnc_LzmaCodeMultiCallPrepare(p, blockSize, dictSize, GET_ALIGNED_ALLOC(), GET_BIG_ALLOC());
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma_v25_01_Enc_LzmaCodeMultiCall(CLzmaEncHandle p, uint8_t *dest, size_t *destLen, CBufferInStream *srcStream, int32_t limit, uint32_t* availableBytes, uint32_t final)
{
    srcStream->vt.Read = BufferInStream_Read;
    return LzmaEnc_LzmaCodeMultiCall(p, dest, destLen, &srcStream->vt, limit, srcStream->pos, availableBytes, final);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Enc_EncodeMultiCallPrepare(CLzma2EncHandle p)
{
    return Lzma2Enc_EncodeMultiCallPrepare(p);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Enc_EncodeMultiCall(CLzma2EncHandle p, uint8_t *outBuf, size_t *outBufSize, CBufferInStream *srcStream, uint32_t init, uint32_t final)
{
    srcStream->vt.Read = BufferInStream_Read;
    return Lzma2Enc_EncodeMultiCall(p, outBuf, outBufSize, &srcStream->vt, init, final);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lzma2_v25_01_Enc_EncodeMultiCallFinalize(CLzma2EncHandle p, uint8_t *outBuf, size_t *outBufSize)
{
    return Lzma2Enc_EncodeMultiCallFinalize(p, outBuf, outBufSize);
}
