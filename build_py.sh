#!/bin/bash 

TOOLS_SUPPORT=/dls_sw/prod/tools/RHEL7-x86_64
LOG4CXX=$TOOLS_SUPPORT/log4cxx/0-10-0dls2
CMAKE=$TOOLS_SUPPORT/cmake/3-9-6
ODINDATA=$TOOLS_SUPPORT/odin-data/1-3-1dls1

PYTHON=$TOOLS_SUPPORT/Python/2-7-13
ODINCONTROL=$TOOLS_SUPPORT/../../common/python/RHEL7-x86_64/odin-control/0-9-0dls1
ENUM=$TOOLS_SUPPORT/enum34/1-1-6
CONFIGPARSER=$TOOLS_SUPPORT/configparser/3-5-0


PREFIX=$(pwd)/prefix
mkdir -p $PREFIX/lib/python2.7/site-packages
cd control
source venv/bin/activate
export PYTHONPATH=$PYTHONPATH:$PREFIX/lib/python2.7/site-packages:$ENUM:$ODINDATA:$CONFIGPARSER:$ODINCONTROL
python setup.py install --prefix $PREFIX
