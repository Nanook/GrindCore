# set (ZSTD_V1_5_6_C_SOURCES_BASE
#     debug.c
#     entropy_common.c
#     error_private.c
#     fse_compress.c
#     fse_decompress.c
#     hist.c
#     huf_compress.c
#     huf_decompress.c
#     pool.c
#     threading.c
#     xxhash.c
#     zstd_common.c
#     zstd_compress.c
#     zstd_compress_literals.c
#     zstd_compress_sequences.c
#     zstd_compress_superblock.c
#     zstd_ddict.c
#     zstd_decompress.c
#     zstd_decompress_block.c
#     zstd_double_fast.c
#     zstd_fast.c
#     zstd_lazy.c
#     zstd_ldm.c
#     zstd_opt.c
#     zstd_v01.c
#     zstd_v02.c
#     zstd_v03.c
#     zstd_v04.c
#     zstd_v05.c
#     zstd_v06.c
#     zstd_v07.c
#     zstdmt_compress.c
# )

# list (TRANSFORM ZSTD_V1_5_6_C_SOURCES_BASE PREPEND "${CMAKE_CURRENT_LIST_DIR}/C/zstd/")

# set (ZSTD_SOURCES_V1_5_6 ${ZSTD_V1_5_6_C_SOURCES_BASE})

# # Create a static library for zstd
# add_library(zstd_v1_5_6 STATIC ${ZSTD_SOURCES_V1_5_6})

# # Include directories
# target_include_directories(zstd_v1_5_6 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/C/zstd)

# # Suppress implicit-fallthrough warnings for zstd sources if building with GCC or Clang
# if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
#     set_source_files_properties(
#         ${ZSTD_SOURCES_V1_5_6}
#         PROPERTIES COMPILE_FLAGS "-Wno-implicit-fallthrough"
#     )
# endif()

# target_compile_definitions(zstd_v1_5_6 PRIVATE ZSTD_NAMESPACE=ZSTD_v1_5_6)

# set_target_properties(zstd_v1_5_6 PROPERTIES LINKER_LANGUAGE C)
