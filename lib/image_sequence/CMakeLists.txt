list(APPEND SOURCE_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/imsq_native.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/imsqs_native.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/quant_native.cpp
)

if(DEFINED IMSQ_USE_NATIVE)
    list(APPEND COMPILE_DEFINITIONS "IMSQ_USE_NATIVE")
elseif(WIN32)
    find_package(Webp CONFIG REQUIRED)

    list(APPEND LINK_LIBRARIES WebP::webp WebP::webpdecoder WebP::webpdemux gdiplus)
    list(APPEND SOURCE_FILES
        ${CMAKE_CURRENT_LIST_DIR}/src/imsq_gdi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/imsqs_gdi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/gdi_initializer.cpp
    )
    list(APPEND INCLUDE_DIRS ${Webp_INCLUDE_DIRS})
    set(COMPILE_DEFINITIONS "IMSQ_USE_GDIPLUS")
else()
    find_package(PkgConfig REQUIRED)

    set(ffmpeg_libs libavcodec libavformat libavutil libswscale libavfilter)

    pkg_check_modules(FFMPEG REQUIRED ${ffmpeg_libs})

    list(APPEND LINK_LIBRARIES ${FFMPEG_LIBRARIES})
    list(APPEND SOURCE_FILES
        ${CMAKE_CURRENT_LIST_DIR}/src/imsq_ffmpeg.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/imsqs_ffmpeg.cpp
    )
    list(APPEND INCLUDE_DIRS ${FFMPEG_INCLUDE_DIRS})
    set(COMPILE_DEFINITIONS "IMSQ_USE_FFMPEG")
endif()

list(APPEND INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../file_io/include)

# -lm
if(CMAKE_COMPILER_IS_GNUCXX)
    list(APPEND LINK_LIBRARIES m)
endif()