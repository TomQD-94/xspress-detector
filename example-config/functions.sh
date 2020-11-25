#!/bin/bash
source /dls_sw/work/common/src/venv2_rhel6/bin/activate
HDFNAME="${HDFNAME:-"test"}"

function odinconfig() {
	http_client.py 127.0.0.1:8888 fr/config/config_file $(pwd)/fr1.json
	http_client.py 127.0.0.1:8888 fp/config/config_file $(pwd)/fp1.json
	http_client.py 127.0.0.1:8888 fp/config/hdf '{"frames": 20, "file": {"path": "/scratch/tmp", "name": "'"$HDFNAME"'"}}'
	http_client.py 127.0.0.1:8888 fp/config/hdf/dataset/data '{"dims": [10,10]}'
	http_client.py 127.0.0.1:8888 fp/config/hdf/dataset/data '{"chunks": [1,10,10]}'
	http_client.py 127.0.0.1:8888 fp/config/hdf/dataset/data '{"datatype": 0}'
	http_client.py 127.0.0.1:8888 fp/config/hdf/dataset/SUM '{"chunks": [1]}'
	http_client.py 127.0.0.1:8888 fp/config/hdf/dataset/SUM '{"datatype": 3}'
}

function odinstatus() {
	http_client.py 127.0.0.1:8888 fp/config/hdf
	http_client.py 127.0.0.1:8888 fp/status
}

function simgenerator() {
	/dls_sw/work/common/src/odin-sim-detector/prefix/bin/SimGenerator
}

function hdfstart() {
	http_client.py 127.0.0.1:8888 fp/config/hdf '{"write": true}'
}

function hdfstop() {
	http_client.py 127.0.0.1:8888 fp/config/hdf '{"write": false}'
}
