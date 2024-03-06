# xspress-detector

This module contains the following Odin support for Xspress 3 and 4 readout systems:

- The `XspressFrameDecoder` and `XspressProcessPlugin` for readout of MCA data
- The `XspressListModeFrameDecoder` and `XspressListModeProcessPlugin` for readout of event
  data, bypassing `libxspress`
- The `xspressControl` C++ application wrapping `libxspress` and exposing a socket
  - This includes a simulated mode for testing detector control without hardware
- An `odin-control` adapter to control the detector via `xspressControl`
- An xspress-specific `odin-control` adapter for `odin-data` exposing additional
  configuration for the xspress process plugins
- An `odin-data` meta writer plugin for handling xspress meta data from the process plugin

## C++ Build

The C++ build uses CMake. There are two required CMake flags to provide install
directories containing `lib/` and `include/` directories:

- `ODINDATA_ROOT_DIR`: Path to an `odin-data` install (Required)
- `LIBXSPRESS_ROOT_DIR`: Path to `libxspress` libraries and headers (Optionally for `xspressControl`)

For example:

```bash
mkdir -p build && cd build
cmake -DCMAKE_INSTALL_PREFIX=... -DODINDATA_ROOT_DIR=... -DLIBXSPRESS_ROOT_DIR=... ../cpp
make -j8 VERBOSE=1
make install
```

See the [documentation][odin-data-docs] for more information on `odin-data`.

## Python Build

The xspress-detector python package requires [odin-control][odin-control-github] and
[odin-data][odin-data-github], which are not on PyPI. They must be installed into a venv
manually before installing xspress-detector. For example:

```bash
python3 -m venv venv && source venv/bin/activate && pip install --upgrade pip
pip install "odin-control @ git+https://git@github.com/odin-detector/odin-control.git"
pip install "odin-data @ git+https://git@github.com/odin-detector/odin-data.git#subdirectory=python"
pip install ./python
```

## Examples

There are example deployments in the `examples` directory. These contain the startup
scripts and config files for a full Odin deployment for an xspress system. Any paths to
libraries should be updated with absolute paths to installs on a specific system.

These are nominally for xspress 3 and 4, however the only practical difference is the
number of channels and by extension the number of data readout applications.

## Container

There is a container that is built and published in CI. However, this does not build the
`xspressControl` application, as the `libxspress` libraries are not checked in to the
repository. To build a container including `xspressControl`, add a directory containing
the `libxspress` libs and headers to `xspress-detector` and uncomment the relevant lines
of the Dockerfile to include them in the container build.

[odin-control-github]: https://github.com/odin-detector/odin-control
[odin-data-github]: https://github.com/odin-detector/odin-data
[odin-data-docs]: https://odin-detector.github.io/odin-data/
