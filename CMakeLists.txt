cmake_minimum_required(VERSION 3.20)
project(Obscura VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

include(FetchContent)
FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.0)
FetchContent_MakeAvailable(raylib)

file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)
add_executable(Obscura ${SOURCES})
target_include_directories(Obscura PRIVATE src)
target_link_libraries(Obscura PRIVATE raylib)
