set(PRBOOM_VERSION "c6e0fcb8325fc7969c91387f56c63433de0f1a53")
set(PRBOOM_DOWNLOAD "https://github.com/libretro/libretro-prboom/archive/${PRBOOM_VERSION}.tar.gz")
set(PRBOOM_DIR "${CMAKE_SOURCE_DIR}/vendor/prboom")

set(PRBOOMTV_DIR "${CMAKE_CURRENT_LIST_DIR}/../libs/prboomtv_libretro")

if (NOT EXISTS "${PRBOOM_DIR}/src")
    FetchContent_Populate(prboom URL ${PRBOOM_DOWNLOAD} SOURCE_DIR ${PRBOOM_DIR})
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

# files in source/ either carry /** @patch */ hunks applied on top of upstream
# files (see scripts/prboomtv_patchs.cpp), or mirror the upstream layout and
# replace the original ones, or are new files compiled as they are
file(GLOB_RECURSE prboomtv_source_files CONFIGURE_DEPENDS
    RELATIVE "${PRBOOMTV_DIR}/source" "${PRBOOMTV_DIR}/source/*.c")

set(prboomtv_patch_files "")
set(prboomtv_override_files "")
foreach(file ${prboomtv_source_files})
    file(STRINGS "${PRBOOMTV_DIR}/source/${file}" patch_marks REGEX "@patch[ \t]")
    if (patch_marks)
        list(APPEND prboomtv_patch_files "${PRBOOMTV_DIR}/source/${file}")
    else()
        list(APPEND prboomtv_override_files "${file}")
    endif()
endforeach()

# the patch tool runs here at configure time, so it is built with the
# machine's own c++, never with the cross toolchain
set(PRBOOMTV_PATCHS_TOOL_SOURCE "${CMAKE_SOURCE_DIR}/scripts/prboomtv_patchs.cpp")
set(PRBOOMTV_PATCHS_TOOL "${CMAKE_BINARY_DIR}/prboomtv/prboomtv_patchs")
set(PRBOOMTV_PATCHS_OUTPUT "${CMAKE_BINARY_DIR}/prboomtv/patchs")

find_program(PRBOOMTV_HOST_CXX NAMES c++ g++ clang++ REQUIRED NO_CMAKE_FIND_ROOT_PATH)

if (NOT EXISTS "${PRBOOMTV_PATCHS_TOOL}" OR "${PRBOOMTV_PATCHS_TOOL_SOURCE}" IS_NEWER_THAN "${PRBOOMTV_PATCHS_TOOL}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/prboomtv")
    execute_process(
        COMMAND "${PRBOOMTV_HOST_CXX}" -std=c++23 -O2 -o "${PRBOOMTV_PATCHS_TOOL}" "${PRBOOMTV_PATCHS_TOOL_SOURCE}"
        RESULT_VARIABLE prboomtv_patchs_result
        ERROR_VARIABLE prboomtv_patchs_error)
    if (prboomtv_patchs_result)
        message(FATAL_ERROR "prboomtv: failed to build the patch tool\n${prboomtv_patchs_error}")
    endif()
endif()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PRBOOMTV_PATCHS_TOOL_SOURCE}" ${prboomtv_patch_files})

execute_process(
    COMMAND "${PRBOOMTV_PATCHS_TOOL}" --source "${PRBOOM_DIR}" --output "${PRBOOMTV_PATCHS_OUTPUT}" ${prboomtv_patch_files}
    RESULT_VARIABLE prboomtv_patchs_result
    OUTPUT_VARIABLE prboomtv_patched_files
    ERROR_VARIABLE prboomtv_patchs_error
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if (prboomtv_patchs_result)
    message(FATAL_ERROR "prboomtv: failed to apply patchs\n${prboomtv_patchs_error}")
endif()
string(REPLACE "\n" ";" prboomtv_patched_files "${prboomtv_patched_files}")

set(prboomtv_files "")
set(prboomtv_patched_dirs "")
foreach(file ${prboomtv_patched_files})
    if (NOT file IN_LIST prboomtv_upstream_files)
        message(FATAL_ERROR "prboomtv: ${file} is patched but is not compiled by the core")
    elseif (file IN_LIST prboomtv_override_files)
        message(FATAL_ERROR "prboomtv: ${file} is both patched and replaced in source/")
    endif()
    # the patched copy lives in the build tree, so its quoted includes need
    # the original directory to keep resolving like upstream
    get_filename_component(dir "${PRBOOM_DIR}/${file}" DIRECTORY)
    list(APPEND prboomtv_patched_dirs "${dir}")
endforeach()
foreach(file ${prboomtv_upstream_files})
    if (file IN_LIST prboomtv_override_files)
        list(APPEND prboomtv_files "${PRBOOMTV_DIR}/source/${file}")
    elseif (file IN_LIST prboomtv_patched_files)
        list(APPEND prboomtv_files "${PRBOOMTV_PATCHS_OUTPUT}/${file}")
    else()
        list(APPEND prboomtv_files "${PRBOOM_DIR}/${file}")
    endif()
endforeach()
foreach(file ${prboomtv_override_files})
    if (NOT file IN_LIST prboomtv_upstream_files)
        list(APPEND prboomtv_files "${PRBOOMTV_DIR}/source/${file}")
    endif()
endforeach()
list(REMOVE_DUPLICATES prboomtv_patched_dirs)

add_library(prboomtv_libretro SHARED "${prboomtv_files}")
target_include_directories(prboomtv_libretro BEFORE PRIVATE "${PRBOOMTV_DIR}/include")
target_include_directories(prboomtv_libretro SYSTEM PRIVATE
    "${PRBOOM_DIR}" "${PRBOOM_DIR}/src" "${PRBOOM_DIR}/libretro"
    "${PRBOOM_DIR}/libretro/libretro-common/include" ${prboomtv_patched_dirs})
target_compile_definitions(prboomtv_libretro PRIVATE
    HAVE_RVORBIS HAVE_RMP3 HAVE_RMODTRACKER HAVE_RWAV HAVE_RPNG HAVE_RJPEG HAVE_THREADS HAVE_MMAP
    INLINE=inline _POSIX_C_SOURCE=199309L _DEFAULT_SOURCE)
target_compile_options(prboomtv_libretro PRIVATE -Wall -W -Wno-unused-parameter -fomit-frame-pointer)
target_link_libraries(prboomtv_libretro PRIVATE m pthread)
set_target_properties(prboomtv_libretro PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS OFF
    OUTPUT_NAME "prboomtv_libretro"
    PREFIX ""
    LINK_FLAGS "-Wl,--version-script=${PRBOOM_DIR}/libretro/link.T -Wl,--no-undefined"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
