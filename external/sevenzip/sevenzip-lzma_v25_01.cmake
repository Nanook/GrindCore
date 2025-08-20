
# Conditional logic
set(SZ_LZMA_LZMA_SOURCES_BASE
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

set(SZ_LZMA_ASM_SOURCES_BASE)

if(USE_ASM)
    if(IS_X64)
        set(USE_X86_ASM 1)
        set(USE_X64_ASM 1)
        set(LZMA_OPT_DEFS Z7_LZMA_DEC_OPT)
    list(APPEND SZ_LZMA_ASM_SOURCES_BASE
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
    # list(APPEND SZ_LZMA_ASM_SOURCES_BASE
        #     arm64/LzmaDecOpt.S
        #     arm64/7zAsm.S
        # )
    # list(APPEND SZ_LZMA_LZMA_SOURCES_BASE
        #     7zCrcOpt.c
        # )
    endif()
endif()

if(NOT USE_X64_ASM)
    list(APPEND SZ_LZMA_LZMA_SOURCES_BASE
        7zCrcOpt.c
        LzFindOpt.c
        #LzmaDec.c
    )
endif()

list(TRANSFORM SZ_LZMA_LZMA_SOURCES_BASE PREPEND "${CMAKE_CURRENT_LIST_DIR}/lzma/")
list(TRANSFORM SZ_LZMA_ASM_SOURCES_BASE PREPEND "${CMAKE_CURRENT_LIST_DIR}/asm/")

set(SZ_LZMA_SOURCES
    ${SZ_LZMA_LZMA_SOURCES_BASE}
    ${SZ_LZMA_ASM_SOURCES_BASE}
)


add_library(lzma_v25_01 STATIC ${SZ_LZMA_SOURCES})
target_include_directories(lzma_v25_01 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/lzma)


if(LZMA_OPT_DEFS)
    target_compile_definitions(lzma_v25_01 PRIVATE ${LZMA_OPT_DEFS})
endif()
