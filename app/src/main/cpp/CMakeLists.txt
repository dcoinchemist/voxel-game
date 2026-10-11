cmake_minimum_required(VERSION 3.22.1)
project(voxelgame CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(voxelgame SHARED jni_bridge.cpp)

find_library(log-lib log)
target_link_libraries(voxelgame GLESv3 ${log-lib})
