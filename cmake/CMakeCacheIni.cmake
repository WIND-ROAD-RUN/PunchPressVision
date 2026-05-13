set(QT_DIR "D:" CACHE PATH "Path to Qt installation")
set(RWUL_DIR "D:" CACHE PATH "Path to RWUL installation")
set(HALCON_DIR "D:" CACHE PATH "Path to Halcon installation")

list(APPEND CMAKE_PREFIX_PATH ${QT_DIR})
list(APPEND CMAKE_PREFIX_PATH ${RWUL_DIR})
list(APPEND CMAKE_PREFIX_PATH ${HALCON_DIR})