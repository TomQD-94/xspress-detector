import pytest
from xspress_detector.control.util import ListModeIPPortGen


def test_gen_special():
    assert ("0.0.0.0", list(range(30125, 30135))) == ListModeIPPortGen.config_for_1_process()

def test_list_endpoints_input():

    assert len(list(ListModeIPPortGen(1,10))) == 10

    with pytest.raises(ValueError):
        ListModeIPPortGen(3,10)
    with pytest.raises(ValueError):
        ListModeIPPortGen(4,10)
    with pytest.raises(ValueError):
        ListModeIPPortGen(6,10)
        ListModeIPPortGen(7,10)
        ListModeIPPortGen(8,10)
        ListModeIPPortGen(9,10)

    for i in range(11,100):
        with pytest.raises(ValueError):
            lmg = ListModeIPPortGen(i,10)


def test_list_endpoints():
    lmg = ListModeIPPortGen(2, 2)
    assert lmg.__next__() == ("192.168.0.65", [30125,30126])
    assert lmg.__next__() == ("192.168.0.65", [30127,30128])
    lmg = ListModeIPPortGen(1, 2)
    assert lmg.__next__() == ("192.168.0.65", [30125])
    assert lmg.__next__() == ("192.168.0.65", [30126])


def test_list_endpoints2():
    for i, (ip, ports) in enumerate(ListModeIPPortGen(10, 20)):
        assert (ip, ports) == ("192.168.0." + str(65+i*4), list(range(30125,30135)))

def test_list_endpoints3():
    port_mapping = \
    [
        {
            "ip" : ip,
            "ports" : ",".join([str(p) for p in ports])+","
        }
        for ip, ports in ListModeIPPortGen(5, 8)
    ]

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