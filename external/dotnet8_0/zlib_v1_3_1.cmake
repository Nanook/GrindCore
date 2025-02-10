set(ZLIB_SOURCES_BASE_v1_3_1
    adler32.c
    compress.c
    crc32.c
    uncompr.c
    deflate.c
    gzguts.h
    trees.c
    zutil.c
    inflate.c
    infback.c
    inftrees.c
    inffast.c
    crc32.h
    deflate.h
    inffast.h
    inffixed.h
    inflate.h
    inftrees.h
    trees.h
    zconf.h
    zlib.h
    zutil.h
)

# Enable custom zlib allocator
add_definitions(-DMY_ZCALLOC)
if(HOST_WIN32 OR CLR_CMAKE_TARGET_WIN32)
    set(ZLIB_SOURCES_BASE ${ZLIB_SOURCES_BASE_v1_3_1} ../../../libs/GrindCore/zlib_v1_3_1_allocator_win.c)
else()
    set(ZLIB_SOURCES_BASE ${ZLIB_SOURCES_BASE_v1_3_1} ../../../libs/GrindCore/zlib_v1_3_1_allocator_unix.c)
endif()

addprefix(ZLIB_SOURCES_v1_3_1 "${CMAKE_CURRENT_LIST_DIR}/zlib_v1_3_1" "${ZLIB_SOURCES_BASE}")

# Create a static library for zlib
add_library(zlib_v1_3_1 STATIC ${ZLIB_SOURCES_v1_3_1})

# Include directories
target_include_directories(zlib_v1_3_1 PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# Add compiler options to suppress fall-through warnings
target_compile_options(zlib_v1_3_1 PRIVATE $<$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>:-Wno-implicit-fallthrough>
)
