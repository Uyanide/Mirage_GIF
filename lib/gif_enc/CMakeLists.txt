list(APPEND SOURCE_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_encoder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_format.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_lzw_dec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gif_lzw_enc.cpp
)

list(APPEND INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/include
)