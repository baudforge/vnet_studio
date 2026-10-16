if(NOT TARGET utf8cpp::utf8cpp)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utfcpp/CMakeLists.txt")
        message(FATAL_ERROR
            "UTF8-CPP is missing. Run: git submodule update --init --recursive"
        )
    endif()

    add_subdirectory(utfcpp EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET ZLIB::ZLIBSTATIC)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/zlib/CMakeLists.txt")
        message(FATAL_ERROR
            "zlib is missing. Run: git submodule update --init --recursive"
        )
    endif()

    set(ZLIB_BUILD_SHARED OFF)
    set(ZLIB_BUILD_STATIC ON)
    set(ZLIB_BUILD_TESTING OFF)
    set(ZLIB_INSTALL OFF)
    add_subdirectory(zlib EXCLUDE_FROM_ALL)
    set_target_properties(zlibstatic PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
    )
endif()
