#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

export HDF5_PLUGIN_PATH=hdf5_1.10/h5plugin

/home/xspress3/ODIN/odin-data/xsp3_ODIN/bin/frameProcessor --ctrl=tcp://0.0.0.0:10004 --config=$SCRIPT_DIR/fp1.json
