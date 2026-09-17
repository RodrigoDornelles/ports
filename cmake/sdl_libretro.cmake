set(SDL2_VERSION "release-2.30.6")
set(SDL2_DOWNLOAD "https://github.com/libsdl-org/SDL/archive/refs/tags/${SDL2_VERSION}.tar.gz")
set(SDL2_DIR "${CMAKE_SOURCE_DIR}/vendor/sdl2")

set(SDL1_VERSION "release-1.2.15")
set(SDL1_DOWNLOAD "https://github.com/libsdl-org/SDL-1.2/archive/refs/tags/${SDL1_VERSION}.tar.gz")
set(SDL1_DIR "${CMAKE_SOURCE_DIR}/vendor/sdl1")

set(X11_DIR "${CMAKE_SOURCE_DIR}/vendor/x11")
set(X11_PARTS xorgproto-2024.1 libX11-1.8.9 libXrandr-1.5.4 libXrender-0.9.11)

set(LIBRETRO_DIR "${CMAKE_SOURCE_DIR}/vendor/retroarch")
set(LIBRETRO_DOWNLOAD "https://raw.githubusercontent.com/libretro/RetroArch/ee3eae34c425a1645b923395854dda7f2543df31/libretro-common/include/libretro.h")

set(SDL_LIBRETRO_DIR "${CMAKE_CURRENT_LIST_DIR}/../libs/sdl_libretro")

if (NOT EXISTS "${SDL2_DIR}/include")
    FetchContent_Populate(sdl2 URL ${SDL2_DOWNLOAD} SOURCE_DIR ${SDL2_DIR})
endif()

if (NOT EXISTS "${SDL1_DIR}/include")
    FetchContent_Populate(sdl1 URL ${SDL1_DOWNLOAD} SOURCE_DIR ${SDL1_DIR})
endif()

# the shims implement libX11 and libXrandr, so they need the canonical headers:
# the struct layouts have to match what the game was compiled against
if (NOT EXISTS "${X11_DIR}/include/X11/Xlib.h")
    foreach(x11_name ${X11_PARTS})
        if (x11_name MATCHES "^xorgproto")
            set(x11_group "proto")
        else()
            set(x11_group "lib")
        endif()
        FetchContent_Populate(${x11_name}
            URL "https://www.x.org/releases/individual/${x11_group}/${x11_name}.tar.gz"
            SOURCE_DIR "${X11_DIR}/${x11_name}")
        file(COPY "${X11_DIR}/${x11_name}/include/X11" DESTINATION "${X11_DIR}/include")
    endforeach()
endif()

if (NOT EXISTS "${LIBRETRO_DIR}/libretro.h")
    file(DOWNLOAD "${LIBRETRO_DOWNLOAD}" "${LIBRETRO_DIR}/libretro.h")
endif()

file(GLOB sdl_libretro_core_files   "${SDL_LIBRETRO_DIR}/source/backend/libretro/*.c")
file(GLOB sdl_libretro_runtime_files "${SDL_LIBRETRO_DIR}/source/common/*.c")
file(GLOB sdl_libretro_shared_files  "${SDL_LIBRETRO_DIR}/source/frontend/sdl/common/*.c"
                                     "${SDL_LIBRETRO_DIR}/source/window/*.c")
file(GLOB sdl_libretro_sdl1_files   "${SDL_LIBRETRO_DIR}/source/frontend/sdl/sdl1/*.c")
file(GLOB sdl_libretro_sdl2_files   "${SDL_LIBRETRO_DIR}/source/frontend/sdl/sdl2/*.c")
file(GLOB sdl_libretro_jack_files   "${SDL_LIBRETRO_DIR}/source/frontend/jack/*.c")
file(GLOB sdl_libretro_x11_files    "${SDL_LIBRETRO_DIR}/source/frontend/x11/*.c")
file(GLOB sdl_libretro_xrandr_files "${SDL_LIBRETRO_DIR}/source/frontend/xrandr/*.c")

