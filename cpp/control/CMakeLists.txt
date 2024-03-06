set(INCLUDE_DIR ${CONTROL_DIR}/include)
set(APP_DIR ${CONTROL_DIR}/src)

include_directories(${INCLUDE_DIR} ${LIBXSPRESS_INCLUDE_DIRS})

add_subdirectory(${APP_DIR})
