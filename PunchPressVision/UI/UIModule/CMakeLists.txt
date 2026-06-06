set(DEMO_TARGET UIModule)

# GLOB扫一层目录下的所有.cpp文件,GLOB_RECURSE扫多层目录下的所有.cpp文件
file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
# GLOB扫一层目录下的所有.h和.hpp文件,GLOB_RECURSE扫多层目录下的所有.h和.hpp文件
file(GLOB INCLUDE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB INCLUDE_HPP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")
list(APPEND INCLUDE_FILES ${INCLUDE_HPP_FILES})

#设置ui文件路径
file(GLOB UI_FILES  "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.ui")

#ui文件UIC编译器手动编译为*_ui.h
qt_wrap_ui(GENERATED_UI_HEADERS ${UI_FILES})

#设置qrc文件路径
file(GLOB QRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/qrc/*.qrc")

# 添加库生成目标
add_library(${DEMO_TARGET} ${SRC_FILES} ${INCLUDE_FILES} ${GENERATED_UI_HEADERS} ${QRC_FILES})

# 添加头文件搜索路径
target_include_directories(${DEMO_TARGET} 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
 PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
)

# 链接底层依赖库
target_link_libraries(${DEMO_TARGET}
	
)

#链接QT库
target_link_libraries(${DEMO_TARGET}
	PRIVATE Qt6::Core Qt6::Widgets Qt6::Concurrent
)

target_link_libraries(${DEMO_TARGET}
	#PUBLIC ${OpenCV_LIBS}
)

#target_link_directories(${DEMO_TARGET}
#	PUBLIC ${OpenCV_INCLUDE_DIRS}
#)

target_link_libraries(${DEMO_TARGET}
	PRIVATE RWUL::rqwu
)

add_library(UI::${DEMO_TARGET} ALIAS ${DEMO_TARGET})


