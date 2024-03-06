#!/bin/bash

prefix/bin/xspress_meta_writer -w xspress_detector.data.xspress_meta_writer.XspressMetaWriter --sensor-shape 36 4096 --data-endpoints tcp://127.0.0.1:10008,tcp://127.0.0.1:10018,tcp://127.0.0.1:10028,tcp://127.0.0.1:10038,tcp://127.0.0.1:10048,tcp://127.0.0.1:10058,tcp://127.0.0.1:10068,tcp://127.0.0.1:10078,tcp://127.0.0.1:10088 --static-log-fields beamline=${BEAMLINE},detector="Xspress4" --log-server "graylog-log-target.diamond.ac.uk:12210"
