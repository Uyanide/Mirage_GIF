function(pkg_append_link_directory link_dirs)
    foreach(name IN LISTS ARGN)
        execute_process(
            COMMAND pkg-config --variable=libdir ${name}
            OUTPUT_VARIABLE PKGCONFIG_LIBDIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        message(STATUS "PKGCONFIG_LIBDIR for ${name}: ${PKGCONFIG_LIBDIR}")

        if(PKGCONFIG_LIBDIR)
            list(APPEND ${link_dirs} ${PKGCONFIG_LIBDIR})
        endif()
    endforeach()

    set(${link_dirs} ${${link_dirs}} PARENT_SCOPE)
endfunction()