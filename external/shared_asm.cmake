# shared_asm: consolidates ASM files that are identical between
# fastlzma2 and lzma_v25_01 to eliminate duplicate-symbol (LNK4006) warnings.
# Only active on x64 with USE_ASM enabled.
#
# Contains: 7zCrcOpt.asm, LzmaDecOpt.asm
# Note: LzFindOpt.asm is NOT shared — it is only used by lzma_v25_01.

set(SHARED_ASM_SOURCES)

if (USE_ASM AND IS_X64)
    set(SHARED_ASM_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/sevenzip/asm/x86/7zCrcOpt.asm
        ${CMAKE_CURRENT_LIST_DIR}/sevenzip/asm/x86/LzmaDecOpt.asm
    )
endif()

if (SHARED_ASM_SOURCES)
    add_library(shared_asm STATIC ${SHARED_ASM_SOURCES})
    set_target_properties(shared_asm PROPERTIES LINKER_LANGUAGE C)
endif()
