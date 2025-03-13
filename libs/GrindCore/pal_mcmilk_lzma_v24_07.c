#include <assert.h>
#include <stdlib.h>
#include <../../C/Alloc.h>

#include "pal_mcmilk_lzma_v24_07.h"

SRes LzmaEnc_MemPrepare(CLzmaEncHandle p, const Byte *src, SizeT srcLen,
    UInt32 keepWindowSize, ISzAllocPtr alloc, ISzAllocPtr allocBig);
SRes LzmaEnc_CodeOneMemBlock(CLzmaEncHandle p, BoolInt reInit,
    Byte *dest, size_t *destLen, UInt32 desiredPackSize, UInt32 *unpackSize);

/* Constructs the LZMA2 decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_Construct(CLzma2Dec *p)
{
    Lzma2Dec_CONSTRUCT(p);
}

/* Frees memory for LZMA2 decoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_FreeProbs(CLzma2Dec *p)
{
    Lzma2Dec_FreeProbs(p, &g_AlignedAlloc);
}

/* Frees memory for the LZMA2 decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_Free(CLzma2Dec *p)
{
    Lzma2Dec_Free(p, &g_AlignedAlloc);
}

/* Allocates LZMA2 probabilities. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_AllocateProbs(CLzma2Dec *p, uint8_t prop)
{
    return Lzma2Dec_AllocateProbs(p, prop, &g_AlignedAlloc);
}

/* Allocates memory for LZMA2 decoder. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_Allocate(CLzma2Dec *p, uint8_t prop)
{
    return Lzma2Dec_Allocate(p, prop, &g_AlignedAlloc);
}

/* Initializes the LZMA2 decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_Init(CLzma2Dec *p)
{
    Lzma2Dec_Init(p);
}

/* Decodes LZMA2 data to the dictionary. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_DecodeToDic(CLzma2Dec *p, size_t dicLimit,
    const uint8_t *src, size_t *srcLen, int32_t finishMode, int32_t *status)
{
    return Lzma2Dec_DecodeToDic(p, dicLimit, src, srcLen, finishMode, (ELzmaStatus *)status);
}

/* Decodes LZMA2 data to the buffer. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_DecodeToBuf(CLzma2Dec *p, uint8_t *dest, size_t *destLen,
    const uint8_t *src, size_t *srcLen, int32_t finishMode, int32_t *status)
{
    return Lzma2Dec_DecodeToBuf(p, dest, destLen, src, srcLen, finishMode, (ELzmaStatus *)status);
}

/* Parses compressed data stream up to the next independent block or next chunk data. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_Parse(CLzma2Dec *p, size_t outSize, 
    const uint8_t *src, size_t *srcLen, int checkFinishBlock)
{
    return Lzma2Dec_Parse(p, outSize, src, srcLen, checkFinishBlock);
}

/* Decodes LZMA2 data in one call. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Decode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t *srcLen,
    uint8_t prop, int32_t finishMode, int32_t *status)
{
    return Lzma2Decode(dest, destLen, src, srcLen, prop, finishMode, (ELzmaStatus *)status, &g_AlignedAlloc);
}


/* Constructs the LZMA2 encoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_Construct(CLzma2EncProps *p)
{
    Lzma2EncProps_Init(p);
}

/* Normalizes the LZMA2 encoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_Normalize(CLzma2EncProps *p)
{
    Lzma2EncProps_Normalize(p);
}

/* Creates a new LZMA2 encoder handle. */
FUNCTIONEXPORT CLzma2EncHandle FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_Create(void)
{
    return Lzma2Enc_Create(&g_AlignedAlloc, &g_BigAlloc);
}

/* Destroys the LZMA2 encoder handle. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_Destroy(CLzma2EncHandle p)
{
    Lzma2Enc_Destroy(p);
}

/* Sets properties for the LZMA2 encoder. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_SetProps(CLzma2EncHandle p, const CLzma2EncProps *props)
{
    // Lzma2Enc_SetProps assigns the passed CLzma2EncProps to CLzma2Enc.
    // Copy the data as C# will remove the original from the stack, this removes any C# requirement for pinning etc
    CLzma2EncProps newProps;
    memcpy(&newProps, props, sizeof(CLzma2EncProps));
    return Lzma2Enc_SetProps(p, &newProps);
}

/* Sets expected data size for the LZMA2 encoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_SetDataSize(CLzma2EncHandle p, UInt64 expectedDataSize)
{
    Lzma2Enc_SetDataSize(p, expectedDataSize);
}

/* Writes properties of the LZMA2 encoder. */
FUNCTIONEXPORT uint8_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_WriteProperties(CLzma2EncHandle p)
{
    return Lzma2Enc_WriteProperties(p);
}

