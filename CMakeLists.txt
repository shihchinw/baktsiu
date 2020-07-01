cmake_minimum_required(VERSION 3.1)
project(baktsiu)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(VERSION "0.2.1" CACHE STRING " " FORCE)

option(USE_NVTX "Use NVidia Tool Extensions" OFF)
option(USE_OPENEXR "Use OpenEXR" OFF)
option(EMBED_SHADERS "Embed shaders into binary" ON)

if(USE_OPENEXR AND NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ext/openexr/OpenEXR")
    message(FATAL_ERROR "Missing dependent thrid-party submodules (like OpenEXR)!"
        "Please execute following command to recover:"
        "\"git submodule update --init --recursive\"")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
        "MinSizeRel" "RelWithDebInfo")
endif()

if(MSVC)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON) # Enable subfolder filters in VC project
endif()

if(WIN32)
    set(EXTRA_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.rc")
elseif(APPLE)
    set(EXTRA_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.icns")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(ext)
add_subdirectory(src)