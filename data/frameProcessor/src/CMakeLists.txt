set(CMAKE_INCLUDE_CURRENT_DIR on)

include_directories(${FRAMEPROCESSOR_DIR}/include ${ODINDATA_INCLUDE_DIRS} ${HDF5_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${LOG4CXX_INCLUDE_DIRS}/.. ${ZEROMQ_INCLUDE_DIRS})

# Add library for sim process plugin
add_library(XspressProcessPlugin SHARED XspressProcessPlugin.cpp XspressProcessPluginLib.cpp)
target_link_libraries(XspressProcessPlugin ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES} ${HDF5_LIBRARIES} ${HDF5HL_LIBRARIES} ${COMMON_LIBRARY})

install(TARGETS XspressProcessPlugin
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

