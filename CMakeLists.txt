cmake_minimum_required(VERSION 3.10)
project(ChickenJockey VERSION 1.0 LANGUAGES CXX)

# Set C++ standard to C++17 (required for std::filesystem)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/utils

)

# Define source files
set(SOURCES
    src/main.cpp
    src/blocker.cpp
    src/service.cpp
    src/watcher.cpp
    src/gui.cpp
    src/utils/crypto.cpp
    src/utils/path.cpp
)

# Locate OpenSSL
find_package(OpenSSL REQUIRED)
if(OpenSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
else()
    message(FATAL_ERROR "OpenSSL not found!")
endif()

# Create the executable
add_executable(ChickenJockey ${SOURCES})

# Link libraries
target_link_libraries(ChickenJockey
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Link Windows libraries if building on Windows
if(WIN32)
    target_link_libraries(ChickenJockey
        advapi32    # Windows service API
        user32      # GUI/MessageBox
        shlwapi     # Path functions
    )
    # Define Unicode build
    target_compile_definitions(ChickenJockey PRIVATE UNICODE _UNICODE)
endif()
