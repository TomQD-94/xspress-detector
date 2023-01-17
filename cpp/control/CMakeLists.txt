set(SUPPORT_DIR ${CONTROL_DIR}/support)
set(INCLUDE_DIR ${CONTROL_DIR}/include)
set(APP_DIR ${CONTROL_DIR}/src)

include_directories(${INCLUDE_DIR})
include_directories(${SUPPORT_DIR})

add_subdirectory(${SUPPORT_DIR})
add_subdirectory(${APP_DIR})
