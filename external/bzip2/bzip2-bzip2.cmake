set (BZIP2_C_SOURCES_BASE
    blocksort.c
    huffman.c
    crctable.c
    randtable.c
    compress.c
    decompress.c
    bzlib.c
)

list (TRANSFORM BZIP2_C_SOURCES_BASE PREPEND    "${CMAKE_CURRENT_LIST_DIR}/bzip2/")

set (BZIP2_SOURCES
    ${BZIP2_C_SOURCES_BASE}
)

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(
        ${BZIP2_SOURCES}
        PROPERTIES COMPILE_FLAGS "-Wno-implicit-fallthrough"
    )
endif()

# bzip2's own source intentionally computes in Int32 (its own typedef for plain int) and packs
# results into narrower UChar/UInt16 storage for compact block metadata (e.g. Huffman code
# lengths, always well under 256, stored as UChar) - safe, deliberate narrowing that predates
# this project by decades, not a bug. This project's MSVC flags promote C4242/C4244 (possible
# loss of data on implicit narrowing) from warnings to hard errors project-wide, which bzip2's
# unmodified upstream source doesn't satisfy. Same situation and same fix as the seekable zstd
# PAL files' MSVC suppression in libs/GrindCore/CMakeLists.txt - scoped to just this vendored
# library's sources, not the whole project.
if (MSVC)
    set_source_files_properties(
        ${BZIP2_SOURCES}
        PROPERTIES COMPILE_FLAGS "/wd4242 /wd4244"
    )
endif()

add_library(bzip2_v1_0_8 STATIC ${BZIP2_SOURCES})
target_include_directories(bzip2_v1_0_8 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/bzip2/)
# We only need the core/utility library API (block + stream), not bzip2's optional FILE*-based
# high-level API (BZ2_bzReadOpen/BZ2_bzWriteOpen/etc.) - PUBLIC so the PAL wrapper compiling
# against the same bzlib.h sees an identical, smaller declaration set (no stdio.h dependency).
target_compile_definitions(bzip2_v1_0_8 PUBLIC BZ_NO_STDIO)
