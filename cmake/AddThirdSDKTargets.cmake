# 检查Halcon路径是否存在
if(NOT EXISTS "${HALCON_DIR}")
    message(FATAL_ERROR "HALCON_DIR does not exist: ${HALCON_DIR}")
endif()

# 定义路径
set(HALCON_INCLUDE_DIR "${HALCON_DIR}/include")
set(HALCON_LIB_DIR "${HALCON_DIR}/lib/x64-win64")
set(HALCON_BIN_DIR "${HALCON_DIR}/bin/x64-win64")

# 检查必要的路径
if(NOT EXISTS "${HALCON_INCLUDE_DIR}")
    message(FATAL_ERROR "HALCON include directory not found: ${HALCON_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${HALCON_LIB_DIR}")
    message(FATAL_ERROR "HALCON lib directory not found: ${HALCON_LIB_DIR}")
endif()

if(NOT EXISTS "${HALCON_BIN_DIR}")
    message(FATAL_ERROR "HALCON bin directory not found: ${HALCON_BIN_DIR}")
endif()

# 创建INTERFACE库目标
add_library(Halcon INTERFACE IMPORTED GLOBAL)

# 设置库文件位置
file(GLOB HALCON_LIB_FILES "${HALCON_LIB_DIR}/*.lib")

if(HALCON_LIB_FILES)
    list(GET HALCON_LIB_FILES 0 HALCON_LIB)
    
    # 对于INTERFACE库，使用target_link_libraries来链接库文件
    target_link_libraries(Halcon INTERFACE "${HALCON_LIB}")
    message(STATUS "Halcon Library: ${HALCON_LIB}")
else()
    message(FATAL_ERROR "No halcon*.lib found in ${HALCON_LIB_DIR}")
endif()

# 设置include目录
target_include_directories(Halcon INTERFACE "${HALCON_INCLUDE_DIR}")

# 收集所有halcon开头的DLL（用于运行时复制）
file(GLOB HALCON_DLLS "${HALCON_BIN_DIR}/halcon*.dll")

if(NOT HALCON_DLLS)
    message(WARNING "No halcon*.dll found in ${HALCON_BIN_DIR}")
else()    
    # 设置DLL文件属性
    set_target_properties(Halcon PROPERTIES
        HALCON_DLL_FILES "${HALCON_DLLS}"
    )
    list(LENGTH HALCON_DLLS DLL_COUNT)
    message(STATUS "Found ${DLL_COUNT} Halcon DLL(s)")
endif()

message(STATUS "Halcon target created successfully")
