#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

xspress_odin --config=$SCRIPT_DIR/odin_server.cfg --logging=debug
