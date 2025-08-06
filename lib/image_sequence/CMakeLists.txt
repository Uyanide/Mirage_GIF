cmake_minimum_required(VERSION 3.20)

list(APPEND image_sequence_source_files
    ${CMAKE_CURRENT_LIST_DIR}/src/imsq_native.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/imsqs_native.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/quant_native.cpp
)

list(APPEND image_sequence_include_dirs
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/include
)

if(CMAKE_COMPILER_IS_GNUCXX)
    list(APPEND image_sequence_link_libraries
        m
    )
endif()

if(DEFINED image_sequence_use_native)
    if(WIN32)
        message(FATAL_ERROR "image_sequence_use_native is not supported on Windows")
    endif()

    list(APPEND image_sequence_compile_definitions "IMSQ_USE_NATIVE")
elseif(WIN32)
    find_package(Webp CONFIG REQUIRED)

    list(APPEND image_sequence_link_libraries
        WebP::webp WebP::webpdecoder WebP::webpdemux
        gdiplus
    )
    list(APPEND image_sequence_source_files
        ${CMAKE_CURRENT_LIST_DIR}/src/imsq_gdi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/imsqs_gdi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/gdi_initializer.cpp
    )
    list(APPEND image_sequence_include_dirs
        ${Webp_INCLUDE_DIRS}
    )
    set(image_sequence_compile_definitions "IMSQ_USE_GDIPLUS")
else()
    find_package(PkgConfig REQUIRED)

    set(ffmpeg_libs libavcodec libavformat libavutil libswscale libavfilter)
    set(webp_libs libwebp)

    pkg_check_modules(FFMPEG REQUIRED ${ffmpeg_libs})
    pkg_check_modules(WEBP REQUIRED ${webp_libs})

    list(APPEND image_sequence_link_libraries
        ${FFMPEG_LIBRARIES}
        ${WEBP_LIBRARIES}
    )
    list(APPEND image_sequence_source_files
        ${CMAKE_CURRENT_LIST_DIR}/src/imsq_ffmpeg.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/imsqs_ffmpeg.cpp
    )
    list(APPEND image_sequence_include_dirs
        ${FFMPEG_INCLUDE_DIRS}
        ${WEBP_INCLUDE_DIRS}
    )
    set(image_sequence_compile_definitions "IMSQ_USE_FFMPEG")
endif()

if(NOT PROJECT_NAME)
    project(image_sequence)

    set(CMAKE_CXX_STANDARD 23)

    if(NOT DEFINED CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release)
    endif()

    if(NOT DEFINED global_link_type AND NOT DEFINED imsq_link_type)
        set(imsq_link_type STATIC)
    elseif(NOT DEFINED imsq_link_type)
        set(imsq_link_type ${global_link_type})
    endif()

    set(IMSQ_SRC
        ${image_sequence_source_files}
    )

    list(APPEND image_sequence_include_dirs ${CMAKE_CURRENT_LIST_DIR}/../file_io/include)

    add_library(image_sequence ${imsq_link_type} ${IMSQ_SRC})

    if(NOT DEFINED global_compile_options AND NOT DEFINED imsq_compile_options)
        if(CMAKE_COMPILER_IS_GNUCXX)
            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                set(imsq_compile_options -Wall -Wextra -O0 -g3)
            else()
                set(imsq_compile_options -Wall -Wextra -O3)
            endif()
        endif()

        if(MSVC)
            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                set(imsq_compile_options /W4 /Od /Zi)
            else()
                set(imsq_compile_options /W4 /O2)
            endif()
        endif()
    elseif(NOT DEFINED imsq_compile_options)
        set(imsq_compile_options ${global_compile_options})
    endif()

    target_compile_options(image_sequence PRIVATE ${imsq_compile_options})

    target_include_directories(image_sequence PRIVATE
        ${image_sequence_include_dirs}
    )

    target_link_libraries(image_sequence PUBLIC ${image_sequence_link_libraries})

    target_link_options(image_sequence PRIVATE ${global_link_options})

    target_compile_definitions(image_sequence PRIVATE ${image_sequence_compile_definitions})
endif()
