import os, time
import argparse
import zmq
import json
import curses

import os
from abc import ABC, abstractmethod
from odin_data.ipc_client import IpcClient
from odin_data.ipc_message import IpcMessage


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

class XspressMessage(ABC):
    @abstractmethod
    def get_message() -> IpcMessage:
        return IpcMessage("msg_type", "msg_val")


class XspressConfigMessage(XspressMessage):
    params_group = "xsp"
    params = {}
    def __init__(self, config: dict):
        for k, v in config.items():
            self.params[k] = v

    def get_message(self):
        msg = IpcMessage("cmd", "configure")
        msg.set_param(self.params_group, self.params)
        return msg

class XspressCommandMessage(XspressMessage):
    params_group = "cmd"
    params = {}

    def __init__(self, config: dict):
        for k, v in config.items():
            self.params[k] = v

    def get_message(self):
        msg = IpcMessage("cmd", "configure")
        msg.set_param(self.params_group, self.params)
        return msg


class XspressDetector(object):

    message_queue = []

    def __init__(self, ip: str, port: int):
        self._client = IpcClient(ip, port)

    def read_config(self):
        msg = IpcMessage("cmd", "request_configuration")
        success, reply = self._client._send_message(msg, 10.0)
        return success, reply

    def write_config(self, config_msg: XspressConfigMessage, timeout=10):
        return self._client._send_message(config_msg, timeout)

    def send_command(self, cmd_msg: XspressCommandMessage, timeout=10):
        return self._client._send_message(cmd_msg, timeout)



def main():
    params = {
        "base_ip": "192.168.0.1",
        "num_cards": 4,
        'max_channels': 36,
        "num_tf": 16384,
        "config_path": "/dls_sw/b18/epics/xspress4/xspress4.36chan_pb/settings",
        "run_flags": 2,
        "trigger_mode": XspressTriggerMode.TM_BURST,
        "frames": 121212121,
        "exposure_time": 999.20,
    }
    config_msg = XspressConfigMessage(params).get_message()
    xsp = XspressDetector("127.0.0.1", 12000)
    print(xsp.write_config(config_msg))
    print(xsp.read_config())
    print(xsp.send_command(XspressConfigMessage({"": None}).get_message()))
    # print(xsp.send_command(XspressCommandMessage({"stop": None}).get_message()))
    # print(xsp.send_command(XspressCommandMessage({"trigger": None}).get_message()))



if __name__ == '__main__':
    main()
