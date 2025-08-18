set (ZSTD_V1_5_2_C_SOURCES_BASE
    common/debug.c
    common/entropy_common.c
    common/error_private.c
    compress/fse_compress.c
    common/fse_decompress.c
    compress/hist.c
    compress/huf_compress.c
    decompress/huf_decompress.c
    common/pool.c
    common/threading.c
    common/xxhash.c
    common/zstd_common.c
    compress/zstd_compress.c
    compress/zstd_compress_literals.c
    compress/zstd_compress_sequences.c
    compress/zstd_compress_superblock.c
    decompress/zstd_ddict.c
    decompress/zstd_decompress.c
    decompress/zstd_decompress_block.c
    compress/zstd_double_fast.c
    compress/zstd_fast.c
    compress/zstd_lazy.c
    compress/zstd_ldm.c
    compress/zstd_opt.c
    legacy/zstd_v01.c
    legacy/zstd_v02.c
    legacy/zstd_v03.c
    legacy/zstd_v04.c
    legacy/zstd_v05.c
    legacy/zstd_v06.c
    legacy/zstd_v07.c
    compress/zstdmt_compress.c
)

# Conditionally add the x86-64 assembly source for BMI2
# if (USE_ASM AND IS_X64)
#     list(APPEND ZSTD_V1_5_2_C_SOURCES_BASE decompress/huf_decompress_amd64.S)
# endif()

list (TRANSFORM ZSTD_V1_5_2_C_SOURCES_BASE PREPEND "${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_2/")

set (ZSTD_SOURCES_V1_5_2
    ${ZSTD_V1_5_2_C_SOURCES_BASE}
)

# Create a static library for zstd 1.5.2
add_library(zstd_v1_5_2 STATIC ${ZSTD_SOURCES_V1_5_2})

# Suppress MSVC C4267 (size_t to int conversion) warnings for zstd sources
if (MSVC)
    set_source_files_properties(${ZSTD_SOURCES_V1_5_2} PROPERTIES COMPILE_FLAGS "/wd4267")
endif()

# Include directories for official zstd
target_include_directories(zstd_v1_5_2 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_2/common
    ${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_2/compress
    ${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_2/decompress
    ${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_2/legacy
)

# Suppress implicit-fallthrough warnings for zstd 1.5.2 sources if building with GCC or Clang
if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(
        ${ZSTD_SOURCES_V1_5_2}
        PROPERTIES COMPILE_FLAGS "-Wno-implicit-fallthrough"
    )
endif()

target_compile_definitions(zstd_v1_5_2 PRIVATE ZSTD_NAMESPACE=ZSTD_v1_5_2 ZSTD_v1_5_2_DISABLE_ASM )

set_target_properties(zstd_v1_5_2 PROPERTIES LINKER_LANGUAGE C)
