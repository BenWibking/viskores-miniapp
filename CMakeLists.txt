cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(ViskoresDemo CXX)
set (CMAKE_CXX_STANDARD 17)

#Find the Viskores package
find_package(Viskores REQUIRED QUIET)

if(TARGET viskores::rendering)
  add_executable(isosurface isosurface.cpp data.cpp)
  add_executable(volrender volrender.cpp data.cpp)
  target_link_libraries(isosurface PRIVATE viskores::filter viskores::rendering viskores::source)
  target_link_libraries(volrender PRIVATE viskores::filter viskores::rendering viskores::source)
endif()
