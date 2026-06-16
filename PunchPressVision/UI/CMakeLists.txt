set(PPV_TARGET_NAME "PunchPressVision")

# 旧 UIModule（重构前外壳）保留编译，但不链接进可执行文件
#add_subdirectory(UIModule)
# 新 UI 模块（复用旧 .ui，接入 App 层）
add_subdirectory(PunchPressUI)
# 可复用 UI 控件库
add_subdirectory(Controls/HalconDisplayLabel)

file(GLOB INCLUDE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(${PPV_TARGET_NAME} ${SRC_FILES} ${INCLUDE_FILES})

target_include_directories(${PPV_TARGET_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PPV_TARGET_NAME}
    PRIVATE
        UI::PunchPressUI
        Halcon
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

add_windeployqt(${PPV_TARGET_NAME})
