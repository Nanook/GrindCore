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

# ---- Symbol Isolation for Static Libraries ----
# For the static library, we use objcopy --localize-hidden after bundling
# to make all non-PAL symbols local. The third-party libs keep their normal
# visibility attributes (needed for the shared library to export correctly).
# The static archive gets post-processed to enforce isolation.

# Optionally generate export header for cross-platform visibility
include(GenerateExportHeader)

# Add shared/consolidated libraries (must come before targets that depend on them)
include(${CLR_SRC_NATIVE_DIR}/external/facebook/facebook-zstd_shared.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/facebook/facebook-shared_xxhash.cmake)
include(${CLR_SRC_NATIVE_DIR}/external/shared_asm.cmake)

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
include(${CLR_SRC_NATIVE_DIR}/external/bzip2/bzip2-bzip2.cmake)

# List all third-party static libraries to link
set(GRINDCORE_THIRD_PARTY_LIBS
    zstd_shared
    shared_xxhash
    zlibng_v2_2_1
    zlib_v1_3_1
    hashes
    fastlzma2_v1_0_1
    lzma_v25_01
    lz4_v1_10_0
    zstd_v1_5_2
    zstd_v1_5_7
    brotli_v1_1_0
    bzip2_v1_0_8
)

# Add shared_asm if it was created (x64 + USE_ASM only)
if (TARGET shared_asm)
    list(APPEND GRINDCORE_THIRD_PARTY_LIBS shared_asm)
endif()

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
    pal_bzip2_v1_0_8.c
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

# macOS: Generate dSYM debug symbols and install alongside the dylib
if(APPLE)
    add_custom_command(TARGET GrindCore POST_BUILD
        COMMENT "Generating dSYM for libGrindCore.dylib"
        COMMAND dsymutil $<TARGET_FILE:GrindCore> -o $<TARGET_FILE:GrindCore>.dSYM
        VERBATIM
    )
    install(TARGETS GrindCore DESTINATION .)
    install(DIRECTORY $<TARGET_FILE:GrindCore>.dSYM DESTINATION .)
endif()

# ---- Static Library (for NativeAOT static linking) ----
# Produces a single fat static archive containing all PAL + third-party objects.
# IPO disabled for cross-toolchain compatibility. GuardCF added on Windows.
#
# Strategy: Build a thin intermediate .a/.lib (just PAL objects), then merge it
# with all third-party .a/.lib files into one fat archive. This is necessary
# because CMake's target_link_libraries on STATIC targets only records metadata.

