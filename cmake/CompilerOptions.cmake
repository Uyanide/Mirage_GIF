if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    # warnings
    list(APPEND global_compile_options -Wall -Wextra)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # debug info
        list(APPEND global_compile_options -O0 -g3)
    else()
        # optimization
        list(APPEND global_compile_options -O3)

        # static linking
        if(WIN32)
            list(APPEND global_link_options -static-libgcc -static-libstdc++)

            set(global_link_type STATIC)
        else()
            # set(global_link_type SHARED)
        endif()

        # strip
        list(APPEND global_link_options -Wl,--strip-all)
    endif()

    # use wmain as entry point for unicode support
    if(WIN32)
        list(APPEND global_link_options -municode)
    endif()
elseif(MSVC)
    # warnings
    list(APPEND global_compile_options /W4)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # debug info
        list(APPEND global_compile_options /Od /Zi)

        # static link crt
        list(APPEND global_compile_options /MTd)
    else()
        # optimization
        list(APPEND global_compile_options /O2)

        # static link crt
        list(APPEND global_compile_options /MT)

        set(global_link_type STATIC)

        # strip
        list(APPEND global_link_options /OPT:REF /OPT:ICF /DEBUG:NONE /INCREMENTAL:NO)
    endif()
else()
    message(FATAL_ERROR "Clang and some other compilers are not supported yet :(")
endif()

# use UNICODE as default character set on windows
if(WIN32)
    list(APPEND global_compile_definitions UNICODE _UNICODE)
endif()

# mock command line for testing
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND global_compile_definitions MOCK_COMMAND_LINE)
endif()

if(DEFINED DISABLE_LOGS)
    list(APPEND global_compile_definitions GENERAL_LOGGER_DISABLE)
endif()
