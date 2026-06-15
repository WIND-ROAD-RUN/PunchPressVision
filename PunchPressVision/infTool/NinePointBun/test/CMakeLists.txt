set(Bun_TARGET Test_infTool_NinePointBun)

file(GLOB_RECURSE INCLUDE_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB_RECURSE SRC_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

list(APPEND SRC_LIST ${INCLUDE_LIST})

add_executable(${Bun_TARGET} ${SRC_LIST})

target_include_directories(${Bun_TARGET}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(${Bun_TARGET} PRIVATE
    infTool::NinePointBun
)

add_windeployqt(${Bun_TARGET})
