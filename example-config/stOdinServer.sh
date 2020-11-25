#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

/dls_sw/work/common/src/venv2_rhel6/prefix/bin/odin_server --config=$SCRIPT_DIR/odin_server.cfg --logging=debug
