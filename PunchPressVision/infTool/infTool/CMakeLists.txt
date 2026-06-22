set(InfTool_TARGET infTool)

file(GLOB_RECURSE INCLUDE_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB_RECURSE SRC_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(APPEND SRC_LIST ${INCLUDE_LIST})

add_library(${InfTool_TARGET} STATIC ${SRC_LIST})

target_link_libraries(${InfTool_TARGET}
	PUBLIC
		inf::infrastructure
		infTool::CalibInfTool
		infTool::NinePointInfTool
		infTool::TwoCameraSpliceInfTool
)

set_target_properties(${InfTool_TARGET} PROPERTIES
	DEBUG_POSTFIX "d"
	RELEASE_POSTFIX ""
)

target_include_directories(${InfTool_TARGET}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	PRIVATE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/private/include>
)

add_library(infTool::${InfTool_TARGET} ALIAS ${InfTool_TARGET})

if(BUILD_TESTING)
add_subdirectory(./test)
endif()
