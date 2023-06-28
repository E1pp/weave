cmake_minimum_required(VERSION 3.14)
project(weave)

include(cmake/Logging.cmake)

include(cmake/CheckCompiler.cmake)
include(cmake/CompileOptions.cmake)
include(cmake/Logging.cmake)
include(cmake/Sanitize.cmake)
include(cmake/Platform.cmake)

option(WEAVE_TESTS "Build Weave tests" OFF)
option(WEAVE_EXAMPLES "Build Weave examples" OFF)
option(WEAVE_WORKLOADS "Build Weave workloads" OFF)
option(WEAVE_DEVELOPER "Weave development mode" OFF)
option(MI_OVERRIDE "Use mimalloc memory allocator" OFF)

add_subdirectory(third_party)

include(cmake/Test.cmake)

if(WEAVE_DEVELOPER)
    project_log("Turn warnings into errors")
    add_compile_options(-Werror)
endif()

add_subdirectory(weave)

if(WEAVE_TESTS OR WEAVE_DEVELOPER)
    add_subdirectory(tests)
endif()

if(WEAVE_WORKLOADS OR WEAVE_DEVELOPER)
    add_subdirectory(workloads)
endif()

if(WEAVE_EXAMPLES OR WEAVE_DEVELOPER)
    add_subdirectory(examples)
endif()


