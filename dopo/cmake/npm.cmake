# The npm packages of package.json (the repository root) that the web build
# copies out of node_modules, as NPM_MODULES/<package>/...; npm ci runs at
# configure time when node_modules is missing or older than
# package-lock.json. love.js is not among them: its repository has no
# package.json, which npm needs (lovejs.cmake).
include_guard(GLOBAL)

set(NPM_MODULES "${CMAKE_SOURCE_DIR}/node_modules")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/package-lock.json")

if (NOT EXISTS "${NPM_MODULES}/.package-lock.json"
        OR "${CMAKE_SOURCE_DIR}/package-lock.json" IS_NEWER_THAN "${NPM_MODULES}/.package-lock.json")
    find_program(NPM_EXECUTABLE npm NO_CMAKE_FIND_ROOT_PATH)
    if (NOT NPM_EXECUTABLE)
        message(FATAL_ERROR "npm: not found; install Node.js, or run npm ci at ${CMAKE_SOURCE_DIR}")
    endif()
    message(STATUS "npm: installing package-lock.json into node_modules")
    execute_process(
        COMMAND "${NPM_EXECUTABLE}" ci --no-audit --no-fund
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE npm_result)
    if (npm_result)
        message(FATAL_ERROR "npm: npm ci failed")
    endif()
endif()

# the path of a file of an installed package, failing when it is not there
function(dopo_npm_file out_var path)
    if (NOT EXISTS "${NPM_MODULES}/${path}")
        message(FATAL_ERROR "npm: no ${path} in node_modules (package.json)")
    endif()
    set(${out_var} "${NPM_MODULES}/${path}" PARENT_SCOPE)
endfunction()
