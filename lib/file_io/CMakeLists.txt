cmake_minimum_required(VERSION 3.20)

project(naive_file_io)

set(CMAKE_CXX_STANDARD 23)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT DEFINED GLOBAL_LINK_TYPE AND NOT DEFINED FILEIO_LINK_TYPE)
    set(FILEIO_LINK_TYPE STATIC)
elseif(NOT DEFINED FILEIO_LINK_TYPE)
    set(FILEIO_LINK_TYPE ${GLOBAL_LINK_TYPE})
endif()

add_library(naive_file_io ${FILEIO_LINK_TYPE}
    ${CMAKE_CURRENT_LIST_DIR}/src/file_reader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/file_writer.cpp
)

if(NOT DEFINED GLOBAL_COMPILE_OPTIONS AND NOT DEFINED FILEIO_COMPILE_OPTIONS)
    if(CMAKE_COMPILER_IS_GNUCXX)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(FILEIO_COMPILE_OPTIONS -Wall -Wextra -O0 -g3)
        else()
            set(FILEIO_COMPILE_OPTIONS -Wall -Wextra -O3)
        endif()
    endif()

    if(MSVC)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(FILEIO_COMPILE_OPTIONS /Od /Zi)
        else()
            set(FILEIO_COMPILE_OPTIONS /O2)
        endif()
    endif()
elseif(NOT DEFINED FILEIO_COMPILE_OPTIONS)
    set(FILEIO_COMPILE_OPTIONS ${GLOBAL_COMPILE_OPTIONS})
endif()

target_include_directories(naive_file_io PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_options(naive_file_io PRIVATE ${FILEIO_COMPILE_OPTIONS})

target_link_options(naive_file_io PRIVATE ${GLOBAL_LINK_OPTIONS})

target_compile_definitions(naive_file_io PRIVATE ${GLOBAL_COMPILE_DEFINITIONS})