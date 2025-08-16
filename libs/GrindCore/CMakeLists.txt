project(GrindCore C)

include(${CMAKE_CURRENT_LIST_DIR}/extra_libs.cmake)

# add_definitions(-DZ7_LZMA_PROB32) # LZMA : can increase the speed on some CPUs, but memory usage for CLzmaDec::probs will be doubled in that case

include(${CLR_SRC_NATIVE_DIR}/external/dotnet9_0/zlib-ng_v2_2_1.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/dotnet8_0/zlib_v1_3_1.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/mcmilk/mcmilk-hashes.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/mcmilk/mcmilk-fast-lzma2.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/mcmilk/mcmilk-lzma.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/mcmilk/mcmilk-lz4.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/mcmilk/mcmilk-zstd_v1_5_2.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/mcmilk/mcmilk-zstd_v1_5_6.cmake)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../Common/pal_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/pal_config.h)


set(NATIVECOMPRESSION_SOURCES
    pal_zlib-ng_v2_2_1.c
    pal_zlib_v1_3_1.c
    pal_mcmilk_hashes.c
    ${HASHES_SOURCES}
    ${FL2_SOURCES}
    ${LZMA_SOURCES}
    ${LZ4_SOURCES}
    pal_mcmilk_lzma_v24_07.c
    pal_mcmilk_lz4_v1_9_4.c
    pal_mcmilk_zstd_v1_5_6.c
    pal_mcmilk_zstd_v1_5_2.c
)

if (NOT CLR_CMAKE_TARGET_BROWSER AND NOT CLR_CMAKE_TARGET_WASI)

    include(${CLR_SRC_NATIVE_DIR}/external/dotnet9_0/brotli_v1_1_0.cmake)

    set (NATIVECOMPRESSION_SOURCES
        ${NATIVECOMPRESSION_SOURCES}
        ${BROTLI_SOURCES}
        pal_brotli_v1_1_0.c
    )

    set (NATIVECOMPRESSION_SOURCES
        ${NATIVECOMPRESSION_SOURCES}
        entrypoints.c
    )
endif ()

if (CLR_CMAKE_TARGET_UNIX OR CLR_CMAKE_TARGET_BROWSER OR CLR_CMAKE_TARGET_WASI)
    set(NATIVE_LIBS_EXTRA)
    append_extra_compression_libs(NATIVE_LIBS_EXTRA)

    # Disable implicit fallthrough warning for Zlib and Brotli
    set(FLAGS -Wno-implicit-fallthrough)

    set_source_files_properties(${NATIVECOMPRESSION_SOURCES} PROPERTIES COMPILE_FLAGS ${FLAGS})

    if (GEN_SHARED_LIB)
        add_definitions(-DBROTLI_SHARED_COMPILATION)
        add_definitions(-DHASHES_SHARED_COMPILATION) # Nanook

        add_library(GrindCore
            SHARED
            ${NATIVECOMPRESSION_SOURCES}
            ${VERSION_FILE_PATH}
        )

        target_link_libraries(GrindCore PRIVATE zstd_v1_5_2 zstd_v1_5_6 zlib_v1_3_1 zlib ${NATIVE_LIBS_EXTRA})
        # target_link_libraries(GrindCore
        #     PRIVATE
        #     ${NATIVE_LIBS_EXTRA}
        # )

        if (NOT CLR_CMAKE_TARGET_MACCATALYST AND NOT CLR_CMAKE_TARGET_IOS AND NOT CLR_CMAKE_TARGET_TVOS AND NOT CLR_CMAKE_TARGET_ANDROID)
            set(DEF_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/GrindCore_unixexports.src)
            set(EXPORTS_FILE ${CMAKE_CURRENT_BINARY_DIR}/GrindCore.exports)
            generate_exports_file(${DEF_SOURCES} ${EXPORTS_FILE})
            set_exports_linker_option(${EXPORTS_FILE})

            add_custom_target(GrindCore_exports DEPENDS ${EXPORTS_FILE})
            add_dependencies(GrindCore GrindCore_exports)

            set_property(TARGET GrindCore APPEND_STRING PROPERTY LINK_FLAGS ${EXPORTS_LINKER_OPTION})
            set_property(TARGET GrindCore APPEND_STRING PROPERTY LINK_DEPENDS ${EXPORTS_FILE})

            add_custom_command(TARGET GrindCore POST_BUILD
                COMMENT "Verifying GrindCore entry points against entrypoints.c"
                COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../verify-entrypoints.sh
                    $<TARGET_FILE:GrindCore>
                    ${CMAKE_CURRENT_SOURCE_DIR}/entrypoints.c
                    ${CMAKE_NM}
                VERBATIM
            )
        endif ()

        install_with_stripped_symbols (GrindCore PROGRAMS .)
    endif ()


    add_library(GrindCore-Static
        STATIC
        ${NATIVECOMPRESSION_SOURCES}
    )

    # Link the necessary zlib versions and zstd_v1_5_2 static lib for PAL v1_5_2
    target_link_libraries(GrindCore-Static PRIVATE zlibstatic_v1_3_1 zlib zstd_v1_5_6_static zstd_v1_5_2_static)

    set_target_properties(GrindCore-Static PROPERTIES OUTPUT_NAME GrindCore CLEAN_DIRECT_OUTPUT 1)
