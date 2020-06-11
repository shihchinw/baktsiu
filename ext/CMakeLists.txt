# Collect source files to be compiled with our own app together.
file(GLOB SRC_FILES imgui/*.h imgui/*.cpp)
set(IMGUI_SRC_FILES ${SRC_FILES} PARENT_SCOPE)

file(GLOB SRC_FILES gl3w/GL/*.h gl3w/GL/*.c)
set(GL3W_SRC_FILES ${SRC_FILES} PARENT_SCOPE)

file(GLOB SRC_FILES docopt/*.h docopt/*.cpp)
set(DOCOPT_SRC_FILES ${SRC_FILES} PARENT_SCOPE)

# Build GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL " " FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL " " FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL " " FORCE)
set(GLFW_BUILD_INSTALL OFF CACHE BOOL " " FORCE)
set(GLFW_INSTALL OFF CACHE BOOL " " FORCE)
set(GLFW_USE_CHDIR OFF CACHE BOOL " " FORCE)
add_subdirectory(glfw EXCLUDE_FROM_ALL)
set_property(TARGET glfw PROPERTY FOLDER "Dependencies")

if(USE_OPENEXR)
    if(WIN32)
        # Build ZLIB on Windows (requirement for OpenEXR)
        add_subdirectory(zlib EXCLUDE_FROM_ALL)

        set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/zlib" CACHE PATH " " FORCE)
        # zlib uses '_d' suffix for the static debug library, and this makes us hard to set
        # different values of ZLIB_LIBRARY for different configurations (ex. Debug/Release).
        # Thus we set ZLIB_LIBRARY to a ' ' before adding subdirectory of OpenEXR, this is
        # a hack to let OpenEXR skip popping errors of missing ZLIB. The true dependency is
        # added by using "target_link_libraries(${PROJECT_NAME} PRIVATE zlibstatic)" later.
        set(ZLIB_LIBRARY " " CACHE FILEPATH " " FORCE) 

        set_property(TARGET zlibstatic PROPERTY FOLDER "Dependencies")
        include_directories(${ZLIB_INCLUDE_DIR} "${CMAKE_CURRENT_BINARY_DIR}/zlib")
    endif()

    # Build OpenEXR
    set(ILMBASE_BUILD_SHARED_LIBS OFF CACHE BOOL " " FORCE)
    set(OPENEXR_BUILD_SHARED_LIBS OFF CACHE BOOL " " FORCE)
    set(PYILMBASE_ENABLE OFF CACHE BOOL " " FORCE)
    set(OPENEXR_VIEWERS_ENABLE OFF CACHE BOOL " " FORCE)
    set(ILMBASE_NAMESPACE_VERSIONING OFF CACHE BOOL " " FORCE)
    set(OPENEXR_NAMESPACE_VERSIONING OFF CACHE BOOL " " FORCE)
    set(OPENEXR_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/openexr CACHE PATH " " FORCE)
    add_subdirectory(openexr EXCLUDE_FROM_ALL)
    set_property(TARGET IexMath IlmThread Half Iex Imath IlmImf PROPERTY FOLDER "Dependencies")
endif()

# spdlog
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL " " FORCE)
set(SPDLOG_ENABLE_PCH OFF CACHE BOOL " " FORCE)
set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL " " FORCE)
add_subdirectory(spdlog)
set_property(TARGET spdlog PROPERTY FOLDER "Dependencies")

set(EXT_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui 
    ${CMAKE_CURRENT_SOURCE_DIR}/gl3w 
    ${CMAKE_CURRENT_SOURCE_DIR}/stb
    ${OPENEXR_INCLUDE_DIRS}
    PARENT_SCOPE)