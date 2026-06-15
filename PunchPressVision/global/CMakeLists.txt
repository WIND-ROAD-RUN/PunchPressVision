set(global_TARGET global)

file(GLOB_RECURSE INCLUDE_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB_RECURSE SRC_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(APPEND SRC_LIST ${INCLUDE_LIST})

add_library(${global_TARGET} STATIC ${SRC_LIST} )

target_link_libraries(${global_TARGET}
	PUBLIC
		Qt6::Core
)

set_target_properties(${global_TARGET} PROPERTIES
	DEBUG_POSTFIX "d"
	RELEASE_POSTFIX ""
)

target_include_directories(${global_TARGET} 
	PUBLIC 
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	PRIVATE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/private/include>
)

add_library(global::${global_TARGET} ALIAS ${global_TARGET})
