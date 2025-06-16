cmake_minimum_required(VERSION 3.20)

project(GIFLsb_native
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/CompilerOptions.cmake)

if(NOT LIB_DIR)
    set(LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../lib)
endif()

if(NOT EXECUTABLE_OUTPUT_PATH)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/../bin)
endif()

if(NOT LIBRARY_OUTPUT_PATH)
    set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/../bin)
endif()

set(SOURCE_FILES "")
list(APPEND SOURCE_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/main_lib.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_lsb_enc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/MimeTypes.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/mark.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_lsb_dec.cpp
)

set(INCLUDE_DIRS "")
list(APPEND INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${LIB_DIR}/include
)

set(COMPILE_DEFINITIONS "")
set(LINK_LIBRARIES "")

set(IMSQ_USE_NATIVE TRUE)
include(${LIB_DIR}/gif_enc/CMakeLists.txt)
include(${LIB_DIR}/image_sequence/CMakeLists.txt)
include(${LIB_DIR}/file_io/CMakeLists.txt)

add_library(giflsb SHARED ${SOURCE_FILES})

target_include_directories(giflsb PRIVATE ${INCLUDE_DIRS})

target_compile_options(giflsb PRIVATE ${GLOBAL_COMPILE_OPTIONS})

target_link_options(giflsb PRIVATE ${GLOBAL_LINK_OPTIONS})

target_compile_definitions(giflsb PRIVATE ${GLOBAL_COMPILE_DEFINITIONS} ${COMPILE_DEFINITIONS})
