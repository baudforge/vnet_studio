cmake_minimum_required(VERSION 3.16)

project(vblf_read_example LANGUAGES CXX)

if(NOT TARGET vblf::vblf)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/libvblf"
        "${CMAKE_CURRENT_BINARY_DIR}/vblf"
    )
endif()

add_executable(libvblf_read main.cpp)
target_link_libraries(libvblf_read PRIVATE vblf::vblf)
target_compile_features(libvblf_read PRIVATE cxx_std_17)
set_target_properties(libvblf_read PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(libvblf_read PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(libvblf_read PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MINGW)
    target_link_options(libvblf_read PRIVATE -municode)
endif()

if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET libvblf_read POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:vblf>" "$<TARGET_FILE_DIR:libvblf_read>"
        VERBATIM
    )
endif()
