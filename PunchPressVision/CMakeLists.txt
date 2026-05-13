set(PPV_TARGET_NAME "PunchPressVision")

file(GLOB INCLUDE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(${PPV_TARGET_NAME} ${SRC_FILES} ${INCLUDE_FILES})

target_include_directories(${PPV_TARGET_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)