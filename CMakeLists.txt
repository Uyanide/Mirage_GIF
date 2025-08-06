cmake_minimum_required(VERSION 3.20)

project(GIFHidePic
    VERSION 1.1.2
    LANGUAGES CXX
)

if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(lib_dir ${CMAKE_CURRENT_LIST_DIR}/lib)

if(WIN32)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/bin/win)
elseif(UNIX AND NOT APPLE)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/bin/linux)
else()
    message(WARNING "Platform not tested, using default output directory.")
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/bin)
endif()

set(target_exe TRUE)

include(${CMAKE_CURRENT_LIST_DIR}/mirage/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/lsb/CMakeLists.txt)
