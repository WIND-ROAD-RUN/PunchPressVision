set(Inf_TARGET ConfigModule)

file(GLOB OSO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/osoFile/*.oso")
oso_wrap_oso(GENERATED_HEADERS ${OSO_FILES} OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/infrastructure/ConfigModule/Config)

file(GLOB_RECURSE INCLUDE_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.h"
)
file(GLOB_RECURSE SRC_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(APPEND SRC_LIST ${INCLUDE_LIST})

add_library(${Inf_TARGET} STATIC ${SRC_LIST} ${GENERATED_HEADERS})

target_link_libraries(${Inf_TARGET} PUBLIC RWUL::oso)

target_link_libraries(${Inf_TARGET} PRIVATE global::global)

set_target_properties(${Inf_TARGET} PROPERTIES
	DEBUG_POSTFIX "d"
	RELEASE_POSTFIX ""
)

target_include_directories(${Inf_TARGET} 
	PUBLIC 
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	PRIVATE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/private/include>
)

add_library(inf::${Inf_TARGET} ALIAS ${Inf_TARGET})

add_subdirectory(./test)