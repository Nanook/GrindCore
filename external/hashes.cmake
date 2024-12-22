set (HASHES_SOURCES_BASE
    blake3.c
    md2.c
    md4.c
    md5.c
    sha3.c
    sha512.c
)

list(TRANSFORM HASHES_SOURCES_BASE PREPEND "${CMAKE_CURRENT_LIST_DIR}/hashes/")

set(HASHES_SOURCES ${HASHES_SOURCES_BASE})
