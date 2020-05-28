function(include_cxx_filesystem_library)

    include(CheckIncludeFileCXX)
    CHECK_INCLUDE_FILE_CXX(filesystem FILESYSTEM_INCLUDE_FILE)
    if (NOT FILESYSTEM_INCLUDE_FILE)
        message(STATUS "Cannot find the C++17 <filesystem> library. Use own implementation.")
        add_definitions(-DMETALL_NOT_USE_CXX17_FILESYSTEM_LIB)
        return()
    endif ()

    if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")) # GCC
        link_libraries(stdc++fs)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")  # Clang or AppleClang
        if (NOT APPLE)
            link_libraries(c++fs)
        else()
            include(get_macos_version)
            get_macos_version()
            message(VERBOSE "macOS version ${MACOS_VERSION}")
            if (MACOS_VERSION VERSION_GREATER_EQUAL 10.15)
                link_libraries(c++fs)
            else ()
                message(STATUS "macOS >= 10.15 is required to use the C++17 <filesystem> library. Use own implementation.")
                add_definitions(-DMETALL_NOT_USE_CXX17_FILESYSTEM_LIB)
            endif ()
        endif ()
    endif ()
endfunction()