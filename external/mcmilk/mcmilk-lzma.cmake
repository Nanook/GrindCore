# https://github.com/kingsznhone/FastLZMA2Net

# Conditional logic
if (USE_ASM)
    if (IS_X64)
        set (USE_X86_ASM 1)
        set (USE_X64_ASM 1)
        add_definitions(-DZ7_LZMA_DEC_OPT)
    elseif (IS_X86)
        set(USE_X86_ASM 1)
    elseif (IS_ARM64)
        set (USE_ARM64_ASM 1)
        add_definitions(-DZ7_LZMA_DEC_OPT)
    endif () # arm
endif ()

set (LZMA_C_SOURCES_BASE
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
# Set source files based on the condition
if (USE_X64_ASM) # X86 fails
    set (LZMA_ASM_SOURCES_BASE
        x86/7zCrcOpt.asm
        x86/LzFindOpt.asm
        x86/LzmaDecOpt.asm
    )
# elseif (USE_ARM64_ASM)
#     set (LZMA_ASM_SOURCES_BASE
#         arm64/LzmaDecOpt.S
#         arm64/7zAsm.S
#     )
#     set (LZMA_C_SOURCES_BASE
#         7zCrcOpt.c
#      )
else () # everything else or USE_ASM is not set
    list(APPEND LZMA_C_SOURCES_BASE
        7zCrcOpt.c
        LzFindOpt.c
        #LzmaDec.c
     )
endif ()

list (TRANSFORM LZMA_C_SOURCES_BASE PREPEND    "${CMAKE_CURRENT_LIST_DIR}/C/")
list (TRANSFORM LZMA_ASM_SOURCES_BASE PREPEND  "${CMAKE_CURRENT_LIST_DIR}/Asm/")

set (LZMA_SOURCES
    ${LZMA_C_SOURCES_BASE}
    ${LZMA_ASM_SOURCES_BASE}
)
