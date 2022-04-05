import asyncio
import logging
import getpass
import inspect

from enum import Enum
from functools import partial
from datetime import datetime

from odin.adapters.adapter import ApiAdapterRequest, ApiAdapter
from odin_data.ipc_message import IpcMessage
from odin_data.ipc_channel import _cast_str
from odin.adapters.parameter_tree import ParameterTree, ParameterTreeError
from tornado.ioloop import PeriodicCallback

from .client import AsyncClient, CallbackClient
from .debug import debug_method

ENDPOINT_TEMPLATE = "tcp://{}:{}"
UPDATE_PARAMS_PERIOD_SECONDS = 0.5
QUEUE_SIZE_LIMIT = 10

XSPRESS_MODE_MCA = "mca"
XSPRESS_MODE_LIST = "list"
XSPRESS_EXPOSURE_LOWER_LIMIT = 1./1000000
XSPRESS_EXPOSURE_UPPER_LIMIT = 20.0

def is_coroutine(object):
    """
    see: https://stackoverflow.com/questions/67020609/partial-of-a-class-coroutine-isnt-a-coroutine-why
    """
    while isinstance(object, partial):
        object = object.func
    return inspect.iscoroutinefunction(object)

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
    DAQ = 6

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

    def str2int(mode):
        x = XspressTriggerMode
        map = dict(
            software=x.TM_SOFTWARE,
            ttl_rising_edge=x.TM_TTL_RISING_EDGE,
            burst=x.TM_BURST,
            ttl_veto_only=x.TM_TTL_VETO_ONLY,
            software_start_stop=x.TM_SOFTWARE_START_STOP,
            idc=x.TM_IDC,
            ttl_both=x.TM_TTL_BOTH,
            lvds_veto_only=x.TM_LVDS_VETO_ONLY,
            lvds_both=x.TM_LVDS_BOTH
        )
        return map[mode]

