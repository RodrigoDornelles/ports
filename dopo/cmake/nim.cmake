# Nim for the build's scripts, fetched once in vendor/nim: nim-lang.org's
# release build where it has one for the host (Linux x86_64), built from
# the release's sources elsewhere. dopo_nim_tool() compiles a script into
# the build tree, as dopo_host_tool() does a C++ one.
include_guard(GLOBAL)

set(NIM_VERSION "2.2.6")
get_filename_component(NIM_DIR "${CMAKE_CURRENT_LIST_DIR}/../../vendor/nim" ABSOLUTE)
# the host's C compiler, which Nim's output goes through: not the target's
# (emcc in the wasm preset)
find_program(DOPO_HOST_CC NAMES cc gcc clang REQUIRED NO_CMAKE_FIND_ROOT_PATH)

set(NIM_STAMP "${NIM_DIR}/.dopo-installed-${NIM_VERSION}")
if (NOT EXISTS "${NIM_STAMP}")
    if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        set(nim_package "nim-${NIM_VERSION}-linux_x64.tar.xz")
    else()
        set(nim_package "nim-${NIM_VERSION}.tar.xz")
    endif()
    set(nim_download "https://nim-lang.org/download/${nim_package}")
    set(nim_archive "${NIM_DIR}.tar.xz")
    message(STATUS "nim: downloading ${nim_package}")
    file(REMOVE_RECURSE "${NIM_DIR}" "${NIM_DIR}.tmp")
    file(DOWNLOAD "${nim_download}" "${nim_archive}" STATUS status)
    list(GET status 0 code)
    if (code)
        message(FATAL_ERROR "nim: failed to download ${nim_download}: ${status}")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${nim_archive}" DESTINATION "${NIM_DIR}.tmp")
    file(RENAME "${NIM_DIR}.tmp/nim-${NIM_VERSION}" "${NIM_DIR}")
    file(REMOVE_RECURSE "${NIM_DIR}.tmp")
    file(REMOVE "${nim_archive}")
    if (NOT EXISTS "${NIM_DIR}/bin/nim")
        message(STATUS "nim: building ${NIM_VERSION} from its sources (the first time takes a while)")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env "CC=${DOPO_HOST_CC}" sh build.sh
            WORKING_DIRECTORY "${NIM_DIR}"
            RESULT_VARIABLE result)
        if (result OR NOT EXISTS "${NIM_DIR}/bin/nim")
            message(FATAL_ERROR "nim: building ${NIM_VERSION} failed")
        endif()
    endif()
    file(TOUCH "${NIM_STAMP}")
endif()

set(NIM_EXECUTABLE "${NIM_DIR}/bin/nim")

# Nim needs to know which compiler family it drives (its atomics, among
# others, are gcc's or clang's builtins), then takes the host's binary
execute_process(COMMAND "${DOPO_HOST_CC}" --version OUTPUT_VARIABLE nim_cc_version ERROR_QUIET)
if (nim_cc_version MATCHES "clang")
    set(NIM_CC_FLAGS --cc:clang "--clang.exe:${DOPO_HOST_CC}" "--clang.linkerexe:${DOPO_HOST_CC}")
else()
    set(NIM_CC_FLAGS --cc:gcc "--gcc.exe:${DOPO_HOST_CC}" "--gcc.linkerexe:${DOPO_HOST_CC}")
endif()

# compiles the Nim script source into <build>/dopo/<name>, again only when
# the script changed; ${name}_TOOL is its path
function(dopo_nim_tool name source)
    set(tool "${CMAKE_BINARY_DIR}/dopo/${name}")
    if (NOT EXISTS "${tool}" OR "${source}" IS_NEWER_THAN "${tool}")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/dopo")
        execute_process(
            COMMAND "${NIM_EXECUTABLE}" c ${NIM_CC_FLAGS} -d:release --hints:off --warnings:off
                "--nimcache:${CMAKE_BINARY_DIR}/dopo/nimcache/${name}" "--out:${tool}" ${ARGN} "${source}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error)
        if (result)
            message(FATAL_ERROR "dopo: failed to build ${name}\n${output}${error}")
        endif()
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    set(${name}_TOOL "${tool}" PARENT_SCOPE)
endfunction()
