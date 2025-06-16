if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_COMPILER_IS_GNUCXX)
    # warnings
    list(APPEND GLOBAL_COMPILE_OPTIONS -Wall -Wextra)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # debug info
        list(APPEND GLOBAL_COMPILE_OPTIONS -O0 -g3)
    else()
        # optimization
        list(APPEND GLOBAL_COMPILE_OPTIONS -O3)

        # static linking
        if(WIN32)
            list(APPEND GLOBAL_LINK_OPTIONS -static-libgcc -static-libstdc++)

            set(GLOBAL_LINK_TYPE STATIC)
        else()
            # set(GLOBAL_LINK_TYPE SHARED)
        endif()

        # strip
        list(APPEND GLOBAL_LINK_OPTIONS -Wl,--strip-all)
    endif()

    # use wmain as entry point for unicode support
    if(WIN32)
        list(APPEND GLOBAL_LINK_OPTIONS -municode)
    endif()
elseif(MSVC)
    # warnings
    list(APPEND GLOBAL_COMPILE_OPTIONS /W4)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # debug info
        list(APPEND GLOBAL_COMPILE_OPTIONS /Od /Zi)

        # static link crt
        list(APPEND GLOBAL_COMPILE_OPTIONS /MTd)
    else()
        # optimization
        list(APPEND GLOBAL_COMPILE_OPTIONS /O2)

        # static link crt
        list(APPEND GLOBAL_COMPILE_OPTIONS /MT)

        set(GLOBAL_LINK_TYPE STATIC)

        # strip
        list(APPEND GLOBAL_LINK_OPTIONS /OPT:REF /OPT:ICF /DEBUG:NONE /INCREMENTAL:NO)
    endif()
else()
    message(FATAL_ERROR "Clang and some other compilers are not supported yet :(")
endif()

# use UNICODE as default character set on windows
if(WIN32)
    list(APPEND GLOBAL_COMPILE_DEFINITIONS UNICODE _UNICODE)
endif()

# mock command line for testing
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND GLOBAL_COMPILE_DEFINITIONS MOCK_COMMAND_LINE)
endif()

if(DEFINED DISABLE_LOGS)
    list(APPEND GLOBAL_COMPILE_DEFINITIONS GENERAL_LOGGER_DISABLE)
endif()
