#!/bin/bash
set -e
mkdir -p test_build prefix
cd test_build
cmake   -DBoost_NO_BOOST_CMAKE=ON               \
        -DCMAKE_INSTALL_PREFIX=$(pwd)/../prefix \
        -DCMAKE_BUILD_TYPE=Debug                \
        -DCMAKE_CXX_FLAGS_DEBUG='-g -O0 -Wall'  \
        -DODINDATA_ROOT_DIR=${ODINDATA_ROOT_DIR} \
        -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=ON  \
        ../data
make -j 1 VERBOSE=1 && make install

