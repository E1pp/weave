# --------------------------------------------------------------------

set(LIB_TARGET weave)

# --------------------------------------------------------------------

# Sources & Includes

get_filename_component(LIB_INCLUDE_PATH ".." ABSOLUTE)
get_filename_component(LIB_PATH "." ABSOLUTE)

file(GLOB_RECURSE LIB_CXX_SOURCES ${LIB_PATH}/*.cpp)
file(GLOB_RECURSE LIB_HEADERS ${LIB_PATH}/*.hpp)

add_library(${LIB_TARGET} STATIC ${LIB_CXX_SOURCES} ${LIB_HEADERS})
target_include_directories(${LIB_TARGET} PUBLIC ${LIB_INCLUDE_PATH})

# --------------------------------------------------------------------

# Dependencies

set(LIB_DEPENDENCIES
        wheels
        twist
        sure
        function2
        fmt
        expected)

if(WEAVE_MIMALLOC)
    list(APPEND LIB_DEPENDENCIES mimalloc)
endif()

ProjectLog("Dependencies: ${LIB_DEPENDENCIES}")

target_link_libraries(${LIB_TARGET} PUBLIC ${LIB_DEPENDENCIES})
