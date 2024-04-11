#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

# Increase maximimum fds available for ZeroMQ sockets
ulimit -n 2048

/home/xspress3/ODIN/venv/bin/xspress_control --config=$SCRIPT_DIR/odin_server.cfg --logging=info --access_logging=ERROR
