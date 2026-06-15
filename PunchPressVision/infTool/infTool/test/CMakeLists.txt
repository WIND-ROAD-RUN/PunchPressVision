set(InfTool_TARGET Test_infTool_infTool)

file(GLOB_RECURSE INCLUDE_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB_RECURSE SRC_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

list(APPEND SRC_LIST ${INCLUDE_LIST})

add_executable(${InfTool_TARGET} ${SRC_LIST})

target_include_directories(${InfTool_TARGET}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(${InfTool_TARGET} PRIVATE
	infTool::infTool
)

add_windeployqt(${InfTool_TARGET})
