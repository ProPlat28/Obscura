cmake_minimum_required(VERSION 3.20)
project(Obscura VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. FetchContent for Dependencies
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
)
# Prevent Raylib from building its own examples/tests
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# 2. Define Executable and Link Libraries
file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)
add_executable(Obscura ${SOURCES})
target_include_directories(Obscura PRIVATE src)
target_link_libraries(Obscura PRIVATE raylib)

if(APPLE)
    target_link_libraries(Obscura PRIVATE "-framework IOKit" "-framework Cocoa" "-framework GLUT")
endif()

# 3. ZERO-ERROR GUARANTEE: Apply strict warnings ONLY to our code, NOT dependencies!
if(MSVC)
    target_compile_options(Obscura PRIVATE /W4 /WX)
else()
    target_compile_options(Obscura PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
