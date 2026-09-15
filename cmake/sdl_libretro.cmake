set(SDL2_VERSION "release-2.30.6")
set(SDL2_DOWNLOAD "https://github.com/libsdl-org/SDL/archive/refs/tags/${SDL2_VERSION}.tar.gz")
set(SDL2_DIR "${CMAKE_SOURCE_DIR}/vendor/sdl2")

set(LIBRETRO_DIR "${CMAKE_SOURCE_DIR}/vendor/retroarch")
set(LIBRETRO_DOWNLOAD "https://raw.githubusercontent.com/libretro/RetroArch/ee3eae34c425a1645b923395854dda7f2543df31/libretro-common/include/libretro.h")

set(SDL_LIBRETRO_DIR "${CMAKE_CURRENT_LIST_DIR}/../libs/sdl_libretro")

if (NOT EXISTS "${SDL2_DIR}/include")
    FetchContent_Populate(sdl2 URL ${SDL2_DOWNLOAD} SOURCE_DIR ${SDL2_DIR})
endif()

if (NOT EXISTS "${LIBRETRO_DIR}/libretro.h")
    file(DOWNLOAD "${LIBRETRO_DOWNLOAD}" "${LIBRETRO_DIR}/libretro.h")
endif()

file(GLOB sdl_libretro_core_files  "${SDL_LIBRETRO_DIR}/source/backend/libretro/*.c")
file(GLOB sdl_libretro_sdl2_files  "${SDL_LIBRETRO_DIR}/source/frontend/sdl2/*.c"
                                   "${SDL_LIBRETRO_DIR}/source/window/*.c")
file(GLOB sdl_libretro_jack_files  "${SDL_LIBRETRO_DIR}/source/frontend/jack/*.c")

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

add_library(sdl_libretro_sdl2 SHARED "${sdl_libretro_sdl2_files}")
target_include_directories(sdl_libretro_sdl2 PRIVATE "${SDL_LIBRETRO_DIR}/include")
target_include_directories(sdl_libretro_sdl2 SYSTEM PRIVATE
    "${SDL2_DIR}/include" "${SDL2_DIR}/src/video/khronos")
target_compile_definitions(sdl_libretro_sdl2 PRIVATE EGL_NO_X11)
target_compile_options(sdl_libretro_sdl2 PRIVATE -Wall -Wextra)
target_link_libraries(sdl_libretro_sdl2 PRIVATE ${CMAKE_DL_LIBS} pthread rt)
set_target_properties(sdl_libretro_sdl2 PROPERTIES
    OUTPUT_NAME "SDL2-2.0"
    SUFFIX ".so.0"
    LINK_FLAGS "-Wl,-soname,libSDL2-2.0.so.0"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
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

add_dependencies(sdl_libretro sdl_libretro_sdl2 sdl_libretro_jack)
