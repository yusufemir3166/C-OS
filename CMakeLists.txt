cmake_minimum_required(VERSION 3.16)
project(C_OS LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "This project currently targets Windows only (Win32 API).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(C-OS WIN32
  src/main.cpp
)

# Win32/GDI libraries
 target_link_libraries(C-OS PRIVATE
  user32
  gdi32
  shell32
  msimg32
)
