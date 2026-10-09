get_filename_component(DOPO_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# host tools run here at configure time, so they are built with the
# machine's own c++, never with the cross toolchain
find_program(DOPO_HOST_CXX NAMES c++ g++ clang++ REQUIRED NO_CMAKE_FIND_ROOT_PATH)

function(dopo_host_tool name source)
    set(tool "${CMAKE_BINARY_DIR}/dopo/${name}")
    if (NOT EXISTS "${tool}" OR "${source}" IS_NEWER_THAN "${tool}")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/dopo")
        execute_process(
            COMMAND "${DOPO_HOST_CXX}" -std=c++23 -O2 ${ARGN} -o "${tool}" "${source}"
            RESULT_VARIABLE result
            ERROR_VARIABLE error)
        if (result)
            message(FATAL_ERROR "dopo: failed to build ${name}\n${error}")
        endif()
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    set(${name}_TOOL "${tool}" PARENT_SCOPE)
endfunction()

dopo_host_tool(dopo_patchs "${DOPO_DIR}/scripts/dopo_patchs.cpp")

# applies the /** @patch */ hunks of patch_files on top of source_dir into
# output_dir (see scripts/dopo_patchs.cpp); out_var gets the patched files,
# relative to source_dir
function(dopo_apply_patchs out_var source_dir output_dir)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ARGN})
    execute_process(
        COMMAND "${dopo_patchs_TOOL}" --source "${source_dir}" --output "${output_dir}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE patched
        ERROR_VARIABLE error
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (result)
        message(FATAL_ERROR "dopo: failed to apply patchs on ${source_dir}\n${error}")
    endif()
    string(REPLACE "\n" ";" patched "${patched}")
    set(${out_var} "${patched}" PARENT_SCOPE)
endfunction()

# the files of dir that carry /** @patch */ hunks go to patch_var, the
# others (relative to dir) to other_var
function(dopo_split_patchs patch_var other_var dir)
    file(GLOB_RECURSE files CONFIGURE_DEPENDS RELATIVE "${dir}" ${ARGN})
    set(patchs "")
    set(others "")
    foreach(file ${files})
        file(STRINGS "${dir}/${file}" marks REGEX "@patch[ \t]")
        if (marks)
            list(APPEND patchs "${dir}/${file}")
        else()
            list(APPEND others "${file}")
        endif()
    endforeach()
    set(${patch_var} "${patchs}" PARENT_SCOPE)
    set(${other_var} "${others}" PARENT_SCOPE)
endfunction()
