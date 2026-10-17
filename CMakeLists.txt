cmake_minimum_required(VERSION 3.16)

project(vnet_studio LANGUAGES C CXX)

option(BUILD_SHARED_LIBS "Build shared libraries." ON)

add_subdirectory(third_party EXCLUDE_FROM_ALL)
add_subdirectory(sdk/libvblf)
add_subdirectory(examples/libvblf_read)
