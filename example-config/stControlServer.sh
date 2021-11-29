SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"
LD_LIBRARY_PATH=$SCRIPT_DIR/../control/libxspress-wrapper/support/ $SCRIPT_DIR/../prefix/bin/xspressControl -d 1 -j $SCRIPT_DIR/xspress.json
