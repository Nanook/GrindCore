# Conditional logic
if (USE_ASM)
    if (IS_X64)
        set(USE_X86_ASM 1)
        set(USE_X64_ASM 1)
    elseif (IS_X86)
        set(USE_X86_ASM 1)
    endif ()
endif ()

set (C_SOURCES_BASE
    Blake2s.c
    CpuArch.c
    Sha1.c
    Sha256.c
)

# Set source files based on the condition
if (USE_X86_ASM)
    set(ASM_SOURCES_BASE
        Sha1Opt.asm
        Sha256Opt.asm
    )
else ()
    set(APPEND C_SOURCES_BASE
        Sha1Opt.c
        Sha256Opt.c
     )
endif ()

set (HASHES_SOURCES_BASE
    blake3.c
    md2.c
    md4.c
    md5.c
    sha3.c
    sha512.c
)

set (ZSTD_SOURCES_BASE
    xxhash.c
)

list(TRANSFORM C_SOURCES_BASE PREPEND      "${CMAKE_CURRENT_LIST_DIR}/C/")
list(TRANSFORM ASM_SOURCES_BASE PREPEND    "${CMAKE_CURRENT_LIST_DIR}/Asm/x86/")
list(TRANSFORM HASHES_SOURCES_BASE PREPEND "${CMAKE_CURRENT_LIST_DIR}/C/hashes/")
list(TRANSFORM ZSTD_SOURCES_BASE PREPEND   "${CMAKE_CURRENT_LIST_DIR}/C/zstd/")

set(HASHES_SOURCES
    ${C_SOURCES_BASE}
    ${ASM_SOURCES_BASE}
    ${HASHES_SOURCES_BASE}
    ${ZSTD_SOURCES_BASE}
)
