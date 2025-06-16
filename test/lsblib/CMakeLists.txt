cmake_minimum_required(VERSION 4.0)

project(lsblib_test)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

add_executable(enc_test
    main_enc.cpp
)

target_link_directories(enc_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../bin/lib/linux
)

target_link_libraries(enc_test PRIVATE
    giflsb
)


target_include_directories(enc_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../lsb/include
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/include
    ${CMAKE_CURRENT_LIST_DIR}/../quant # stb_image
)

add_executable(dec_test
    main_dec.cpp
)

target_link_directories(enc_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../bin/lib/linux
)

target_link_libraries(enc_test PRIVATE
    giflsb
)


target_include_directories(enc_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../lsb/include
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/include
)