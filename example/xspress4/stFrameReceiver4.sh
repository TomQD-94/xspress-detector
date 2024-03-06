#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

prefix/bin/frameReceiver --io-threads 1 --ctrl=tcp://0.0.0.0:10030 --config=$SCRIPT_DIR/fr4.json --log-config $SCRIPT_DIR/log4cxx.xml
