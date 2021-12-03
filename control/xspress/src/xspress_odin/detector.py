import os, time
import argparse
import json
import curses
import os
import logging
import zmq
import random
import tornado
import getpass


from time import sleep
from enum import Enum
from functools import partial
from datetime import datetime

from abc import ABC, abstractmethod
from odin_data.ipc_client import IpcClient
from odin_data.ipc_message import IpcMessage
from zmq.eventloop.zmqstream import ZMQStream
from zmq.utils.monitor import parse_monitor_message
from zmq.utils.strtypes import unicode, cast_bytes
from odin_data.ipc_channel import _cast_str
from odin.adapters.parameter_tree import ParameterTree, ParameterTreeError


ENDPOINT_TEMPLATE = "tcp://{}:{}"
QUEUE_SIZE_LIMIT = 10
UPDATE_PARAMS_PERIOD_MILLI_SECONDS = 5000//1

EVENT_MAP = {}
for name in dir(zmq):
    if name.startswith('EVENT_'):
        value = getattr(zmq, name)
        print("%21s : %4i" % (name, value))
        EVENT_MAP[value] = name

class MessageType(Enum):
    CMD = 1
    CONFIG_XSP = 2
    REQUEST = 3
    CONFIG_ROOT = 4

class XspressTriggerMode:
    TM_SOFTWARE            = 0
    TM_TTL_RISING_EDGE     = 1
    TM_BURST               = 2
    TM_TTL_VETO_ONLY       = 3
    TM_SOFTWARE_START_STOP = 4
    TM_IDC                 = 5
    TM_TTL_BOTH            = 6
    TM_LVDS_VETO_ONLY      = 7
    TM_LVDS_BOTH           = 8

class XspressDetectorStr:
    CONFIG_ADAPTER_START_TIME    = "start_time"
    CONFIG_ADAPTER_UP_TIME       = "up_time"
    CONFIG_ADAPTER_CONNECTED      = "connected"
    CONFIG_ADAPTER_USERNAME      = "username"

    CONFIG_SHUTDOWN              = "shutdown"
    CONFIG_DEBUG                 = "debug_level"
    CONFIG_CTRL_ENDPOINT         = "ctrl_endpoint"
    CONFIG_REQUEST               = "request_configuration"

    CONFIG_XSP                   = "xsp"
    CONFIG_XSP_NUM_CARDS         = "num_cards"
    CONFIG_XSP_NUM_TF            = "num_tf"
    CONFIG_XSP_BASE_IP           = "base_ip"
    CONFIG_XSP_MAX_CHANNELS      = "max_channels"
    CONFIG_XSP_MAX_SPECTRA       = "max_spectra"
    CONFIG_XSP_DEBUG             = "debug"
    CONFIG_XSP_CONFIG_PATH       = "config_path"
    CONFIG_XSP_CONFIG_SAVE_PATH  = "config_save_path"
    CONFIG_XSP_USE_RESGRADES     = "use_resgrades"
    CONFIG_XSP_RUN_FLAGS         = "run_flags"
    CONFIG_XSP_DTC_ENERGY        = "dtc_energy"
    CONFIG_XSP_TRIGGER_MODE      = "trigger_mode"
    CONFIG_XSP_INVERT_F0         = "invert_f0"
    CONFIG_XSP_INVERT_VETO       = "invert_veto"
    CONFIG_XSP_DEBOUNCE          = "debounce"
    CONFIG_XSP_EXPOSURE_TIME     = "exposure_time"
    CONFIG_XSP_FRAMES            = "frames"

    CONFIG_DAQ                   = "daq"
    CONFIG_DAQ_ENABLED           = "enabled"
    CONFIG_DAQ_ZMQ_ENDPOINTS     = "endpoints"

    CONFIG_CMD                   = "cmd"
    CONFIG_CMD_CONNECT           = "connect"
    CONFIG_CMD_SAVE              = "save"
    CONFIG_CMD_RESTORE           = "restore"
    CONFIG_CMD_START             = "start"
    CONFIG_CMD_STOP              = "stop"
    CONFIG_CMD_TRIGGER           = "trigger"


class XspressDetectorException(Exception):
    pass

