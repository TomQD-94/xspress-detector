#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

/usr/local/bin/xspressControl -j $SCRIPT_DIR/xspress.json -s 1
