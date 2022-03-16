import logging
import zmq
import random
import tornado
import asyncio

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

class AsyncXspressClient:
    pass


class AbstractClient:
    MESSAGE_ID_MAX = 2**32

    def __init__(self, ip, port, callback):
        self.ip = ip
        self.port = port
        self.endpoint = ENDPOINT_TEMPLATE.format(ip, port)
        self.connected = False
        self.message_id = 0
        self.queue_size = 0

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

    def get_id(self):
        id = self.message_id
        self.message_id = (self.message_id + 1) % self.MESSAGE_ID_MAX
        return id

    def is_connected(self):
        return self.connected

class AsyncClient(AbstractClient):

    def __init__(self, ip, port):
        self.send_buffer = {}
        self.recv_buffer = {}

        super().__init__(ip, port, self._on_recv_callback)

    async def send_recv(self, msg: IpcMessage, timeout=1):
        if not self.connected:
            raise ConnectionError(f"AsyncClient.send_recv: Failed to send msg {id}. ZMQ socket not connected.\nmessage: {msg.encode()}")
        id = self.get_id()
        msg.set_msg_id(id)
        self.send_buffer[id] = msg
        logging.info("Sending message:\n%s", msg.encode())
        self.stream.send(cast_bytes(msg.encode()))

        time_elapsed = 0
        time_to_sleep = 0.001
        while time_elapsed < timeout:
            await asyncio.sleep(time_to_sleep)
            time_elapsed +=time_to_sleep
            time_to_sleep *= 2
            if id in self.recv_buffer:
                self.send_buffer.pop(id)
                return self.recv_buffer.pop(id)
        self.send_buffer.pop(id)
        raise TimeoutError(f"AsyncClient.send_recv timed out on message: {id}.")

    def _on_recv_callback(self, msg):
        data = _cast_str(msg[0])
        logging.debug(f"message recieved = {data}")
        ipc_msg : IpcMessage = IpcMessage(from_str=data)
        id = ipc_msg.get_msg_id()
        if id in self.send_buffer: # else it's already timed out 
            self.recv_buffer[id] = ipc_msg

class CallbackClient(AbstractClient):

    def __init__(self, ip, port, callback):
        super().__init__(ip, port, callback)

    def send(self, msg: IpcMessage):
        n_flushed = self.stream.flush() # This is probably not needed
        logging.debug("number of events flushed: {}".format(n_flushed))
        msg.set_msg_id(self.get_id())
        logging.info("Sending message:\n%s", msg.encode())
        self.stream.send(cast_bytes(msg.encode()))
        logging.error(f"queue size = {self.queue_size}")
