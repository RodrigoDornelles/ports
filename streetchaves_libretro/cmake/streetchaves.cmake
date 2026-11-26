get_filename_component(STREETCHAVES_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(STREETCHAVES_STB_VERSION "2c980bb59875b0d32144a71867fbdebb2f77cd20")
set(STREETCHAVES_STB_DOWNLOAD "https://raw.githubusercontent.com/nothings/stb/${STREETCHAVES_STB_VERSION}")
set(STREETCHAVES_STB_DIR "${CMAKE_SOURCE_DIR}/vendor/stb")

set(STREETCHAVES_LIBRETRO_DOWNLOAD "https://raw.githubusercontent.com/libretro/RetroArch/ee3eae34c425a1645b923395854dda7f2543df31/libretro-common/include/libretro.h")
set(STREETCHAVES_LIBRETRO_DIR "${CMAKE_SOURCE_DIR}/vendor/retroarch")

# Comic Relief (OFL): metric-compatible with the Comic Sans MS the game
# took from Windows, so text measured for centering lands where it did
set(COMICRELIEF_VERSION "993d03b988291b8609cd3fc24e10b6a982c79de9")
set(COMICRELIEF_DOWNLOAD "https://raw.githubusercontent.com/google/fonts/${COMICRELIEF_VERSION}/ofl/comicrelief")
set(COMICRELIEF_DIR "${CMAKE_SOURCE_DIR}/vendor/comicrelief")

# dmc_unrar (GPL-2.0-or-later, see its COPYING): the game as the .rar it was
# shared in, RAR 1.5 to 5, one C file
set(STREETCHAVES_UNRAR_VERSION "ab8bbddfb84f4b5853c8184be84a8c2565f4a4bb")
set(STREETCHAVES_UNRAR_DOWNLOAD "https://raw.githubusercontent.com/DrMcCoy/dmc_unrar/${STREETCHAVES_UNRAR_VERSION}")
set(STREETCHAVES_UNRAR_DIR "${CMAKE_SOURCE_DIR}/vendor/dmc_unrar")

function(streetchaves_download url file)
    if (NOT EXISTS "${file}")
        file(DOWNLOAD "${url}" "${file}" STATUS status)
        list(GET status 0 code)
        if (code)
            file(REMOVE "${file}")
            message(FATAL_ERROR "streetchaves: failed to download ${url}: ${status}")
        endif()
    endif()
endfunction()

foreach(file stb_image.h stb_truetype.h)
    streetchaves_download("${STREETCHAVES_STB_DOWNLOAD}/${file}" "${STREETCHAVES_STB_DIR}/${file}")
endforeach()
streetchaves_download("${STREETCHAVES_LIBRETRO_DOWNLOAD}" "${STREETCHAVES_LIBRETRO_DIR}/libretro.h")
foreach(file dmc_unrar.c COPYING)
    streetchaves_download("${STREETCHAVES_UNRAR_DOWNLOAD}/${file}" "${STREETCHAVES_UNRAR_DIR}/${file}")
endforeach()
foreach(file ComicRelief-Regular.ttf OFL.txt)
    streetchaves_download("${COMICRELIEF_DOWNLOAD}/${file}" "${COMICRELIEF_DIR}/${file}")
endforeach()

# the font, embedded as ComicRelief_Regular[] into streetchaves/font.h of the
# build tree; the header is only rewritten on change
dopo_host_tool(dopo_xxd "${DOPO_DIR}/scripts/xxd.cpp")
set(STREETCHAVES_INCLUDE_GENERATED "${CMAKE_BINARY_DIR}/streetchaves/include")
file(MAKE_DIRECTORY "${STREETCHAVES_INCLUDE_GENERATED}/streetchaves")
execute_process(
    COMMAND "${dopo_xxd_TOOL}" "${COMICRELIEF_DIR}/ComicRelief-Regular.ttf"
    OUTPUT_FILE "${CMAKE_BINARY_DIR}/streetchaves/font.h"
    RESULT_VARIABLE streetchaves_xxd_result
    ERROR_VARIABLE streetchaves_xxd_error)
if (streetchaves_xxd_result)
    message(FATAL_ERROR "streetchaves: failed to embed the font\n${streetchaves_xxd_error}")
endif()
file(COPY_FILE "${CMAKE_BINARY_DIR}/streetchaves/font.h"
    "${STREETCHAVES_INCLUDE_GENERATED}/streetchaves/font.h" ONLY_IF_DIFFERENT)

# the x86 dumped into game/assembly.tar.xz (scripts/dump.nim), unpacked and
# turned into C the games call (scripts/lift.nim): x86v1*.c and their
# declarations for Nim, x86v1.nim; the archive or the lifter changing reconfigures
dopo_nim_tool(streetchaves_lift "${STREETCHAVES_DIR}/scripts/lift.nim")
set(STREETCHAVES_ASSEMBLY "${STREETCHAVES_DIR}/library/game/assembly.tar.xz")
set(STREETCHAVES_LIFTED "${CMAKE_BINARY_DIR}/streetchaves/lifted")
set(streetchaves_unpacked "${CMAKE_BINARY_DIR}/streetchaves/assembly")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${STREETCHAVES_ASSEMBLY}")
file(REMOVE_RECURSE "${streetchaves_unpacked}")
file(MAKE_DIRECTORY "${streetchaves_unpacked}" "${STREETCHAVES_LIFTED}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xf "${STREETCHAVES_ASSEMBLY}"
    WORKING_DIRECTORY "${streetchaves_unpacked}"
    RESULT_VARIABLE streetchaves_unpack_result)
if (streetchaves_unpack_result)
    message(FATAL_ERROR "streetchaves: failed to unpack ${STREETCHAVES_ASSEMBLY}")
endif()
file(GLOB streetchaves_dumps "${streetchaves_unpacked}/*.s")
execute_process(
    COMMAND "${streetchaves_lift_TOOL}" x86v1 "${STREETCHAVES_LIFTED}" ${streetchaves_dumps}
    RESULT_VARIABLE streetchaves_lift_result
    OUTPUT_VARIABLE streetchaves_lift_output
    ERROR_VARIABLE streetchaves_lift_output)
if (streetchaves_lift_result)
    message(FATAL_ERROR "streetchaves: failed to lift the dumps\n${streetchaves_lift_output}")
endif()

# the games in Nim (library/streetchaves.nim and what it imports), with Delphi's
# defaults: no overflow nor range checks
dopo_nim_library(streetchaves_game "${STREETCHAVES_DIR}/library/streetchaves.nim"
    --overflowChecks:off --rangeChecks:off --nimMainPrefix:streetchaves_
    "--path:${STREETCHAVES_LIFTED}")
target_include_directories(streetchaves_game PRIVATE "${STREETCHAVES_DIR}/include")

add_library(streetchaves_unrar STATIC "${STREETCHAVES_UNRAR_DIR}/dmc_unrar.c")
target_include_directories(streetchaves_unrar PUBLIC "${STREETCHAVES_UNRAR_DIR}")
target_compile_definitions(streetchaves_unrar PRIVATE _FILE_OFFSET_BITS=64)
target_compile_options(streetchaves_unrar PRIVATE -w)
set_target_properties(streetchaves_unrar PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE streetchaves_files CONFIGURE_DEPENDS "${STREETCHAVES_DIR}/library/*.c")
file(GLOB streetchaves_lifted "${STREETCHAVES_LIFTED}/x86v1*.c")
list(APPEND streetchaves_files ${streetchaves_lifted})

if (EMSCRIPTEN)
    # RetroArch's web player links one archive: the games' and dmc_unrar's
    # objects go into it
    add_library(streetchaves_libretro STATIC "${streetchaves_files}"
        $<TARGET_OBJECTS:streetchaves_game> $<TARGET_OBJECTS:streetchaves_unrar>)
    set_target_properties(streetchaves_libretro PROPERTIES OUTPUT_NAME "streetchaves_libretro_emscripten")
else()
    add_library(streetchaves_libretro SHARED "${streetchaves_files}")
    target_link_options(streetchaves_libretro PRIVATE -Wl,--no-undefined)
endif()
target_include_directories(streetchaves_libretro PRIVATE "${STREETCHAVES_DIR}/include" "${STREETCHAVES_INCLUDE_GENERATED}")
target_include_directories(streetchaves_libretro SYSTEM PRIVATE "${STREETCHAVES_LIBRETRO_DIR}" "${STREETCHAVES_STB_DIR}")
target_compile_definitions(streetchaves_libretro PRIVATE _DEFAULT_SOURCE)
target_compile_options(streetchaves_libretro PRIVATE -Wall -Wextra -fwrapv)
target_link_libraries(streetchaves_libretro PRIVATE streetchaves_game streetchaves_unrar m)
set_target_properties(streetchaves_libretro PROPERTIES
    C_EXTENSIONS OFF
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

if (EMSCRIPTEN)
    # the web build: a launcher (www/index.html) whose Play opens the game
    # page of each version (www/cybergamba.html, the 1.5A) on the game as hosted
    set(STREETCHAVES_CONTENT_URL
        "https://raw.githubusercontent.com/RodrigoDornelles/RodrigoDornelles/refs/heads/master/abandonwares/StreetChaves_1.5A.rar"
        CACHE STRING "the game the web build's Play and Download fetch")
    set(streetchaves_www "${PORTS_DIST}/streetchaves")
    set(streetchaves_www_outputs "")
    foreach(copy "${STREETCHAVES_DIR}/www/style.css=style.css" "${DOPO_DIR}/www/sw.js=sw.js"
            "${STREETCHAVES_DIR}/assets/320x200.png=banner.png")
        string(REPLACE "=" ";" copy "${copy}")
        list(GET copy 0 source)
        list(GET copy 1 file)
        add_custom_command(OUTPUT "${streetchaves_www}/${file}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${source}" "${streetchaves_www}/${file}"
            DEPENDS "${source}")
        list(APPEND streetchaves_www_outputs "${streetchaves_www}/${file}")
    endforeach()
    include("${DOPO_DIR}/cmake/www.cmake")
    dopo_www_page("${streetchaves_www}/index.html" "${STREETCHAVES_DIR}/www/index.html"
        "title=Street Chaves"
        "description=Street Chaves 1.5A, the 2004 fan-made fighting game of the vila, in the browser."
        "root=../" "back=../"
        "title_before=Street " "title_accent=Chaves" "online=1" "content=${STREETCHAVES_CONTENT_URL}"
        "image=https://raw.githubusercontent.com/RodrigoDornelles/ports/refs/heads/main/streetchaves_libretro/assets/320x200.png")
    dopo_www_page("${streetchaves_www}/cybergamba.html" "${STREETCHAVES_DIR}/www/cybergamba.html" "content=${STREETCHAVES_CONTENT_URL}")
    list(APPEND streetchaves_www_outputs "${streetchaves_www}/index.html" "${streetchaves_www}/cybergamba.html")
    add_custom_target(streetchaves_www ALL DEPENDS ${streetchaves_www_outputs})
endif()
