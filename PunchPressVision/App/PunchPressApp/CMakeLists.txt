set(App_TARGET PunchPressApp)

file(GLOB_RECURSE INCLUDE_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB_RECURSE SRC_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(APPEND SRC_LIST ${INCLUDE_LIST})

add_library(${App_TARGET} STATIC ${SRC_LIST} )

set_target_properties(${App_TARGET} PROPERTIES
	DEBUG_POSTFIX "d"
	RELEASE_POSTFIX ""
)

target_include_directories(${App_TARGET}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	PRIVATE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/private/include>
)

target_link_libraries(${App_TARGET}
	PUBLIC
		bun::Business
		Qt6::Core
)

add_library(app::${App_TARGET} ALIAS ${App_TARGET})

add_subdirectory(./test)
