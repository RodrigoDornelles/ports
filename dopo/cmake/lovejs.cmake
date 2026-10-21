# love.js, 2dengine's LÖVE 11.5 for the browser (MIT), fetched from jsDelivr
# at a fixed commit. A game's lovejs folder goes on top of it: @patch fixes
# to love.js's own files, named after them, and Lua modules it lacks; its
# other patches are the game's, which the page applies, not love.js's.
# It needs no special headers: this build runs without threads.
include_guard(GLOBAL)

set(LOVEJS_VERSION "9355186de22db13bd88bf2a0db75d2925647d036")
set(LOVEJS_DOWNLOAD "https://cdn.jsdelivr.net/gh/2dengine/love.js@${LOVEJS_VERSION}")
set(LOVEJS_DIR "${CMAKE_SOURCE_DIR}/vendor/lovejs")
set(LOVEJS_FILES
    player.js style.css license.txt
    lua/normalize1.lua lua/normalize2.lua
    11.5/love.js 11.5/love.wasm 11.5/license.txt)

# the names a fix to love.js has, the rest being the game's patches
set(LOVEJS_NAMES "")
foreach(file ${LOVEJS_FILES})
    get_filename_component(name "${file}" NAME)
    list(APPEND LOVEJS_NAMES "${name}")
endforeach()

set(lovejs_stamp "${LOVEJS_DIR}/.dopo-${LOVEJS_VERSION}")
if (NOT EXISTS "${lovejs_stamp}")
    file(REMOVE_RECURSE "${LOVEJS_DIR}")
    foreach(file ${LOVEJS_FILES})
        file(DOWNLOAD "${LOVEJS_DOWNLOAD}/${file}" "${LOVEJS_DIR}/${file}" STATUS status)
        list(GET status 0 code)
        if (code)
            message(FATAL_ERROR "lovejs: failed to download ${file}: ${status}")
        endif()
    endforeach()
    file(TOUCH "${lovejs_stamp}")
endif()

# copies the player into output_dir: love.js with source_dir's fixes, and
# its modules in lua/; out_var gets the files written
function(dopo_lovejs out_var output_dir source_dir)
    dopo_split_patchs(found modules "${source_dir}" "${source_dir}/*")
    list(FILTER modules INCLUDE REGEX "\\.lua$")
    set(patchs "")
    foreach(patch ${found})
        get_filename_component(name "${patch}" NAME)
        if (name IN_LIST LOVEJS_NAMES)
            list(APPEND patchs "${patch}")
        endif()
    endforeach()
    get_filename_component(name "${output_dir}" NAME)
    set(patched_dir "${CMAKE_BINARY_DIR}/lovejs/${name}")
    dopo_apply_patchs(patched "${LOVEJS_DIR}" "${patched_dir}" ${patchs})

    set(copies "")
    foreach(file ${LOVEJS_FILES})
        if (file IN_LIST patched)
            list(APPEND copies "${patched_dir}/${file}" "${file}")
        else()
            list(APPEND copies "${LOVEJS_DIR}/${file}" "${file}")
        endif()
    endforeach()
    foreach(module ${modules})
        list(APPEND copies "${source_dir}/${module}" "lua/${module}")
    endforeach()

    set(outputs "")
    while (copies)
        list(POP_FRONT copies source file)
        add_custom_command(OUTPUT "${output_dir}/${file}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${source}" "${output_dir}/${file}"
            DEPENDS "${source}")
        list(APPEND outputs "${output_dir}/${file}")
    endwhile()
    set(${out_var} "${outputs}" PARENT_SCOPE)
endfunction()
