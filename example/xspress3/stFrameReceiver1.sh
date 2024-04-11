#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

/home/xspress3/ODIN/odin-data/xsp3_ODIN/bin/frameReceiver --io-threads 1 --ctrl=tcp://0.0.0.0:10000 --config=$SCRIPT_DIR/fr1.json
