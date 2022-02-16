import logging
import getpass

from enum import Enum
from functools import partial
from datetime import datetime

from odin_data.ipc_message import IpcMessage
from odin_data.ipc_channel import _cast_str
from odin.adapters.parameter_tree import ParameterTree, ParameterTreeError
from tornado.ioloop import PeriodicCallback

from .client import XspressClient
from .debug import debug_method

ENDPOINT_TEMPLATE = "tcp://{}:{}"
UPDATE_PARAMS_PERIOD_SECONDS = 5
QUEUE_SIZE_LIMIT = 10

class AuotoPeriodicCallback(object):
    """
    Automatically start and stop the periodic callbacks based on the value of another callback (predicate).
    In order to start/stop the main callback in time, the predicate will be checked at time/2.
    """
    def __init__(self, callback, predicate, time_seconds, time_min=0.01):
        self.time_min = time_min
        self.check_time_valid(time_seconds)
        self.callback = callback
        self.predicate = predicate
        callback_time = time_seconds*1000
        predicate_time = callback_time/2.0
        self.callback_sched = PeriodicCallback(self.callback, callback_time)
        self.predicate_sched = PeriodicCallback(self._auto_callback, predicate_time)

    def check_time_valid(self, time):
        if time < self.time_min:
            raise ValueError("Time provided is smaller than limit")

    def _auto_callback(self):
        if self.callback_sched.is_running() and not self.predicate():
            self.callback_sched.stop()
        elif not self.callback_sched.is_running() and self.predicate():
            self.callback_sched.start()
        
    def start(self):
        self.predicate_sched.start()

    def stop(self):
        self.predicate_sched.stop()
        self.callback_sched.stop()

    def is_running(self):
        return self.predicate_sched.is_running()

    def set_time(self, time_seconds: float):
        self.check_time_valid(time_seconds)
        time = time_seconds*1000
        self.callback_sched.callback_time = time
        self.predicate_sched.callback_time = time/2.0

    def get_time(self):
        return self.callback_sched.callback_time / 1000.0


class MessageType(Enum):
    CMD = 1
    CONFIG_XSP = 2
    REQUEST = 3
    CONFIG_ROOT = 4
    CONFIG_APP = 5

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
    CONFIG_STATUS                = "status"
    CONFIG_STATUS_SENSOR         = "sensor"
    CONFIG_STATUS_SENSOR_HEIGHT  = "height"
    CONFIG_STATUS_SENSOR_WIDTH   = "width"
    CONFIG_STATUS_SENSOR_BYTES   = "bytes"
    CONFIG_STATUS_MANUFACTURER   = "manufacturer"
    CONFIG_STATUS_MODEL          = "model"


    CONFIG_ADAPTER               = "adapter"
    CONFIG_ADAPTER_RESET         = "reset"
    CONFIG_ADAPTER_SCAN          = "scan"
    CONFIG_ADAPTER_UPDATE        = "update"
    CONFIG_ADAPTER_START_TIME    = "start_time"
    CONFIG_ADAPTER_UP_TIME       = "up_time"
    CONFIG_ADAPTER_CONNECTED     = "connected"
    CONFIG_ADAPTER_USERNAME      = "username"

    CONFIG_APP                   = "app"
    CONFIG_APP_SHUTDOWN          = "shutdow"
    CONFIG_APP_DEBUG             = "debug_level"
    CONFIG_APP_CTRL_ENDPOINT     = "ctrl_endpoint"
    CONFIG_REQUEST               = "request_configuration"

    CONFIG_XSP                   = "config"
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
    CONFIG_XSP_NUM_IMAGES        = "num_images" # so only "num_images" is used

    CONFIG_DAQ                   = "daq"
    CONFIG_DAQ_ENABLED           = "enabled"
    CONFIG_DAQ_ZMQ_ENDPOINTS     = "endpoints"

    CONFIG_CMD                   = "cmd"
    CONFIG_CMD_CONNECT           = "connect"
    CONFIG_CMD_DISCONNECT        = "disconnect"
    CONFIG_CMD_SAVE              = "save"
    CONFIG_CMD_RESTORE           = "restore"
    CONFIG_CMD_START             = "start"
    CONFIG_CMD_STOP              = "stop"
    CONFIG_CMD_TRIGGER           = "trigger"
    CONFIG_CMD_ACQUIRE           = "acquire"


class XspressDetectorException(Exception):
    pass


