# the site's pages: mustache templates rendered at build time by
# scripts/www.nim, with the partials of www/partials that every page shares
# (head, header) and the stylesheet they link, www/site.css
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/nim.cmake")

set(NIM_MUSTACHE_VERSION "v0.4.3")
set(NIM_MUSTACHE_DOWNLOAD "https://github.com/soasme/nim-mustache/archive/refs/tags/${NIM_MUSTACHE_VERSION}.tar.gz")
set(NIM_MUSTACHE_DIR "${CMAKE_SOURCE_DIR}/vendor/nim_mustache")

if (NOT EXISTS "${NIM_MUSTACHE_DIR}/src/mustache.nim")
    FetchContent_Populate(nim_mustache URL ${NIM_MUSTACHE_DOWNLOAD} SOURCE_DIR ${NIM_MUSTACHE_DIR})
endif()

dopo_nim_tool(dopo_www "${DOPO_DIR}/scripts/www.nim" "--path:${NIM_MUSTACHE_DIR}/src")

# the stylesheet and the site's icons, at the root of the site
set(dopo_site_outputs "")
foreach(source www/site.css assets/favicon.ico assets/favicon-16x16.png assets/favicon-32x32.png
        assets/apple-touch-icon.png assets/android-chrome-192x192.png assets/android-chrome-512x512.png)
    get_filename_component(file "${source}" NAME)
    add_custom_command(OUTPUT "${PORTS_DIST}/${file}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${DOPO_DIR}/${source}" "${PORTS_DIST}/${file}"
        DEPENDS "${DOPO_DIR}/${source}")
    list(APPEND dopo_site_outputs "${PORTS_DIST}/${file}")
endforeach()
add_custom_target(dopo_site ALL DEPENDS ${dopo_site_outputs})

# renders template into output; the rest is passed to www.nim: more
# --partials dirs, --data files and key=value
function(dopo_www_page output template)
    file(GLOB partials CONFIGURE_DEPENDS "${DOPO_DIR}/www/partials/*.mustache")
    set(depends "")
    set(next "")
    foreach(arg ${ARGN})
        if (next STREQUAL "--data")
            list(APPEND depends "${arg}")
        elseif (next STREQUAL "--partials")
            file(GLOB own CONFIGURE_DEPENDS "${arg}/*.mustache")
            list(APPEND depends ${own})
        endif()
        set(next "${arg}")
    endforeach()
    add_custom_command(OUTPUT "${output}"
        COMMAND "${dopo_www_TOOL}" --partials "${DOPO_DIR}/www/partials"
            --template "${template}" --output "${output}" ${ARGN}
        DEPENDS "${template}" ${partials} ${depends} "${dopo_www_TOOL}"
        VERBATIM)
endfunction()
