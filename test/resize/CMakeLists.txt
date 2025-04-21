cmake_minimum_required(VERSION 3.20)

project(resize_test)

set(CMAKE_CXX_STANDARD 23)

set(IMSQ_USE_NATIVE true)
set(LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../../lib)
add_subdirectory(${LIB_DIR}/image_sequence ${CMAKE_BINARY_DIR}/lib_image_sequence)

add_executable(resize_test main.cpp)

target_link_directories(resize_test PRIVATE
    ${CMAKE_BINARY_DIR}/lib_image_sequence
)

target_include_directories(resize_test PRIVATE
    ${LIB_DIR}/image_sequence/include
    ${LIB_DIR}/include
)

target_link_libraries(resize_test PRIVATE image_sequence)