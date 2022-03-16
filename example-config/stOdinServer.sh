#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"
#xspress_odin --config=$SCRIPT_DIR/odin_server.cfg --logging=debug --static-path=$(SCRIPT_DIR)/static
/dls_sw/work/R3.14.12.7/support/xspress-odin/xspress-detector/control/xspress/.venvs/xspress-ZEYHa99R/bin/xspress_odin \
	--config=$SCRIPT_DIR/odin_server.cfg
