if(NOT TARGET utf8cpp::utf8cpp)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utfcpp/CMakeLists.txt")
        message(FATAL_ERROR
            "UTF8-CPP is missing. Run: git submodule update --init --recursive"
        )
    endif()

    add_subdirectory(utfcpp EXCLUDE_FROM_ALL)
endif()
