import pytest
from xspress_detector.control.parameter_tree import (
    ValueParameter,
    ReadOnlyVirtualParameter,
    ListParameter,
    NotPuttableParameterException,
    NotSettableParameterException,
    WriteOnlyVirtualParameter,
    ValueParameter,
    XspressParameterTree,
    bound_validator,
    is_pos,
)


class CalledException(Exception):
    """We'll use this exception to verify that a callback has been called"""
    pass
def raise_exception(ex):
    raise ex

@pytest.mark.asyncio
async def test_get_virtual_parameter():
    param = ReadOnlyVirtualParameter(int, get_cb=lambda: 5)
    assert param.get() == 5
    with pytest.raises(NotSettableParameterException):
        param.set(-3)
    with pytest.raises(NotPuttableParameterException):
        await param.put(-3)

@pytest.mark.asyncio
async def test_put_virtual_parameter():
    put_callback = lambda x: 22
    param = WriteOnlyVirtualParameter(int, put_cb=put_callback, validators=[is_pos,])
    assert 22 == await param.put(1)
    with pytest.raises(TypeError):
        assert 1 == await param.put("hello")
    with pytest.raises(ValueError):
        await param.put(-1)
    with pytest.raises(NotSettableParameterException):
        await param.set(-1)

@pytest.mark.asyncio
async def test_int_value_parameter():
    param = ValueParameter(float, 0.0, put_cb=lambda: 0.0, validators=[bound_validator(0,10)])
    assert 0.0 == param.get()
    param.set(1.1)
    assert 1.1 == param.get()
    with pytest.raises(ValueError):
        param.set(99.9)
    with pytest.raises(ValueError):
        await param.put(99.9)

@pytest.mark.asyncio
async def test_str_value_parameter():
    str_validator = lambda s: True if s.startswith("h") else raise_exception(ValueError)
    param = ValueParameter(str, "hello", put_cb=lambda: print("hello"),validators=[str_validator])
    assert "hello" == param.get()
    param.set("hola")
    assert "hola" == param.get()
    with pytest.raises(ValueError):
        await param.put("bonjour")
    with pytest.raises(TypeError):
        await param.put(1)

@pytest.mark.asyncio
async def test_list_value_parameter():
    validate_element = lambda x: True if x < 22 else raise_exception(ValueError)
    list_validator = lambda l: [x for x in l if validate_element(x)]
    param = ListParameter(put_cb=lambda x: [],validators=[list_validator])
    assert [] == param.get()
    param.set([1,1,1,1])
    assert [1,1,1,1] == param.get()
    assert [] == await param.put([21])
    with pytest.raises(ValueError):
        await param.put([22])
    with pytest.raises(TypeError):
        await param.put(1)

async def dummy_async_callback(x):
    return 42

@pytest.mark.asyncio
async def test_parameter_tree():
    _tree = {
        "a": {
            "b": ValueParameter(int, 3),
            "c": ListParameter(put_cb=dummy_async_callback),
            "d": {
                "e": ValueParameter(str, "hello", put_cb=dummy_async_callback),
                "f": ListParameter(put_cb=dummy_async_callback),
            }
        }
    }
    tree = XspressParameterTree(_tree)
    assert 3 == tree.get("a/b")
    tree.set("a/c", [1,2,3,4])
    assert [1,2,3,4] == tree.get("a/c")
    assert 42 == await tree.put("a/c/0", 100)
    assert [100,2,3,4] == tree.get("a/c")
    assert "hello" == tree.get("a/d/e")
    tree.set("a/d", {"e":"bye", "f":[1,2,3]})
    assert "bye" == tree.get("a/d/e")
    assert {"e":"bye", "f":[1,2,3]} == tree.get("a/d")
    with pytest.raises(TypeError):
        tree.set("a/d", {"e":1, "f":[1,2,3]})
    with pytest.raises(TypeError):
        tree.set("a/d", {"e":"33", "f":2})



