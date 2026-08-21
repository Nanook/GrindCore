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
include(${CLR_SRC_NATIVE_DIR}/external/lz4/lz4-lz4.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/sevenzip/sevenzip-lzma_v25_01.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/facebook/facebook-zstd_v1_5_2.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/facebook/facebook-zstd_v1_5_7.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/dotnet9_0/brotli_v1_1_0.cmake)

# List all third-party static libraries to link
set(GRINDCORE_THIRD_PARTY_LIBS
    zlibng_v2_2_1
    zlib_v1_3_1
    hashes
    fastlzma2_v1_0_1
    lzma_v25_01
    lz4_v1_10_0
    zstd_v1_5_2
    zstd_v1_5_7
    brotli_v1_1_0
)

# List all PAL and main GrindCore sources
set(GRINDCORE_SOURCES
    pal_zlib-ng_v2_2_1.c
    pal_zlib_v1_3_1.c
    pal_mcmilk_hashes.c
    pal_lz4_lz4_v1_10_0.c
    pal_sevenzip_lzma_v25_01.c
    pal_facebook_zstd_v1_5_2.c
    pal_facebook_zstd_v1_5_2_seekable.c
    pal_facebook_zstd_v1_5_7.c
    pal_facebook_zstd_v1_5_7_seekable.c
    pal_brotli_v1_1_0.c
    entrypoints.c
    # Add any other GrindCore sources as needed
)

# Suppress MSVC narrowing warnings for seekable PAL wrappers on 32-bit targets
# These files pass through U64 values from zstd's seekable API which truncate to size_t on x86
if (MSVC)
    set_source_files_properties(
        pal_facebook_zstd_v1_5_2_seekable.c
        pal_facebook_zstd_v1_5_7_seekable.c
        PROPERTIES COMPILE_FLAGS "/wd4242 /wd4244"
    )
endif()

# ---- Export List Generation ----
set(ENTRYPOINTS_C ${CMAKE_CURRENT_SOURCE_DIR}/entrypoints.c)

if(WIN32)
    set(EXPORT_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/../generate_exports.ps1)
    set(EXPORTS_FILE ${CMAKE_CURRENT_BINARY_DIR}/GrindCore.def)
    add_custom_command(
        OUTPUT ${EXPORTS_FILE}
        COMMAND powershell ${EXPORT_SCRIPT} ${ENTRYPOINTS_C} ${EXPORTS_FILE}
        DEPENDS ${ENTRYPOINTS_C} ${EXPORT_SCRIPT}
    )
elseif(APPLE)
    set(EXPORT_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/../generate_exports.sh)
    set(EXPORTS_FILE ${CMAKE_CURRENT_BINARY_DIR}/GrindCore_macosexports.list)
    add_custom_command(
        OUTPUT ${EXPORTS_FILE}
        COMMAND sh ${EXPORT_SCRIPT} osx ${ENTRYPOINTS_C} ${EXPORTS_FILE}
        DEPENDS ${ENTRYPOINTS_C} ${EXPORT_SCRIPT}
    )
elseif(UNIX)
    set(EXPORT_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/../generate_exports.sh)
    set(EXPORTS_FILE ${CMAKE_CURRENT_BINARY_DIR}/GrindCore_unixexports.src)
    add_custom_command(
        OUTPUT ${EXPORTS_FILE}
        COMMAND sh ${EXPORT_SCRIPT} unix ${ENTRYPOINTS_C} ${EXPORTS_FILE}
        DEPENDS ${ENTRYPOINTS_C} ${EXPORT_SCRIPT}
    )
endif()

