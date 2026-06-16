set(UI_TARGET PunchPressUI)

# 收集源码与头文件
file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB INCLUDE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/*.h")

# .ui 文件用 UIC 手动编译为 ui_*.h（根 CMake 未开启 AUTOUIC）
file(GLOB UI_FILES "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.ui")
qt_wrap_ui(GENERATED_UI_HEADERS ${UI_FILES})

# 资源文件
file(GLOB QRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/qrc/*.qrc")

add_library(${UI_TARGET} STATIC
	${SRC_FILES} ${INCLUDE_FILES} ${GENERATED_UI_HEADERS} ${QRC_FILES})

set_target_properties(${UI_TARGET} PROPERTIES
	DEBUG_POSTFIX "d"
	RELEASE_POSTFIX ""
)

target_include_directories(${UI_TARGET}
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_BINARY_DIR}   # 生成的 ui_*.h
)

target_link_libraries(${UI_TARGET}
	PUBLIC
		app::PunchPressApp
		UI::HalconDisplayLabel
		Halcon
		Qt6::Core Qt6::Widgets Qt6::Concurrent
		RWUL::rqwu
)

add_library(UI::${UI_TARGET} ALIAS ${UI_TARGET})
