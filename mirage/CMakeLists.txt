cmake_minimum_required(VERSION 3.20)

project(GifMirage
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/CompilerOptions.cmake)

if(NOT lib_dir)
    set(lib_dir ${CMAKE_CURRENT_LIST_DIR}/../lib)
endif()

if(NOT gif_enc_included)
    include(${lib_dir}/gif_enc/CMakeLists.txt)
    set(gif_enc_included TRUE)
endif()

if(NOT image_sequence_included)
    include(${lib_dir}/image_sequence/CMakeLists.txt)
    set(image_sequence_included TRUE)
endif()

if(NOT file_io_included)
    include(${lib_dir}/file_io/CMakeLists.txt)
    set(file_io_included TRUE)
endif()

if(NOT EXECUTABLE_OUTPUT_PATH)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/../bin)
endif()

add_executable(GIFMirage
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_options.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_mirage.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
    ${gif_enc_source_files}
    ${image_sequence_source_files}
    ${file_io_source_files}
)

target_link_libraries(GIFMirage PRIVATE
    ${image_sequence_link_libraries}
)

target_include_directories(GIFMirage PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${lib_dir}/include
    ${gif_enc_include_dirs}
    ${image_sequence_include_dirs}
    ${file_io_include_dirs}
)

target_compile_options(GIFMirage PRIVATE ${global_compile_options})

target_link_options(GIFMirage PRIVATE ${global_link_options})

target_compile_definitions(GIFMirage PRIVATE
    ${global_compile_definitions}
    ${image_sequence_compile_definitions}
)