# Shared library (for P/Invoke)
add_library(GrindCore SHARED ${GRINDCORE_SOURCES})
target_link_libraries(GrindCore PRIVATE ${GRINDCORE_THIRD_PARTY_LIBS})
set_target_properties(GrindCore PROPERTIES
    OUTPUT_NAME "GrindCore"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_include_directories(GrindCore PUBLIC ${CMAKE_CURRENT_BINARY_DIR}) # For export header
target_compile_definitions(GrindCore PRIVATE ${PALEXPORT_DEF})

# Set export file linker flags and dependencies after target creation
if(WIN32)
    set_target_properties(GrindCore PROPERTIES LINK_FLAGS "/DEF:${EXPORTS_FILE}")
elseif(APPLE)
    set_target_properties(GrindCore PROPERTIES LINK_FLAGS "-Wl,-exported_symbols_list,${EXPORTS_FILE}")
elseif(UNIX)
    set_target_properties(GrindCore PROPERTIES LINK_FLAGS "-Wl,--version-script=${EXPORTS_FILE}")
endif()

add_custom_target(GrindCoreExports DEPENDS ${EXPORTS_FILE})
add_dependencies(GrindCore GrindCoreExports)

# Export verification (optional, keep if you use it)
if (UNIX AND NOT APPLE AND NOT ANDROID)
    add_custom_command(TARGET GrindCore POST_BUILD
        COMMENT "Verifying GrindCore entry points against entrypoints.c"
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../verify-entrypoints.sh
            $<TARGET_FILE:GrindCore>
            ${CMAKE_CURRENT_SOURCE_DIR}/entrypoints.c
            ${CMAKE_NM}
        VERBATIM
    )
    install_with_stripped_symbols (GrindCore PROGRAMS .)
endif()

# ---- Fat Static Libraries ----
# CMake's target_link_libraries() on STATIC library targets only records metadata;
# it does NOT merge the linked libraries' object files into the archive. To produce
# self-contained static libraries suitable for linking, we use a post-build bundling
# step that merges all constituent archives into one fat archive.
include(${CMAKE_CURRENT_SOURCE_DIR}/../bundle_static_lib.cmake)

# Static library (for static linking)
# The thin intermediate lib goes to obj/static; the fat bundled output goes to static/
add_library(GrindCoreStatic STATIC ${GRINDCORE_SOURCES})
target_link_libraries(GrindCoreStatic PRIVATE ${GRINDCORE_THIRD_PARTY_LIBS})
set_target_properties(GrindCoreStatic PROPERTIES
    OUTPUT_NAME "GrindCore_thin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/obj/static"
)
target_include_directories(GrindCoreStatic PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(GrindCoreStatic PRIVATE ${PALEXPORT_DEF})

# Bundle into a fat static library containing all third-party object code
bundle_static_library(GrindCoreStatic GrindCoreStatic_Bundled "${CMAKE_BINARY_DIR}/static" "GrindCore")

# AOT static library (for NativeAOT scenarios)
# Disable interprocedural optimization so the archive can be linked with any
# toolchain version, not only the exact one used to build.
add_library(GrindCore.Aot STATIC ${GRINDCORE_SOURCES})
target_link_libraries(GrindCore.Aot PRIVATE ${GRINDCORE_THIRD_PARTY_LIBS})
set_target_properties(GrindCore.Aot PROPERTIES
    OUTPUT_NAME "GrindCore.Aot_thin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/obj/aot"
    INTERPROCEDURAL_OPTIMIZATION OFF
)
target_include_directories(GrindCore.Aot PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(GrindCore.Aot PRIVATE ${PALEXPORT_DEF})

# Bundle into a fat static library
bundle_static_library(GrindCore.Aot GrindCoreAot_Bundled "${CMAKE_BINARY_DIR}/aot" "GrindCore.Aot")

# AOT variant with Control Flow Guard (Windows-specific security hardening)
add_library(GrindCore.Aot.GuardCF STATIC ${GRINDCORE_SOURCES})
target_link_libraries(GrindCore.Aot.GuardCF PRIVATE ${GRINDCORE_THIRD_PARTY_LIBS})
set_target_properties(GrindCore.Aot.GuardCF PROPERTIES
    OUTPUT_NAME "GrindCore.Aot.GuardCF_thin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/obj/aot"
    INTERPROCEDURAL_OPTIMIZATION OFF
)
target_include_directories(GrindCore.Aot.GuardCF PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(GrindCore.Aot.GuardCF PRIVATE ${PALEXPORT_DEF})
if(MSVC)
    target_compile_options(GrindCore.Aot.GuardCF PRIVATE /guard:cf)
endif()

# Bundle into a fat static library
bundle_static_library(GrindCore.Aot.GuardCF GrindCoreAotGuardCF_Bundled "${CMAKE_BINARY_DIR}/aot" "GrindCore.Aot.GuardCF")

# Install targets
# Shared library installed directly via CMake target
install(TARGETS GrindCore DESTINATION .)

# Fat static libraries installed from bundled output
install(FILES ${GrindCoreStatic_Bundled_OUTPUT} DESTINATION .)
install(FILES ${GrindCoreAot_Bundled_OUTPUT} DESTINATION .)
install(FILES ${GrindCoreAotGuardCF_Bundled_OUTPUT} DESTINATION .)

# Generate export header for cross-platform symbol visibility
GENERATE_EXPORT_HEADER(GrindCore
    BASE_NAME GrindCore
    EXPORT_MACRO_NAME GrindCore_EXPORT
    EXPORT_FILE_NAME GrindCore_Export.h
    STATIC_DEFINE GrindCore_BUILT_AS_STATIC
)
