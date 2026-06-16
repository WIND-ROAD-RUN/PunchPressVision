set(CTL_TARGET HalconWidgets)

# 收集源文件与头文件（GLOB 自动包含所有新增控件）
file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB INCLUDE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/*.h")

add_library(${CTL_TARGET} STATIC
    ${SRC_FILES} ${INCLUDE_FILES})

set_target_properties(${CTL_TARGET} PROPERTIES
    DEBUG_POSTFIX "d"
    RELEASE_POSTFIX ""
)

target_include_directories(${CTL_TARGET}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${CTL_TARGET}
    PUBLIC
        Halcon
        Qt6::Widgets
)

add_library(UI::${CTL_TARGET} ALIAS ${CTL_TARGET})
