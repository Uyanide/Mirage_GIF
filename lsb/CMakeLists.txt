cmake_minimum_required(VERSION 3.20)

project(GIFLsb
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

add_executable(GIFLsb-enc
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_lsb_enc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/main_enc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/MimeTypes.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/options_enc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/mark.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/file_reader.cpp
)

target_link_directories(GIFLsb-enc PRIVATE
    ${CMAKE_BINARY_DIR}/lib_gif_enc
    ${CMAKE_BINARY_DIR}/lib_image_sequence
)

target_link_libraries(GIFLsb-enc PRIVATE
    naive_gif
    image_sequence)

target_include_directories(GIFLsb-enc PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${LIB_DIR}/include
    ${LIB_DIR}/gif_enc/include
    ${LIB_DIR}/image_sequence/include
)

target_compile_options(GIFLsb-enc PRIVATE ${GLOBAL_COMPILE_OPTIONS})

target_link_options(GIFLsb-enc PRIVATE ${GLOBAL_LINK_OPTIONS})

target_compile_definitions(GIFLsb-enc PRIVATE ${GLOBAL_COMPILE_DEFINITIONS})

add_executable(GIFLsb-dec
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_lsb_dec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/main_dec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/options_dec.cpp
)

target_link_directories(GIFLsb-dec PRIVATE
    ${CMAKE_BINARY_DIR}/lib_image_sequence
)

target_link_libraries(GIFLsb-dec PRIVATE image_sequence)

target_include_directories(GIFLsb-dec PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${LIB_DIR}/include
    ${LIB_DIR}/image_sequence/include
)

target_compile_options(GIFLsb-dec PRIVATE ${GLOBAL_COMPILE_OPTIONS})

target_link_options(GIFLsb-dec PRIVATE ${GLOBAL_LINK_OPTIONS})

target_compile_definitions(GIFLsb-dec PRIVATE ${GLOBAL_COMPILE_DEFINITIONS})
