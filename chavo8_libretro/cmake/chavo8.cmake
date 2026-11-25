get_filename_component(CHAVO8_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(CHAVO8_STB_VERSION "2c980bb59875b0d32144a71867fbdebb2f77cd20")
set(CHAVO8_STB_DOWNLOAD "https://raw.githubusercontent.com/nothings/stb/${CHAVO8_STB_VERSION}")
set(CHAVO8_STB_DIR "${CMAKE_SOURCE_DIR}/vendor/stb")

set(CHAVO8_LIBRETRO_DOWNLOAD "https://raw.githubusercontent.com/libretro/RetroArch/ee3eae34c425a1645b923395854dda7f2543df31/libretro-common/include/libretro.h")
set(CHAVO8_LIBRETRO_DIR "${CMAKE_SOURCE_DIR}/vendor/retroarch")

# Comic Relief (OFL): metric-compatible with the Comic Sans MS the game
# took from Windows, so text measured for centering lands where it did
set(COMICRELIEF_VERSION "993d03b988291b8609cd3fc24e10b6a982c79de9")
set(COMICRELIEF_DOWNLOAD "https://raw.githubusercontent.com/google/fonts/${COMICRELIEF_VERSION}/ofl/comicrelief")
set(COMICRELIEF_DIR "${CMAKE_SOURCE_DIR}/vendor/comicrelief")

function(chavo8_download url file)
    if (NOT EXISTS "${file}")
        file(DOWNLOAD "${url}" "${file}" STATUS status)
        list(GET status 0 code)
        if (code)
            file(REMOVE "${file}")
            message(FATAL_ERROR "chavo8: failed to download ${url}: ${status}")
        endif()
    endif()
endfunction()

foreach(file stb_image.h stb_truetype.h)
    chavo8_download("${CHAVO8_STB_DOWNLOAD}/${file}" "${CHAVO8_STB_DIR}/${file}")
endforeach()
chavo8_download("${CHAVO8_LIBRETRO_DOWNLOAD}" "${CHAVO8_LIBRETRO_DIR}/libretro.h")
foreach(file ComicRelief-Regular.ttf OFL.txt)
    chavo8_download("${COMICRELIEF_DOWNLOAD}/${file}" "${COMICRELIEF_DIR}/${file}")
endforeach()

# the font, embedded as ComicRelief_Regular[] into chavo8/font.h of the
# build tree; the header is only rewritten on change
dopo_host_tool(dopo_xxd "${DOPO_DIR}/scripts/xxd.cpp")
set(CHAVO8_INCLUDE_GENERATED "${CMAKE_BINARY_DIR}/chavo8/include")
file(MAKE_DIRECTORY "${CHAVO8_INCLUDE_GENERATED}/chavo8")
execute_process(
    COMMAND "${dopo_xxd_TOOL}" "${COMICRELIEF_DIR}/ComicRelief-Regular.ttf"
    OUTPUT_FILE "${CMAKE_BINARY_DIR}/chavo8/font.h"
    RESULT_VARIABLE chavo8_xxd_result
    ERROR_VARIABLE chavo8_xxd_error)
if (chavo8_xxd_result)
    message(FATAL_ERROR "chavo8: failed to embed the font\n${chavo8_xxd_error}")
endif()
file(COPY_FILE "${CMAKE_BINARY_DIR}/chavo8/font.h"
    "${CHAVO8_INCLUDE_GENERATED}/chavo8/font.h" ONLY_IF_DIFFERENT)

# the x86 dumped into game/v1/dump (scripts/dump.nim), as C the games call
# (scripts/lift.nim): x86v1.c and its declarations for Nim, x86v1.nim; a dump that
# changes reconfigures
dopo_nim_tool(chavo8_lift "${CHAVO8_DIR}/scripts/lift.nim")
set(CHAVO8_LIFTED "${CMAKE_BINARY_DIR}/chavo8/lifted")
file(MAKE_DIRECTORY "${CHAVO8_LIFTED}")
file(GLOB chavo8_dumps CONFIGURE_DEPENDS "${CHAVO8_DIR}/library/game/v1/dump/*.s")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${chavo8_dumps})
execute_process(
    COMMAND "${chavo8_lift_TOOL}" x86v1 "${CHAVO8_LIFTED}" ${chavo8_dumps}
    RESULT_VARIABLE chavo8_lift_result
    OUTPUT_VARIABLE chavo8_lift_output
    ERROR_VARIABLE chavo8_lift_output)
if (chavo8_lift_result)
    message(FATAL_ERROR "chavo8: failed to lift the dumps\n${chavo8_lift_output}")
endif()

# the games in Nim (library/chavo8.nim and what it imports), with Delphi's
# defaults: no overflow nor range checks
dopo_nim_library(chavo8_game "${CHAVO8_DIR}/library/chavo8.nim"
    --overflowChecks:off --rangeChecks:off --nimMainPrefix:chavo8_
    "--path:${CHAVO8_LIFTED}")
target_include_directories(chavo8_game PRIVATE "${CHAVO8_DIR}/include")

file(GLOB_RECURSE chavo8_files CONFIGURE_DEPENDS "${CHAVO8_DIR}/library/*.c")
list(APPEND chavo8_files "${CHAVO8_LIFTED}/x86v1.c")

if (EMSCRIPTEN)
    add_library(chavo8_libretro STATIC "${chavo8_files}")
else()
    add_library(chavo8_libretro SHARED "${chavo8_files}")
endif()
target_include_directories(chavo8_libretro PRIVATE "${CHAVO8_DIR}/include" "${CHAVO8_INCLUDE_GENERATED}")
target_include_directories(chavo8_libretro SYSTEM PRIVATE "${CHAVO8_LIBRETRO_DIR}" "${CHAVO8_STB_DIR}")
target_compile_definitions(chavo8_libretro PRIVATE _DEFAULT_SOURCE)
target_compile_options(chavo8_libretro PRIVATE -Wall -Wextra -fwrapv)
target_link_libraries(chavo8_libretro PRIVATE chavo8_game m)
set_target_properties(chavo8_libretro PROPERTIES
    C_EXTENSIONS OFF
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
