cmake_minimum_required(VERSION 3.14)

if(DEFINED TARGET)
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

if (PORTS_USE_SDL_LIBRETRO)
    include(cmake/sdl_libretro.cmake)
endif()
