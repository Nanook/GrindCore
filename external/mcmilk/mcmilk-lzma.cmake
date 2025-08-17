# https://github.com/kingsznhone/FastLZMA2Net

# Conditional logic
set(LZMA_C_SOURCES_BASE
    7zStream.c
    Alloc.c
    Bra.c
    CpuArch.c
    Delta.c
    LzFind.c
    LzFindMt.c
    Lzma2Dec.c
    Lzma2DecMt.c
    Lzma2Enc.c
    # Lzma86Dec.c
    # Lzma86Enc.c
    LzmaDec.c
    LzmaEnc.c
    MtCoder.c
    MtDec.c
    Threads.c
    LzmaDec.c
)

set(LZMA_ASM_SOURCES_BASE)

if(USE_ASM)
    if(IS_X64)
        set(USE_X86_ASM 1)
        set(USE_X64_ASM 1)
        set(LZMA_OPT_DEFS Z7_LZMA_DEC_OPT)
        list(APPEND LZMA_ASM_SOURCES_BASE
            x86/7zCrcOpt.asm
            x86/LzFindOpt.asm
            x86/LzmaDecOpt.asm
        )
    elseif(IS_X86)
        set(USE_X86_ASM 1)
        # Add x86 ASM sources here if needed
    elseif(IS_ARM64)
        set(USE_ARM64_ASM 1)
        set(LZMA_OPT_DEFS Z7_LZMA_DEC_OPT)
        # Uncomment and adjust if you want to use ARM64 ASM sources
        # list(APPEND LZMA_ASM_SOURCES_BASE
        #     arm64/LzmaDecOpt.S
        #     arm64/7zAsm.S
        # )
        # list(APPEND LZMA_C_SOURCES_BASE
        #     7zCrcOpt.c
        # )
    endif()
endif()

if(NOT USE_X64_ASM)
    list(APPEND LZMA_C_SOURCES_BASE
        7zCrcOpt.c
        LzFindOpt.c
        #LzmaDec.c
    )
endif()

list(TRANSFORM LZMA_C_SOURCES_BASE PREPEND "${CMAKE_CURRENT_LIST_DIR}/C/")
list(TRANSFORM LZMA_ASM_SOURCES_BASE PREPEND "${CMAKE_CURRENT_LIST_DIR}/Asm/")

set(LZMA_SOURCES
    ${LZMA_C_SOURCES_BASE}
    ${LZMA_ASM_SOURCES_BASE}
)

add_library(lzma_v24_07 STATIC ${LZMA_SOURCES})
target_include_directories(lzma_v24_07 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/C)

if(LZMA_OPT_DEFS)
    target_compile_definitions(lzma_v24_07 PRIVATE ${LZMA_OPT_DEFS})
endif()
