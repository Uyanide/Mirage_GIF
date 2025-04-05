cmake_minimum_required(VERSION 3.20)

project(image_sequence)

set(CMAKE_CXX_STANDARD 23)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(WebP CONFIG REQUIRED)
find_package(Leptonica CONFIG REQUIRED)

add_library(image_sequence STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/imsq_gdi.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/quant_leptonica.cpp
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
            set(TARGET_COMPILE_OPTIONS /W4 /Od /Zi)
        else()
            set(TARGET_COMPILE_OPTIONS /W4 /O2)
        endif()
    endif()
endif()

target_compile_options(image_sequence PRIVATE ${TARGET_COMPILE_OPTIONS})

target_include_directories(image_sequence PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/include
)

set(TARGET_LINK_LIBRARIES
    gdiplus
    leptonica
    WebP::webp WebP::webpdecoder WebP::webpdemux)

# -lm
if(CMAKE_COMPILER_IS_GNUCXX)
    set(TARGET_LINK_LIBRARIES ${TARGET_LINK_LIBRARIES} m)
endif()

target_link_libraries(image_sequence PUBLIC ${TARGET_LINK_LIBRARIES})

target_link_options(image_sequence PRIVATE ${TARGET_LINK_OPTIONS})

target_compile_definitions(image_sequence PRIVATE ${TARGET_COMPILE_DEFINITIONS})