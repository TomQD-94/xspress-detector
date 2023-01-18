"""
Created on March 2022

:author: Alan Greer
"""
import json
import logging
import os
import asyncio
import time

from odin_data.control.odin_data_adapter import OdinDataAdapter
from odin_data.control.fp_compression_adapter import FPCompressionAdapter
from odin.adapters.adapter import (
    ApiAdapterResponse,
    request_types,
    response_types,
)
from tornado import escape
from tornado.escape import json_encode, json_decode


def bool_from_string(value):
    bool_value = False
    if value.lower() == 'true' or value.lower() == '1':
        bool_value = True
    return bool_value


class FPXspressAdapter(FPCompressionAdapter):
    """
    FPXspressAdapter class

    This class provides control of FP applications that are running as part of an
    Xspress detector

    The rank must be 0 for every FP and the number of processes set to 1.
    The number of frames and acquisition ID must also be sent to the process
    plugin of the FP.

    This class will override the setup_rank method to apply the corrections
    for an Xspress detector.
    """
    def __init__(self, **kwargs):
        """
        Initialise the OdinDataAdapter object

        :param kwargs:
        """
        logging.debug("FPXspressAdapter init called")
        self._xsp_adapter = None
        self._xsp_adapter_name = "xsp"
        super(FPXspressAdapter, self).__init__(**kwargs)


    def initialize(self, adapters):
        """Initialize the adapter after it has been loaded.
        Find and record the FR adapter for later error checks
        """
        logging.info("Intercepting the Xspress control adapter")
        if self._xsp_adapter_name in adapters:
            self._xsp_adapter = adapters[self._xsp_adapter_name]
            logging.info("FP adapter initiated connection to Xspress adapter: {}".format(self._xsp_adapter_name))
        else:
            logging.error("FP adapter could not connect to the Xspress adapter: {}".format(self._xsp_adapter_name))
        super(FPXspressAdapter, self).initialize(adapters)

    @request_types('application/json', 'application/vnd.odin-native')
    @response_types('application/json', default='application/json')
    def put(self, path, request):  # pylint: disable=W0613
        if path == self._command:
            # Check the mode we are running in (mca or list)
            mode = self._xsp_adapter.detector.mode

            if mode == 'list':
                num_clients = self._xsp_adapter.detector.num_process_list
                write = bool_from_string(str(escape.url_unescape(request.body)))
                if write:
                    config = {'hdf': {'write': write}}
                    # Before attempting to write files, make some simple error checks

                    # Check if we have a valid buffer status from the FR adapter
                    valid, reason = self.check_fr_status()
                    if not valid:
                        raise RuntimeError(reason)

                    # Check the file path is valid
                    if not os.path.isdir(str(self._param['config/hdf/file/path'])):
                        raise RuntimeError("Invalid path specified [{}]".format(str(self._param['config/hdf/file/path'])))
                    # Check the filename exists
                    if str(self._param['config/hdf/file/name']) == '':
                        raise RuntimeError("File name must not be empty")

                    # First setup the rank for the frameProcessor applications
                    self.setup_rank()
                    rank = 0
                    for client in self._clients[0:num_clients]:
                        # Send the configuration required to setup the acquisition
                        # The file path is the same for all clients
                        parameters = {
                            'hdf': {
                                'frames': self._param['config/hdf/frames']
                            }
                        }
                        # Send the number of frames first
                        client.send_configuration(parameters)
                        parameters = {
                            'hdf': {
                                'acquisition_id': self._param['config/hdf/acquisition_id'],
                                'file': {
                                    'path': str(self._param['config/hdf/file/path']),
                                    'name': str(self._param['config/hdf/file/name']),
                                    'extension': str(self._param['config/hdf/file/extension'])
                                }
                            }
                        }
                        client.send_configuration(parameters)
                        rank += 1
                    for client in self._clients[0:num_clients]:
                        # Send the configuration required to start the acquisition
                        client.send_configuration(config)
                else:
                    # Flush and close FPs in list mode
                    for client in self._clients[0:num_clients]:
                        try:
                            parameters = {
                                'xspress-list': {
                                    'flush': True
                                }
                            }
                            client.send_configuration(parameters)
                        except Exception as err:
                            logging.debug(OdinDataAdapter.ERROR_FAILED_TO_SEND)
                            logging.error("Error: %s", err)

                status_code = 200
                response = {}
                logging.info("Xspress FP adapter flushed in list mode")
                return ApiAdapterResponse(response, status_code=status_code)

        return super(FPXspressAdapter, self).put(path, request)


    def setup_rank(self):
        # Attempt initialisation of the connected clients
        for client in self._clients:
            try:
                # Setup the number of processes and the rank for each client
                parameters = {
                    'hdf': {
                        'process': {
                            'number': 1,
                            'rank': 0
                        }
                    },
                    'xspress': {
                        'frames': self._param['config/hdf/frames'],
                        'acq_id': self._param['config/hdf/acquisition_id']
                    },
                    'xspress-list': {
                        'reset': True
                    }
                }
                logging.debug("Sending: {}".format(parameters))

                client.send_configuration(parameters)
            except Exception as err:
                logging.debug(OdinDataAdapter.ERROR_FAILED_TO_SEND)
                logging.error("Error: %s", err)
