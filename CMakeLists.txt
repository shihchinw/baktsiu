cmake_minimum_required(VERSION 3.1)
project(BakTsiu)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(VERSION "0.1.0" CACHE STRING " " FORCE)

option(USE_NVTX "Use NVidia Tool Extensions" OFF)
option(USE_OPENEXR "Use OpenEXR" OFF)
option(EMBED_SHADERS "Embed shaders into binary" ON)

if(USE_OPENEXR AND NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ext/openexr/OpenEXR")
  message(FATAL_ERROR "Missing dependent thrid-party submodules (like OpenEXR)!"
    "Please execute following command to recover:"
    "\"git submodule update --init --recursive\"")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON) # Enable subfolder filters in VC project

if(WIN32)
    set(EXTRA_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.rc")
endif()

add_subdirectory(ext)
add_subdirectory(src)
