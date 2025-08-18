cmake_minimum_required(VERSION 3.20)
project(GrindCore C)

include(${CMAKE_CURRENT_LIST_DIR}/extra_libs.cmake)

# Generate pal_config.h
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../Common/pal_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/pal_config.h
)

# Export macro for PAL functions
if(WIN32)
    set(PALEXPORT_DEF "PALEXPORT=__declspec(dllexport)")
else()
    set(PALEXPORT_DEF "PALEXPORT=__attribute__((visibility(\"default\")))")
endif()

# Optionally generate export header for cross-platform visibility
include(GenerateExportHeader)

# Add all algorithm/version libraries
include(${CLR_SRC_NATIVE_DIR}/external/dotnet9_0/zlib-ng_v2_2_1.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/dotnet8_0/zlib_v1_3_1.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/mcmilk/mcmilk-hashes.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/mcmilk/mcmilk-fast-lzma2.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/mcmilk/mcmilk-lzma.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/mcmilk/mcmilk-lz4.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/facebook/facebook-zstd_v1_5_2.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/facebook/facebook-zstd_v1_5_7.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/dotnet9_0/brotli_v1_1_0.cmake)

# List all third-party static libraries to link
set(GRINDCORE_THIRD_PARTY_LIBS
    zlibng_v2_2_1
    zlib_v1_3_1
    hashes
    fastlzma2_v1_0_1
    lzma_v24_07
    lz4_v1_9_4
    zstd_v1_5_2
    zstd_v1_5_7
    brotli_v1_1_0
)

# List all PAL and main GrindCore sources
set(GRINDCORE_SOURCES
    pal_zlib-ng_v2_2_1.c
    pal_zlib_v1_3_1.c
    pal_mcmilk_hashes.c
    pal_mcmilk_lzma_v24_07.c
    pal_mcmilk_lz4_v1_9_4.c
    pal_facebook_zstd_v1_5_2.c
    pal_facebook_zstd_v1_5_7.c
    pal_brotli_v1_1_0.c
    entrypoints.c
    # Add any other GrindCore sources as needed
)

# Shared library (for P/Invoke)
add_library(GrindCore SHARED ${GRINDCORE_SOURCES})
target_link_libraries(GrindCore PRIVATE ${GRINDCORE_THIRD_PARTY_LIBS})
set_target_properties(GrindCore PROPERTIES
    OUTPUT_NAME "GrindCore"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_include_directories(GrindCore PUBLIC ${CMAKE_CURRENT_BINARY_DIR}) # For export header
target_compile_definitions(GrindCore PRIVATE ${PALEXPORT_DEF})

# Export control for Windows
if(WIN32)
    set(DEF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/GrindCore.def)
    set_target_properties(GrindCore PROPERTIES LINK_FLAGS "/DEF:${DEF_FILE}")
    add_custom_command(TARGET GrindCore POST_BUILD
        COMMAND dumpbin /exports $<TARGET_FILE:GrindCore> > $<TARGET_FILE_DIR:GrindCore>/GrindCore.exports.txt
        COMMENT "Dumping exports for GrindCore.dll"
        VERBATIM
    )
endif()

# Export control for Unix/macOS
if(UNIX)
    set(EXPORTS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/GrindCore_unixexports.src)
    if(APPLE)
        # macOS: Use -exported_symbols_list
        set_target_properties(GrindCore PROPERTIES LINK_FLAGS "-Wl,-exported_symbols_list,${EXPORTS_FILE}")
    else()
        # Linux: Use --version-script
        set_target_properties(GrindCore PROPERTIES LINK_FLAGS "-Wl,--version-script=${EXPORTS_FILE}")
    endif()
    set_target_properties(GrindCore PROPERTIES C_VISIBILITY_PRESET hidden)
endif()

# Static library (for static linking)
add_library(GrindCoreStatic STATIC ${GRINDCORE_SOURCES})
target_link_libraries(GrindCoreStatic PRIVATE ${GRINDCORE_THIRD_PARTY_LIBS})
set_target_properties(GrindCoreStatic PROPERTIES
    OUTPUT_NAME "GrindCore"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/static"
)
target_include_directories(GrindCoreStatic PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(GrindCoreStatic PRIVATE ${PALEXPORT_DEF})

# AOT static library (for NativeAOT scenarios)
add_library(GrindCore.Aot STATIC ${GRINDCORE_SOURCES})
target_link_libraries(GrindCore.Aot PRIVATE ${GRINDCORE_THIRD_PARTY_LIBS})
set_target_properties(GrindCore.Aot PROPERTIES
    OUTPUT_NAME "GrindCore.Aot"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot"
)
target_include_directories(GrindCore.Aot PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(GrindCore.Aot PRIVATE ${PALEXPORT_DEF})

# Optionally, add a GuardCF variant for AOT with Control Flow Guard
add_library(GrindCore.Aot.GuardCF STATIC ${GRINDCORE_SOURCES})
target_link_libraries(GrindCore.Aot.GuardCF PRIVATE ${GRINDCORE_THIRD_PARTY_LIBS})
set_target_properties(GrindCore.Aot.GuardCF PROPERTIES
    OUTPUT_NAME "GrindCore.Aot.GuardCF"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/aot"
)
target_include_directories(GrindCore.Aot.GuardCF PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(GrindCore.Aot.GuardCF PRIVATE ${PALEXPORT_DEF})

# Install targets
install(TARGETS GrindCoreStatic DESTINATION .)
install(TARGETS GrindCore DESTINATION .)
install(TARGETS GrindCore.Aot DESTINATION .)
install(TARGETS GrindCore.Aot.GuardCF DESTINATION .)

# Generate export header for cross-platform symbol visibility
GENERATE_EXPORT_HEADER(GrindCore
    BASE_NAME GrindCore
    EXPORT_MACRO_NAME GrindCore_EXPORT
    EXPORT_FILE_NAME GrindCore_Export.h
    STATIC_DEFINE GrindCore_BUILT_AS_STATIC
)

# Entry points verification (optional, keep if you use it)
if (UNIX AND NOT APPLE AND NOT ANDROID)
    add_custom_command(TARGET GrindCore POST_BUILD
        COMMENT "Verifying GrindCore entry points against entrypoints.c"
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../verify-entrypoints.sh
            $<TARGET_FILE:GrindCore>
            ${CMAKE_CURRENT_SOURCE_DIR}/entrypoints.c
            ${CMAKE_NM}
        VERBATIM
    )
endif()