add_library(sdl_libretro SHARED "${sdl_libretro_core_files}")
target_include_directories(sdl_libretro PRIVATE "${SDL_LIBRETRO_DIR}/include")
target_include_directories(sdl_libretro SYSTEM PRIVATE "${SDL2_DIR}/include" "${LIBRETRO_DIR}")
target_compile_options(sdl_libretro PRIVATE -Wall -Wextra)
target_link_libraries(sdl_libretro PRIVATE ${CMAKE_DL_LIBS} rt)
set_target_properties(sdl_libretro PROPERTIES
    OUTPUT_NAME "sdl_libretro"
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# the shim libraries share source/window and source/frontend/sdl/common, which
# only use calls that SDL 1.2 and SDL 2 declare the same way
function(sdl_libretro_shim target major soname sources include_dirs)
    add_library(${target} SHARED "${sources}" "${sdl_libretro_shared_files}"
                                 "${sdl_libretro_runtime_files}")
    target_include_directories(${target} PRIVATE "${SDL_LIBRETRO_DIR}/include")
    target_include_directories(${target} SYSTEM PRIVATE ${include_dirs}
        "${SDL2_DIR}/src/video/khronos")
    target_compile_definitions(${target} PRIVATE EGL_NO_X11 SHIM_SDL_MAJOR=${major})
    target_compile_options(${target} PRIVATE -Wall -Wextra)
    target_link_libraries(${target} PRIVATE ${CMAKE_DL_LIBS} pthread rt)
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${soname}"
        SUFFIX ""
        LINK_FLAGS "-Wl,-soname,${soname}"
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    )
endfunction()

sdl_libretro_shim(sdl_libretro_sdl2 2 "libSDL2-2.0.so.0"
    "${sdl_libretro_sdl2_files}" "${SDL2_DIR}/include")
sdl_libretro_shim(sdl_libretro_sdl1 1 "libSDL-1.2.so.0"
    "${sdl_libretro_sdl1_files}" "${SDL_LIBRETRO_DIR}/include/sdl1;${SDL1_DIR}/include")

# libX11 and libXrandr for games that never heard of SDL; they keep their own
# EGL context and present through the real driver, so there is nothing to capture
add_library(sdl_libretro_x11 SHARED "${sdl_libretro_x11_files}"
                                    "${sdl_libretro_runtime_files}")
target_include_directories(sdl_libretro_x11 PRIVATE "${SDL_LIBRETRO_DIR}/include")
target_include_directories(sdl_libretro_x11 SYSTEM PRIVATE "${X11_DIR}/include")
target_compile_definitions(sdl_libretro_x11 PRIVATE
    SHIM_TAG="[libX11-shim]" SHIM_API=DOPO_IPC_API_X11)
target_compile_options(sdl_libretro_x11 PRIVATE -Wall -Wextra)
target_link_libraries(sdl_libretro_x11 PRIVATE ${CMAKE_DL_LIBS} pthread rt)
set_target_properties(sdl_libretro_x11 PROPERTIES
    PREFIX "" OUTPUT_NAME "libX11.so.6" SUFFIX ""
    LINK_FLAGS "-Wl,-soname,libX11.so.6"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/x11
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

add_library(sdl_libretro_xrandr SHARED "${sdl_libretro_xrandr_files}")
target_include_directories(sdl_libretro_xrandr PRIVATE "${SDL_LIBRETRO_DIR}/include")
target_include_directories(sdl_libretro_xrandr SYSTEM PRIVATE "${X11_DIR}/include")
target_compile_options(sdl_libretro_xrandr PRIVATE -Wall -Wextra)
set_target_properties(sdl_libretro_xrandr PROPERTIES
    PREFIX "" OUTPUT_NAME "libXrandr.so.2" SUFFIX ""
    LINK_FLAGS "-Wl,-soname,libXrandr.so.2"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/x11
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

add_library(sdl_libretro_jack SHARED "${sdl_libretro_jack_files}")
target_include_directories(sdl_libretro_jack PRIVATE "${SDL_LIBRETRO_DIR}/include")
target_compile_options(sdl_libretro_jack PRIVATE -Wall -Wextra)
target_link_libraries(sdl_libretro_jack PRIVATE ${CMAKE_DL_LIBS} pthread)
set_target_properties(sdl_libretro_jack PROPERTIES
    OUTPUT_NAME "jack"
    SUFFIX ".so.0"
    LINK_FLAGS "-Wl,-soname,libjack.so.0"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

add_dependencies(sdl_libretro
    sdl_libretro_sdl1 sdl_libretro_sdl2
    sdl_libretro_x11 sdl_libretro_xrandr
    sdl_libretro_jack)
