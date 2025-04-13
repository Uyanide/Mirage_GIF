if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    # warnings
    set(TARGET_COMPILE_OPTIONS ${TARGET_COMPILE_OPTIONS} -Wall -Wextra)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # debug info
        set(TARGET_COMPILE_OPTIONS ${TARGET_COMPILE_OPTIONS} -O0 -g3)
    else()
        # optimization
        set(TARGET_COMPILE_OPTIONS ${TARGET_COMPILE_OPTIONS} -O3)

        # static linking
        # set(TARGET_LINK_OPTIONS ${TARGET_LINK_OPTIONS} -static-libgcc -static-libstdc++)

        # strip
        set(TARGET_LINK_OPTIONS ${TARGET_LINK_OPTIONS} -Wl,--strip-all)
    endif()

    # use wmain as entry point for unicode support
    if(WIN32)
        set(TARGET_LINK_OPTIONS ${TARGET_LINK_OPTIONS} -municode)
    endif()
elseif(MSVC)
    # warnings
    set(TARGET_COMPILE_OPTIONS ${TARGET_COMPILE_OPTIONS} /W4)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # debug info
        set(TARGET_COMPILE_OPTIONS ${TARGET_COMPILE_OPTIONS} /Od /Zi)

        # static link crt
        set(TARGET_COMPILE_OPTIONS ${TARGET_COMPILE_OPTIONS} /MTd)
    else()
        # optimization
        set(TARGET_COMPILE_OPTIONS ${TARGET_COMPILE_OPTIONS} /O2)

        # static link crt
        set(TARGET_COMPILE_OPTIONS ${TARGET_COMPILE_OPTIONS} /MT)

        # strip
        set(TARGET_LINK_OPTIONS ${TARGET_LINK_OPTIONS} /OPT:REF /OPT:ICF /DEBUG:NONE /INCREMENTAL:NO)
    endif()
else()
    message(FATAL_ERROR "Clang and some other compilers are not supported yet :(")
endif()

# use UNICODE as default character set on windows
if(WIN32)
    set(TARGET_COMPILE_DEFINITIONS ${TARGET_COMPILE_DEFINITIONS} UNICODE _UNICODE)
endif()

# mock command line for testing
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(TARGET_COMPILE_DEFINITIONS ${TARGET_COMPILE_DEFINITIONS} MOCK_COMMAND_LINE)
endif()