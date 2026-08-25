# shared_xxhash: single copy of xxHash compiled with XXH_NAMESPACE=ZSTD_
# Eliminates duplicate ZSTD_XXH* symbols between zstd_v1_5_7, hashes, and fastlzma2.
#
# Note: zstd_v1_5_2 uses XXH_NAMESPACE=ZSTD_v1_5_2_ (different symbols) so it
# keeps its own xxhash.c.

set(SHARED_XXHASH_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_7/common/xxhash.c
)

add_library(shared_xxhash STATIC ${SHARED_XXHASH_SOURCES})

target_include_directories(shared_xxhash PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/zstd_v1_5_7/common
)

# The xxhash.h in v1.5.7/common already sets XXH_NAMESPACE=ZSTD_ by default.
# No extra definitions needed — the header handles it.

# Suppress MSVC conversion warnings
if (MSVC)
    set_source_files_properties(${SHARED_XXHASH_SOURCES} PROPERTIES COMPILE_FLAGS "/wd4267")
endif()

set_target_properties(shared_xxhash PROPERTIES LINKER_LANGUAGE C)
