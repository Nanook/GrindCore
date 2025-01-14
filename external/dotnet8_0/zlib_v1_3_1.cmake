set(ZLIB_SOURCES_BASE
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

# enable custom zlib allocator
add_definitions(-DMY_ZCALLOC)
if(HOST_WIN32 OR CLR_CMAKE_TARGET_WIN32)
    set(ZLIB_SOURCES_BASE ${ZLIB_SOURCES_BASE} ../../../libs/GrindCore/zlib_v1_3_1_allocator_win.c)
else()
    set(ZLIB_SOURCES_BASE ${ZLIB_SOURCES_BASE} ../../../libs/GrindCore/zlib_v1_3_1_allocator_unix.c)
endif()

addprefix(ZLIB_SOURCES "${CMAKE_CURRENT_LIST_DIR}/zlib_v1_3_1"  "${ZLIB_SOURCES_BASE}")
