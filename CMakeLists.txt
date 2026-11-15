

cmake_minimum_required(VERSION 3.21)
set(CMAKE_C_STANDARD 23)

project(strome)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# CEF binary distribution root. Override with -DCEF_ROOT=... (e.g. cef/macosarm64).
if(NOT DEFINED CEF_ROOT)
  if(APPLE)
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
      set(CEF_PLATFORM "macosarm64")
    else()
      set(CEF_PLATFORM "macosx64")
    endif()
  else()
    set(CEF_PLATFORM "linux64")
  endif()
  set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef/${CEF_PLATFORM}")
endif()
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")

find_package(CEF REQUIRED)

add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)


add_subdirectory(src/)
