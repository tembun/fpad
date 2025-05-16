project(fpad)
cmake_minimum_required(VERSION 3.0)
set(CMAKE_BUILD_TYPE Release)
add_definitions(-Wall)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(HAIKU)
    set(CMAKE_INSTALL_PREFIX "/boot/home/config/non-packaged/apps/fpad")
  elseif(APPLE)
    set(CMAKE_INSTALL_PREFIX "/Applications")
  elseif(UNIX)
    set(CMAKE_INSTALL_PREFIX "/usr/local")
  endif()
endif()

option(WITHOUT_X11 "Compiling without X11..." OFF)

add_subdirectory(src)
