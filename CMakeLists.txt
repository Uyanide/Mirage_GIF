cmake_minimum_required(VERSION 3.20)

project(GIFHidePic
    VERSION 1.1.0
    LANGUAGES CXX
)

set(CMAKE_BUILD_TYPE Release)

set(LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/lib)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/bin)

include(${CMAKE_CURRENT_LIST_DIR}/mirage/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/lsb/CMakeLists.txt)