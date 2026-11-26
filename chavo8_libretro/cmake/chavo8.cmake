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

# dmc_unrar (GPL-2.0-or-later, see its COPYING): the game as the .rar it was
# shared in, RAR 1.5 to 5, one C file
set(CHAVO8_UNRAR_VERSION "ab8bbddfb84f4b5853c8184be84a8c2565f4a4bb")
set(CHAVO8_UNRAR_DOWNLOAD "https://raw.githubusercontent.com/DrMcCoy/dmc_unrar/${CHAVO8_UNRAR_VERSION}")
set(CHAVO8_UNRAR_DIR "${CMAKE_SOURCE_DIR}/vendor/dmc_unrar")

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
foreach(file dmc_unrar.c COPYING)
    chavo8_download("${CHAVO8_UNRAR_DOWNLOAD}/${file}" "${CHAVO8_UNRAR_DIR}/${file}")
endforeach()
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

# the x86 dumped into game/assembly.tar.xz (scripts/dump.nim), unpacked and
# turned into C the games call (scripts/lift.nim): x86v1*.c and their
# declarations for Nim, x86v1.nim; the archive or the lifter changing reconfigures
dopo_nim_tool(chavo8_lift "${CHAVO8_DIR}/scripts/lift.nim")
set(CHAVO8_ASSEMBLY "${CHAVO8_DIR}/library/game/assembly.tar.xz")
set(CHAVO8_LIFTED "${CMAKE_BINARY_DIR}/chavo8/lifted")
set(chavo8_unpacked "${CMAKE_BINARY_DIR}/chavo8/assembly")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CHAVO8_ASSEMBLY}")
file(REMOVE_RECURSE "${chavo8_unpacked}")
file(MAKE_DIRECTORY "${chavo8_unpacked}" "${CHAVO8_LIFTED}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xf "${CHAVO8_ASSEMBLY}"
    WORKING_DIRECTORY "${chavo8_unpacked}"
    RESULT_VARIABLE chavo8_unpack_result)
if (chavo8_unpack_result)
    message(FATAL_ERROR "chavo8: failed to unpack ${CHAVO8_ASSEMBLY}")
endif()
file(GLOB chavo8_dumps "${chavo8_unpacked}/*.s")
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

add_library(chavo8_unrar STATIC "${CHAVO8_UNRAR_DIR}/dmc_unrar.c")
target_include_directories(chavo8_unrar PUBLIC "${CHAVO8_UNRAR_DIR}")
target_compile_definitions(chavo8_unrar PRIVATE _FILE_OFFSET_BITS=64)
target_compile_options(chavo8_unrar PRIVATE -w)
set_target_properties(chavo8_unrar PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE chavo8_files CONFIGURE_DEPENDS "${CHAVO8_DIR}/library/*.c")
file(GLOB chavo8_lifted "${CHAVO8_LIFTED}/x86v1*.c")
list(APPEND chavo8_files ${chavo8_lifted})

if (EMSCRIPTEN)
    # RetroArch's web player links one archive: the games' and dmc_unrar's
    # objects go into it
    add_library(chavo8_libretro STATIC "${chavo8_files}"
        $<TARGET_OBJECTS:chavo8_game> $<TARGET_OBJECTS:chavo8_unrar>)
    set_target_properties(chavo8_libretro PROPERTIES OUTPUT_NAME "chavo8_libretro_emscripten")
else()
    add_library(chavo8_libretro SHARED "${chavo8_files}")
    target_link_options(chavo8_libretro PRIVATE -Wl,--no-undefined)
endif()
target_include_directories(chavo8_libretro PRIVATE "${CHAVO8_DIR}/include" "${CHAVO8_INCLUDE_GENERATED}")
target_include_directories(chavo8_libretro SYSTEM PRIVATE "${CHAVO8_LIBRETRO_DIR}" "${CHAVO8_STB_DIR}")
target_compile_definitions(chavo8_libretro PRIVATE _DEFAULT_SOURCE)
target_compile_options(chavo8_libretro PRIVATE -Wall -Wextra -fwrapv)
target_link_libraries(chavo8_libretro PRIVATE chavo8_game chavo8_unrar m)
set_target_properties(chavo8_libretro PROPERTIES
    C_EXTENSIONS OFF
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

if (EMSCRIPTEN)
    # the web build: a launcher (www/index.html) whose Play opens the game
    # page (www/play.html) on the game as hosted, or on one's own copy
    set(CHAVO8_CONTENT_URL
        "https://raw.githubusercontent.com/RodrigoDornelles/RodrigoDornelles/refs/heads/master/abandonwares/StreetChaves_1.5A.rar"
        CACHE STRING "the game the web build's Play and Download fetch")
    set(chavo8_www "${PORTS_DIST}/chavo8")
    set(chavo8_www_outputs "")
    foreach(copy "${CHAVO8_DIR}/www/style.css=style.css" "${DOPO_DIR}/www/sw.js=sw.js")
        string(REPLACE "=" ";" copy "${copy}")
        list(GET copy 0 source)
        list(GET copy 1 file)
        add_custom_command(OUTPUT "${chavo8_www}/${file}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${source}" "${chavo8_www}/${file}"
            DEPENDS "${source}")
        list(APPEND chavo8_www_outputs "${chavo8_www}/${file}")
    endforeach()
    include("${DOPO_DIR}/cmake/www.cmake")
    dopo_www_page("${chavo8_www}/index.html" "${CHAVO8_DIR}/www/index.html"
        "title=Street Chaves"
        "description=Street Chaves 1.5A, the 2004 fan-made fighting game of the vila, in the browser."
        "root=../" "back=../" "source=/tree/main/chavo8_libretro"
        "title_before=Street " "title_accent=Chaves" "content=${CHAVO8_CONTENT_URL}")
    dopo_www_page("${chavo8_www}/play.html" "${CHAVO8_DIR}/www/play.html" "content=${CHAVO8_CONTENT_URL}")
    list(APPEND chavo8_www_outputs "${chavo8_www}/index.html" "${chavo8_www}/play.html")
    add_custom_target(chavo8_www ALL DEPENDS ${chavo8_www_outputs})
endif()
