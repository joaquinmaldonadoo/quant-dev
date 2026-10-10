cmake_minimum_required(VERSION 3.20)
project(hft_mm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(HFT_BUILD_TESTS "Compilar los tests" ON)
option(HFT_NATIVE "Compilar con -march=native" OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
if(HFT_NATIVE)
  add_compile_options(-march=native)
endif()

find_package(Threads REQUIRED)

add_library(hft_core
  src/config.cpp
  src/logger.cpp
)
target_include_directories(hft_core PUBLIC include)
target_link_libraries(hft_core PUBLIC Threads::Threads)

if(HFT_BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
  )
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  add_subdirectory(tests)
endif()
