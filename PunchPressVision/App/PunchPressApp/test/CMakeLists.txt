set(App_TARGET Test_app_PunchPressApp)

file(GLOB_RECURSE INCLUDE_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB_RECURSE SRC_LIST CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

list(APPEND SRC_LIST ${INCLUDE_LIST})

add_executable(${App_TARGET} ${SRC_LIST})

target_include_directories(${App_TARGET}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(${App_TARGET} PRIVATE
    app::PunchPressApp
)

add_windeployqt(${App_TARGET})
