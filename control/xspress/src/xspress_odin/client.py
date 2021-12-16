import logging
import zmq
import random
import tornado

from odin_data.ipc_message import IpcMessage
from zmq.eventloop.zmqstream import ZMQStream
from zmq.utils.monitor import parse_monitor_message
from zmq.utils.strtypes import unicode, cast_bytes
from odin_data.ipc_channel import _cast_str


ENDPOINT_TEMPLATE = "tcp://{}:{}"
QUEUE_SIZE_LIMIT = 10

EVENT_MAP = {}
for name in dir(zmq):
    if name.startswith('EVENT_'):
        value = getattr(zmq, name)
        print("%21s : %4i" % (name, value))
        EVENT_MAP[value] = name


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
        self.monitor_socket = self.socket.get_monitor_socket(events=zmq.EVENT_CONNECTED | zmq.EVENT_HANDSHAKE_SUCCEEDED | zmq.EVENT_DISCONNECTED)

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
