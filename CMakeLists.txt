cmake_minimum_required(VERSION 3.20)
project(Thrust VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

add_executable(thrust WIN32
    src/main.cpp
    src/Game.cpp
    src/Physics.cpp
    src/Renderer.cpp
    src/Audio.cpp
)

target_include_directories(thrust PRIVATE src)
target_link_libraries(thrust PRIVATE raylib)

if(MINGW)
    target_link_options(thrust PRIVATE -static -static-libgcc -static-libstdc++)
endif()
