cmake_minimum_required(VERSION 3.16)
project(kotoproto-os LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(firmware)
add_subdirectory(platforms/sim)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
