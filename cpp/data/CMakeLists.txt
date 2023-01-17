set(FRAMERECEIVER_DIR ${DATA_DIR}/frameReceiver)
set(FRAMEPROCESSOR_DIR ${DATA_DIR}/frameProcessor)
set(FRAMESIMULATOR_DIR ${DATA_DIR}/frameSimulator)

include_directories(${COMMON_DIR}/include)

add_subdirectory(${FRAMERECEIVER_DIR})
add_subdirectory(${FRAMEPROCESSOR_DIR})
