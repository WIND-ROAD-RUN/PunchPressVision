cmake_minimum_required(VERSION 3.10)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeCacheIni.cmake)
#include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/AddThirdSDKTargets.cmake)

project(PunchPressVision VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Concurrent Network)

add_subdirectory(PunchPressVision)