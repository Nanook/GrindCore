#ifndef ZIP7_INC_S7_LZMA2_V24_07_DEC_H
#define ZIP7_INC_S7_LZMA2_V24_07_DEC_H

#ifdef _WIN32
    #include <stdint.h>
    #include <windows.h>
    #define FUNCTIONEXPORT
    #define FUNCTIONCALLINGCONVENCTION WINAPI
#else
    #include "pal_types.h"
    #include "pal_compiler.h"
    #define FUNCTIONEXPORT PALEXPORT
    #define FUNCTIONCALLINGCONVENCTION
#endif

#include "external/mcmilk/C/LzmaEnc.h"
#include "external/mcmilk/C/LzmaDec.h"
#include "external/mcmilk/C/Lzma2Enc.h"
#include "external/mcmilk/C/Lzma2Dec.h"

EXTERN_C_BEGIN

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_Construct(CLzma2Dec *p);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_FreeProbs(CLzma2Dec *p);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_Free(CLzma2Dec *p);
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_AllocateProbs(CLzma2Dec *p, uint8_t prop);
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_Allocate(CLzma2Dec *p, uint8_t prop);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_Init(CLzma2Dec *p);

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_DecodeToDic(CLzma2Dec *p, size_t dicLimit,
    const uint8_t *src, size_t *srcLen, int32_t finishMode, int32_t *status);

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_DecodeToBuf(CLzma2Dec *p, uint8_t *dest, size_t *destLen,
    const uint8_t *src, size_t *srcLen, int32_t finishMode, int32_t *status);
    
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Dec_Parse(CLzma2Dec *p, size_t outSize, 
    const uint8_t *src, size_t *srcLen, int checkFinishBlock);

#define S7_Lzma2_v24_07_Dec_GetUnpackExtra(p)  ((p)->isExtraMode ? (p)->unpackSize : 0)

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Decode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t *srcLen,
    uint8_t prop, int32_t finishMode, int32_t *status);


FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_Construct(CLzma2EncProps *p);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_Normalize(CLzma2EncProps *p);
FUNCTIONEXPORT CLzma2EncHandle FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_Create(void);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_Destroy(CLzma2EncHandle p);
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_SetProps(CLzma2EncHandle p, const CLzma2EncProps *props);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_SetDataSize(CLzma2EncHandle p, UInt64 expectedDataSize);
FUNCTIONEXPORT Byte FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_WriteProperties(CLzma2EncHandle p);
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma2_v24_07_Enc_Encode2(CLzma2EncHandle p,
    uint8_t *outBuf,
    size_t *outBufSize,
    const uint8_t *inData,
    size_t inDataSize,
    ICompressProgressPtr progress);

    
/* ---------- CLzmaDecHandle Interface ---------- */

/* Constructs the LZMA decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_Construct(CLzmaDec *p);

/* Initializes the LZMA decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_Init(CLzmaDec *p);

/* Allocates memory for LZMA decoder properties. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_AllocateProbs(CLzmaDec *p, const uint8_t *props, uint32_t propsSize);

/* Frees memory for LZMA decoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_FreeProbs(CLzmaDec *p);

/* Allocates memory for the LZMA decoder. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_Allocate(CLzmaDec *p, const uint8_t *props, uint32_t propsSize);

/* Frees memory for the LZMA decoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_Free(CLzmaDec *p);

/* Decodes data to the internal dictionary buffer. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_DecodeToDic(CLzmaDec *p, size_t dicLimit,
    const uint8_t *src, size_t *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status);

/* Decodes data to a buffer. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_DecodeToBuf(CLzmaDec *p, uint8_t *dest, size_t *destLen,
    const uint8_t *src, size_t *srcLen, ELzmaFinishMode finishMode, ELzmaStatus *status);

/* Decodes data in one call. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Dec_LzmaDecode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t *srcLen,
    const uint8_t *propData, uint32_t propSize, ELzmaFinishMode finishMode, ELzmaStatus *status);

/* ---------- CLzmaEncHandle Interface ---------- */

/* Allocates memory for LZMA encoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_EncProps_Init(CLzmaEncProps *p);

/* Normalizes the LZMA encoder properties. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_EncProps_Normalize(CLzmaEncProps *p);

/* Gets the dictionary size from the LZMA encoder properties. */
FUNCTIONEXPORT uint32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_EncProps_GetDictSize(const CLzmaEncProps *props2);

/* Creates a new LZMA encoder handle. */
FUNCTIONEXPORT CLzmaEncHandle FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_Create(void);

/* Destroys the LZMA encoder handle. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_Destroy(CLzmaEncHandle p);

/* Sets properties for the LZMA encoder. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_SetProps(CLzmaEncHandle p, const CLzmaEncProps *props);

/* Sets the expected data size for the LZMA encoder. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_SetDataSize(CLzmaEncHandle p, uint64_t expectedDataSize);

/* Writes properties of the LZMA encoder to a buffer. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_WriteProperties(CLzmaEncHandle p, uint8_t *properties, size_t *size);

/* Checks if the LZMA encoder should write an end mark. */
FUNCTIONEXPORT unsigned FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_IsWriteEndMark(CLzmaEncHandle p);

/* Encodes data using the LZMA encoder with a streaming interface. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_Encode(CLzmaEncHandle p, ISeqOutStreamPtr outStream, ISeqInStreamPtr inStream,
    ICompressProgressPtr progress);

/* Encodes data using the LZMA encoder with a memory interface. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_MemEncode(CLzmaEncHandle p, uint8_t *dest, size_t *destLen, const uint8_t *src, size_t srcLen,
    int writeEndMark, ICompressProgressPtr progress);

/* Encodes data using the LZMA encoder in one call. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_LzmaEncode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t srcLen,
    const CLzmaEncProps *props, uint8_t *propsEncoded, size_t *propsSize, int writeEndMark, ICompressProgressPtr progress);

// Nanook
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION S7_Lzma_v24_07_Enc_LzmaCodeOneMemBlock(CLzmaEncHandle p, uint32_t reInit, uint8_t *dest, size_t *destLen, const uint8_t *src, size_t srcLen, uint32_t desiredPackSize, uint32_t *unpackSize);

EXTERN_C_END

#endif
