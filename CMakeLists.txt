

cmake_minimum_required(VERSION 3.21)


project(strome)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef/macosx64")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")

find_package(CEF REQUIRED)

add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)


add_subdirectory(src/)
