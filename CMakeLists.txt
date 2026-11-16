

cmake_minimum_required(VERSION 3.21)
set(CMAKE_C_STANDARD 23)

project(strome)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CEF_VERSION "156.0.15+gcf671c0+chromium-156.0.8078.80"
CACHE STRING "CEF binary distribution version")


if(NOT DEFINED CEF_ROOT)
  if(APPLE)
    if(CMAKE_OSX_ARCHITECTURES STREQUAL
      "arm64" OR CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
      set(CEF_PLATFORM "macosarm64")
    else()
      set(CEF_PLATFORM "macosx64")
    endif()
  else()
    set(CEF_PLATFORM "linux64")
  endif()
  set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef/${CEF_PLATFORM}")
endif()

# Download the matching CEF binary distribution if it is not present.
if(NOT EXISTS "${CEF_ROOT}/cmake/cef_variables.cmake")
  set(_cef_archive "cef_binary_${CEF_VERSION}_${CEF_PLATFORM}.tar.bz2")
  set(_cef_url "https://cef-builds.spotifycdn.com/${_cef_archive}")
  set(_cef_tmp "${CMAKE_CURRENT_SOURCE_DIR}/cef/.tmp")
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cef")
  file(MAKE_DIRECTORY "${_cef_tmp}")
  message(STATUS "Downloading CEF
  ${CEF_VERSION} (${CEF_PLATFORM}) from ${_cef_url}")
  file(DOWNLOAD "${_cef_url}" "${_cef_tmp}/${_cef_archive}"
  SHOW_PROGRESS STATUS _cef_dl_status)
  list(GET _cef_dl_status 0 _cef_dl_code)
  if(NOT _cef_dl_code EQUAL 0)
    list(GET _cef_dl_status 1 _cef_dl_msg)
    message(FATAL_ERROR "Failed to download CEF: ${_cef_dl_msg}")
  endif()
  message(STATUS "Extracting CEF archive")
  execute_process(
    COMMAND tar -xjf "${_cef_tmp}/${_cef_archive}" -C "${_cef_tmp}"
    RESULT_VARIABLE _cef_tar_result
  )
  if(NOT _cef_tar_result EQUAL 0)
    message(FATAL_ERROR "Failed to extract CEF archive")
  endif()
  file(RENAME "${_cef_tmp}/cef_binary_${CEF_VERSION}_${CEF_PLATFORM}"
  "${CEF_ROOT}")
  file(REMOVE_RECURSE "${_cef_tmp}")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")

find_package(CEF REQUIRED)

add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)


add_subdirectory(src/)
