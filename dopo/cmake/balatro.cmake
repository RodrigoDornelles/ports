# the Balatro patcher page (www/balatro.html): the player's own Balatro.exe
# or Balatro.love is patched in the browser by scripts/patchs.nim built for
# JS, with the patchs of games/balatro, made for Balatro 1.0.1o-FULL
set(BALATRO_DIR "${DOPO_DIR}/games/balatro")

include("${CMAKE_CURRENT_LIST_DIR}/nim.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/www.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/lovejs.cmake")

# the page's groups of patchs, a folder each, and their order; on a
# conflict the first one listed wins. Each is <name>.lua and/or <name>.fs
set(BALATRO_GROUPS Performance Memory)
set(BALATRO_PERFORMANCE
    low_graphics
    crt_toggle
    reduced_motion
    solid_background
    cheap_background
    render_scale
    quiet_logs
    screensize)
set(BALATRO_MEMORY
    texture_memory
    no_collabs
    no_splash_logos
    slim_fonts
    font_atlas
    canvas_release
    gc_pressure)

# lovejs is the browser's: love.js's fixes (named after its files) and
# modules, and the patchs Balatro needs to run there, always applied when it
# plays in the browser, never shown nor put in a download
file(GLOB balatro_web CONFIGURE_DEPENDS RELATIVE "${BALATRO_DIR}/lovejs"
    "${BALATRO_DIR}/lovejs/*.lua" "${BALATRO_DIR}/lovejs/*.fs")
set(BALATRO_WEB "")
foreach(file ${balatro_web})
    file(STRINGS "${BALATRO_DIR}/lovejs/${file}" marks REGEX "@patch[ \t]")
    if (marks AND NOT file IN_LIST LOVEJS_NAMES)
        string(REGEX REPLACE "\\.[^.]*$" "" name "${file}")
        list(APPEND BALATRO_WEB "${name}")
    endif()
endforeach()
list(REMOVE_DUPLICATES BALATRO_WEB)

set(balatro_www "${PORTS_DIST}/balatro")
set(balatro_outputs "${balatro_www}/patchs.js")
dopo_nim_js(balatro_patchs "${DOPO_DIR}/scripts/patchs.nim" "${balatro_www}/patchs.js")

dopo_www_page("${balatro_www}/index.html" "${DOPO_DIR}/www/balatro.html"
    "title=Balatro"
    "description=Your own Balatro in the browser, or patched lighter for TV boxes and handhelds."
    "root=../" "back=../" "source=/tree/main/dopo/games/balatro"
    "title_before=Bal" "title_accent=a" "title_after=tro"
    "image=https://raw.githubusercontent.com/RodrigoDornelles/ports/refs/heads/main/dopo/assets/balatro/320x200.png")
list(APPEND balatro_outputs "${balatro_www}/index.html")

set(balatro_copies
    "${DOPO_DIR}/www/balatro.js" balatro.js
    "${DOPO_DIR}/assets/balatro/320x200.png" banner.png
    "${DOPO_DIR}/assets/balatro/192x192.png" icon-192x192.png
    "${DOPO_DIR}/assets/balatro/512x512.png" icon-512x512.png
    "${DOPO_DIR}/defaults/balatro_zaper.txt" defaults_zaper.txt)
# the JSON list of the patchs in dir (relative to games/balatro), and
# their files to the copies
function(balatro_list out_var dir)
    set(entries "")
    foreach(patch ${ARGN})
        set(files "")
        foreach(ext lua fs)
            if (EXISTS "${BALATRO_DIR}/${dir}/${patch}.${ext}")
                list(APPEND balatro_copies "${BALATRO_DIR}/${dir}/${patch}.${ext}" "patchs/${patch}.${ext}")
                list(APPEND files "\"${patch}.${ext}\"")
            endif()
        endforeach()
        if (NOT files)
            message(FATAL_ERROR "balatro: no ${BALATRO_DIR}/${dir}/${patch}.lua or .fs")
        endif()
        list(JOIN files ", " files)
        list(APPEND entries "{\"name\": \"${patch}\", \"files\": [${files}]}")
    endforeach()
    list(JOIN entries ",\n    " entries)
    set(${out_var} "${entries}" PARENT_SCOPE)
    set(balatro_copies "${balatro_copies}" PARENT_SCOPE)
endfunction()

set(balatro_groups "")
foreach(group ${BALATRO_GROUPS})
    string(TOUPPER "${group}" key)
    string(TOLOWER "${group}" dir)
    balatro_list(entries "${dir}" ${BALATRO_${key}})
    list(APPEND balatro_groups "  {\"group\": \"${group}\", \"patchs\": [\n    ${entries}\n  ]}")
endforeach()
list(JOIN balatro_groups ",\n" balatro_groups)
file(CONFIGURE OUTPUT "${balatro_www}/patchs.json" CONTENT "[\n${balatro_groups}\n]\n")

balatro_list(entries lovejs ${BALATRO_WEB})
file(CONFIGURE OUTPUT "${balatro_www}/web.json" CONTENT "[\n    ${entries}\n]\n")

# the game in the browser, installable: love.js with the game the page
# leaves on the device
dopo_lovejs(balatro_play_outputs "${balatro_www}/play" "${BALATRO_DIR}/lovejs")
list(APPEND balatro_outputs ${balatro_play_outputs})
dopo_www_page("${balatro_www}/play/index.html" "${DOPO_DIR}/www/balatro_play.html" "root=../../")
list(APPEND balatro_outputs "${balatro_www}/play/index.html")
list(APPEND balatro_copies
    "${DOPO_DIR}/www/balatro_play.webmanifest" play/manifest.webmanifest
    "${DOPO_DIR}/www/sw.js" play/sw.js)

while (balatro_copies)
    list(POP_FRONT balatro_copies source file)
    add_custom_command(OUTPUT "${balatro_www}/${file}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${source}" "${balatro_www}/${file}"
        DEPENDS "${source}")
    list(APPEND balatro_outputs "${balatro_www}/${file}")
endwhile()

add_custom_target(balatro_www ALL DEPENDS ${balatro_outputs})
