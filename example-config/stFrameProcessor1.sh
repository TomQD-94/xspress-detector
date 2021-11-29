#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

export HDF5_PLUGIN_PATH=/dls_sw/prod/tools/RHEL6-x86_64/hdf5filters/0-3-0/prefix/hdf5_1.10/h5plugin

cd /dls_sw/work/common/src/odin-data_rhel6
gdbserver 127.0.0.1:1414 prefix/bin/frameProcessor --ctrl=tcp://0.0.0.0:5004 --ready=tcp://127.0.0.1:5001 --release=tcp://127.0.0.1:5002 --logconfig $SCRIPT_DIR/log4cxx.xml
