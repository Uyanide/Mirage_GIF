cmake_minimum_required(VERSION 4.0)

project(quant_test)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_BUILD_TYPE Release)

add_executable(quant_test_native
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/image_sequence/src/quant_native.cpp
)

target_include_directories(quant_test_native PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/image_sequence/include
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/include
)

target_compile_options(quant_test_native PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -O3
)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LEPTONICA REQUIRED lept)

add_executable(quant_test_lept
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/image_sequence/src/quant_leptonica.cpp
)

target_include_directories(quant_test_lept PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/image_sequence/include
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/include
    ${LEPTONICA_INCLUDE_DIRS}
)

target_link_libraries(quant_test_lept
    ${LEPTONICA_LIBRARIES}
)

target_compile_options(quant_test_lept PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -O3
)