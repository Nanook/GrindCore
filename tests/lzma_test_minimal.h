#ifndef LZMA_TEST_MINIMAL_H
#define LZMA_TEST_MINIMAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Only the minimal types and function prototypes needed for the test

typedef unsigned int UInt32;
typedef int Int32;
typedef unsigned long long UInt64;

typedef void* CLzmaEncHandle;

typedef struct {
    int level;       /* 0 <= level <= 9 */
    UInt32 dictSize; /* (1 << 12) <= dictSize <= (1 << 27) for 32-bit version
                                            (1 << 12) <= dictSize <= (3 << 29) for 64-bit version
                                            default = (1 << 24) */
    int lc;          /* 0 <= lc <= 8, default = 3 */
    int lp;          /* 0 <= lp <= 4, default = 0 */
    int pb;          /* 0 <= pb <= 4, default = 2 */
    int algo;        /* 0 - fast, 1 - normal, default = 1 */
    int fb;          /* 5 <= fb <= 273, default = 32 */
    int btMode;      /* 0 - hashChain Mode, 1 - binTree mode - normal, default = 1 */
    int numHashBytes; /* 2, 3 or 4, default = 4 */
    unsigned numHashOutBits;  /* default = ? */
    UInt32 mc;       /* 1 <= mc <= (1 << 30), default = 32 */
    unsigned writeEndMark;  /* 0 - do not write EOPM, 1 - write EOPM, default = 0 */
    int numThreads;  /* 1 or 2, default = 2 */
    Int32 affinityGroup;
    UInt64 reduceSize; /* estimated size of data that will be compressed. default = (UInt64)(Int64)-1.
                                                Encoder uses this value to reduce dictionary size */
    UInt64 affinity;
    UInt64 affinityInGroup;
} CLzmaEncProps;
typedef struct CLzmaDec CLzmaDec;

typedef struct {
    void* vt;
    uint8_t* buffer;
    uint64_t size;
    uint64_t pos;
    uint64_t remaining;
    uint64_t processed;
    int finished;
    int count;
    size_t lastSize;
} CBufferInStream;

// LZMA encode/decode API
void SZ_Lzma_v25_01_EncProps_Init(CLzmaEncProps *p);
void SZ_Lzma_v25_01_EncProps_Normalize(CLzmaEncProps *p);
int32_t SZ_Lzma_v25_01_Enc_LzmaEncode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t srcLen,
    const CLzmaEncProps *props, uint8_t *propsEncoded, size_t *propsSize, int writeEndMark, void *progress);
int32_t SZ_Lzma_v25_01_Dec_LzmaDecode(uint8_t *dest, size_t *destLen, const uint8_t *src, size_t *srcLen,
    const uint8_t *propData, unsigned propSize, int finishMode, int *status);

// Block encoding and encoder handle API
CLzmaEncHandle SZ_Lzma_v25_01_Enc_Create(void);
void SZ_Lzma_v25_01_Enc_Destroy(CLzmaEncHandle p);
int32_t SZ_Lzma_v25_01_Enc_SetProps(CLzmaEncHandle p, const CLzmaEncProps *props);
int32_t SZ_Lzma_v25_01_Enc_LzmaCodeMultiCallPrepare(CLzmaEncHandle p, UInt32 *blockSize, UInt32 *dictSize, UInt32 final);
int32_t SZ_Lzma_v25_01_Enc_LzmaCodeMultiCall(CLzmaEncHandle p, uint8_t *dest, size_t *destLen, void *srcStream, int32_t limit, UInt32* availableBytes, UInt32 final);

// One-call memory encode API
void SZ_Lzma_v25_01_Enc_SetDataSize(CLzmaEncHandle p, UInt64 expectedDataSize);
int32_t SZ_Lzma_v25_01_Enc_WriteProperties(CLzmaEncHandle p, uint8_t *properties, size_t *size);
int32_t SZ_Lzma_v25_01_Enc_MemEncode(CLzmaEncHandle p, uint8_t *dest, size_t *destLen, const uint8_t *src, size_t srcLen,
    int writeEndMark, void *progress);
void SZ_Lzma_v25_01_Enc_SetDataSize(CLzmaEncHandle p, UInt64 expectedDataSize);
int32_t SZ_Lzma_v25_01_Enc_WriteProperties(CLzmaEncHandle p, uint8_t *properties, size_t *size);
int32_t SZ_Lzma_v25_01_Enc_MemEncode(CLzmaEncHandle p, uint8_t *dest, size_t *destLen, const uint8_t *src, size_t srcLen,
    int writeEndMark, void *progress);

#ifdef __cplusplus
}
#endif

#endif // LZMA_TEST_MINIMAL_H
