set (LZ4_C_SOURCES_BASE
    lz4.c
    lz4hc.c
    lz4frame.c
    xxhash.c
)

list (TRANSFORM LZ4_C_SOURCES_BASE PREPEND    "${CMAKE_CURRENT_LIST_DIR}/lz4/")

set (LZ4_SOURCES
    ${LZ4_C_SOURCES_BASE}
)

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(
        ${LZ4_SOURCES}
        PROPERTIES COMPILE_FLAGS "-Wno-implicit-fallthrough"
    )
endif()

add_library(lz4_v1_10_0 STATIC ${LZ4_SOURCES})
target_include_directories(lz4_v1_10_0 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/lz4/)
