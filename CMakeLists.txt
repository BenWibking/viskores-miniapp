cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(VolRenderTest CXX)
set (CMAKE_CXX_STANDARD 17)

# Find required packages
find_package(MPI REQUIRED)
find_package(Viskores REQUIRED)

# Disable testing for miniGraphics to avoid extra dependencies
#set(MINIGRAPHICS_ENABLE_TESTING OFF CACHE BOOL "Disable testing")

# Add miniGraphics subdirectory
#add_subdirectory(miniGraphics)

# Build MPI volume rendering executable with Viskores dependency
add_executable(volrender_mpi 
  volrender_mpi.cpp
  data.cpp)

target_link_libraries(volrender_mpi PRIVATE 
  MPI::MPI_CXX
  viskores::cont
  viskores::filter
  viskores::rendering
  viskores::source)

target_include_directories(volrender_mpi PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/Common)
