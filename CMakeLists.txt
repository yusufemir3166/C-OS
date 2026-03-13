cmake_minimum_required(VERSION 3.16)
project(C_OS LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  message(FATAL_ERROR "This project targets Windows only (native or cross-compiled).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(C-OS WIN32
  src/main.cpp
  resources/version.rc
)

# Win32/GDI libraries
 target_link_libraries(C-OS PRIVATE
  user32
  gdi32
  shell32
  msimg32
)
