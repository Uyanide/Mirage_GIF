cmake_minimum_required(VERSION 3.20)

project(GIFHidePic
    VERSION 1.1.1
    LANGUAGES CXX
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/lib)

if(WIN32)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/bin/win)
elseif(UNIX AND NOT APPLE)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/bin/linux)
else()
    message(WARNING "Platform not tested, using default output directory.")
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/bin)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/mirage/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/lsb/CMakeLists.txt)