class XspressClient:
    MESSAGE_ID_MAX = 2**32

    def __init__(self, ip, port, callback=None):
        self.ip = ip
        self.port = port
        self.endpoint = ENDPOINT_TEMPLATE.format(ip, port)
        self.connected = False

        self.context = zmq.Context.instance()
        self.socket = self.context.socket(zmq.DEALER)
        self.socket.setsockopt(zmq.IDENTITY, self._generate_identity())  # pylint: disable=no-member
        self.socket.connect(self.endpoint)
        self.monitor_socket = self.socket.get_monitor_socket()

        io_loop = tornado.ioloop.IOLoop.current()
        self.stream = ZMQStream(self.socket, io_loop=io_loop)
        self.callback = callback
        self.register_on_recv_callback(self.callback)
        self.stream.on_send(self._on_send_callback)

        self.monitor_stream = ZMQStream(self.monitor_socket, io_loop=io_loop)
        self.monitor_stream.on_recv(self._monitor_on_recv_callback)

        self.message_id = 0
        self.queue_size = 0

    def _monitor_on_recv_callback(self, msg):
        msg = parse_monitor_message(msg)
        msg.update({'description': EVENT_MAP[msg['event']]})
        event_type = msg['event']
        if event_type & (zmq.EVENT_CONNECTED | zmq.EVENT_HANDSHAKE_SUCCEEDED):
            self.connected = True
            logging.info("Control Server is Connected")
        elif event_type & zmq.EVENT_DISCONNECTED:
            self.connected = False
            logging.info("Control Server is Disconnected")

        logging.info("Monitor msg: {}".format(msg))

    def _generate_identity(self):
        identity = "{:04x}-{:04x}".format(
            random.randrange(0x10000), random.randrange(0x10000)
        )
        return cast_bytes(identity)

    def test_callback(self, msg):
        data = _cast_str(msg[0])
        logging.debug(f"data type = {type(data)}")
        logging.debug(f"data = {data}")
        ipc_msg = IpcMessage(from_str=data)
        logging.debug(f"queue size = {self.queue_size}")
        logging.debug(f"ZmqMessage: {msg}")
        logging.debug(f"IpcMessage __dict__: {ipc_msg.__dict__}")
        logging.debug(f"IpcMessage msg.params: {ipc_msg.get_params()}")

    def _on_send_callback(self, msg, status):
        self.queue_size += 1

    def get_queue_size(self):
        return self.queue_size

    def register_on_recv_callback(self, callback):
        def wrap(msg):
            self.queue_size -= 1
            callback(msg)
        self.stream.on_recv(wrap)

    def register_on_send_callback(self, callback):
        def wrap(msg, status):
            self.queue_size += 1
            callback(msg, status)
        self.stream.on_send(wrap)

    def send_command(self, msg):
        pass

    def send_requst(self, msg: IpcMessage):
        n_flushed = self.stream.flush() # This is probably not needed
        logging.debug("number of events flushed: {}".format(n_flushed))
        msg.set_msg_id(self.message_id)
        self.message_id = (self.message_id + 1) % self.MESSAGE_ID_MAX
        logging.info("Sending message:\n%s", msg.encode())
        self.stream.send(cast_bytes(msg.encode()))
        logging.error(f"queue size = {self.queue_size}")

    def send_config(self, msg):
        pass
    def is_connected(self):
        return self.connected

