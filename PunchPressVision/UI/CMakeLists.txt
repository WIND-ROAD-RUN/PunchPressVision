set(PPV_TARGET_NAME "PunchPressVision")

file(GLOB INCLUDE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(${PPV_TARGET_NAME} ${SRC_FILES} ${INCLUDE_FILES})

target_include_directories(${PPV_TARGET_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PPV_TARGET_NAME}
    PRIVATE
        Halcon
)

target_link_libraries(${PPV_TARGET_NAME}
    PRIVATE
        Qt6::Core Qt6::Widgets
)

get_target_property(HALCON_DLLS Halcon HALCON_DLL_FILES)
if(HALCON_DLLS)
    add_custom_command(TARGET ${PPV_TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${HALCON_DLLS}
        $<TARGET_FILE_DIR:${PPV_TARGET_NAME}>
        COMMENT "Copying Halcon DLLs to output directory..."
    )
endif()

add_custom_command(TARGET ${PPV_TARGET_NAME} POST_BUILD
	COMMAND ${QT_DIR}/bin/windeployqt.exe --no-compiler-runtime --no-opengl-sw $<TARGET_FILE:${PPV_TARGET_NAME}>
	COMMENT "Running windeployqt to package Qt dependencies for ${PPV_TARGET_NAME}"
)