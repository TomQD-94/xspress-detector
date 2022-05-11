
class XspressListEndpoints:
    IP_PREFIX = "192.168.0."
    BASE_IP = 65
    IP_STEP = 4
    BASE_PORT = 30125
    MAX_PORT = 30134
    NUM_PORTS_PER_IP = 10

    def __init__(self, num_endpoint):
        self.ip_last = self.BASE_IP
        self.port = self.BASE_PORT
        if self.NUM_PORTS_PER_IP % num_endpoint != 0:
            raise ValueError(f"num_endpoints must be a factor of {self.NUM_PORTS_PER_IP}")
        self.n = num_endpoint
        self.current_count = 0
    
    def get_next_config(self):
        ip = self.IP_PREFIX + str(self.ip_last)
        ports = []
        for i in range(self.n):
            port_number = self.BASE_PORT + self.current_count
            if port_number > self.MAX_PORT:
                raise ValueError(f"logic bug. port number can't be bigger than {self.MAX_PORT}")
            ports.append(self.BASE_PORT + self.current_count)
            self.current_count += 1
        if self.current_count  >= self.NUM_PORTS_PER_IP:
            self.ip_last += self.IP_STEP
        self.current_count =  self.current_count % self.NUM_PORTS_PER_IP
        return ip, ports


            
