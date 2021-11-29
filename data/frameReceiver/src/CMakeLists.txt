set(CMAKE_INCLUDE_CURRENT_DIR on)

include_directories(${FRAMERECEIVER_DIR}/include ${ODINDATA_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${LOG4CXX_INCLUDE_DIRS}/.. ${ZEROMQ_INCLUDE_DIRS})

# Add library for sim decoder
add_library(XspressFrameDecoder SHARED XspressFrameDecoder.cpp XspressFrameDecoderLib.cpp)
target_link_libraries(XspressFrameDecoder ${ODINDATA_LIBRARIES} ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES})

install(TARGETS XspressFrameDecoder
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

