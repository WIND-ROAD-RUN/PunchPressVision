cmake_minimum_required(VERSION 3.10)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeCacheIni.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/AddThirdSDKTargets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/func.cmake)

project(PunchPressVision VERSION 0.1 LANGUAGES CXX)

include(CTest)

list(APPEND CMAKE_PREFIX_PATH ${RWUL_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源码与注释包含中文，统一以 UTF-8 编译，避免 GBK(936) 代码页下
# 行尾字节被误判为续行符（C4819 / 语法错误）。
if(MSVC)
	add_compile_options(/utf-8)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/output/$<CONFIG>")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(OpenCV REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Concurrent Network)
find_package(RWUL REQUIRED COMPONENTS oso core rqwcm rqwu hoepModbus lgm)

add_subdirectory(PunchPressVision)