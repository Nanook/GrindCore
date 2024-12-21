include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/hashes")

set (HASHES_SOURCES_BASE
    blake3.c
    md2.c
    md4.c
    md5.c
    sha3.c
    sha512.c
)

addprefix(HASHES_SOURCES "${CMAKE_CURRENT_LIST_DIR}/hashes" "${HASHES_SOURCES_BASE}")
