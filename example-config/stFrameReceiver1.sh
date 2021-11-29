#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

cd /dls_sw/work/common/src/odin-data_rhel6
prefix/bin/frameReceiver --sharedbuf=exc_buf_1 -m 104857600 --ctrl=tcp://0.0.0.0:5050 --ready=tcp://127.0.0.1:5001 --release=tcp://127.0.0.1:5002 --logconfig $SCRIPT_DIR/log4cxx.xml --rxtype=zmq -p 1515 -i 127.0.0.1