class XspressDetectorStr:
    STATUS                       = "status"
    STATUS_ERROR                 = "error"
    STATUS_STATE                 = "state"
    STATUS_CONNECTED             = "connected"
    STATUS_RECONNECT_REQUIRED    = "reconnect_required"
    STATUS_SENSOR                = "sensor"
    STATUS_SENSOR_HEIGHT         = "height"
    STATUS_SENSOR_WIDTH          = "width"
    STATUS_SENSOR_BYTES          = "bytes"
    STATUS_MANUFACTURER          = "manufacturer"
    STATUS_MODEL                 = "model"
    STATUS_ACQ_COMPLETE          = "acquisition_complete"
    STATUS_FRAMES                = "frames_acquired"
    STATUS_SCALAR_0              = "scalar_0"
    STATUS_SCALAR_1              = "scalar_1"
    STATUS_SCALAR_2              = "scalar_2"
    STATUS_SCALAR_3              = "scalar_3"
    STATUS_SCALAR_4              = "scalar_4"
    STATUS_SCALAR_5              = "scalar_5"
    STATUS_SCALAR_6              = "scalar_6"
    STATUS_SCALAR_7              = "scalar_7"
    STATUS_SCALAR_8              = "scalar_8"
    STATUS_DTC                   = "dtc"
    STATUS_INP_EST               = "inp_est"
    STATUS_TEMP_0                = "temp_0"
    STATUS_TEMP_1                = "temp_1"
    STATUS_TEMP_2                = "temp_2"
    STATUS_TEMP_3                = "temp_3"
    STATUS_TEMP_4                = "temp_4"
    STATUS_TEMP_5                = "temp_5"
    STATUS_CH_FRAMES             = "ch_frames_acquired"
    STATUS_FEM_DROPPED_FRAMES    = "fem_dropped_frames"
    STATUS_CARDS_CONNECTED       = "cards_connected"
    STATUS_NUM_CH_CONNECTED      = "num_ch_connected"

    CONFIG_ADAPTER               = "adapter"
    CONFIG_ADAPTER_RESET         = "reset"
    CONFIG_ADAPTER_SCAN          = "scan"
    CONFIG_ADAPTER_UPDATE        = "update"
    CONFIG_ADAPTER_START_TIME    = "start_time"
    CONFIG_ADAPTER_UP_TIME       = "up_time"
    CONFIG_ADAPTER_CONNECTED     = "connected"
    CONFIG_ADAPTER_USERNAME      = "username"
    CONFIG_ADAPTER_CONFIG_RAW    = "config_raw"
    CONFIG_ADAPTER_DEBUG_LEVEL   = "debug_level"

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

    CONFIG_XSP_MODE                  = "mode"
    CONFIG_XSP_MODE_CONTROL          = "mode_control"
    CONFIG_XSP_SCA5_LOW              = "sca5_low_lim"
    CONFIG_XSP_SCA5_HIGH             = "sca5_high_lim"
    CONFIG_XSP_SCA6_LOW              = "sca6_low_lim"
    CONFIG_XSP_SCA6_HIGH             = "sca6_high_lim"
    CONFIG_XSP_SCA4_THRESH           = "sca4_threshold"

    CONFIG_XSP_DTC_FLAGS             = "dtc_flags"
    CONFIG_XSP_DTC_ALL_EVT_OFF       = "dtc_all_evt_off"
    CONFIG_XSP_DTC_ALL_EVT_GRAD      = "dtc_all_evt_grad"
    CONFIG_XSP_DTC_ALL_EVT_RATE_OFF  = "dtc_all_evt_rate_off"
    CONFIG_XSP_DTC_ALL_EVT_RATE_GRAD = "dtc_all_evt_rate_grad"
    CONFIG_XSP_DTC_IN_WIN_OFF        = "dtc_in_win_off"
    CONFIG_XSP_DTC_IN_WIN_GRAD       = "dtc_in_win_grad"
    CONFIG_XSP_DTC_IN_WIN_RATE_OFF   = "dtc_in_win_rate_off"
    CONFIG_XSP_DTC_IN_WIN_RATE_GRAD  = "dtc_in_win_rate_grad"


    CONFIG_DAQ                   = "daq"
    CONFIG_DAQ_ENABLED           = "enabled"
    CONFIG_DAQ_ZMQ_ENDPOINTS     = "endpoints"

    CONFIG_CMD                   = "command"
    CONFIG_CMD_RECONFIGURE       = "reconfigure"
    CONFIG_CMD_CONNECT           = "connect"
    CONFIG_CMD_DISCONNECT        = "disconnect"
    CONFIG_CMD_SAVE              = "save"
    CONFIG_CMD_RESTORE           = "restore"
    CONFIG_CMD_START             = "start"
    CONFIG_CMD_STOP              = "stop"
    CONFIG_CMD_TRIGGER           = "trigger"
    CONFIG_CMD_START_ACQUISITION = "start_acquisition"
    CONFIG_CMD_STOP_ACQUISITION  = "stop_acquisition"

    VERSION                      = "version"
    VERSION_XSPRESS_DETECTOR     = "xspress-detector"
    VERSION_XSPRESS_DETECTOR_MAJOR = "major"
    VERSION_XSPRESS_DETECTOR_MINOR = "minor"
    VERSION_XSPRESS_DETECTOR_PATCH = "patch"
    VERSION_XSPRESS_DETECTOR_SHORT = "short"
    VERSION_XSPRESS_DETECTOR_FULL = "full"

    API                          = "api"


class XspressDetectorException(Exception):
    pass

class NotAcknowledgeException(Exception):
    pass