add_library(GrindCoreStatic STATIC ${GRINDCORE_SOURCES})
target_link_libraries(GrindCoreStatic PRIVATE ${GRINDCORE_THIRD_PARTY_LIBS})
set_target_properties(GrindCoreStatic PROPERTIES
    OUTPUT_NAME "GrindCore_thin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/obj"
    INTERPROCEDURAL_OPTIMIZATION OFF
)
target_include_directories(GrindCoreStatic PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
if(WIN32)
    target_compile_definitions(GrindCoreStatic PRIVATE "PALEXPORT=")
    target_compile_options(GrindCoreStatic PRIVATE /guard:cf)
else()
    target_compile_definitions(GrindCoreStatic PRIVATE ${PALEXPORT_DEF})
endif()

# ---- Fat Archive Bundling ----
# Merge the thin GrindCore archive with all third-party archives into one file.
set(FAT_STATIC_DIR "${CMAKE_BINARY_DIR}/static")
file(MAKE_DIRECTORY "${FAT_STATIC_DIR}")

if(WIN32)
    set(FAT_STATIC_LIB "${FAT_STATIC_DIR}/GrindCore.lib")
    # On Windows, use lib.exe to merge all .lib files
    set(_merge_cmd ${CMAKE_AR} /NOLOGO /OUT:${FAT_STATIC_LIB} $<TARGET_FILE:GrindCoreStatic>)
    foreach(_dep ${GRINDCORE_THIRD_PARTY_LIBS})
        list(APPEND _merge_cmd $<TARGET_FILE:${_dep}>)
    endforeach()

    add_custom_command(
        OUTPUT ${FAT_STATIC_LIB}
        COMMAND ${_merge_cmd}
        DEPENDS GrindCoreStatic ${GRINDCORE_THIRD_PARTY_LIBS}
        COMMENT "Bundling fat static library: GrindCore.lib"
        VERBATIM
    )
elseif(APPLE)
    set(FAT_STATIC_LIB "${FAT_STATIC_DIR}/libGrindCore.a")
    # On macOS, use libtool to merge all .a files
    find_program(_libtool libtool)
    set(_merge_cmd ${_libtool} -static -o ${FAT_STATIC_LIB} $<TARGET_FILE:GrindCoreStatic>)
    foreach(_dep ${GRINDCORE_THIRD_PARTY_LIBS})
        list(APPEND _merge_cmd $<TARGET_FILE:${_dep}>)
    endforeach()

    add_custom_command(
        OUTPUT ${FAT_STATIC_LIB}
        COMMAND ${_merge_cmd}
        DEPENDS GrindCoreStatic ${GRINDCORE_THIRD_PARTY_LIBS}
        COMMENT "Bundling fat static library: libGrindCore.a"
        VERBATIM
    )
else()
    set(FAT_STATIC_LIB "${FAT_STATIC_DIR}/libGrindCore.a")
    # On Linux, use AR MRI script to merge archives (handles duplicate object names)
    set(_mri_script "${FAT_STATIC_DIR}/GrindCore_bundle.mri")
    set(_mri_content "CREATE ${FAT_STATIC_LIB}\n")
    set(_mri_content "${_mri_content}ADDLIB $<TARGET_FILE:GrindCoreStatic>\n")
    foreach(_dep ${GRINDCORE_THIRD_PARTY_LIBS})
        set(_mri_content "${_mri_content}ADDLIB $<TARGET_FILE:${_dep}>\n")
    endforeach()
    set(_mri_content "${_mri_content}SAVE\nEND\n")
    file(GENERATE OUTPUT "${_mri_script}" CONTENT "${_mri_content}")

    set(_ar_merge_script "${FAT_STATIC_DIR}/GrindCore_ar_merge.sh")
    file(GENERATE OUTPUT "${_ar_merge_script}" CONTENT
"#!/bin/sh
rm -f \"${FAT_STATIC_LIB}\"
${CMAKE_AR} -M < \"${_mri_script}\"
")

    add_custom_command(
        OUTPUT ${FAT_STATIC_LIB}
        COMMAND sh "${_ar_merge_script}"
        DEPENDS GrindCoreStatic ${GRINDCORE_THIRD_PARTY_LIBS}
        COMMENT "Bundling fat static library: libGrindCore.a"
    )
endif()

add_custom_target(GrindCoreFat ALL DEPENDS ${FAT_STATIC_LIB})

# Install targets
# Shared library first, then fat static archive (overwrites any import .lib on Windows)
install(TARGETS GrindCore DESTINATION .)
install(FILES ${FAT_STATIC_LIB} DESTINATION .)

# Install PDB debug symbols on Windows (equivalent to .dbg on Linux)
if(WIN32)
    install(FILES $<TARGET_PDB_FILE:GrindCore> DESTINATION . OPTIONAL)
endif()

# ---- Symbol Localization & Verification (Unix/macOS) ----
# ---- Symbol Localization DISABLED ----
# The objcopy/nmedit localization step was stripping internal symbols (FL2POOL_*,
# RMF_buildTable, LZMA2_encode, etc.) that are needed by other objects within the
# same archive. For static archives used in AOT linking, symbol visibility is
# handled by the final linker — localization is unnecessary and actively harmful
# on newer toolchains (Ubuntu 22.04+ / Clang 15+).

# Generate export header for cross-platform symbol visibility
GENERATE_EXPORT_HEADER(GrindCore
    BASE_NAME GrindCore
    EXPORT_MACRO_NAME GrindCore_EXPORT
    EXPORT_FILE_NAME GrindCore_Export.h
    STATIC_DEFINE GrindCore_BUILT_AS_STATIC
)
