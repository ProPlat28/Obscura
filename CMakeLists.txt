cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

project(Obscura VERSION 1.0.0 LANGUAGES CXX C)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Force static libraries" FORCE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Raylib
FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.0)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# cpp-httplib
FetchContent_Declare(cpphttplib GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git GIT_TAG v0.14.1)
FetchContent_MakeAvailable(cpphttplib)

file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)
add_executable(Obscura ${SOURCES})

target_include_directories(Obscura PRIVATE src)
target_link_libraries(Obscura PRIVATE raylib)

# Link cpp-httplib
if(TARGET httplib::httplib)
    target_link_libraries(Obscura PRIVATE httplib::httplib)
else()
    target_include_directories(Obscura PRIVATE ${cpphttplib_SOURCE_DIR})
endif()

# ==========================================
# OPENSSL SUPPORT (Required for HTTPS on Render)
# ==========================================
find_package(OpenSSL REQUIRED)
target_compile_definitions(Obscura PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(Obscura PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Threading & Windows Sockets
find_package(Threads REQUIRED)
target_link_libraries(Obscura PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(Obscura PRIVATE ws2_32)
endif()

# Zero-Error Guarantee
if(MSVC)
    target_compile_options(Obscura PRIVATE /W4 /WX)
else()
    target_compile_options(Obscura PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
