cmake_minimum_required(VERSION 3.10)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeCacheIni.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/AddThirdSDKTargets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/func.cmake)

project(PunchPressVision VERSION 0.1 LANGUAGES CXX)

# ======================================================
# 版号机制：版本号 + 构建 ID（时间戳_git短哈希）
# ======================================================
set(PPV_VERSION_MAJOR 1)
set(PPV_VERSION_MINOR 0)
set(PPV_VERSION_PATCH 0)
set(PPV_VERSION_BUILD 0)
set(PPV_VERSION_STRING "${PPV_VERSION_MAJOR}.${PPV_VERSION_MINOR}.${PPV_VERSION_PATCH}.${PPV_VERSION_BUILD}")

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d_%H%M%S")
if(GIT_COMMIT_SHORT)
    set(PPV_BUILD_ID "${BUILD_TIMESTAMP}_${GIT_COMMIT_SHORT}")
else()
    set(PPV_BUILD_ID "${BUILD_TIMESTAMP}")
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/PunchPressVision/global/include/global/BuildVersion.hpp.in
    ${CMAKE_BINARY_DIR}/PunchPressVision/global/include/global/BuildVersion.hpp
    @ONLY
)

file(WRITE ${CMAKE_BINARY_DIR}/build.version "${PPV_BUILD_ID}\n")

include(CTest)

list(APPEND CMAKE_PREFIX_PATH ${RWUL_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源码与注释包含中文，统一以 UTF-8 编译，避免 GBK(936) 代码页下
# 行尾字节被误判为续行符（C4819 / 语法错误）。
if(MSVC)
	add_compile_options(/utf-8)
endif()

# Release 模式：全屏无边框显示
add_compile_definitions($<$<CONFIG:Release>:PPV_RELEASE_FULLSCREEN>)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/output/$<CONFIG>/PunchPressVision")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(OpenCV REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Concurrent Network)
find_package(RWUL REQUIRED COMPONENTS oso core rqwcm rqwu hoepModbus lgm)

add_subdirectory(PunchPressVision)