class XspressDetector(object):

    def __init__(self, ip: str, port: int, debug_level=logging.INFO):
        self.logger = logging.getLogger()
        self.logger.setLevel(debug_level)

        self.endpoint = ENDPOINT_TEMPLATE.format("0.0.0.0", 12000)
        self.start_time = datetime.now()
        self.username = getpass.getuser()
        self.config_raw : dict = {}

        self._callback_client = CallbackClient(ip, port, self.on_recv_callback)
        self._async_client = AsyncClient(ip, port)
        self.timeout = 1
        self.xsp_connected: bool = False
        self.sched = AuotoPeriodicCallback(self.read_config, self._callback_client.is_connected, UPDATE_PARAMS_PERIOD_SECONDS)

        # root level parameter tree members
        self.ctr_endpoint = ENDPOINT_TEMPLATE.format(ip, port)
        self.ctr_debug = 0

        self.api : str = ""
        self.error : str = ""
        self.state : str = ""

        self.temp_0 : list[float] = []
        self.temp_1 : list[float] = []
        self.temp_2 : list[float] = []
        self.temp_3 : list[float] = []
        self.temp_4 : list[float] = []
        self.temp_5 : list[float] = []

        self.ch_frames_acquired : list[int] = []
        self.fem_dropped_frames : list[int] = []
        self.num_ch_connected : list[int] = []
        self.cards_connected : list[int] = []

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
        self.endpoints : list[str] = [] # daq endpoints

        # status parameter tree members
        self.connected : bool = False
        self.reconnect_required : bool = False
        self.acquisition_complete : bool = False
        self.frames_acquired : int = 0

        self.mode: str = "" # 'mca' or 'list' for readback

        self.sca5_low_lim : list[int] = []
        self.sca5_high_lim : list[int] = []
        self.sca6_low_lim : list[int] = []
        self.sca6_high_lim : list[int] = []
        self.sca4_threshold : list[int] = []

        self.dtc_flags : list[int] = []
        self.dtc_all_evt_off : list[float] = []
        self.dtc_all_evt_grad : list[float] = []
        self.dtc_all_evt_rate_off : list[float] = []
        self.dtc_all_evt_rate_grad : list[float] = []
        self.dtc_in_win_off : list[float] = []
        self.dtc_in_win_grad : list[float] = []
        self.dtc_in_win_rate_off : list[float] = []
        self.dtc_in_win_rate_grad : list[float] = []

        self.dtc : list[float] = []
        self.inp_est : list[float] = []

        self.scalar_0 : list[int] = []
        self.scalar_1 : list[int] = []
        self.scalar_2 : list[int] = []
        self.scalar_3 : list[int] = []
        self.scalar_4 : list[int] = []
        self.scalar_5 : list[int] = []
        self.scalar_6 : list[int] = []
        self.scalar_7 : list[int] = []
        self.scalar_8 : list[int] = []

        # cmd parameter tree members. No state to store.
        # connect, disconnect, save, restore, start, stop, trigger

        # version params
        self.version_full = ""
        self.version_major = 0
        self.version_minor = 0
        self.version_patch = 0
        self.version_short = ""

        tree = \
        {
            XspressDetectorStr.API : (lambda: self.api, partial(self._set, 'api'), {}),
            XspressDetectorStr.CONFIG_APP :
            {

                XspressDetectorStr.CONFIG_APP_DEBUG :             (lambda: self.ctr_debug, partial(self._set, 'ctr_debug'), {}),
                XspressDetectorStr.CONFIG_APP_CTRL_ENDPOINT :     (lambda: self.ctr_endpoint, partial(self._set, 'ctr_endpoint'), {}),
            },
            XspressDetectorStr.CONFIG_DAQ :
            {
                XspressDetectorStr.CONFIG_DAQ_ENABLED :          (lambda: self.daq_enabled, partial(self._set, 'daq_enabled')),
                XspressDetectorStr.CONFIG_DAQ_ZMQ_ENDPOINTS :    (lambda: self.endpoints, partial(self._set, 'endpoints')),
            },
            XspressDetectorStr.CONFIG_ADAPTER : {
                XspressDetectorStr.CONFIG_ADAPTER_START_TIME:     (lambda: self.start_time.strftime("%B %d, %Y %H:%M:%S"), {}),
                XspressDetectorStr.CONFIG_ADAPTER_UP_TIME:        (lambda: str(datetime.now() - self.start_time), {}),
                XspressDetectorStr.CONFIG_ADAPTER_CONNECTED:      (self._callback_client.is_connected, {}),
                XspressDetectorStr.CONFIG_ADAPTER_USERNAME:       (lambda: self.username, {}),
                XspressDetectorStr.CONFIG_ADAPTER_SCAN:           (self.sched.get_time, {}),
                XspressDetectorStr.CONFIG_ADAPTER_CONFIG_RAW:     (lambda: self.config_raw, {}),
                XspressDetectorStr.CONFIG_ADAPTER_DEBUG_LEVEL:     (lambda: self.logger.level, {}),
            },
            XspressDetectorStr.STATUS : {
                XspressDetectorStr.STATUS_SENSOR : {
                    XspressDetectorStr.STATUS_SENSOR_HEIGHT: (lambda: self.max_channels, {}),
                    XspressDetectorStr.STATUS_SENSOR_WIDTH: (lambda: self.max_spectra, {}),
                    XspressDetectorStr.STATUS_SENSOR_BYTES: (lambda: self.max_spectra * self.max_spectra * 4, {}), # 4 bytes per int32 point
                },
                XspressDetectorStr.STATUS_MANUFACTURER: (lambda: "Quantum Detectors", {}),
                XspressDetectorStr.STATUS_MODEL: (lambda: "Xspress 4", {}),
                XspressDetectorStr.STATUS_ACQ_COMPLETE : (lambda: self.acquisition_complete, partial(self._set, 'acquisition_complete'), {}),
                XspressDetectorStr.STATUS_FRAMES : (lambda: self.frames_acquired, partial(self._set, 'frames_acquired'), {}),

                XspressDetectorStr.STATUS_SCALAR_0 : (lambda: self.scalar_0, partial(self._set, 'scalar_0')),
                XspressDetectorStr.STATUS_SCALAR_1 : (lambda: self.scalar_1, partial(self._set, 'scalar_1')),
                XspressDetectorStr.STATUS_SCALAR_2 : (lambda: self.scalar_2, partial(self._set, 'scalar_2')),
                XspressDetectorStr.STATUS_SCALAR_3 : (lambda: self.scalar_3, partial(self._set, 'scalar_3')),
                XspressDetectorStr.STATUS_SCALAR_4 : (lambda: self.scalar_4, partial(self._set, 'scalar_4')),
                XspressDetectorStr.STATUS_SCALAR_5 : (lambda: self.scalar_5, partial(self._set, 'scalar_5')),
                XspressDetectorStr.STATUS_SCALAR_6 : (lambda: self.scalar_6, partial(self._set, 'scalar_6')),
                XspressDetectorStr.STATUS_SCALAR_7 : (lambda: self.scalar_7, partial(self._set, 'scalar_7')),
                XspressDetectorStr.STATUS_SCALAR_8 : (lambda: self.scalar_8, partial(self._set, 'scalar_8')),

                XspressDetectorStr.STATUS_DTC : (lambda: self.dtc, partial(self._set, 'dtc')),
                XspressDetectorStr.STATUS_INP_EST : (lambda: self.inp_est, partial(self._set, 'inp_est')),

                XspressDetectorStr.STATUS_ERROR : (lambda: self.error, partial(self._set, 'error'), {}),
                XspressDetectorStr.STATUS_STATE : (lambda: self.state, partial(self._set, 'state'), {}),
                XspressDetectorStr.STATUS_CONNECTED : (lambda: self.connected, partial(self._set, 'connected'), {}),
                XspressDetectorStr.STATUS_RECONNECT_REQUIRED : (lambda: self.reconnect_required, partial(self._set, 'reconnect_required'), {}),
                XspressDetectorStr.STATUS_TEMP_0 : (lambda: self.temp_0, partial(self._set, 'temp_0')),
                XspressDetectorStr.STATUS_TEMP_1 : (lambda: self.temp_1, partial(self._set, 'temp_1')),
                XspressDetectorStr.STATUS_TEMP_2 : (lambda: self.temp_2, partial(self._set, 'temp_2')),
                XspressDetectorStr.STATUS_TEMP_3 : (lambda: self.temp_3, partial(self._set, 'temp_3')),
                XspressDetectorStr.STATUS_TEMP_4 : (lambda: self.temp_4, partial(self._set, 'temp_4')),
                XspressDetectorStr.STATUS_TEMP_5 : (lambda: self.temp_5, partial(self._set, 'temp_5')),
                XspressDetectorStr.STATUS_CH_FRAMES : (lambda: self.ch_frames_acquired, partial(self._set, 'ch_frames_acquired')),
                XspressDetectorStr.STATUS_FEM_DROPPED_FRAMES : (lambda: self.fem_dropped_frames, partial(self._set, 'fem_dropped_frames')),
                XspressDetectorStr.STATUS_CARDS_CONNECTED : (lambda: self.cards_connected, partial(self._set, 'cards_connected')),
                XspressDetectorStr.STATUS_NUM_CH_CONNECTED : (lambda: self.num_ch_connected, partial(self._set, 'num_ch_connected')),
            },

            XspressDetectorStr.CONFIG_XSP :
            {
                XspressDetectorStr.CONFIG_XSP_MODE :              (lambda: self.mode, partial(self._set, "mode"), {}),

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
                XspressDetectorStr.CONFIG_XSP_SCA5_LOW :          (lambda: self.sca5_low_lim, partial(self._set, "sca5_low_lim"), {}),
                XspressDetectorStr.CONFIG_XSP_SCA5_HIGH :         (lambda: self.sca5_high_lim, partial(self._set, "sca5_high_lim"), {}),
                XspressDetectorStr.CONFIG_XSP_SCA6_LOW :          (lambda: self.sca6_low_lim, partial(self._set, "sca6_low_lim"), {}),
                XspressDetectorStr.CONFIG_XSP_SCA6_HIGH :         (lambda: self.sca6_high_lim, partial(self._set, "sca6_high_lim"), {}),
                XspressDetectorStr.CONFIG_XSP_SCA4_THRESH :       (lambda: self.sca4_threshold, partial(self._set, "sca4_threshold"), {}),

                XspressDetectorStr.CONFIG_XSP_DTC_FLAGS :             (lambda: self.dtc_flags, partial(self._set, "dtc_flags"), {}),
                XspressDetectorStr.CONFIG_XSP_DTC_ALL_EVT_OFF :       (lambda: self.dtc_all_evt_off, partial(self._set, "dtc_all_evt_off"), {}),
                XspressDetectorStr.CONFIG_XSP_DTC_ALL_EVT_GRAD :      (lambda: self.dtc_all_evt_grad, partial(self._set, "dtc_all_evt_grad"), {}),
                XspressDetectorStr.CONFIG_XSP_DTC_ALL_EVT_RATE_OFF :  (lambda: self.dtc_all_evt_rate_off, partial(self._set, "dtc_all_evt_rate_off"), {}),
                XspressDetectorStr.CONFIG_XSP_DTC_ALL_EVT_RATE_GRAD : (lambda: self.dtc_all_evt_rate_grad, partial(self._set, "dtc_all_evt_rate_grad"), {}),

                XspressDetectorStr.CONFIG_XSP_DTC_IN_WIN_OFF :        (lambda: self.dtc_in_win_off, partial(self._set, "dtc_in_win_off"), {}),
                XspressDetectorStr.CONFIG_XSP_DTC_IN_WIN_GRAD :       (lambda: self.dtc_in_win_grad, partial(self._set, "dtc_in_win_grad"), {}),
                XspressDetectorStr.CONFIG_XSP_DTC_IN_WIN_RATE_OFF :   (lambda: self.dtc_in_win_rate_off, partial(self._set, "dtc_in_win_rate_off"), {}),
                XspressDetectorStr.CONFIG_XSP_DTC_IN_WIN_RATE_GRAD :  (lambda: self.dtc_in_win_rate_grad, partial(self._set, "dtc_in_win_rate_grad"), {}),
            },
            XspressDetectorStr.VERSION :
            {
                XspressDetectorStr.VERSION_XSPRESS_DETECTOR : {
                    XspressDetectorStr.VERSION_XSPRESS_DETECTOR_FULL :  (lambda: self.version_full, partial(self._set, "version_full")),
                    XspressDetectorStr.VERSION_XSPRESS_DETECTOR_MAJOR : (lambda: self.version_major, partial(self._set, "version_major")),
                    XspressDetectorStr.VERSION_XSPRESS_DETECTOR_MINOR : (lambda: self.version_minor, partial(self._set, "version_minor")),
                    XspressDetectorStr.VERSION_XSPRESS_DETECTOR_PATCH : (lambda: self.version_patch, partial(self._set, "version_patch")),
                    XspressDetectorStr.VERSION_XSPRESS_DETECTOR_SHORT : (lambda: self.version_short, partial(self._set, "version_short")),
                }
            },

        }
        self.parameter_tree = ParameterTree(tree)
        self.put_tree = \
        {
            XspressDetectorStr.CONFIG_APP :
            {
                XspressDetectorStr.CONFIG_APP_DEBUG : (None, partial(self._put, MessageType.CONFIG_APP, XspressDetectorStr.CONFIG_APP_DEBUG), {}),
                XspressDetectorStr.CONFIG_APP_CTRL_ENDPOINT: (None, partial(self._put, MessageType.CONFIG_APP, XspressDetectorStr.CONFIG_APP_CTRL_ENDPOINT), {}),
                XspressDetectorStr.CONFIG_APP_SHUTDOWN: (None, partial(self._put, MessageType.CONFIG_APP, XspressDetectorStr.CONFIG_APP_SHUTDOWN), {}),
            },
            XspressDetectorStr.CONFIG_DAQ :
            {
                XspressDetectorStr.CONFIG_DAQ_ENABLED : (None, partial(self._put, MessageType.CONFIG_APP, XspressDetectorStr.CONFIG_APP_DEBUG), {}),
            },

            XspressDetectorStr.CONFIG_REQUEST: (None, self.read_config, {}),
            XspressDetectorStr.CONFIG_ADAPTER :
            {
                XspressDetectorStr.CONFIG_ADAPTER_UPDATE: (None, self.do_updates, {}),
                XspressDetectorStr.CONFIG_ADAPTER_SCAN: (None, self.sched.set_time, {}),
                XspressDetectorStr.CONFIG_ADAPTER_RESET: (None, self.reset, {}),
                XspressDetectorStr.CONFIG_ADAPTER_DEBUG_LEVEL: (None, self.logger.setLevel, {}),
            },
            XspressDetectorStr.CONFIG_XSP :
            {
                XspressDetectorStr.CONFIG_XSP_MODE_CONTROL : (None, self.set_mode, {}),

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
                XspressDetectorStr.CONFIG_XSP_EXPOSURE_TIME : (None, partial(self.set_exposure), {}),
                XspressDetectorStr.CONFIG_XSP_NUM_IMAGES : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_NUM_IMAGES), {}),
                XspressDetectorStr.CONFIG_XSP_SCA5_LOW : (None, partial(self._put, MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_SCA5_LOW), {}),
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
                XspressDetectorStr.CONFIG_CMD_START_ACQUISITION : (None, partial(self.acquire, 1)),
                XspressDetectorStr.CONFIG_CMD_STOP_ACQUISITION : (None, partial(self.acquire, 0)),
                XspressDetectorStr.CONFIG_CMD_RECONFIGURE : (None, self.reconfigure),
            },
        }
        self.put_parameter_tree = ParameterTree(self.put_tree)

    def set_fr_handler(self, handler):
        self.fr_handler: ApiAdapter = handler
        self.logger.error(f"self.fr_handler set to {handler}")
        req = ApiAdapterRequest(None, accept='application/json')
        resp = self.fr_handler.get(path="config", request=req).data
        logging.error(f"recieved from fr adaptor: {resp}")

    def set_fp_handler(self, handler):
        self.fp_handler: ApiAdapter = handler
        self.logger.error(f"self.fp_handler set to {handler}")
        req = ApiAdapterRequest(None, accept='application/json')
        resp = self.fr_handler.get(path="config", request=req).data
        logging.error(f"recieved from fr adaptor: {resp}")
    
    async def configure_frs_fps(self, mode: int):
        port_mapping = [
            {
                "ip": "192.168.0.65",
                "ports": "30125,30126,30127,30128,30129,"
            },
            {
                "ip": "192.168.0.65",
                "ports": "30130,30131,30132,30133,30134,"
            },
            {
                "ip": "192.168.0.69",
                "ports": "30125,30126,30127,30128,30129,"
            },
            {
                "ip": "192.168.0.69",
                "ports": "30130,30131,30132,30133,30134,"
            },
            {
                "ip": "192.168.0.73",
                "ports": "30125,30126,30127,30128,30129,"
            },
            {
                "ip": "192.168.0.73",
                "ports": "30130,30131,30132,30133,30134,"
            },
            {
                "ip": "192.168.0.77",
                "ports": "30125,30126,30127,30128,30129,"
            },
            {
                "ip": "192.168.0.77",
                "ports": "30130,30131,30132,30133,30134,"
            }
        ]

            
        if mode == XSPRESS_MODE_LIST:
            fp_req = ApiAdapterRequest('{"index":"list"}')
            resp = self.fp_handler.put(path="config/execute", request=fp_req)
            logging.error(f"response from fp_handler: {resp.status_code}, {resp.data}")
            port_config = \
            { f"{10000+(index*10)}" :
                {
                    "rx_ports": port_mapping[index]['ports'],
                    "rx_type": "udp",
                    "decoder_type": "XspressListMode",
                    "rx_address": port_mapping[index]['ip'],
                    "rx_recv_buffer_size":30000000
                }
                for index in range(8)
            }
        elif mode == XSPRESS_MODE_MCA:
            fp_req = ApiAdapterRequest('{"index":"mca"}')
            resp = self.fp_handler.put(path="config/execute", request=fp_req)
            logging.error(f"response from fp_handler: {resp.status_code}, {resp.data}")
            port_config = \
            { f"{10000+(index*10)}": 
                {
                    "rx_ports": "{},".format(index + 15150),
                    "rx_type": "zmq",
                    "decoder_type": "Xspress",
                    "rx_address": "127.0.0.1"
                }
                for index in range(9)
            }
        else:
            raise ValueError("invalid mode requested")

        tasks = ()
        for port, config in port_config.items():
            client = AsyncClient("127.0.0.1", port)
            tasks += (asyncio.create_task(self.async_send_task(client, config)),)
        result = await asyncio.gather(*tasks)
        [logging.error(f"recieved reply: {r}") for r in result]
    
    async def async_send_task(self, client, config):
        await client.wait_till_connected()
        msg = self._build_message(MessageType.CONFIG_XSP, config)
        resp = await client.send_recv(msg)
        if resp.get_msg_type() == resp.NACK:
            raise NotAcknowledgeException()
        return resp


    async def reconfigure(self, *unused):
        resp = await self._put(MessageType.CMD, XspressDetectorStr.CONFIG_CMD_DISCONNECT, 1)
        resp = await self._put(MessageType.CMD, XspressDetectorStr.CONFIG_CMD_CONNECT, 1)
        if self.mode == XSPRESS_MODE_MCA:
            await self.configure_frs_fps(XSPRESS_MODE_MCA)
            resp = await self._async_client.send_recv(self.initial_daq_msg)
        else:
            await self.configure_frs_fps(XSPRESS_MODE_LIST)
        return resp

    async def acquire(self, value, *unused):
        if value:
            reply = await self._put(MessageType.CMD, XspressDetectorStr.CONFIG_CMD_START, 1)
            self.acquisition_complete = False
            return reply
        else:
            return await self._put(MessageType.CMD, XspressDetectorStr.CONFIG_CMD_STOP, 1)

    async def set_mode(self, value):
        if value == 0:
            return await self._put(MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_MODE, XSPRESS_MODE_MCA)
        else: # mode == "list"
            return await self._put(MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_MODE, XSPRESS_MODE_LIST)

    async def set_exposure(self, time: float):
        if time < XSPRESS_EXPOSURE_LOWER_LIMIT or time > XSPRESS_EXPOSURE_UPPER_LIMIT:
            raise ValueError((
                f"Cannot set exposure to values outside of the range "
                f"({XSPRESS_EXPOSURE_LOWER_LIMIT}, {XSPRESS_EXPOSURE_UPPER_LIMIT})"
                f".\n value provided was {time}"
            ))
        return await self._put(MessageType.CONFIG_XSP, XspressDetectorStr.CONFIG_XSP_EXPOSURE_TIME, time)
    


    def do_updates(self, value: int):
        if value:
            self.sched.start()
        else:
            self.sched.stop()

    def _set(self, attr_name, value):
        setattr(self, attr_name, value)

    async def _put(self, message_type: MessageType, config_str: str,  value: any):
        self.logger.debug(debug_method())
        if not self._async_client.is_connected():
            raise ConnectionError("Control server is not connected! Check if it is running and tcp endpoint is configured correctly")
        if message_type == MessageType.CONFIG_ROOT:
            msg = self._build_message_single(config_str, value)
        else:
            msg = self._build_message(message_type, {config_str:value})
        resp = await self._async_client.send_recv(msg)
        return resp

    def put(self, path: str, data):
        try:
            self.put_parameter_tree.set(path, data)
            return {'async_message sent': None}
        except ParameterTreeError as e:
            self.logger.error(f"parameter_tree error: {e}")
            raise XspressDetectorException(e)

    async def put_array(self, path, data):
        self.logger.error(debug_method())
        path = path.split("/")
        index = int(path[-1])
        array_name = path[-2]
        sub_path = path[-3]
        arr = self.__dict__[array_name].copy()
        print(arr)
        arr[index] = data
        msg = self._build_message_single(sub_path, {array_name: arr})
        resp : IpcMessage = await self._async_client.send_recv(msg)
        if resp.get_msg_type() == resp.NACK:
            raise NotAcknowledgeException(f"failed to set {data} to {path} on the control server:\nMessge recieved: {resp}")
        self.__dict__[array_name] = arr
        return resp.encode()

    async def put_single(self, path, data):
        # param_name = int(path[-1])
        # sub_path = path[-2]
        tokens = path.split("/")
        callback = self.put_tree
        for token in tokens:
            callback = callback[token]
        callback = callback[1]
        if is_coroutine(callback):
            resp = await callback(data)
            if resp.get_msg_type() == resp.NACK:
                raise NotAcknowledgeException(f"failed to set {data} to {path} on the control server:\nMessge recieved: {resp}")
            return resp.encode()
        else:
            callback(data)
            return {f"callback at {path} was called with args": data}


    def get(self, path):
        try:
            return self.parameter_tree.get(path)
        except ParameterTreeError as e:
            self.logger.error(f"parameter_tree error: {e}")
            raise LookupError(e)


    def on_recv_callback(self, msg):
        BASE_PATH = ""
        data = _cast_str(msg[0])
        self.logger.debug(f"queue size = {self._callback_client.get_queue_size()}")
        self.logger.debug(f"message recieved = {data}")
        ipc_msg : IpcMessage = IpcMessage(from_str=data)

        if not ipc_msg.is_valid():
            raise XspressDetectorException("IpcMessage recieved is not valid")
        if ipc_msg.get_msg_val() == XspressDetectorStr.CONFIG_REQUEST:
            self.config_raw = ipc_msg.get_params()
            if ipc_msg.get_msg_type() == IpcMessage.ACK and ipc_msg.get_params():
                self._param_tree_set_recursive(BASE_PATH, ipc_msg.get_params())
            else:
                pass

    def _param_tree_set_recursive(self, path, params):
        if not isinstance(params, dict):
            try:
                path = path.strip("/")
                self.parameter_tree.set(path, params)
                # self.logger.debug(f"XspressDetector._param_tree_set_recursive: parameter path {path} was set to {params}")
            except ParameterTreeError as e:
                self.logger.error(e)
                self.logger.warning(
                    (
                        f"XspressDetector._param_tree_set_recursive: parameter path {path} is not in parameter tree\n"
                        f"params = {params}"
                    )
                )
                pass
        else:
            for param_name, params in params.items():
                self._param_tree_set_recursive(f"{path}/{param_name}", params)

    def configure(self,
        num_cards,
        num_tf,
        base_ip,
        max_channels,
        max_spectra,
        settings_path,
        run_flags,
        debug,
        daq_endpoints,
        ):
        self.logger.critical(debug_method())
        self.num_cards = num_cards
        self.num_tf = num_tf
        self.base_ip = base_ip
        self.max_channels = max_channels
        self.max_spectra = max_spectra
        self.settings_path = settings_path
        self.run_flags = run_flags
        self.debug = debug
        self.endpoints = daq_endpoints

        x = XspressDetectorStr
        config = {
            x.CONFIG_XSP_NUM_CARDS: self.num_cards,
            x.CONFIG_XSP_NUM_TF: self.num_tf,
            x.CONFIG_XSP_BASE_IP: self.base_ip,
            x.CONFIG_XSP_MAX_CHANNELS: self.max_channels,
            x.CONFIG_XSP_MAX_SPECTRA: self.max_spectra,
            x.CONFIG_XSP_CONFIG_PATH: self.settings_path,
            x.CONFIG_XSP_RUN_FLAGS: self.run_flags,
            x.CONFIG_XSP_DEBUG: self.debug,
        }
        self.initial_config_msg = self._build_message(MessageType.CONFIG_XSP, config)
        self.initial_daq_msg = self._build_message(MessageType.DAQ, {
            x.CONFIG_DAQ_ZMQ_ENDPOINTS: daq_endpoints,
            x.CONFIG_DAQ_ENABLED: True,
        })
        self.sched.start()

    async def reset(self, *unused):
        resp = await self._async_client.send_recv(self.initial_config_msg)
        if self.mode == XSPRESS_MODE_MCA:
            resp = await self._async_client.send_recv(self.initial_daq_msg)
        return resp
        


    def _connect(self):
        command = { "connect": None }
        command_message = self._build_message(MessageType.CMD, command)

        self._callback_client.send(command_message)
        # success, _ = self._client.send_configuration(command_message, target=XspressDetectorStr.CONFIG_CMD)
        # if not success:
        #     self.connected = False
        #     self.logger.error("could not connect to Xspress")
        #     raise XspressDetectorException("could not connect to the xspress unit")
        self.connected = True
        self._restore()


    def _restore(self):
        command = { "restore": None }
        command_message = self._build_message(MessageType.CMD, command)
        # success, reply =  self._client.send_configuration(command_message, target=XspressDetectorStr.CONFIG_CMD)
        self._callback_client.send(command_message)

    def _build_message_single(self, param_str: str, value: any):
        msg = IpcMessage("cmd", "configure")
        msg.set_param(param_str, value)
        return msg

    def _build_message(self, message_type: MessageType, config: dict = None):
        if message_type == MessageType.REQUEST:
            msg = IpcMessage("cmd", XspressDetectorStr.CONFIG_REQUEST)
            return msg
        elif message_type == MessageType.CONFIG_APP:
            params_group = XspressDetectorStr.CONFIG_APP
        elif message_type == MessageType.CONFIG_XSP:
            params_group = XspressDetectorStr.CONFIG_XSP
        elif message_type == MessageType.CMD:
            params_group = XspressDetectorStr.CONFIG_CMD
        elif message_type == MessageType.DAQ:
            params_group = XspressDetectorStr.CONFIG_DAQ
        else:
            raise XspressDetectorException(f"XspressDetector._build_message: {message_type} is not type MessageType")
        msg = IpcMessage("cmd", "configure")
        msg.set_param(params_group, config)
        return msg

    def read_config(self, *unused):
        self._callback_client.send(self._build_message(MessageType.REQUEST))


