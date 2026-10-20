# RetroArch's web player (Makefile.emscripten) with a core linked in, plus
# the dopo patchs of source/retroarch/: the dopo netplay calls, with rooms and
# netplay over WebRTC (Trystero), so cores list, host and join rooms from
# their own menus in the browser.
set(RETROARCH_VERSION "v1.22.2")
set(RETROARCH_DOWNLOAD "https://github.com/libretro/RetroArch/archive/refs/tags/${RETROARCH_VERSION}.tar.gz")
set(RETROARCH_SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/retroarch_source")

get_filename_component(RETROARCH_DIR "${CMAKE_CURRENT_LIST_DIR}/../source/retroarch" ABSOLUTE)

if (NOT EXISTS "${RETROARCH_SOURCE_DIR}/Makefile.emscripten")
    FetchContent_Populate(retroarch URL ${RETROARCH_DOWNLOAD} SOURCE_DIR ${RETROARCH_SOURCE_DIR})
endif()

# *.c carry /** @patch */ hunks on top of RetroArch's sources; the *.js
# next to them are emscripten libraries linked into the player, except
# player.js, the module the pages import
dopo_split_patchs(retroarch_patch_files retroarch_other_files
    "${RETROARCH_DIR}" "${RETROARCH_DIR}/*.c")
if (retroarch_other_files)
    message(FATAL_ERROR "retroarch: ${retroarch_other_files} carry no @patch hunk")
endif()
file(GLOB retroarch_js_libraries CONFIGURE_DEPENDS "${RETROARCH_DIR}/*.js")
list(REMOVE_ITEM retroarch_js_libraries "${RETROARCH_DIR}/player.js")
list(SORT retroarch_js_libraries)

set(RETROARCH_PATCHS_OUTPUT "${CMAKE_BINARY_DIR}/retroarch/patchs")
dopo_apply_patchs(retroarch_patched_files "${RETROARCH_SOURCE_DIR}" "${RETROARCH_PATCHS_OUTPUT}"
    ${retroarch_patch_files})

# Makefile.emscripten builds in its own tree, so it runs on a copy of the
# sources with the patched files over it; files no longer patched go back
# to upstream's
set(RETROARCH_TREE "${CMAKE_BINARY_DIR}/retroarch/tree")
set(retroarch_tree_stamp "${RETROARCH_TREE}/.dopo-${RETROARCH_VERSION}")
if (NOT EXISTS "${retroarch_tree_stamp}")
    message(STATUS "retroarch: copying ${RETROARCH_VERSION} to the build tree")
    file(REMOVE_RECURSE "${RETROARCH_TREE}")
    file(COPY "${RETROARCH_SOURCE_DIR}/" DESTINATION "${RETROARCH_TREE}" PATTERN ".git" EXCLUDE)
    file(TOUCH "${retroarch_tree_stamp}")
endif()
set(retroarch_patched_list "${CMAKE_BINARY_DIR}/retroarch/patched.txt")
if (EXISTS "${retroarch_patched_list}")
    file(STRINGS "${retroarch_patched_list}" retroarch_was_patched)
    foreach(file ${retroarch_was_patched})
        if (NOT file IN_LIST retroarch_patched_files)
            file(COPY_FILE "${RETROARCH_SOURCE_DIR}/${file}" "${RETROARCH_TREE}/${file}")
        endif()
    endforeach()
endif()
string(REPLACE ";" "\n" retroarch_patched_text "${retroarch_patched_files}")
file(WRITE "${retroarch_patched_list}" "${retroarch_patched_text}\n")

set(retroarch_sync_commands "")
set(retroarch_patched_paths "")
foreach(file ${retroarch_patched_files})
    list(APPEND retroarch_sync_commands
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${RETROARCH_PATCHS_OUTPUT}/${file}" "${RETROARCH_TREE}/${file}")
    list(APPEND retroarch_patched_paths "${RETROARCH_PATCHS_OUTPUT}/${file}")
endforeach()

# Makefile.emscripten with the dopo headers and libraries on top
set(retroarch_makefile "${CMAKE_BINARY_DIR}/retroarch/dopo.mk")
set(retroarch_js_flags "")
foreach(library ${retroarch_js_libraries})
    string(APPEND retroarch_js_flags " --js-library ${library}")
endforeach()
file(WRITE "${retroarch_makefile}" "include Makefile.emscripten
DEFINES += -I${CMAKE_SOURCE_DIR}/dopo/include
LDFLAGS +=${retroarch_js_flags} -lidbfs.js
")

cmake_host_system_information(RESULT retroarch_jobs QUERY NUMBER_OF_LOGICAL_CORES)

# builds <name>_libretro.js and .wasm with core_target linked in, into
# output_dir
function(retroarch_web_player core_target name output_dir)
    set(outputs "${output_dir}/${name}_libretro.js" "${output_dir}/${name}_libretro.wasm")
    add_custom_command(
        OUTPUT ${outputs}
        ${retroarch_sync_commands}
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${core_target}>" "${RETROARCH_TREE}/libretro_emscripten.a"
        COMMAND "${CMAKE_COMMAND}" -E env
            "CC=${EMSCRIPTEN_ROOT_PATH}/emcc" "CXX=${EMSCRIPTEN_ROOT_PATH}/em++"
            "LD=${EMSCRIPTEN_ROOT_PATH}/emcc" "AR=${EMSCRIPTEN_ROOT_PATH}/emar"
            make -C "${RETROARCH_TREE}" -f "${retroarch_makefile}" -j${retroarch_jobs}
                LIBRETRO=${name}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
        COMMAND "${CMAKE_COMMAND}" -E copy
            "${RETROARCH_TREE}/${name}_libretro.js" "${RETROARCH_TREE}/${name}_libretro.wasm"
            "${output_dir}"
        DEPENDS ${core_target} ${retroarch_patched_paths} ${retroarch_js_libraries}
            "${retroarch_makefile}"
        COMMENT "retroarch: web player with ${name}"
        VERBATIM)
    add_custom_target(retroarch_${name} ALL DEPENDS ${outputs})
endfunction()

# what every core's pages share: the player
add_custom_command(OUTPUT "${PORTS_DIST}/retroarch/player.js"
    COMMAND "${CMAKE_COMMAND}" -E copy "${RETROARCH_DIR}/player.js" "${PORTS_DIST}/retroarch/player.js"
    DEPENDS "${RETROARCH_DIR}/player.js")
add_custom_target(retroarch_www ALL DEPENDS "${PORTS_DIST}/retroarch/player.js")

if (TARGET prboomtv_libretro)
    retroarch_web_player(prboomtv_libretro prboomtv "${PORTS_DIST}/prboomtv")
endif()
