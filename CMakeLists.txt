cmake_minimum_required(VERSION 3.20)

# Handles CMake 4.x compatibility safely without relying on command-line arguments
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

project(Obscura VERSION 1.0.0 LANGUAGES CXX C)

# Forces static linking to prevent Windows DLL configuration errors
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Force static libraries" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. FetchContent for Dependencies
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# 2. Define Executable and Link Libraries
file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)
add_executable(Obscura ${SOURCES})
target_include_directories(Obscura PRIVATE src)
target_link_libraries(Obscura PRIVATE raylib)

# ==========================================
# 3. NETWORK SUPPORT (Cross-Platform)
# ==========================================
# Find threading library for asynchronous netcode (std::thread / std::jthread)
find_package(Threads REQUIRED)
target_link_libraries(Obscura PRIVATE Threads::Threads)

# Link Windows Sockets (Winsock2) for UDP/TCP networking
if(WIN32)
    target_link_libraries(Obscura PRIVATE ws2_32)
endif()

# 4. ZERO-ERROR GUARANTEE: Apply strict warnings ONLY to our code
if(MSVC)
    target_compile_options(Obscura PRIVATE /W4 /WX)
else()
    target_compile_options(Obscura PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
