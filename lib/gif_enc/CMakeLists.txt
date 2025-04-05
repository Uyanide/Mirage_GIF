cmake_minimum_required(VERSION 3.20)

project(gif_enc)

set(CMAKE_CXX_STANDARD 23)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(gif_enc STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_encoder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_format.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_lzw_dec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_lzw_enc.cpp
)

if(NOT TARGET_COMPILE_OPTIONS)
    if(CMAKE_COMPILER_IS_GNUCXX)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(TARGET_COMPILE_OPTIONS -Wall -Wextra -O0 -g3)
        else()
            set(TARGET_COMPILE_OPTIONS -Wall -Wextra -O3)
        endif()
    endif()

    if(MSVC)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(TARGET_COMPILE_OPTIONS /Od /Zi)
        else()
            set(TARGET_COMPILE_OPTIONS /O2)
        endif()
    endif()
endif()

target_include_directories(gif_enc PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_options(gif_enc PRIVATE ${TARGET_COMPILE_OPTIONS})

target_link_options(gif_enc PRIVATE ${TARGET_LINK_OPTIONS})

target_compile_definitions(gif_enc PRIVATE ${TARGET_COMPILE_DEFINITIONS})