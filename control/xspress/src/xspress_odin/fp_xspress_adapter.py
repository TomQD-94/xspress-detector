"""
Created on March 2022

:author: Alan Greer
"""
import json
import logging
import os
from odin_data.ipc_tornado_client import IpcTornadoClient
from odin_data.util import remove_prefix, remove_suffix
from odin_data.odin_data_adapter import OdinDataAdapter
from odin_data.frame_processor_adapter import FrameProcessorAdapter
from odin_data.fp_compression_adapter import FPCompressionAdapter
from odin.adapters.adapter import ApiAdapter, ApiAdapterRequest, ApiAdapterResponse, request_types, response_types
from odin.adapters.parameter_tree import ParameterTree, ParameterTreeError
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
        super(FPXspressAdapter, self).__init__(**kwargs)

    @request_types('application/json', 'application/vnd.odin-native')
    @response_types('application/json', default='application/json')
    def put(self, path, request):  # pylint: disable=W0613
        logging.error("IN HERE!")
        if path == self._command:
            write = bool_from_string(str(escape.url_unescape(request.body)))
            if not write:
                # Attempt initialisation of the connected clients
                for client in self._clients:
                    try:
                        parameters = {
                            'xspress-list': {
                                'flush': True
                            }
                        }
                        logging.error("Sending: {}".format(parameters))
                        client.send_configuration(parameters)
                    except Exception as err:
                        logging.debug(OdinDataAdapter.ERROR_FAILED_TO_SEND)
                        logging.error("Error: %s", err)
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
                logging.error("Sending: {}".format(parameters))

                client.send_configuration(parameters)
            except Exception as err:
                logging.debug(OdinDataAdapter.ERROR_FAILED_TO_SEND)
                logging.error("Error: %s", err)
