set(PRBOOM_VERSION "c6e0fcb8325fc7969c91387f56c63433de0f1a53")
set(PRBOOM_DOWNLOAD "https://github.com/libretro/libretro-prboom/archive/${PRBOOM_VERSION}.tar.gz")
set(PRBOOM_DIR "${CMAKE_SOURCE_DIR}/vendor/prboom")

get_filename_component(PRBOOMTV_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(ODAMEX_VERSION "3063c2e8fd8938ad16736c6715c5e83142d640a4")
set(ODAMEX_DOWNLOAD "https://raw.githubusercontent.com/odamex/odamex/${ODAMEX_VERSION}")
set(ODAMEX_DIR "${CMAKE_SOURCE_DIR}/vendor/odamex")

set(STB_VERSION "2c980bb59875b0d32144a71867fbdebb2f77cd20")
set(STB_DOWNLOAD "https://raw.githubusercontent.com/nothings/stb/${STB_VERSION}/stb_image.h")
set(STB_DIR "${CMAKE_SOURCE_DIR}/vendor/stb")

# JSON for the netplay lobby's room list (MIT, a single header)
set(JSMN_VERSION "v1.1.0")
set(JSMN_DOWNLOAD "https://raw.githubusercontent.com/zserge/jsmn/${JSMN_VERSION}")
set(JSMN_DIR "${CMAKE_SOURCE_DIR}/vendor/jsmn")

if (NOT EXISTS "${PRBOOM_DIR}/src")
    FetchContent_Populate(prboom URL ${PRBOOM_DOWNLOAD} SOURCE_DIR ${PRBOOM_DIR})
endif()

# version major: PrBoom's own (PACKAGE_VERSION in its config.h)
file(STRINGS "${PRBOOM_DIR}/src/config.h" prboom_package_version
    REGEX "^#define PACKAGE_VERSION \"[0-9]+\.")
if (NOT prboom_package_version MATCHES "\"([0-9]+)\.")
    message(FATAL_ERROR "prboomtv: no PACKAGE_VERSION in ${PRBOOM_DIR}/src/config.h")
endif()
set(PRBOOMTV_VERSION_MAJOR "${CMAKE_MATCH_1}")

# version patch: the upstream commit's date as yymmdd (committer date,
# UTC), e.g. 261005 for 2026-10-05; the date is asked to GitHub once per
# commit and kept next to the sources
set(PRBOOM_DATE_FILE "${PRBOOM_DIR}/.prboomtv-date-${PRBOOM_VERSION}")
if (NOT EXISTS "${PRBOOM_DATE_FILE}")
    set(prboom_commit_json "${CMAKE_BINARY_DIR}/prboomtv/commit.json")
    file(DOWNLOAD "https://api.github.com/repos/libretro/libretro-prboom/commits/${PRBOOM_VERSION}"
        "${prboom_commit_json}" STATUS status)
    list(GET status 0 code)
    if (code)
        message(FATAL_ERROR "prboomtv: failed to get the date of commit ${PRBOOM_VERSION}: ${status}")
    endif()
    file(READ "${prboom_commit_json}" prboom_commit)
    string(JSON prboom_date GET "${prboom_commit}" commit committer date)
    file(WRITE "${PRBOOM_DATE_FILE}" "${prboom_date}")
endif()
file(READ "${PRBOOM_DATE_FILE}" prboom_date)
if (NOT prboom_date MATCHES "^[0-9][0-9]([0-9][0-9])-([0-9][0-9])-([0-9][0-9])")
    message(FATAL_ERROR "prboomtv: unexpected commit date '${prboom_date}' in ${PRBOOM_DATE_FILE}")
endif()
set(PRBOOMTV_VERSION_PATCH "${CMAKE_MATCH_1}${CMAKE_MATCH_2}${CMAKE_MATCH_3}")
message(STATUS "prboomtv: based on libretro-prboom ${PRBOOM_VERSION} of ${prboom_date}: version major ${PRBOOMTV_VERSION_MAJOR}, patch ${PRBOOMTV_VERSION_PATCH}")

# only the Odamex big font (FONTB01..63, '!'..'_'), its palette and license;
# the whole repository would be ~80MB for a few kilobytes of glyphs
if (NOT EXISTS "${ODAMEX_DIR}/LICENSE")
    set(odamex_files wad/doom.gpl README.md)
    foreach(i RANGE 1 63)
        string(LENGTH "${i}" digits)
        if (digits EQUAL 1)
            set(i "0${i}")
        endif()
        list(APPEND odamex_files "wad/graphics/fontb${i}.png")
    endforeach()
    foreach(file ${odamex_files} LICENSE)
        file(DOWNLOAD "${ODAMEX_DOWNLOAD}/${file}" "${ODAMEX_DIR}/${file}" STATUS status)
        list(GET status 0 code)
        if (code)
            message(FATAL_ERROR "prboomtv: failed to download odamex ${file}: ${status}")
        endif()
    endforeach()
endif()

if (NOT EXISTS "${STB_DIR}/stb_image.h")
    file(DOWNLOAD "${STB_DOWNLOAD}" "${STB_DIR}/stb_image.h")
endif()

if (NOT EXISTS "${JSMN_DIR}/jsmn.h")
    foreach(file jsmn.h LICENSE)
        file(DOWNLOAD "${JSMN_DOWNLOAD}/${file}" "${JSMN_DIR}/${file}" STATUS status)
        list(GET status 0 code)
        if (code)
            message(FATAL_ERROR "prboomtv: failed to download jsmn ${file}: ${status}")
        endif()
    endforeach()
endif()

# upstream Makefile.common (unix, threads on, fluidsynth off)
set(prboomtv_upstream_files
    libretro/libretro.c
    libretro/libretro_sound.c
    libretro/libretro-common/compat/compat_strcasestr.c
    libretro/libretro-common/encodings/encoding_utf.c
    libretro/libretro-common/compat/compat_snprintf.c
    libretro/libretro-common/compat/compat_strl.c
    libretro/libretro-common/compat/compat_posix_string.c
    libretro/libretro-common/compat/fopen_utf8.c
    libretro/libretro-common/streams/file_stream.c
    libretro/libretro-common/streams/file_stream_transforms.c
    libretro/libretro-common/string/stdstring.c
    libretro/libretro-common/string/rstrtod.c
    libretro/libretro-common/vfs/vfs_implementation.c
    libretro/libretro-common/file/file_path.c
    libretro/libretro-common/file/file_path_io.c
    libretro/libretro-common/time/rtime.c
    libretro/libretro-common/formats/png/rpng.c
    libretro/libretro-common/formats/png/rpng_apng.c
    libretro/libretro-common/formats/jpeg/rjpeg.c
    libretro/libretro-common/formats/wav/rwav.c
    libretro/libretro-common/streams/trans_stream.c
    libretro/libretro-common/streams/trans_stream_pipe.c
    libretro/libretro-common/streams/trans_stream_deflate.c
    libretro/libretro-common/encodings/encoding_deflate.c
    libretro/libretro-common/encodings/encoding_crc32.c
    libretro/libretro-common/formats/vorbis/rvorbis.c
    libretro/libretro-common/formats/mp3/rmp3.c
    libretro/libretro-common/formats/mod/rmodtracker.c
    libretro/libretro-common/formats/data_transfer.c
    libretro/libretro-common/formats/audio_transfer.c
    libretro/libretro-common/formats/image_transfer.c
    libretro/libretro-common/memmap/memmap.c
    libretro/libretro-common/vfs/vfs_hybrid.c
    libretro/libretro-common/file/retro_dirent.c
    src/am_map.c
    src/i_cpu_features.c
    src/d_deh.c
    src/dsda_hacked.c
    src/d_items.c
    src/d_main.c
    src/doomstat.c
    src/dstrings.c
    src/f_finale.c
    src/f_wipe.c
    src/g_game.c
    src/hu_lib.c
    src/hu_stuff.c
    src/info.c
    src/m_argv.c
    src/m_bbox.c
    src/m_cheat.c
    src/m_menu.c
    src/m_misc.c
    src/m_random.c
    src/p_ceilng.c
    src/p_doors.c
    src/p_enemy.c
    src/p_floor.c
    src/p_inter.c
    src/p_lights.c
    src/p_map.c
    src/p_maputl.c
    src/p_mobj.c
    src/p_plats.c
    src/p_pspr.c
    src/p_saveg.c
    src/p_setup.c
    src/scanner.c
    src/udmf.c
    src/w_pk3.c
    src/map_format.c
    src/heretic/info.c
    src/heretic/sounds.c
    src/heretic/p_action.c
    src/heretic/in_lude.c
    src/heretic/f_finale.c
    src/hexen/sounds.c
    src/hexen/info.c
    src/hexen/p_mapinfo.c
    src/hexen/p_lightning.c
    src/hexen/p_anim.c
    src/hexen/f_finale.c
    src/hexen/in_lude.c
    src/hexen/sn_sonix.c
    src/hexen/p_acs.c
    src/hexen/po_man.c
    src/hexen/sv_save.c
    src/hexen/p_spec_hexen.c
    src/p_sight.c
    src/p_spec.c
    src/p_switch.c
    src/p_telept.c
    src/p_tick.c
    src/p_user.c
    src/r_bsp.c
    src/r_data.c
    src/r_draw.c
    src/r_drawtc.c
    src/vid_mode.c
    src/r_main.c
    src/r_plane.c
    src/r_drawcmd.c
    src/r_rendermt.c
    src/r_segs.c
    src/r_sky.c
    src/r_things.c
    src/r_patch.c
    src/s_sound.c
    src/sounds.c
    src/st_lib.c
    src/st_stuff.c
    src/tables.c
    src/v_video.c
    src/w_wad.c
    src/z_zone.c
    src/w_memcache.c
    src/r_fps.c
    src/r_filter.c
    src/p_genlin.c
    src/r_demo.c
    src/z_bmalloc.c
    src/lprintf.c
    src/wi_stuff.c
    src/d_client.c
    src/memio.c
    src/mus2mid.c
    src/dbopl.c
    src/opl.c
    src/opl_queue.c
    src/oplplayer.c
    src/flplayer.c
    src/libretro_midiout.c
    src/sc55.c
    src/sc55player.c
    src/sc88.c
    src/midifile.c
    src/madplayer.c
    src/modplayer.c
    src/oggplayer.c
    src/u_scanner.c
    src/u_mapinfo.c
    src/u_musinfo.c
    src/u_decorate.c
    src/u_zsndinfo.c
    src/p_slope.c
    src/p_vslope.c
    src/p_skybox.c
    src/p_sectorportal.c
    src/p_lineportal.c
    src/p_zacs.c
    src/p_conversation.c
    src/u_png.c
    src/u_ztextures.c
    src/u_zsecact.c
    src/p_ffloor.c
    src/u_zmapinfo.c
    src/u_zanimdefs.c
    src/u_brightmap.c
    src/u_dynlight.c
    src/r_dynlight.c
    src/u_voxel.c
    src/r_voxel.c
    src/u_decaldef.c
    src/r_decal.c
    libretro/libretro-common/rthreads/rthreads.c
    libretro/libretro-common/rthreads/retro_eventcount.c
)

if (EMSCRIPTEN)
    # linked into RetroArch (retroarch/cmake/retroarch.cmake), which brings
    # its own libretro-common, as upstream's STATIC_LINKING does; and no
    # threads, so the page needs no SharedArrayBuffer
    list(REMOVE_ITEM prboomtv_upstream_files
        libretro/libretro-common/vfs/vfs_hybrid.c
        libretro/libretro-common/file/retro_dirent.c
        libretro/libretro-common/rthreads/rthreads.c
        libretro/libretro-common/rthreads/retro_eventcount.c)
endif()

# files in library/core/ either carry /** @patch */ hunks applied on top of upstream
# files (see dopo/scripts/dopo_patchs.cpp), or mirror the upstream layout and
# replace the original ones, or are new files compiled as they are
dopo_split_patchs(prboomtv_patch_files prboomtv_override_files
    "${PRBOOMTV_DIR}/library/core" "${PRBOOMTV_DIR}/library/core/*.c")

# the Odamex big font, converted into an embedded WAD for the menus
set(PRBOOMTV_GENERATED "${CMAKE_BINARY_DIR}/prboomtv/generated")
file(MAKE_DIRECTORY "${PRBOOMTV_GENERATED}")
dopo_host_tool(prboomtv_font "${PRBOOMTV_DIR}/scripts/prboomtv_font.cpp" -isystem "${STB_DIR}")
execute_process(
    COMMAND "${prboomtv_font_TOOL}"
        --glyphs "${ODAMEX_DIR}/wad/graphics"
        --palette "${ODAMEX_DIR}/wad/doom.gpl"
        --wad "${PRBOOMTV_GENERATED}/dopo_wad_data.h"
        --font "${PRBOOMTV_GENERATED}/dopo_font_data.h"
    RESULT_VARIABLE prboomtv_font_result
    ERROR_VARIABLE prboomtv_font_error)
if (prboomtv_font_result)
    message(FATAL_ERROR "prboomtv: failed to convert the Odamex font\n${prboomtv_font_error}")
endif()

set(PRBOOMTV_PATCHS_OUTPUT "${CMAKE_BINARY_DIR}/prboomtv/patchs")
dopo_apply_patchs(prboomtv_patched_files "${PRBOOM_DIR}" "${PRBOOMTV_PATCHS_OUTPUT}" ${prboomtv_patch_files})

set(prboomtv_files "")
set(prboomtv_patched_dirs "")
foreach(file ${prboomtv_patched_files})
    if (NOT file IN_LIST prboomtv_upstream_files)
        message(FATAL_ERROR "prboomtv: ${file} is patched but is not compiled by the core")
    elseif (file IN_LIST prboomtv_override_files)
        message(FATAL_ERROR "prboomtv: ${file} is both patched and replaced in library/core/")
    endif()
    # the patched copy lives in the build tree, so its quoted includes need
    # the original directory to keep resolving like upstream
    get_filename_component(dir "${PRBOOM_DIR}/${file}" DIRECTORY)
    list(APPEND prboomtv_patched_dirs "${dir}")
endforeach()
foreach(file ${prboomtv_upstream_files})
    if (file IN_LIST prboomtv_override_files)
        list(APPEND prboomtv_files "${PRBOOMTV_DIR}/library/core/${file}")
    elseif (file IN_LIST prboomtv_patched_files)
        list(APPEND prboomtv_files "${PRBOOMTV_PATCHS_OUTPUT}/${file}")
    else()
        list(APPEND prboomtv_files "${PRBOOM_DIR}/${file}")
    endif()
endforeach()
foreach(file ${prboomtv_override_files})
    if (NOT file IN_LIST prboomtv_upstream_files)
        list(APPEND prboomtv_files "${PRBOOMTV_DIR}/library/core/${file}")
    endif()
endforeach()
list(REMOVE_DUPLICATES prboomtv_patched_dirs)

if (EMSCRIPTEN)
    add_library(prboomtv_libretro STATIC "${prboomtv_files}")
else()
    add_library(prboomtv_libretro SHARED "${prboomtv_files}")
endif()
target_include_directories(prboomtv_libretro BEFORE PRIVATE "${PRBOOMTV_DIR}/include" "${CMAKE_SOURCE_DIR}/dopo/include")
target_include_directories(prboomtv_libretro SYSTEM PRIVATE
    "${PRBOOM_DIR}" "${PRBOOM_DIR}/src" "${PRBOOM_DIR}/libretro"
    "${PRBOOM_DIR}/libretro/libretro-common/include" ${prboomtv_patched_dirs}
    "${PRBOOMTV_GENERATED}" "${JSMN_DIR}")
target_compile_definitions(prboomtv_libretro PRIVATE
    HAVE_RVORBIS HAVE_RMP3 HAVE_RMODTRACKER HAVE_RWAV HAVE_RPNG HAVE_RJPEG
    INLINE=inline _POSIX_C_SOURCE=199309L _DEFAULT_SOURCE
    DOPO_VERSION_MAJOR=${PRBOOMTV_VERSION_MAJOR} DOPO_VERSION_PATCH=${PRBOOMTV_VERSION_PATCH})
target_compile_options(prboomtv_libretro PRIVATE -Wall -W -Wno-unused-parameter -fomit-frame-pointer)
set_target_properties(prboomtv_libretro PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS OFF
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
if (EMSCRIPTEN)
    # upstream's platform=emscripten. RetroArch brings its own, older
    # libretro-common: two symbols it also defines in objects the core
    # still pulls in (its crc32 file has functions RetroArch's lacks,
    # rmodtracker embeds ibxm) keep a name of their own, and the two
    # functions whose signature changed since go through
    # library/core/dopo/web.c
    target_compile_definitions(prboomtv_libretro PRIVATE STATIC_LINKING HAVE_STRLWR
        encoding_crc32=prboomtv_encoding_crc32 IBXM_VERSION=prboomtv_IBXM_VERSION)
    set(prboomtv_web_callers "")
    foreach(file ${prboomtv_files})
        if (NOT file MATCHES "/libretro-common/")
            list(APPEND prboomtv_web_callers "${file}")
        endif()
    endforeach()
    set_source_files_properties(${prboomtv_web_callers} PROPERTIES COMPILE_DEFINITIONS
        "path_get_size=prboomtv_path_get_size;rpng_process_image=prboomtv_rpng_process_image")
    set_target_properties(prboomtv_libretro PROPERTIES OUTPUT_NAME "prboomtv_libretro_emscripten")
else()
    target_compile_definitions(prboomtv_libretro PRIVATE HAVE_THREADS HAVE_MMAP)
    target_link_libraries(prboomtv_libretro PRIVATE m pthread)
    set_target_properties(prboomtv_libretro PROPERTIES
        OUTPUT_NAME "prboomtv_libretro"
        LINK_FLAGS "-Wl,--version-script=${PRBOOM_DIR}/libretro/link.T -Wl,--no-undefined")
endif()

if (EMSCRIPTEN)
    set(PRBOOMTV_WEB_GAMES
        "doom1:Doom (Shareware)"
        "freedoom1:Freedoom: Phase 1"
        "freedoom2:Freedoom: Phase 2"
        CACHE STRING "games of the web build, as id:title")

    set(prboomtv_www "${PORTS_DIST}/prboomtv")
    set(prboomtv_www_outputs "")
    foreach(file index.html style.css)
        add_custom_command(OUTPUT "${prboomtv_www}/${file}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${PRBOOMTV_DIR}/www/${file}" "${prboomtv_www}/${file}"
            DEPENDS "${PRBOOMTV_DIR}/www/${file}")
        list(APPEND prboomtv_www_outputs "${prboomtv_www}/${file}")
    endforeach()

    find_program(PRBOOMTV_ZSTD zstd REQUIRED)
    set(prboomtv_games_archive "${PRBOOMTV_DIR}/wads.tar.zst")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${prboomtv_games_archive}")
    execute_process(
        COMMAND "${PRBOOMTV_ZSTD}" -dcq --long=30 "${prboomtv_games_archive}"
        COMMAND tar -t
        OUTPUT_VARIABLE prboomtv_game_wads
        RESULT_VARIABLE prboomtv_games_result
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (prboomtv_games_result)
        message(FATAL_ERROR "prboomtv: cannot read ${prboomtv_games_archive}")
    endif()
    string(REPLACE "\n" ";" prboomtv_game_wads "${prboomtv_game_wads}")

    set(prboomtv_games_json "")
    foreach(game ${PRBOOMTV_WEB_GAMES})
        if (NOT game MATCHES "^([a-z0-9_]+):(.+)$")
            message(FATAL_ERROR "prboomtv: '${game}' in PRBOOMTV_WEB_GAMES is not id:title")
        endif()
        set(GAME_ID "${CMAKE_MATCH_1}")
        set(GAME_TITLE "${CMAKE_MATCH_2}")
        if (NOT "${GAME_ID}.wad" IN_LIST prboomtv_game_wads)
            message(FATAL_ERROR "prboomtv: ${GAME_ID}.wad is not in ${prboomtv_games_archive}")
        endif()
        configure_file("${PRBOOMTV_DIR}/www/game.html" "${prboomtv_www}/${GAME_ID}/index.html" @ONLY)
        string(REPLACE "\"" "\\\"" title "${GAME_TITLE}")
        if (prboomtv_games_json)
            string(APPEND prboomtv_games_json ",\n")
        endif()
        string(APPEND prboomtv_games_json "  {\"id\": \"${GAME_ID}\", \"title\": \"${title}\"}")
    endforeach()
    file(CONFIGURE OUTPUT "${prboomtv_www}/games.json" CONTENT "[\n${prboomtv_games_json}\n]\n")

    set(prboomtv_roms "${prboomtv_www}/roms")
    set(prboomtv_games_tar "${CMAKE_BINARY_DIR}/prboomtv/games.tar")
    list(TRANSFORM prboomtv_game_wads PREPEND "${prboomtv_roms}/" OUTPUT_VARIABLE prboomtv_rom_outputs)
    add_custom_command(OUTPUT ${prboomtv_rom_outputs}
        COMMAND "${PRBOOMTV_ZSTD}" -dqf --long=30 "${prboomtv_games_archive}" -o "${prboomtv_games_tar}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${prboomtv_roms}"
        COMMAND "${CMAKE_COMMAND}" -E chdir "${prboomtv_roms}" "${CMAKE_COMMAND}" -E tar xf "${prboomtv_games_tar}" --touch
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${prboomtv_games_tar}"
        DEPENDS "${prboomtv_games_archive}"
        COMMENT "prboomtv: WADs into ${prboomtv_roms}"
        VERBATIM)

    add_custom_target(prboomtv_www ALL DEPENDS ${prboomtv_www_outputs} ${prboomtv_rom_outputs})
endif()
