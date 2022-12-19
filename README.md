25th October 2021
=================

Build instructions
==================

This module can be built as a DLS tool

```
dls-checkout-module.py -a tools xspress-detector
cd xspress-detector
build_program -t . .
```

This will compile the C++ wrapper application and install the 
binary into the ./prefix directory.


Execution instructions
======================

To run the application requires a control entry point and this can
be configured with a JSON file.  

Create a file called xspress.json and add the following to it

```
[
  {
    "ctrl_endpoint": "tcp://127.0.0.1:12000"
  }
]
```

Currently to run the application requires setting LD_LIBRARY_PATH
to point to the libxspress files, this will be updated soon.

from a terminal 

```
LD_LIBRARY_PATH=$(pwd)/control/libxspress-wrapper/support/ ./prefix/bin/xspressControl -d 1 -j xspress.json 
```

You should see output similar to the following:

```
5 [0x7fead18e5b00] DEBUG Xspress.App null - Debug level set to  1
6 [0x7fead18e5b00] INFO Xspress.App null - XspressController version -128-NOTFOUND starting up
6 [0x7feac0545700] DEBUG Xspress.XspressController null - Running IPC thread service
6 [0x7fead18e5b00] DEBUG Xspress.LibXspressWrapper null - Constructing LibXspressWrapper
7 [0x7fead18e5b00] DEBUG Xspress.XspressDetector null - Constructing XspressDetector
7 [0x7fead18e5b00] DEBUG Xspress.XspressController null - Constructing XspressController
9 [0x7fead18e5b00] DEBUG Xspress.XspressController null - Configuration submitted: {"params":{"ctrl_endpoint":"tcp://127.0.0.1:12000"},"msg_type":"cmd","msg_val":"configure","id":0,"timestamp":"2021-10-25T11:50:36.298574"}
9 [0x7fead18e5b00] DEBUG Xspress.XspressController null - Connecting control channel to endpoint: tcp://127.0.0.1:12000
11 [0x7fead18e5b00] INFO Xspress.XspressController null - Running Xspress controller
```

You can run with --help to get a full list of options.
