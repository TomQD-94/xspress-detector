"""Implementation of Xspress Meta Writer

This module is a subclass of the odin_data MetaWriter and handles Xspress 
specific meta messages, writing them to disk.

Alan Greer, Diamond Light Source
"""

import numpy as np
import struct
from datetime import datetime

from odin_data.meta_writer.meta_writer import MetaWriter, FRAME
from odin_data.meta_writer.hdf5dataset import (
    Int32HDF5Dataset,
    Int64HDF5Dataset,
    Float32HDF5Dataset,
    Float64HDF5Dataset,
    StringHDF5Dataset,
)
from odin_data.util import construct_version_dict
import _version as versioneer

# Data message types
XSPRESS_SCALARS = "xspress_scalars"
XSPRESS_DTC = "xspress_dtc"
XSPRESS_INP_EST = "xspress_inp_est"

# Number of scalars per channel
XSPRESS_SCALARS_PER_CHANNEL = 9

# Dataset names
DATASET_SCALAR = "scalar_chan"
DATASET_DTC = "dtc_chan"
DATASET_INP_EST = "inp_est"
DATASET_DAQ_VERSION = "data_version"
DATASET_META_VERSION = "meta_version"


class XspressMetaWriter(MetaWriter):
    """Implementation of MetaWriter that also handles Xspress meta messages"""

    def __init__(self, name, directory, endpoints, config):
        # This must be defined for _define_detector_datasets in base class __init__
        self._sensor_shape = config.sensor_shape

        super(XspressMetaWriter, self).__init__(name, directory, endpoints, config)
#        self._detector_finished = False  # Require base class to check we have finished

        self._series = None
        self._flush_time = datetime.now()

    def _define_detector_datasets(self):
        dsets = []
        self._logger.error("IN HERE")
        for index in range(36):
            scalar_name = "{}{}".format(DATASET_SCALAR, index)
            dtc_name = "{}{}".format(DATASET_DTC, index)
            inp_est_name = "{}{}".format(DATASET_INP_EST, index)
            self._logger.error("Adding dataset: {}".format(scalar_name))
            dsets.append(Int32HDF5Dataset(scalar_name, shape=(0, 9), maxshape=(None, 9), rank=2, cache=True, block_size=2000))
            self._logger.error("Adding dataset: {}".format(dtc_name))
            #dsets.append(Float64HDF5Dataset(dtc_name))
            dsets.append(Float64HDF5Dataset(dtc_name, shape=(0,), maxshape=(None,), rank=1, cache=True, block_size=2000))
            self._logger.error("Adding dataset: {}".format(inp_est_name))
            #dsets.append(Float64HDF5Dataset(inp_est_name))
            dsets.append(Float64HDF5Dataset(inp_est_name, shape=(0,), maxshape=(None,), rank=1, cache=True, block_size=2000))

        dsets.append(Int64HDF5Dataset(DATASET_DAQ_VERSION))
        dsets.append(Int64HDF5Dataset(DATASET_META_VERSION))
        return dsets

    @property
    def detector_message_handlers(self):
        return {
            XSPRESS_SCALARS: self.handle_xspress_scalars,
            XSPRESS_DTC: self.handle_xspress_dtc,
            XSPRESS_INP_EST: self.handle_xspress_inp_est,
        }

    def handle_xspress_scalars(self, header, _data):
        """Handle global header message part 1"""
        #self._logger.error("%s | Handling xspress scalar message", self._name)
        #self._logger.error("{}".format(header))
        # Extract the channel number from the header
        channel = header['channel_index']
        #self._logger.error("Channel index: {}".format(channel))

        format_str = '{}i'.format(header['qty_scalars']*header['number_of_frames'])
        array = struct.unpack(format_str, _data)

        # Number of channels 
        number_of_channels = header['number_of_channels']
        # Number of frames
        number_of_frames = header['number_of_frames']
        #self._logger.debug("Rank: {}  Index {}  Array_Size {}".format(rank, index, array_size))
        #self._logger.error(array)
        for frame in range(number_of_frames):
            frame_index = frame*number_of_channels*XSPRESS_SCALARS_PER_CHANNEL
            frame_array = array[frame_index:frame_index+(number_of_channels*XSPRESS_SCALARS_PER_CHANNEL)]
            for index in range(number_of_channels):
                arr_index = index*XSPRESS_SCALARS_PER_CHANNEL
                scalars = frame_array[arr_index:arr_index+XSPRESS_SCALARS_PER_CHANNEL]
                dataset_name = "{}{}".format(DATASET_SCALAR, channel+index)
                #self._logger.error("Length before: {}".format(self._datasets[dataset_name]._h5py_dataset.len()))
                self._add_value(dataset_name, scalars, offset=None)
                #self._logger.error("Length after: {}".format(self._datasets[dataset_name]._h5py_dataset.len()))
            
#        if (datetime.now() - self._flush_time).total_seconds() > 1.0:
#            self._flush_datasets()
#            self._flush_time = datetime.now()

    def handle_xspress_dtc(self, header, _data):
        """Handle global header message part 1"""
        #self._logger.error("%s | Handling xspress dtc message", self._name)
        #self._logger.error("{}".format(header))
        # Extract the channel number from the header
        channel = header['channel_index']
        # Extract Number of channels 
        number_of_channels = header['number_of_channels']
        # Number of frames
        number_of_frames = header['number_of_frames']

        format_str = '{}d'.format(number_of_channels*number_of_frames)
        array = struct.unpack(format_str, _data)

        for frame in range(number_of_frames):
            frame_index = frame*number_of_channels
            frame_array = array[frame_index:frame_index+number_of_channels]
            for index in range(number_of_channels):
                dataset_name = "{}{}".format(DATASET_DTC, channel+index)
                self._add_value(dataset_name, frame_array[index], offset=None)

        #if (datetime.now() - self._flush_time).total_seconds() > 0.1:
        #    self._flush_datasets()
        #    self._flush_time = datetime.now()

            
    def handle_xspress_inp_est(self, header, _data):
        """Handle global header message part 1"""
        #self._logger.error("%s | Handling xspress dtc message", self._name)
        #self._logger.error("{}".format(header))
        # Extract the channel number from the header
        channel = header['channel_index']
        # Extract Number of channels 
        number_of_channels = header['number_of_channels']
        # Number of frames
        number_of_frames = header['number_of_frames']

        format_str = '{}d'.format(number_of_channels*number_of_frames)
        array = struct.unpack(format_str, _data)

        for frame in range(number_of_frames):
            frame_index = frame*number_of_channels
            frame_array = array[frame_index:frame_index+number_of_channels]
            for index in range(number_of_channels):
                dataset_name = "{}{}".format(DATASET_INP_EST, channel+index)
                self._add_value(dataset_name, frame_array[index], offset=None)

    @staticmethod
    def get_version():
        return (
            "xspress-detector",
            construct_version_dict(versioneer.get_versions()["version"]),
        )
