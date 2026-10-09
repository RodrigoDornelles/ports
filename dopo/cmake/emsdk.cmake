# the Emscripten SDK for the wasm preset, fetched and activated once in
# vendor/emsdk; included before project() so its toolchain is the one used.
# 3.1.46 is the version RetroArch's web player is built with
# (pkg/emscripten/README.md).
set(EMSDK_VERSION "3.1.46")
set(EMSDK_DOWNLOAD "https://github.com/emscripten-core/emsdk/archive/refs/tags/${EMSDK_VERSION}.tar.gz")
set(EMSDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../../vendor/emsdk")
get_filename_component(EMSDK_DIR "${EMSDK_DIR}" ABSOLUTE)

set(EMSDK_STAMP "${EMSDK_DIR}/.dopo-activated-${EMSDK_VERSION}")
if (NOT EXISTS "${EMSDK_STAMP}")
    set(emsdk_archive "${EMSDK_DIR}.tar.gz")
    if (NOT EXISTS "${EMSDK_DIR}/emsdk")
        message(STATUS "emsdk: downloading ${EMSDK_VERSION}")
        file(DOWNLOAD "${EMSDK_DOWNLOAD}" "${emsdk_archive}" STATUS status)
        list(GET status 0 code)
        if (code)
            message(FATAL_ERROR "emsdk: failed to download ${EMSDK_DOWNLOAD}: ${status}")
        endif()
        file(ARCHIVE_EXTRACT INPUT "${emsdk_archive}" DESTINATION "${EMSDK_DIR}.tmp")
        file(RENAME "${EMSDK_DIR}.tmp/emsdk-${EMSDK_VERSION}" "${EMSDK_DIR}")
        file(REMOVE_RECURSE "${EMSDK_DIR}.tmp")
        file(REMOVE "${emsdk_archive}")
    endif()
    foreach(step install activate)
        message(STATUS "emsdk: ${step} ${EMSDK_VERSION} (the first time takes a while)")
        execute_process(
            COMMAND "${EMSDK_DIR}/emsdk" ${step} ${EMSDK_VERSION}
            WORKING_DIRECTORY "${EMSDK_DIR}"
            RESULT_VARIABLE result)
        if (result)
            message(FATAL_ERROR "emsdk: '${step} ${EMSDK_VERSION}' failed")
        endif()
    endforeach()
    file(TOUCH "${EMSDK_STAMP}")
endif()

set(EMSCRIPTEN_ROOT "${EMSDK_DIR}/upstream/emscripten")
set(CMAKE_TOOLCHAIN_FILE "${EMSCRIPTEN_ROOT}/cmake/Modules/Platform/Emscripten.cmake" CACHE STRING "emscripten toolchain" FORCE)
