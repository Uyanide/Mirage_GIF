cmake_minimum_required(VERSION 3.20)

project(GifMirage
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CompilerOptions.cmake)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../mirage/lib)

add_subdirectory(${LIB_DIR}/image_sequence ${CMAKE_CURRENT_BINARY_DIR}/image_sequence)

add_executable(whatever
    main_imsq.cpp
)

target_link_directories(whatever PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/image_sequence
)

target_link_libraries(whatever PRIVATE
    image_sequence)

target_include_directories(whatever PRIVATE
    ${LIB_DIR}/image_sequence
    ${LIB_DIR}/include
)

if(WIN32)
    target_compile_definitions(whatever PRIVATE UNICODE _UNICODE)
endif()

target_compile_options(whatever PRIVATE ${TARGET_COMPILE_OPTIONS})

target_link_options(whatever PRIVATE ${TARGET_LINK_OPTIONS})

target_compile_definitions(whatever PRIVATE ${TARGET_COMPILE_DEFINITIONS})