else ()
    if (GEN_SHARED_LIB)
        include (GenerateExportHeader)
        add_definitions(-DVER_FILEDESCRIPTION_STR="GrindCore")
        add_library(GrindCore
            SHARED
            ${NATIVECOMPRESSION_SOURCES}
            GrindCore.def
            ${VERSION_FILE_RC_PATH}
        )

        target_link_libraries(GrindCore PRIVATE zstd_v1_5_2 zstd_v1_5_6 zlib_v1_3_1 zlib)
    endif ()

    if (NOT GEN_SHARED_LIB AND NOT CLR_CMAKE_TARGET_MACCATALYST AND NOT CLR_CMAKE_TARGET_IOS AND NOT CLR_CMAKE_TARGET_TVOS AND NOT CLR_CMAKE_TARGET_ANDROID AND NOT CLR_CMAKE_TARGET_BROWSER AND NOT CLR_CMAKE_TARGET_WASI)
        set(NATIVECOMPRESSION_SOURCES ${NATIVECOMPRESSION_SOURCES} entrypoints.c)
    endif ()

    add_library(GrindCore-Static
        STATIC
        ${NATIVECOMPRESSION_SOURCES}
    )

    # Link the necessary zlib versions
    target_link_libraries(GrindCore-Static PRIVATE brotli_v1_1_0 zstd_v1_5_2_static zstd_v1_5_6_static zlibstatic_v1_3_1 zlib)

    if(STATIC_LIBS_ONLY)
        add_library(GrindCore.Aot
            STATIC
            ${NATIVECOMPRESSION_SOURCES}
        )

        target_link_libraries(GrindCore.Aot PRIVATE brotli_v1_1_0 zstd_v1_5_2_static zstd_v1_5_6_static zlibstatic_v1_3_1 zlib)

        set_target_properties(GrindCore.Aot PROPERTIES CLR_CONTROL_FLOW_GUARD OFF)
        set_target_properties(GrindCore.Aot PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

        add_library(GrindCore.Aot.GuardCF
            STATIC
            ${NATIVECOMPRESSION_SOURCES}
        )

        target_link_libraries(GrindCore.Aot.GuardCF PRIVATE brotli_v1_1_0 zstd_v1_5_2_static zstd_v1_5_6_static zlibstatic_v1_3_1 zlib)

        set_target_properties(GrindCore.Aot.GuardCF PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

        install_static_library(GrindCore.Aot aotsdk nativeaot)
        install_static_library(GrindCore.Aot.GuardCF aotsdk nativeaot)
    endif ()

    if (GEN_SHARED_LIB)
        GENERATE_EXPORT_HEADER( GrindCore
         BASE_NAME GrindCore
         EXPORT_MACRO_NAME GrindCore_EXPORT
         EXPORT_FILE_NAME GrindCore_Export.h
         STATIC_DEFINE GrindCore_BUILT_AS_STATIC
        )

        install (TARGETS GrindCore DESTINATION .)
        install (FILES $<TARGET_PDB_FILE:GrindCore> DESTINATION .)
    endif ()
endif ()

if((NOT CLR_CMAKE_USE_SYSTEM_ZLIB) AND STATIC_LIBS_ONLY)
    install_static_library(zlib_v1_3_1 aotsdk nativeaot)
    install_static_library(zlib aotsdk nativeaot)
endif()

install (TARGETS GrindCore-Static DESTINATION ${STATIC_LIB_DESTINATION} COMPONENT libs)
