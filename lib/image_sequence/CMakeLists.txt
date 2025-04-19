cmake_minimum_required(VERSION 3.20)

project(image_sequence)

set(CMAKE_CXX_STANDARD 23)

if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT DEFINED GLOBAL_LINK_TYPE AND NOT DEFINED IMSQ_LINK_TYPE)
    set(IMSQ_LINK_TYPE STATIC)
elseif(NOT DEFINED IMSQ_LINK_TYPE)
    set(IMSQ_LINK_TYPE ${GLOBAL_LINK_TYPE})
endif()

if(WIN32)
    find_package(Leptonica CONFIG REQUIRED)
    find_package(Webp CONFIG REQUIRED)

    list(APPEND IMSQ_LINK_LIBRARIES WebP::webp WebP::webpdecoder WebP::webpdemux gdiplus leptonica)
    list(APPEND IMSQ_SRC
        ${CMAKE_CURRENT_LIST_DIR}/src/imsq_gdi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/imsqs_gdi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/gdi_initializer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/quant_leptonica.cpp)
    list(APPEND IMSQ_INCLUDE_DIRS ${Webp_INCLUDE_DIRS} ${Leptonica_INCLUDE_DIRS})
    set(IMSQ_COMPILE_DEFINITIONS "IMSQ_USE_GDIPLUS")
else()
    find_package(PkgConfig REQUIRED)

    # include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/PkgconfigUtils.cmake)
    set(leptonica_libs lept)
    set(ffmpeg_libs libavcodec libavformat libavutil libswscale libavfilter)

    pkg_check_modules(LEPTONICA REQUIRED ${leptonica_libs})
    pkg_check_modules(FFMPEG REQUIRED ${ffmpeg_libs})

    # set(IMSQ_LINK_DIRS "")
    # pkg_append_link_directory(IMSQ_LINK_DIRS ${leptonica_libs} ${ffmpeg_libs})
    list(APPEND IMSQ_LINK_LIBRARIES ${FFMPEG_LIBRARIES} ${LEPTONICA_LIBRARIES})
    list(APPEND IMSQ_SRC
        ${CMAKE_CURRENT_LIST_DIR}/src/imsq_ffmpeg.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/imsqs_ffmpeg.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/quant_leptonica.cpp)
    list(APPEND IMSQ_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIRS} ${LEPTONICA_INCLUDE_DIRS})
    set(IMSQ_COMPILE_DEFINITIONS "IMSQ_USE_FFMPEG")
endif()

list(APPEND IMSQ_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/../file_io/include)

add_library(image_sequence ${IMSQ_LINK_TYPE} ${IMSQ_SRC})

if(NOT DEFINED GLOBAL_COMPILE_OPTIONS AND NOT DEFINED IMSQ_COMPILE_OPTIONS)
    if(CMAKE_COMPILER_IS_GNUCXX)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(IMSQ_COMPILE_OPTIONS -Wall -Wextra -O0 -g3)
        else()
            set(IMSQ_COMPILE_OPTIONS -Wall -Wextra -O3)
        endif()
    endif()

    if(MSVC)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(IMSQ_COMPILE_OPTIONS /W4 /Od /Zi)
        else()
            set(IMSQ_COMPILE_OPTIONS /W4 /O2)
        endif()
    endif()
elseif(NOT DEFINED IMSQ_COMPILE_OPTIONS)
    set(IMSQ_COMPILE_OPTIONS ${GLOBAL_COMPILE_OPTIONS})
endif()

target_compile_options(image_sequence PRIVATE ${IMSQ_COMPILE_OPTIONS})

target_include_directories(image_sequence PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${IMSQ_INCLUDE_DIRS}
)

# -lm
if(CMAKE_COMPILER_IS_GNUCXX)
    list(APPEND IMSQ_LINK_LIBRARIES m)
endif()

target_link_directories(image_sequence PUBLIC ${IMSQ_LINK_DIRS})

target_link_libraries(image_sequence PUBLIC ${IMSQ_LINK_LIBRARIES})

target_link_options(image_sequence PRIVATE ${GLOBAL_LINK_OPTIONS})

target_compile_definitions(image_sequence PRIVATE ${GLOBAL_COMPILE_DEFINITIONS} ${IMSQ_COMPILE_DEFINITIONS})