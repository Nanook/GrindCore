# zstd_shared: consolidates internal code that is identical between
# zstd v1.5.2 and v1.5.7 to eliminate duplicate-symbol (LNK4006) warnings.
# Uses v1.5.7 source files. Compiled with v1.5.7 include paths.
#
# Contains: debug.c, error_private.c, legacy decoders (zstd_v01-v07)
#
# NOTE: pool.c, threading.c, and zstdmt_compress.c are per-version because
# ZSTD_MULTITHREAD is enabled and v1.5.2 uses prefixed symbols
# (POOL_v1_5_2_*, ZSTDMT_v1_5_2_*) to avoid collision with v1.5.7.

set(ZSTD_SHARED_SOURCES_BASE
    common/debug.c
    common/error_private.c
    legacy/zstd_v01.c
    legacy/zstd_v02.c
    legacy/zstd_v03.c
    legacy/zstd_v04.c
    legacy/zstd_v05.c
    legacy/zstd_v06.c
    legacy/zstd_v07.c
)

list(TRANSFORM ZSTD_SHARED_SOURCES_BASE PREPEND "${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_7/")

set(ZSTD_SHARED_SOURCES ${ZSTD_SHARED_SOURCES_BASE})

add_library(zstd_shared STATIC ${ZSTD_SHARED_SOURCES})

target_include_directories(zstd_shared PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_7
    ${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_7/common
    ${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_7/compress
    ${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_7/decompress
    ${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_7/legacy
)

# Suppress MSVC conversion warnings
if (MSVC)
    set_source_files_properties(${ZSTD_SHARED_SOURCES} PROPERTIES COMPILE_FLAGS "/wd4267 /wd4242 /wd4244")
endif()

# Suppress implicit-fallthrough warnings for legacy decoders (GCC/Clang)
if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(
        ${ZSTD_SHARED_SOURCES}
        PROPERTIES COMPILE_FLAGS "-Wno-implicit-fallthrough"
    )
endif()

target_compile_definitions(zstd_shared PRIVATE ZSTD_DISABLE_ASM)

set_target_properties(zstd_shared PROPERTIES LINKER_LANGUAGE C)
