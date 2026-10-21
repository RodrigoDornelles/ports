# the Balatro patcher page (www/balatro.html): the player's own Balatro.exe
# or Balatro.love is patched in the browser by scripts/patchs.nim built for
# JS, with the patchs of games/balatro, made for Balatro 1.0.1o-FULL
set(BALATRO_DIR "${DOPO_DIR}/games/balatro")

include("${CMAKE_CURRENT_LIST_DIR}/nim.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/www.cmake")

# the page's groups of patchs and their order; on a conflict the first one
# listed wins. Each is dopo_<name>.lua and/or dopo_<name>.fs
set(BALATRO_GROUPS Performance Memory)
set(BALATRO_PERFORMANCE
    dopo_low_graphics
    dopo_crt_toggle
    dopo_reduced_motion
    dopo_solid_background
    dopo_cheap_background
    dopo_render_scale
    dopo_quiet_logs
    dopo_screensize)
set(BALATRO_MEMORY
    dopo_texture_memory
    dopo_no_collabs
    dopo_no_splash_logos
    dopo_slim_fonts
    dopo_font_atlas
    dopo_canvas_release
    dopo_gc_pressure)

set(balatro_www "${PORTS_DIST}/balatro")
set(balatro_outputs "${balatro_www}/patchs.js")
dopo_nim_js(balatro_patchs "${DOPO_DIR}/scripts/patchs.nim" "${balatro_www}/patchs.js")

dopo_www_page("${balatro_www}/index.html" "${DOPO_DIR}/www/balatro.html"
    "title=Balatro"
    "description=Your own Balatro in the browser, or patched lighter for TV boxes and handhelds."
    "root=../" "back=../" "source=/tree/main/dopo/games/balatro"
    "title_before=Bal" "title_accent=a" "title_after=tro")
list(APPEND balatro_outputs "${balatro_www}/index.html")

set(balatro_copies
    "${DOPO_DIR}/www/balatro.js" balatro.js
    "${DOPO_DIR}/assets/balatro.png" banner.png
    "${DOPO_DIR}/defaults/balatro_zaper.txt" defaults_zaper.txt)
set(balatro_groups "")
foreach(group ${BALATRO_GROUPS})
    string(TOUPPER "${group}" key)
    set(patchs "")
    foreach(patch ${BALATRO_${key}})
        set(files "")
        foreach(ext lua fs)
            if (EXISTS "${BALATRO_DIR}/${patch}.${ext}")
                list(APPEND balatro_copies "${BALATRO_DIR}/${patch}.${ext}" "patchs/${patch}.${ext}")
                list(APPEND files "\"${patch}.${ext}\"")
            endif()
        endforeach()
        if (NOT files)
            message(FATAL_ERROR "balatro: no ${BALATRO_DIR}/${patch}.lua or .fs")
        endif()
        list(JOIN files ", " files)
        list(APPEND patchs "    {\"name\": \"${patch}\", \"files\": [${files}]}")
    endforeach()
    list(JOIN patchs ",\n" patchs)
    list(APPEND balatro_groups "  {\"group\": \"${group}\", \"patchs\": [\n${patchs}\n  ]}")
endforeach()
list(JOIN balatro_groups ",\n" balatro_groups)
file(CONFIGURE OUTPUT "${balatro_www}/patchs.json" CONTENT "[\n${balatro_groups}\n]\n")

while (balatro_copies)
    list(POP_FRONT balatro_copies source file)
    add_custom_command(OUTPUT "${balatro_www}/${file}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${source}" "${balatro_www}/${file}"
        DEPENDS "${source}")
    list(APPEND balatro_outputs "${balatro_www}/${file}")
endwhile()

add_custom_target(balatro_www ALL DEPENDS ${balatro_outputs})
