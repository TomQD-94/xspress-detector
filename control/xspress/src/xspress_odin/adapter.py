"""
adapter.py - EXCALIBUR API adapter for the ODIN server.

Tim Nicholls, STFC Application Engineering Group
"""

import traceback
import logging
import re
from tornado.escape import json_decode
from odin.adapters.adapter import ApiAdapter, ApiAdapterResponse, request_types, response_types
from xspress_odin.detector import XspressDetector, XspressDetectorException

from .debug import debug_method


def require_valid_detector(func):
    """Decorator method for request handler methods to check that adapter has valid detector."""
    def wrapper(_self, path, request):
        if _self.detector is None:
            return ApiAdapterResponse(
                'Invalid detector configuration', status_code=500
            )
        return func(_self, path, request)
    return wrapper


class XspressAdapter(ApiAdapter):
    """XspressAdapter class.

    This class provides the adapter interface between the ODIN server and the Xspress detector
    system, transforming the REST-like API HTTP verbs into the appropriate Xspress application
    control actions
    """

    def __init__(self, **kwargs):
        """Initialise the ExcaliburAdapter object.

        :param kwargs: keyword arguments passed to ApiAdapter as options.
        """
        # Initialise the ApiAdapter base class to store adapter options
        super(XspressAdapter, self).__init__(**kwargs)

        # Compile the regular expression used to resolve paths into actions and resources
        self.path_regexp = re.compile('(.*?)/(.*)')

        self.detector = None
        try:
            endpoint = self.options['endpoint']
            ip, port = endpoint.split(":")
            self.detector = XspressDetector(ip, port)
            logging.info(f"instaciated XspressDetector with ip = {ip} and port {port}")

            base_ip = self.options['base_ip']
            num_cards = int(self.options['num_cards'])
            max_channels = int(self.options['max_channels'])
            settings_path = self.options['settings_path']
            # run_flags = self.options['run_flags']
            num_tf = int(self.options["num_tf"])
            debug = int(self.options["debug"])
            # trigger_mode = XspressTriggerMode.TM_SOFTWARE
            # self.detector.configure(base_ip, num_cards, max_channels, config_path, run_flags, trigger_mode, frames, exposure_time)
            self.detector.configure(num_cards, num_tf, base_ip, max_channels, settings_path, debug)
            logging.debug('done configuring detector')
        except XspressDetectorException as e:
            logging.error('XspressAdapter failed to initialise detector: %s', e)
        except Exception as e:
            logging.error('Unhandled Exception:\n %s', traceback.format_exc())
        logging.info('exiting XspressAdapter.__init__')

    @request_types('application/json')
    @response_types('application/json', default='application/json')
    @require_valid_detector
    def get(self, path, request):
        """Handle an HTTP GET request.

        This method is the implementation of the HTTP GET handler for ExcaliburAdapter.

        :param path: URI path of the GET request
        :param request: Tornado HTTP request object
        :return: ApiAdapterResponse object to be returned to the client
        """
        logging.debug(f"XspressAdapter.get called with path: {path}")
        try:
            response = self.detector.get(path)
            # transform singel value jsons form {"param_name": value} to {"value": value} to work with the ADOdin client
            first_value = next(iter(response.values()))
            if len(response) == 1 and not isinstance(first_value, dict):
                response = {"value": first_value}

            status_code = 200
        except LookupError as e:
            response = {'invalid path': str(e)}
            logging.error(f"XspressAdapter.get: Invalid path: {path}")
            status_code = 400

        return ApiAdapterResponse(response, status_code=status_code)

    @request_types('application/json')
    @response_types('application/json', default='application/json')
    @require_valid_detector
    def put(self, path, request):
        """Handle an HTTP PUT request.

        This method is the implementation of the HTTP PUT handler for ExcaliburAdapter/

        :param path: URI path of the PUT request
        :param request: Tornado HTTP request object
        :return: ApiAdapterResponse object to be returned to the client
        """
        logging.debug(debug_method())
        try:
            data = json_decode(request.body)
            response = self.detector.put(path, data)
            status_code = 200
        except ConnectionError as e:
            response = {'error': str(e)}
            status_code = 500
            logging.error(e)
        except (TypeError, ValueError) as e:
            response = {'error': 'Failed to decode PUT request body {}:\n {}'.format(data, e)}
            logging.error(traceback.format_exc())
            status_code = 400
        except Exception as e:
            logging.error(traceback.format_exc())
            response = {'error' : 'Unhandled exception :{}'.format(e)}
            status_code = 400

        return ApiAdapterResponse(response, status_code=status_code)

    @request_types('application/json')
    @response_types('application/json', default='application/json')
    @require_valid_detector
    def delete(self, path, request):
        """Handle an HTTP DELETE request.

        This method is the implementation of the HTTP DELETE verb for ExcaliburAdapter.

        :param path: URI path of the DELETE request
        :param request: Tornado HTTP request object
        :return: ApiAdapterResponse object to be returned to the client
        """
        response = {'response': '{}: DELETE on path {}'.format(self.name, path)}
        status_code = 200

        logging.debug(response)

        return ApiAdapterResponse(response, status_code=status_code)

    def cleanup(self):
        if self.detector:
            self.detector.shutdown()

