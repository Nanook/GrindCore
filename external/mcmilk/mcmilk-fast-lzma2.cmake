# https://github.com/kingsznhone/FastLZMA2Net

# Conditional logic
if (USE_ASM)
    if (IS_X64)
        set (USE_X86_ASM 1)
        set (USE_X64_ASM 1)
    elseif (IS_X86)
        set(USE_X86_ASM 1)
    elseif (IS_ARM64)
        set (USE_ARM64_ASM 1)
    endif () # arm
endif ()

# Set source files based on the condition
if (USE_X86_ASM) # also USE_X64_ASM
    set (FL2_ASM_SOURCES_BASE
        x86/7zCrcOpt.asm
        x86/LzmaDecOpt.asm
    )
elseif (USE_ARM64_ASM)
    set (FL2_ASM_SOURCES_BASE
        arm64/LzmaDecOpt.S
        arm64/7zAsm.S
    )
    set (FL2_C_SOURCES_BASE
        7zCrcOpt.c
     )
else () # arm
    set (FL2_C_SOURCES_BASE
        7zCrcOpt.c
        LzmaDec.c
     )
endif ()

set (FL2_SOURCES_BASE
    dict_buffer.c
    fl2_common.c
    fl2_compress.c
    fl2_decompress.c
    fl2_pool.c
    fl2_threading.c
    lzma2_dec.c
    lzma2_enc.c
    radix_bitpack.c
    radix_mf.c
    radix_struct.c
    range_enc.c
    util.c
)

set (FL2_ZSTD_SOURCES_BASE
    xxhash.c
)

include_directories ("${CMAKE_CURRENT_LIST_DIR}/C/zstd")

list (TRANSFORM FL2_C_SOURCES_BASE PREPEND    "${CMAKE_CURRENT_LIST_DIR}/C/")
list (TRANSFORM FL2_ASM_SOURCES_BASE PREPEND  "${CMAKE_CURRENT_LIST_DIR}/Asm/")
list (TRANSFORM FL2_SOURCES_BASE PREPEND      "${CMAKE_CURRENT_LIST_DIR}/C/fast-lzma2/")
list (TRANSFORM FL2_ZSTD_SOURCES_BASE PREPEND "${CMAKE_CURRENT_LIST_DIR}/C/zstd/")

set (FL2_SOURCES
    ${FL2_C_SOURCES_BASE}
    ${FL2_ASM_SOURCES_BASE}
    ${FL2_ZSTD_SOURCES_BASE}
    ${FL2_SOURCES_BASE}
)
