FROM ghcr.io/odin-detector/odin-data-build:latest AS build

# Copy xspress-detector source in for build
COPY . /tmp/xspress-detector
# COPY ./libxspress /libxspress

# C++
WORKDIR /tmp/xspress-detector
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=/odin -DODINDATA_ROOT_DIR=/odin -DLIBXSPRESS_ROOT_DIR=/libxspress ../cpp && \
    make -j8 VERBOSE=1 && \
    make install

# Python
WORKDIR /tmp/xspress-detector/python
RUN python -m pip install .

# Final image
FROM ghcr.io/odin-detector/odin-data-runtime:latest
COPY --from=build /odin /odin
# COPY --from=build /libxspress /libxspress
ENV PATH=/odin/bin:/odin/venv/bin:$PATH
WORKDIR /odin