class XspressDetector(object):

    def __init__(self, ip: str, port: int, dubug=logging.ERROR):
        logging.getLogger().setLevel(logging.DEBUG)

        self.endpoint = ENDPOINT_TEMPLATE.format("0.0.0.0", 12000)
        self.start_time = datetime.now()
        self.username = getpass.getuser()

        self._sync_client = IpcClient(ip, port) # calls zmq_sock.connect() so might throw?
        self._async_client = XspressClient(ip, port, callback=self.on_recv_callback)
        self._async_client.register_on_send_callback(self.on_send_callback)
        self.timeout = 1
        self.ctrl_enpoint_connected: bool = False
        self.xsp_connected: bool = False
        self.sched = tornado.ioloop.PeriodicCallback(self.read_config, UPDATE_PARAMS_PERIOD_MILLI_SECONDS)

        # root level parameter tree members
        self.ctr_endpoint = ENDPOINT_TEMPLATE.format(ip, port)
        self.ctr_debug = 0

        # xsp parameter tree members
        self.num_cards : int = 0
        self.num_tf : int = 0
        self.base_ip : str = None
        self.max_channels : str = 0
        self.max_spectra : int = 0
        self.debug : int = 0
        self.settings_path : str = None
        self.settings_save_path : str = None
        self.use_resgrade : bool = None
        self.run_flags : int = 0
        self.dtc_energy : float = 0.0
        self.trigger_mode : XspressTriggerMode = None
        self.num_frames : int = 0
        self.invert_f0 : bool = None
        self.invert_veto : bool = None
        self.debounce : bool = None # is this a bool I was guessing when writing this?
        self.exposure_time : float = 0.0
        self.frames : int = 0

        # daq parameter tree members
        self.daq_enabled : bool = 0
        self.daq_endpoints : list[str] = []

        # cmd parameter tree members. No state to store.
        # connect, save, restore, start, stop, trigger

        tree = \
        {
            XspressDetectorStr.CONFIG_ADAPTER_START_TIME: (lambda: self.start_time.strftime("%B %d, %Y %H:%M:%S"), {}),
            XspressDetectorStr.CONFIG_ADAPTER_UP_TIME: (lambda: str(datetime.now() - self.start_time), {}),
            XspressDetectorStr.CONFIG_ADAPTER_CONNECTED: (lambda: self._async_client.is_connected(), {}),
            XspressDetectorStr.CONFIG_ADAPTER_USERNAME: (lambda: self.username, {}),

            XspressDetectorStr.CONFIG_DEBUG : (lambda: self.ctr_debug, partial(self._set, 'ctr_debug'), {}),
            XspressDetectorStr.CONFIG_CTRL_ENDPOINT: (lambda: self.ctr_endpoint, partial(self._set, 'ctr_endpoint'), {}),
            XspressDetectorStr.CONFIG_XSP :
            {
                XspressDetectorStr.CONFIG_XSP_NUM_CARDS : (lambda: self.num_cards, partial(self._set, 'num_cards'), {}),
                XspressDetectorStr.CONFIG_XSP_NUM_TF : (lambda: self.num_tf, partial(self._set, 'num_tf'), {}),
                XspressDetectorStr.CONFIG_XSP_BASE_IP : (lambda: self.base_ip, partial(self._set, 'base_ip'), {}),
                XspressDetectorStr.CONFIG_XSP_MAX_CHANNELS : (lambda: self.max_channels, partial(self._set, 'max_channels'), {}),
                XspressDetectorStr.CONFIG_XSP_MAX_SPECTRA : (lambda: self.max_spectra, partial(self._set, 'max_spectra'), {}),
                XspressDetectorStr.CONFIG_XSP_DEBUG : (lambda: self.debug, partial(self._set, 'debug'), {}),
                XspressDetectorStr.CONFIG_XSP_CONFIG_PATH : (lambda: self.settings_path, partial(self._set, "settings_path"), {}),
                XspressDetectorStr.CONFIG_XSP_CONFIG_SAVE_PATH : (lambda: self.settings_save_path, partial(self._set, "settings_save_path"), {}),
                XspressDetectorStr.CONFIG_XSP_USE_RESGRADES : (lambda: self.use_resgrade, partial(self._set, "use_resgrade"), {}),
                XspressDetectorStr.CONFIG_XSP_RUN_FLAGS : (lambda: self.run_flags, partial(self._set, "run_flags"), {}),
                XspressDetectorStr.CONFIG_XSP_DTC_ENERGY : (lambda: self.dtc_energy, partial(self._set, "dtc_energy"), {}),
                XspressDetectorStr.CONFIG_XSP_TRIGGER_MODE : (lambda: self.trigger_mode, partial(self._set, "trigger_mode"), {}),
                XspressDetectorStr.CONFIG_XSP_INVERT_F0 : (lambda: self.invert_f0, partial(self._set, "invert_f0"), {}),
                XspressDetectorStr.CONFIG_XSP_INVERT_VETO : (lambda: self.invert_veto, partial(self._set, "invert_veto"), {}),
                XspressDetectorStr.CONFIG_XSP_DEBOUNCE : (lambda: self.debounce, partial(self._set, "debounce"), {}),
                XspressDetectorStr.CONFIG_XSP_EXPOSURE_TIME : (lambda: self.exposure_time, partial(self._set, "exposure_time"), {}),
                XspressDetectorStr.CONFIG_XSP_FRAMES : (lambda: self.frames, partial(self._set, "frames"), {}),
            }
        }
        self.parameter_tree = ParameterTree(tree)
        put_tree = \
        {
            XspressDetectorStr.CONFIG_DEBUG : (None, partial(self._put, MessageType.CONFIG_ROOT, XspressDetectorStr.CONFIG_DEBUG), {}),
            XspressDetectorStr.CONFIG_CTRL_ENDPOINT: (None, partial(self._put, MessageType.CONFIG_ROOT, XspressDetectorStr.CONFIG_CTRL_ENDPOINT), {}),
            XspressDetectorStr.CONFIG_XSP :
            {
                XspressDetectorStr.CONFIG_XSP_BASE_IP : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_BASE_IP), {}),
                XspressDetectorStr.CONFIG_XSP_CONFIG_PATH : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_CONFIG_PATH), {}),
            }
        }
        self.put_parameter_tree = ParameterTree(put_tree)


    def _set(self, attr_name, value):
        setattr(self, attr_name, value)

    def _put(self, message_type: MessageType, config_str: str,  value: any):
        if message_type == MessageType.CONFIG_ROOT:
            msg = self._build_message_single(config_str, value)
        else:
            msg = self._build_message(message_type, value)
        self._async_client.send_requst(msg)

    def put(self, path: str, data):
        try:
            self.put_parameter_tree.set(path, data)
            return {'async_message sent': None}
        except ParameterTreeError as e:
            logging.error(f"parameter_tree error: {e}")
            raise XspressDetectorException(e)


    def get(self, path):
        try:
            return self.parameter_tree.get(path)
        except ParameterTreeError as e:
            logging.error(f"parameter_tree error: {e}")
            raise LookupError(e)

    def _stop_periodic_callbacks_till_queue_is_cleared(self):
        if self._async_client.get_queue_size() > QUEUE_SIZE_LIMIT:
            if self.sched.is_running():
                logging.error("Queue size exceeded maximmum limit of {}.\n\tCurrent queue size: {}".format(QUEUE_SIZE_LIMIT, self._async_client.get_queue_size()))
                self.sched.stop()
        else:
            if not self.sched.is_running():
                self.sched.start()

    def on_send_callback(self, msg, status):
        self._stop_periodic_callbacks_till_queue_is_cleared()

    def on_recv_callback(self, msg):
        self._stop_periodic_callbacks_till_queue_is_cleared()
        BASE_PATH = ""
        data = _cast_str(msg[0])
        logging.debug(f"queue size = {self._async_client.get_queue_size()}")
        logging.debug(f"message recieved = {data}")
        ipc_msg = IpcMessage(from_str=data)

        if not ipc_msg.is_valid():
            raise XspressDetectorException("IpcMessage recieved is not valid")
        if ipc_msg.get_msg_val() == XspressDetectorStr.CONFIG_REQUEST:
            if ipc_msg.get_msg_type() == IpcMessage.ACK and ipc_msg.get_params():
                self._param_tree_set_recursive(BASE_PATH, ipc_msg.get_params())
            else:
                pass

    def _param_tree_set_recursive(self, path, params):
        if not isinstance(params, dict):
            try:
                path = path.strip("/")
                self.parameter_tree.set(path, params)
                logging.debug(f"XspressDetector._param_tree_set_recursive: parameter path {path} was set to {params}")
            except ParameterTreeError:
                logging.warning(f"XspressDetector._param_tree_set_recursive: parameter path {path} is not in parameter tree")
                pass
        else:
            for param_name, params in params.items():
                self._param_tree_set_recursive(f"{path}/{param_name}", params)

    def configure(self, num_cards, num_tf, base_ip, max_channels, settings_path, debug):
        self.num_cards = num_cards
        self.num_tf = num_tf
        self.base_ip = base_ip
        self.max_channels = max_channels
        self.debug = debug
        self.settings_path = settings_path

        config = {
            "base_ip" : self.base_ip,
            "num_cards" : self.num_cards,
            "num_tf" : self.num_tf,
            "max_channels" : self.max_channels,
            "config_path" : self.settings_path,
            "debug" : self.debug,
            "run_flags": 2,
        }
        config_msg = self._build_message(MessageType.CONFIG_XSP, config)
        self._async_client.send_requst(config_msg)

        # self._connect() # uncomment this when ready to test with real XSP
        self.sched.start()



    def _connect(self):
        command = { "connect": None }
        command_message = self._build_message(MessageType.CMD, command)

        self._async_client.send_requst(command_message)
        # success, _ = self._client.send_configuration(command_message, target=XspressDetectorStr.CONFIG_CMD)
        # if not success:
        #     self.connected = False
        #     logging.error("could not connect to Xspress")
        #     raise XspressDetectorException("could not connect to the xspress unit")
        self.connected = True
        self._restore()


    def _restore(self):
        command = { "restore": None }
        command_message = self._build_message(MessageType.CMD, command)
        # success, reply =  self._client.send_configuration(command_message, target=XspressDetectorStr.CONFIG_CMD)
        self._async_client.send_requst(command_message)

    def _build_message_single(self, param_str: str, value: any):
        msg = IpcMessage("cmd", "configure")
        msg.set_param(param_str, value)
        return msg

    def _build_message(self, message_type: MessageType, config: dict = None):
        if message_type == MessageType.REQUEST:
            msg = IpcMessage("cmd", "request_configuration")
            return msg
        elif message_type == MessageType.CONFIG_XSP:
            params_group = "xsp"
        elif message_type == MessageType.CMD:
            params_group = "cmd"
        else:
            raise XspressDetectorException(f"XspressDetector._build_config_message: {message_type} is not type MessageType")
        msg = IpcMessage("cmd", "configure")
        msg.set_param(params_group, config)
        return msg

    def read_config(self):
        self._async_client.send_requst(self._build_message(MessageType.REQUEST))

