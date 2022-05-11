import pytest
from xspress_odin.util import XspressListEndpoints


def test_list_endpoints_input():
    with pytest.raises(ValueError):
        xle = XspressListEndpoints(3)
    with pytest.raises(ValueError):
        xle = XspressListEndpoints(4)
    with pytest.raises(ValueError):
        xle = XspressListEndpoints(6)
        xle = XspressListEndpoints(7)
        xle = XspressListEndpoints(8)
        xle = XspressListEndpoints(9)

    for i in range(11,100):
        with pytest.raises(ValueError):
            xle = XspressListEndpoints(i)


def test_list_endpoints():
    xle = XspressListEndpoints(2)
    assert xle.get_next_config() == ("192.168.0.65", [30125,30126])
    assert xle.get_next_config() == ("192.168.0.65", [30127,30128])
    xle = XspressListEndpoints(1)
    assert xle.get_next_config() == ("192.168.0.65", [30125])
    assert xle.get_next_config() == ("192.168.0.65", [30126])


def test_list_endpoints2():
    xle = XspressListEndpoints(10)
    for i in range(20):
        assert xle.get_next_config() == ("192.168.0." + str(65+i*4), list(range(30125,30135)))

def test_list_endpoints3():
    xle = XspressListEndpoints(5)
    port_mapping = []
    for i in range(8):
        ip, ports = xle.get_next_config()
        ports = [str(p) for p in ports]
        d = {"ip": ip, "ports": ",".join(ports)+","}
        port_mapping.append(d)

    port_mapping_ref = [
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

    assert port_mapping == port_mapping_ref