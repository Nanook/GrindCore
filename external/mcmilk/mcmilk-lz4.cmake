set (LZ4_C_SOURCES_BASE
    lz4.c
    lz4hc.c
    lz4frame.c
)

set (LZ4_DEPS_SOURCES_BASE
    xxhash.c
)

list (TRANSFORM LZ4_C_SOURCES_BASE PREPEND    "${CMAKE_CURRENT_LIST_DIR}/C/lz4/")
list (TRANSFORM LZ4_DEPS_SOURCES_BASE PREPEND "${CMAKE_CURRENT_LIST_DIR}/C/7z-deps/")

set (LZ4_SOURCES
    ${FL2_DEPS_SOURCES_BASE}
    ${LZ4_C_SOURCES_BASE}
)

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(
        ${LZ4_SOURCES}
        PROPERTIES COMPILE_FLAGS "-Wno-implicit-fallthrough"
    )
endif()

add_library(lz4_v1_9_4 STATIC ${LZ4_SOURCES})
target_include_directories(lz4_v1_9_4 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/C/lz4)
