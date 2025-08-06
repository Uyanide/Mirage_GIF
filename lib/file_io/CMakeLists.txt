list(APPEND file_io_source_files
    ${CMAKE_CURRENT_LIST_DIR}/src/file_reader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/file_writer.cpp
)

list(APPEND file_io_include_dirs
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/include
)
