set (ZSTD_V1_5_2_C_SOURCES_BASE
    debug.c
    entropy_common.c
    error_private.c
    fse_compress.c
    fse_decompress.c
    hist.c
    huf_compress.c
    huf_decompress.c
    pool.c
    threading.c
    xxhash.c
    zstd_common.c
    zstd_compress.c
    zstd_compress_literals.c
    zstd_compress_sequences.c
    zstd_compress_superblock.c
    zstd_ddict.c
    zstd_decompress.c
    zstd_decompress_block.c
    zstd_double_fast.c
    zstd_fast.c
    zstd_lazy.c
    zstd_ldm.c
    zstd_opt.c
    zstd_v01.c
    zstd_v02.c
    zstd_v03.c
    zstd_v04.c
    zstd_v05.c
    zstd_v06.c
    zstd_v07.c
    zstdmt_compress.c
)

list (TRANSFORM ZSTD_V1_5_2_C_SOURCES_BASE PREPEND    "${CMAKE_CURRENT_LIST_DIR}/C/zstd_v1_5_2/")

set (ZSTD_SOURCES_V1_5_2
    ${ZSTD_V1_5_2_C_SOURCES_BASE}
)


# Create a static library for zstd 1.5.2
add_library(zstd_v1_5_2_static STATIC ${ZSTD_SOURCES_V1_5_2})

# Include directories (match zlib pattern)
target_include_directories(zstd_v1_5_2_static PUBLIC ${CMAKE_CURRENT_LIST_DIR}/C/zstd_v1_5_2)

# Suppress implicit-fallthrough warnings for zstd 1.5.2 sources if building with GCC or Clang
if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(
        ${ZSTD_SOURCES_V1_5_2}
        PROPERTIES COMPILE_FLAGS "-Wno-implicit-fallthrough"
    )
endif()
