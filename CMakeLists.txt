cmake_minimum_required(VERSION 3.14)

option(PORTS_WASM "build for the browser: emscripten, the cores linked into RetroArch's web player" OFF)

if(PORTS_WASM)
    include(dopo/cmake/emsdk.cmake)
elseif(DEFINED TARGET)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/zig-toolchain.cmake" CACHE STRING "zig cross compiler")
endif()

project(ports C)

include(FetchContent)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "build type" FORCE)
endif()

set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

option(PORTS_USE_SDL_LIBRETRO "build sdl_libretro core (libretro backend + sdl2/jack frontends)" ON)
option(PORTS_USE_PRBOOMTV_LIBRETRO "build prboomtv_libretro core (prboom patched for tv)" ON)
option(PORTS_USE_RETROARCH "build RetroArch's web player with the cores (wasm only)" ${PORTS_WASM})

include(dopo/cmake/dopo.cmake)

# the web build's pages and everything they load, one folder per core
set(PORTS_DIST "${CMAKE_BINARY_DIR}/dist")

if (PORTS_USE_SDL_LIBRETRO)
    include(cmake/sdl_libretro.cmake)
endif()

if (PORTS_USE_PRBOOMTV_LIBRETRO)
    include(prboomtv_libretro/cmake/prboomtv.cmake)
endif()

if (PORTS_USE_RETROARCH)
    if (NOT EMSCRIPTEN)
        message(FATAL_ERROR "PORTS_USE_RETROARCH builds the web player: use the wasm preset")
    endif()
    include(retroarch/cmake/retroarch.cmake)
endif()
