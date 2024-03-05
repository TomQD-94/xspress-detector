set(INCLUDE_DIR ${CONTROL_DIR}/include)
set(APP_DIR ${CONTROL_DIR}/src)

include_directories(${INCLUDE_DIR})

add_subdirectory(${APP_DIR})
