set (LZ4_C_SOURCES_BASE
    lz4.c
    lz4hc.c
    lz4frame.c
)

list (TRANSFORM LZ4_C_SOURCES_BASE PREPEND    "${CMAKE_CURRENT_LIST_DIR}/C/lz4/")

set (LZ4_SOURCES
    ${LZ4_C_SOURCES_BASE}
)
add_library(lz4_v1_9_4 STATIC ${LZ4_SOURCES})
target_include_directories(lz4_v1_9_4 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/C/lz4)
