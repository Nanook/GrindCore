# bundle_static_lib.cmake
#
# Provides a function to create a "fat" static library that bundles all object
# files from a target and its PRIVATE static library dependencies into a single
# archive. This is necessary because CMake's target_link_libraries() on STATIC
# libraries only records metadata -- it does NOT merge the linked libraries'
# object files into the output archive.
#
# Usage:
#   bundle_static_library(<target> <bundled_target_name> <output_dir> <output_name>)
#
# Parameters:
#   target             - The STATIC library target to bundle
#   bundled_target_name - Name for the custom target that produces the fat archive
#   output_dir         - Directory where the fat archive will be placed
#   output_name        - Base name for the output file (without lib prefix or extension)
#
# The bundled library path is exported as ${bundled_target_name}_OUTPUT in the
# calling scope for use in install() commands.

function(bundle_static_library tgt bundled_tgt_name output_dir output_name)
    # Collect all static library dependencies (recursively)
    function(_recursively_collect_dependencies input_target all_deps_var)
        set(_deps_found ${${all_deps_var}})
        get_target_property(_link_libs ${input_target} LINK_LIBRARIES)
        if(_link_libs)
            foreach(_dep ${_link_libs})
                if(TARGET ${_dep})
                    # Resolve alias targets
                    get_target_property(_aliased ${_dep} ALIASED_TARGET)
                    if(_aliased)
                        set(_dep ${_aliased})
                    endif()
                    get_target_property(_dep_type ${_dep} TYPE)
                    if(_dep_type STREQUAL "STATIC_LIBRARY")
                        list(FIND _deps_found ${_dep} _idx)
                        if(_idx EQUAL -1)
                            list(APPEND _deps_found ${_dep})
                            _recursively_collect_dependencies(${_dep} _deps_found)
                        endif()
                    endif()
                endif()
            endforeach()
        endif()
        set(${all_deps_var} ${_deps_found} PARENT_SCOPE)
    endfunction()

    set(_all_static_deps "")
    _recursively_collect_dependencies(${tgt} _all_static_deps)
    list(REMOVE_DUPLICATES _all_static_deps)

    # Determine output file path
    if(WIN32)
        set(_bundled_lib "${output_dir}/${output_name}.lib")
    else()
        set(_bundled_lib "${output_dir}/lib${output_name}.a")
    endif()

    # Ensure output directory exists
    file(MAKE_DIRECTORY "${output_dir}")

    if(WIN32)
        # On Windows with MSVC, CMAKE_AR points to lib.exe
        set(_lib_exe ${CMAKE_AR})

        # Build the lib command with all inputs
        set(_bundle_command ${_lib_exe} /NOLOGO /OUT:${_bundled_lib})
        list(APPEND _bundle_command "$<TARGET_FILE:${tgt}>")
        foreach(_dep ${_all_static_deps})
            list(APPEND _bundle_command "$<TARGET_FILE:${_dep}>")
        endforeach()

        add_custom_command(
            OUTPUT ${_bundled_lib}
            COMMAND ${_bundle_command}
            DEPENDS ${tgt} ${_all_static_deps}
            COMMENT "Bundling fat static library: ${output_name}.lib"
            VERBATIM
        )

    elseif(APPLE)
        # On macOS, use libtool to merge all .a files (handles duplicate object names)
        find_program(_libtool libtool)
        if(NOT _libtool)
            message(FATAL_ERROR "libtool not found, required for bundling static libraries on macOS")
        endif()

        set(_bundle_command ${_libtool} -static -o ${_bundled_lib})
        list(APPEND _bundle_command "$<TARGET_FILE:${tgt}>")
        foreach(_dep ${_all_static_deps})
            list(APPEND _bundle_command "$<TARGET_FILE:${_dep}>")
        endforeach()

        add_custom_command(
            OUTPUT ${_bundled_lib}
            COMMAND ${_bundle_command}
            DEPENDS ${tgt} ${_all_static_deps}
            COMMENT "Bundling fat static library: lib${output_name}.a"
            VERBATIM
        )

    else()
        # On Linux/Unix, use AR with MRI script to merge archives.
        # MRI scripts handle duplicate object names correctly.
        #
        # We generate the MRI script at configure time using file(GENERATE)
        # which supports generator expressions to get the actual library paths.

        set(_mri_script "${output_dir}/${output_name}_bundle.mri")

        # Build MRI script content using generator expressions
        set(_mri_content "CREATE ${_bundled_lib}\n")
        set(_mri_content "${_mri_content}ADDLIB $<TARGET_FILE:${tgt}>\n")
        foreach(_dep ${_all_static_deps})
            set(_mri_content "${_mri_content}ADDLIB $<TARGET_FILE:${_dep}>\n")
        endforeach()
        set(_mri_content "${_mri_content}SAVE\nEND\n")

        file(GENERATE OUTPUT "${_mri_script}" CONTENT "${_mri_content}")

        # Use a shell script to pipe the MRI script to ar -M
        set(_ar_merge_script "${output_dir}/${output_name}_ar_merge.sh")
        file(GENERATE OUTPUT "${_ar_merge_script}" CONTENT
"#!/bin/sh
rm -f \"${_bundled_lib}\"
${CMAKE_AR} -M < \"${_mri_script}\"
")

        add_custom_command(
            OUTPUT ${_bundled_lib}
            COMMAND sh "${_ar_merge_script}"
            DEPENDS ${tgt} ${_all_static_deps}
            COMMENT "Bundling fat static library: lib${output_name}.a"
        )
    endif()

    add_custom_target(${bundled_tgt_name} ALL DEPENDS ${_bundled_lib})
    add_dependencies(${bundled_tgt_name} ${tgt})

    # Export the bundled library path so callers can install it
    set_target_properties(${bundled_tgt_name} PROPERTIES
        BUNDLED_OUTPUT "${_bundled_lib}"
    )
    # Export to parent scope for use in install() commands
    set(${bundled_tgt_name}_OUTPUT "${_bundled_lib}" PARENT_SCOPE)
endfunction()
