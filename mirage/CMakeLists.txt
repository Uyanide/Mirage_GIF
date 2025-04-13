cmake_minimum_required(VERSION 3.20)

project(GifMirage
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

if(NOT GIF_ENC_BUILT)
    add_subdirectory(${LIB_DIR}/gif_enc ${CMAKE_BINARY_DIR}/lib_gif_enc)
    set(GIF_ENC_BUILT TRUE)
endif()

if(NOT IMAGE_SEQUENCE_BUILT)
    add_subdirectory(${LIB_DIR}/image_sequence ${CMAKE_BINARY_DIR}/lib_image_sequence)
    set(IMAGE_SEQUENCE_BUILT TRUE)
endif()

if(NOT EXECUTABLE_OUTPUT_PATH)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/../bin)
endif()

add_executable(GIFMirage
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_options.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_mirage.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
)

target_link_directories(GIFMirage PRIVATE
    ${CMAKE_BINARY_DIR}/lib_gif_enc
    ${CMAKE_BINARY_DIR}/image_sequence
)

target_link_libraries(GIFMirage PRIVATE
    gif_enc
    image_sequence)

target_include_directories(GIFMirage PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${LIB_DIR}/gif_enc/include
    ${LIB_DIR}/image_sequence/include
    ${LIB_DIR}/include
)

if(WIN32)
    target_compile_definitions(GIFMirage PRIVATE UNICODE _UNICODE)
endif()

target_compile_options(GIFMirage PRIVATE ${TARGET_COMPILE_OPTIONS})

target_link_options(GIFMirage PRIVATE ${TARGET_LINK_OPTIONS})

target_compile_definitions(GIFMirage PRIVATE ${TARGET_COMPILE_DEFINITIONS})