/* Encodes data using the LZMA2 encoder. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_Encode2(CLzma2EncHandle p,
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
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_Construct(CLzmaDec *p)
{
    LzmaDec_Construct(p);
}

/* Initializes the LZMA decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_Init(CLzmaDec *p)
{
    LzmaDec_Init(p);
}

/* Allocates memory for LZMA decoder properties. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_AllocateProbs(CLzmaDec *p, const uint8_t *props, uint32_t propsSize)
{
    return LzmaDec_AllocateProbs(p, props, propsSize, &g_AlignedAlloc);
}

/* Frees memory for LZMA decoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_FreeProbs(CLzmaDec *p)
{
    LzmaDec_FreeProbs(p, &g_AlignedAlloc);
}

/* Allocates memory for the LZMA decoder. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_Allocate(CLzmaDec *p, const uint8_t *props, uint32_t propsSize)
{
    return LzmaDec_Allocate(p, props, propsSize, &g_AlignedAlloc);
}

/* Frees memory for the LZMA decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_Free(CLzmaDec *p)
{
    LzmaDec_Free(p, &g_AlignedAlloc);
}

/* Decodes data to the internal dictionary buffer. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_DecodeToDic(CLzmaDec *p, size_t dicLimit,
    const uint8_t *src, size_t *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status)
{
    return LzmaDec_DecodeToDic(p, dicLimit, src, srcLen, finishMode, status);
}

/* Decodes data to a buffer. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_DecodeToBuf(CLzmaDec *p, uint8_t *dest, size_t *destLen,
    const uint8_t *src, size_t *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status)
{
    return LzmaDec_DecodeToBuf(p, dest, destLen, src, srcLen, finishMode, status);
}

/* Decodes data in one call. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_LzmaDecode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t *srcLen,
    const uint8_t *propData, unsigned propSize, ELzmaFinishMode finishMode, ELzmaStatus *status)
{
    return LzmaDecode(dest, destLen, src, srcLen, propData, propSize, finishMode, status, &g_AlignedAlloc);
}



/* ---------- CLzmaEncHandle Interface ---------- */

/* Allocates memory for LZMA encoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_EncProps_Init(CLzmaEncProps *p)
{
    LzmaEncProps_Init(p);
}

/* Normalizes the LZMA encoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_EncProps_Normalize(CLzmaEncProps *p)
{
    LzmaEncProps_Normalize(p);
}

/* Gets the dictionary size from the LZMA encoder properties. */
FUNCTIONEXPORT uint32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_EncProps_GetDictSize(const CLzmaEncProps *props2)
{
    return LzmaEncProps_GetDictSize(props2);
}

/* Creates a new LZMA encoder handle. */
FUNCTIONEXPORT CLzmaEncHandle FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_Create(void)
{
    return LzmaEnc_Create(&g_AlignedAlloc);
}

/* Destroys the LZMA encoder handle. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_Destroy(CLzmaEncHandle p)
{
    LzmaEnc_Destroy(p, &g_AlignedAlloc, &g_BigAlloc);
}

/* Sets properties for the LZMA encoder. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_SetProps(CLzmaEncHandle p, const CLzmaEncProps *props)
{
    return LzmaEnc_SetProps(p, props);
}

/* Sets the expected data size for the LZMA encoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_SetDataSize(CLzmaEncHandle p, uint64_t expectedDataSize)
{
    LzmaEnc_SetDataSize(p, expectedDataSize);
}

/* Writes properties of the LZMA encoder to a buffer. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_WriteProperties(CLzmaEncHandle p, uint8_t *properties, size_t *size)
{
    return LzmaEnc_WriteProperties(p, properties, size);
}

/* Checks if the LZMA encoder should write an end mark. */
FUNCTIONEXPORT unsigned FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_IsWriteEndMark(CLzmaEncHandle p)
{
    return LzmaEnc_IsWriteEndMark(p);
}

/* Encodes data using the LZMA encoder with a streaming interface. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_Encode(CLzmaEncHandle p, ISeqOutStreamPtr outStream, ISeqInStreamPtr inStream,
    ICompressProgressPtr progress)
{
    return LzmaEnc_Encode(p, outStream, inStream, progress, &g_AlignedAlloc, &g_BigAlloc);
}

/* Encodes data using the LZMA encoder with a memory interface. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_MemEncode(CLzmaEncHandle p, uint8_t *dest, size_t *destLen, const uint8_t *src, size_t srcLen,
    int writeEndMark, ICompressProgressPtr progress)
{
    return LzmaEnc_MemEncode(p, dest, destLen, src, srcLen, writeEndMark, progress, &g_AlignedAlloc, &g_BigAlloc);
}

/* Encodes data using the LZMA encoder in one call. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_LzmaEncode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t srcLen,
    const CLzmaEncProps *props, uint8_t *propsEncoded, size_t *propsSize, int writeEndMark, ICompressProgressPtr progress)
{
    return LzmaEncode(dest, destLen, src, srcLen, props, propsEncoded, propsSize, writeEndMark, progress, &g_AlignedAlloc, &g_BigAlloc);
}

// Nanook
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_LzmaCodeOneMemBlock(CLzmaEncHandle p, uint32_t reInit, uint8_t *dest, size_t *destLen, const uint8_t *src, size_t srcLen, uint32_t desiredPackSize, uint32_t *unpackSize)
{
    SRes res = LzmaEnc_MemPrepare(p, src, srcLen, 0, &g_AlignedAlloc, &g_BigAlloc);

    SRes res2 = LzmaEnc_CodeOneMemBlock(p, reInit, dest, destLen, desiredPackSize, unpackSize);

    return res2;
}
