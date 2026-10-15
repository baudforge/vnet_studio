cmake_minimum_required(VERSION 3.16)

project(vblf LANGUAGES CXX)

option(BUILD_SHARED_LIBS "Build shared libraries." ON)

if(NOT TARGET utf8cpp::utf8cpp)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party"
        "${CMAKE_CURRENT_BINARY_DIR}/third_party"
        EXCLUDE_FROM_ALL
    )
endif()

add_library(vblf
    inc/vblf.h
    src/vblf.cpp
)
add_library(vblf::vblf ALIAS vblf)

target_link_libraries(vblf PRIVATE utf8cpp::utf8cpp)

target_compile_features(vblf PRIVATE cxx_std_17)
set_target_properties(vblf PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

target_include_directories(vblf PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>"
    "$<INSTALL_INTERFACE:include>"
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(vblf
        PUBLIC VBLF_SHARED
        PRIVATE VBLF_BUILD_EXPORTS
    )
endif()

if(MSVC)
    target_compile_options(vblf PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(vblf PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)

install(TARGETS vblf
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(FILES inc/vblf.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
