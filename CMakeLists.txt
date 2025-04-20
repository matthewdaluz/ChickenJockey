cmake_minimum_required(VERSION 3.15)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

project(ChickenJockey VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

add_executable(ChickenJockey
    src/main.cpp
    src/blocker.cpp
    src/watcher.cpp
    src/gui.cpp
    src/utils/crypto.cpp
    src/utils/path.cpp
)

set(APP_MANIFEST "${CMAKE_SOURCE_DIR}/app.manifest")

# Disable default manifest from MSVC and embed our own cleanly
set_target_properties(ChickenJockey PROPERTIES
    VS_GLOBAL_EnableManifest FALSE
)

set(APP_MANIFEST "${CMAKE_SOURCE_DIR}/app.manifest")

add_custom_command(TARGET ChickenJockey POST_BUILD
    COMMAND mt.exe -manifest ${APP_MANIFEST} -outputresource:$<TARGET_FILE:ChickenJockey>;#1
    COMMENT "Embedding custom app.manifest"
)

add_executable(hostswriter src/utils/hostswriter.cpp)


target_link_libraries(ChickenJockey PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    advapi32
    user32
    shlwapi
)

target_include_directories(ChickenJockey PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_compile_definitions(ChickenJockey PRIVATE UNICODE _UNICODE)
target_compile_definitions(ChickenJockey PRIVATE _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
