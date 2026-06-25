set(Bun_TARGET ToolTwoCameraSplice)

file(GLOB_RECURSE INCLUDE_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB_RECURSE SRC_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# .ui 文件用 UIC 编译为 ui_*.h
file(GLOB UI_FILES "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.ui")
qt_wrap_ui(GENERATED_UI_HEADERS ${UI_FILES})

list(APPEND SRC_LIST ${INCLUDE_LIST} ${GENERATED_UI_HEADERS})

add_executable(${Bun_TARGET} ${SRC_LIST})

target_include_directories(${Bun_TARGET}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${Bun_TARGET} PRIVATE
    infTool::TwoCameraSpliceInfTool
    inf::infrastructure
    UI::HalconWidgets
    Halcon
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    ${OpenCV_LIBS}
)

add_windeployqt(${Bun_TARGET})

# 将 build.version 复制到可执行文件输出目录，供运行时版本一致性检查
add_custom_command(TARGET ${Bun_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_BINARY_DIR}/build.version
    $<TARGET_FILE_DIR:${Bun_TARGET}>
    COMMENT "Copying build.version to output directory..."
)
