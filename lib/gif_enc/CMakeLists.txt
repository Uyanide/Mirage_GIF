cmake_minimum_required(VERSION 3.20)

list(APPEND gif_enc_source_files
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_encoder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_format.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_lzw_dec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_lzw_enc.cpp
)

list(APPEND gif_enc_include_dirs
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/include
)

if(NOT PROJECT_NAME)
    project(naive_gif)

    set(CMAKE_CXX_STANDARD 23)

    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release)
    endif()

    if(NOT DEFINED global_link_type AND NOT DEFINED gif_enc_link_type)
        set(gif_enc_link_type STATIC)
    elseif(NOT DEFINED gif_enc_link_type)
        set(gif_enc_link_type ${global_link_type})
    endif()

    add_library(naive_gif ${gif_enc_link_type} ${gif_enc_source_files})

    if(NOT DEFINED global_compile_definitions AND NOT DEFINED gifenc_compile_definitions)
        if(CMAKE_COMPILER_IS_GNUCXX)
            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                set(gifenc_compile_definitions -Wall -Wextra -O0 -g3)
            else()
                set(gifenc_compile_definitions -Wall -Wextra -O3)
            endif()
        endif()

        if(MSVC)
            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                set(gifenc_compile_definitions /Od /Zi)
            else()
                set(gifenc_compile_definitions /O2)
            endif()
        endif()
    elseif(NOT DEFINED gifenc_compile_definitions)
        set(gifenc_compile_definitions ${global_compile_definitions})
    endif()

    target_include_directories(naive_gif PRIVATE
        ${gif_enc_include_dirs}
    )

    target_compile_options(naive_gif PRIVATE ${gifenc_compile_definitions})

    target_link_options(naive_gif PRIVATE ${GLOBAL_LINK_OPTIONS})

    target_compile_definitions(naive_gif PRIVATE ${GLOBAL_COMPILE_DEFINITIONS})
endif()
