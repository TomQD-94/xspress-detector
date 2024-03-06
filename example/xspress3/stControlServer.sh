#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

prefix/bin/xspressControl -j $SCRIPT_DIR/xspress.json -s 1