class XspressDetector(object):

    def __init__(self, ip: str, port: int, dubug=logging.ERROR):
        logging.getLogger().setLevel(logging.DEBUG)

        self.endpoint = ENDPOINT_TEMPLATE.format("0.0.0.0", 12000)
        self.start_time = datetime.now()
        self.username = getpass.getuser()

        self._async_client = XspressClient(ip, port, callback=self.on_recv_callback)
        self.timeout = 1
        self.xsp_connected: bool = False
        self.sched = AuotoPeriodicCallback(self.read_config, self._async_client.is_connected, UPDATE_PARAMS_PERIOD_SECONDS)

        # root level parameter tree members
        self.ctr_endpoint = ENDPOINT_TEMPLATE.format(ip, port)
        self.ctr_debug = 0

        # xsp parameter tree members
        self.num_cards : int = 0
        self.num_tf : int = 0
        self.base_ip : str = ""
        self.max_channels : int = 0
        self.max_spectra : int = 0
        self.debug : int = 0
        self.settings_path : str = ""
        self.settings_save_path : str = ""
        self.use_resgrades : bool = False
        self.run_flags : int = 0
        self.dtc_energy : float = 0.0
        self.trigger_mode : int = 0
        self.invert_f0 : int = 0
        self.invert_veto : int = 0
        self.debounce : int = 0
        self.exposure_time : float = 0.0
        self.num_images : int = 0

        # daq parameter tree members
        self.daq_enabled : bool = False
        self.daq_endpoints : list[str] = []

        # cmd parameter tree members. No state to store.
        # connect, disconnect, save, restore, start, stop, trigger

        tree = \
        {
            
            XspressDetectorStr.CONFIG_APP :
            {

                XspressDetectorStr.CONFIG_APP_DEBUG :             (lambda: self.ctr_debug, partial(self._set, 'ctr_debug'), {}),
                XspressDetectorStr.CONFIG_APP_CTRL_ENDPOINT:      (lambda: self.ctr_endpoint, partial(self._set, 'ctr_endpoint'), {}),
            },

            XspressDetectorStr.CONFIG_ADAPTER : {
                XspressDetectorStr.CONFIG_ADAPTER_START_TIME:     (lambda: self.start_time.strftime("%B %d, %Y %H:%M:%S"), {}),
                XspressDetectorStr.CONFIG_ADAPTER_UP_TIME:        (lambda: str(datetime.now() - self.start_time), {}),
                XspressDetectorStr.CONFIG_ADAPTER_CONNECTED:      (self._async_client.is_connected, {}),
                XspressDetectorStr.CONFIG_ADAPTER_USERNAME:       (lambda: self.username, {}),
                XspressDetectorStr.CONFIG_ADAPTER_SCAN:           (self.sched.get_time, {}),
            },
            XspressDetectorStr.CONFIG_STATUS : {
                XspressDetectorStr.CONFIG_STATUS_SENSOR : {
                    XspressDetectorStr.CONFIG_STATUS_SENSOR_HEIGHT: (lambda: self.max_channels, {}),
                    XspressDetectorStr.CONFIG_STATUS_SENSOR_WIDTH: (lambda: self.max_spectra, {}),
                    XspressDetectorStr.CONFIG_STATUS_SENSOR_BYTES: (lambda: self.max_spectra * self.max_spectra * 4, {}), # 4 bytes per int32 point
                },
                XspressDetectorStr.CONFIG_STATUS_MANUFACTURER: (lambda: "Quandum Detectors", {}),
                XspressDetectorStr.CONFIG_STATUS_MODEL: (lambda: "Xspress 4", {}),
            },

            XspressDetectorStr.CONFIG_XSP :
            {
                XspressDetectorStr.CONFIG_XSP_NUM_CARDS :         (lambda: self.num_cards, partial(self._set, 'num_cards'), {}),
                XspressDetectorStr.CONFIG_XSP_NUM_TF :            (lambda: self.num_tf, partial(self._set, 'num_tf'), {}),
                XspressDetectorStr.CONFIG_XSP_BASE_IP :           (lambda: self.base_ip, partial(self._set, 'base_ip'), {}),
                XspressDetectorStr.CONFIG_XSP_MAX_CHANNELS :      (lambda: self.max_channels, partial(self._set, 'max_channels'), {}),

                XspressDetectorStr.CONFIG_XSP_MAX_SPECTRA :       (lambda: self.max_spectra, partial(self._set, 'max_spectra'), {}),
                XspressDetectorStr.CONFIG_XSP_DEBUG :             (lambda: self.debug, partial(self._set, 'debug'), {}),
                XspressDetectorStr.CONFIG_XSP_CONFIG_PATH :       (lambda: self.settings_path, partial(self._set, "settings_path"), {}),
                XspressDetectorStr.CONFIG_XSP_CONFIG_SAVE_PATH :  (lambda: self.settings_save_path, partial(self._set, "settings_save_path"), {}),
                XspressDetectorStr.CONFIG_XSP_USE_RESGRADES :     (lambda: self.use_resgrades, partial(self._set, "use_resgrades"), {}),
                XspressDetectorStr.CONFIG_XSP_RUN_FLAGS :         (lambda: self.run_flags, partial(self._set, "run_flags"), {}),
                XspressDetectorStr.CONFIG_XSP_DTC_ENERGY :        (lambda: self.dtc_energy, partial(self._set, "dtc_energy"), {}),
                XspressDetectorStr.CONFIG_XSP_TRIGGER_MODE :      (lambda: self.trigger_mode, partial(self._set, "trigger_mode"), {}),
                XspressDetectorStr.CONFIG_XSP_INVERT_F0 :         (lambda: self.invert_f0, partial(self._set, "invert_f0"), {}),
                XspressDetectorStr.CONFIG_XSP_INVERT_VETO :       (lambda: self.invert_veto, partial(self._set, "invert_veto"), {}),
                XspressDetectorStr.CONFIG_XSP_DEBOUNCE :          (lambda: self.debounce, partial(self._set, "debounce"), {}),
                XspressDetectorStr.CONFIG_XSP_EXPOSURE_TIME :     (lambda: self.exposure_time, partial(self._set, "exposure_time"), {}),
                XspressDetectorStr.CONFIG_XSP_NUM_IMAGES :        (lambda: self.num_images, partial(self._set, "num_images"), {}),
            }
        }
        self.parameter_tree = ParameterTree(tree)
        put_tree = \
        {
            XspressDetectorStr.CONFIG_APP :
            {
                XspressDetectorStr.CONFIG_APP_DEBUG : (None, partial(self._put, MessageType.CONFIG_APP, XspressDetectorStr.CONFIG_APP_DEBUG), {}),
                XspressDetectorStr.CONFIG_APP_CTRL_ENDPOINT: (None, partial(self._put, MessageType.CONFIG_APP, XspressDetectorStr.CONFIG_APP_CTRL_ENDPOINT), {}),
                XspressDetectorStr.CONFIG_APP_SHUTDOWN: (None, partial(self._put, MessageType.CONFIG_APP, XspressDetectorStr.CONFIG_APP_SHUTDOWN), {}),
            },

            XspressDetectorStr.CONFIG_REQUEST: (None, self.read_config, {}),
            XspressDetectorStr.CONFIG_ADAPTER :
            {
                XspressDetectorStr.CONFIG_ADAPTER_UPDATE: (None, self.do_updates, {}),
                XspressDetectorStr.CONFIG_ADAPTER_SCAN: (None, self.sched.set_time, {}),
                XspressDetectorStr.CONFIG_ADAPTER_RESET: (None, self.reset, {}),
            },
            XspressDetectorStr.CONFIG_XSP :
            {
                XspressDetectorStr.CONFIG_XSP_NUM_CARDS : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_NUM_CARDS), {}),
                XspressDetectorStr.CONFIG_XSP_NUM_TF : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_NUM_TF), {}),
                XspressDetectorStr.CONFIG_XSP_BASE_IP : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_BASE_IP), {}),
                XspressDetectorStr.CONFIG_XSP_MAX_CHANNELS : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_MAX_CHANNELS), {}),
                XspressDetectorStr.CONFIG_XSP_MAX_SPECTRA : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_MAX_SPECTRA), {}),
                XspressDetectorStr.CONFIG_XSP_DEBUG : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_DEBUG), {}),
                XspressDetectorStr.CONFIG_XSP_CONFIG_PATH : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_CONFIG_PATH), {}),
                XspressDetectorStr.CONFIG_XSP_CONFIG_SAVE_PATH : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_CONFIG_SAVE_PATH), {}),
                XspressDetectorStr.CONFIG_XSP_USE_RESGRADES : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_USE_RESGRADES), {}),
                XspressDetectorStr.CONFIG_XSP_RUN_FLAGS : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_RUN_FLAGS), {}),
                XspressDetectorStr.CONFIG_XSP_DTC_ENERGY : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_DTC_ENERGY), {}),
                XspressDetectorStr.CONFIG_XSP_TRIGGER_MODE : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_TRIGGER_MODE), {}),
                XspressDetectorStr.CONFIG_XSP_INVERT_F0 : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_INVERT_F0), {}),
                XspressDetectorStr.CONFIG_XSP_INVERT_VETO : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_INVERT_VETO), {}),
                XspressDetectorStr.CONFIG_XSP_DEBOUNCE : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_DEBOUNCE), {}),
                XspressDetectorStr.CONFIG_XSP_TRIGGER_MODE : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_TRIGGER_MODE), {}),
                XspressDetectorStr.CONFIG_XSP_EXPOSURE_TIME : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_EXPOSURE_TIME), {}),
                XspressDetectorStr.CONFIG_XSP_NUM_IMAGES : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_NUM_IMAGES), {}),
            },
            XspressDetectorStr.CONFIG_CMD :
            {
                XspressDetectorStr.CONFIG_CMD_CONNECT : (None, partial(self._put, MessageType.CMD, XspressDetectorStr.CONFIG_CMD_CONNECT)),
                XspressDetectorStr.CONFIG_CMD_DISCONNECT : (None, partial(self._put, MessageType.CMD, XspressDetectorStr.CONFIG_CMD_DISCONNECT)),
                XspressDetectorStr.CONFIG_CMD_SAVE : (None, partial(self._put, MessageType.CMD, XspressDetectorStr.CONFIG_CMD_SAVE)),
                XspressDetectorStr.CONFIG_CMD_RESTORE : (None, partial(self._put, MessageType.CMD, XspressDetectorStr.CONFIG_CMD_RESTORE)),
                XspressDetectorStr.CONFIG_CMD_START : (None, partial(self._put, MessageType.CMD, XspressDetectorStr.CONFIG_CMD_START)),
                XspressDetectorStr.CONFIG_CMD_STOP : (None, partial(self._put, MessageType.CMD, XspressDetectorStr.CONFIG_CMD_STOP)),
                XspressDetectorStr.CONFIG_CMD_TRIGGER : (None, partial(self._put, MessageType.CMD, XspressDetectorStr.CONFIG_CMD_TRIGGER)),
                XspressDetectorStr.CONFIG_CMD_ACQUIRE : (None, self.acquire)
            }
        }
        self.put_parameter_tree = ParameterTree(put_tree)

    def acquire(self, value):
        if value:
            self._put(MessageType.CMD, XspressDetectorStr.CONFIG_CMD_START, 1)
        else:
            self._put(MessageType.CMD, XspressDetectorStr.CONFIG_CMD_STOP, 1)

    def do_updates(self, value: int):
        if value:
            self.sched.start()
        else:
            self.sched.stop()
        
    def _set(self, attr_name, value):
        setattr(self, attr_name, value)

    def _put(self, message_type: MessageType, config_str: str,  value: any):
        logging.debug(debug_method())
        if not self._async_client.is_connected():
            raise ConnectionError("Control server is not connected! Check if it is running and tcp endpoint is configured correctly")
        if message_type == MessageType.CONFIG_ROOT:
            msg = self._build_message_single(config_str, value)
        else:
            msg = self._build_message(message_type, {config_str:value})
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


    def on_recv_callback(self, msg):
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
                # logging.debug(f"XspressDetector._param_tree_set_recursive: parameter path {path} was set to {params}")
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
        self.initial_config_msg = self._build_message(MessageType.CONFIG_XSP, config)
        self._async_client.send_requst(self.initial_config_msg)

        # self._connect() # uncomment this when ready to test with real XSP
        self.sched.start()

    def reset(self, *unused):
        self._async_client.send_requst(self.initial_config_msg)


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
            msg = IpcMessage(XspressDetectorStr.CONFIG_CMD, XspressDetectorStr.CONFIG_REQUEST)
            return msg
        elif message_type == MessageType.CONFIG_APP:
            params_group = XspressDetectorStr.CONFIG_APP
        elif message_type == MessageType.CONFIG_XSP:
            params_group = XspressDetectorStr.CONFIG_XSP
        elif message_type == MessageType.CMD:
            params_group = XspressDetectorStr.CONFIG_CMD
        else:
            raise XspressDetectorException(f"XspressDetector._build_message: {message_type} is not type MessageType")
        msg = IpcMessage("cmd", "configure")
        msg.set_param(params_group, config)
        return msg

    def read_config(self, *unused):
        self._async_client.send_requst(self._build_message(MessageType.REQUEST))

