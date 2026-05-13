if(NOT EXISTS "${HALCON_DIR}")
    message(FATAL_ERROR "HALCON_DIR does not exist: ${HALCON_DIR}")
endif()

set(HALCON_INCLUDE_DIR "${HALCON_DIR}/include")
set(HALCON_LIB_DIR "${HALCON_DIR}/lib/x64-win64")
set(HALCON_BIN_DIR "${HALCON_DIR}/bin/x64-win64")

if(NOT EXISTS "${HALCON_INCLUDE_DIR}")
    message(FATAL_ERROR "HALCON include directory not found: ${HALCON_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${HALCON_LIB_DIR}")
    message(FATAL_ERROR "HALCON lib directory not found: ${HALCON_LIB_DIR}")
endif()

if(NOT EXISTS "${HALCON_BIN_DIR}")
    message(FATAL_ERROR "HALCON bin directory not found: ${HALCON_BIN_DIR}")
endif()

add_library(Halcon INTERFACE IMPORTED GLOBAL)

file(GLOB HALCON_LIBS "${HALCON_LIB_DIR}/*.lib")

target_link_libraries(Halcon INTERFACE "${HALCON_LIBS}")

target_include_directories(Halcon INTERFACE "${HALCON_INCLUDE_DIR}")

file(GLOB HALCON_DLLS "${HALCON_BIN_DIR}/halcon*.dll")

if(NOT HALCON_DLLS)
    message(WARNING "No halcon*.dll found in ${HALCON_BIN_DIR}")
else()    
    set_target_properties(Halcon PROPERTIES
        HALCON_DLL_FILES "${HALCON_DLLS}"
    )
    list(LENGTH HALCON_DLLS DLL_COUNT)
    message(STATUS "Found ${DLL_COUNT} Halcon DLL(s)")
endif()

message(STATUS "Halcon target created successfully")
