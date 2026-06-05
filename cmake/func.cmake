function(add_windeployqt target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${QT_DIR}/bin/windeployqt.exe --no-compiler-runtime --no-opengl-sw $<TARGET_FILE:${target_name}>
        COMMENT "Running windeployqt to package Qt dependencies for ${target_name}"
    )
endfunction()