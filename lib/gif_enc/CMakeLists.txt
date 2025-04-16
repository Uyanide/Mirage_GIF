cmake_minimum_required(VERSION 3.20)

project(naive_gif)

set(CMAKE_CXX_STANDARD 23)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT DEFINED GLOBAL_LINK_TYPE AND NOT DEFINED GIFENC_LINK_TYPE)
    set(GIFENC_LINK_TYPE STATIC)
elseif(NOT DEFINED GIFENC_LINK_TYPE)
    set(GIFENC_LINK_TYPE ${GLOBAL_LINK_TYPE})
endif()

add_library(naive_gif ${GIFENC_LINK_TYPE}
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_encoder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_format.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_lzw_dec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_lzw_enc.cpp
)

if(NOT DEFINED GLOBAL_COMPILE_OPTIONS AND NOT DEFINED GIFENC_COMPILE_OPTIONS)
    if(CMAKE_COMPILER_IS_GNUCXX)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(GIFENC_COMPILE_OPTIONS -Wall -Wextra -O0 -g3)
        else()
            set(GIFENC_COMPILE_OPTIONS -Wall -Wextra -O3)
        endif()
    endif()

    if(MSVC)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(GIFENC_COMPILE_OPTIONS /Od /Zi)
        else()
            set(GIFENC_COMPILE_OPTIONS /O2)
        endif()
    endif()
elseif(NOT DEFINED GIFENC_COMPILE_OPTIONS)
    set(GIFENC_COMPILE_OPTIONS ${GLOBAL_COMPILE_OPTIONS})
endif()

target_include_directories(naive_gif PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_options(naive_gif PRIVATE ${GIFENC_COMPILE_OPTIONS})

target_link_options(naive_gif PRIVATE ${GLOBAL_LINK_OPTIONS})

target_compile_definitions(naive_gif PRIVATE ${GLOBAL_COMPILE_DEFINITIONS})