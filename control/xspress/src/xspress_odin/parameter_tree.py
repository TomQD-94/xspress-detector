
import inspect
from functools import partial, wraps
from typing import Callable, List, Union

VALIDATE = True

def is_coroutine(object):
    """
    see: https://stackoverflow.com/questions/67020609/partial-of-a-class-coroutine-isnt-a-coroutine-why
    """
    while isinstance(object, partial):
        object = object.func
    return inspect.iscoroutinefunction(object)

class ParameterException(Exception):
    pass

class NotGettableParameterException(ParameterException):
    pass

class NotPuttableParameterException(ParameterException):
    pass

class NotSettableParameterException(ParameterException):
    pass

class ParameterInterface:
    def __init__(self, *args):
        raise ValueError(f"{self.__class__.__name__} is an interface and cannot be instantiated")
    def get(self): # -> Any | raises NotGettableException
        raise NotImplementedError()
    def set(self, value): # -> None | rasies NotSettableException
        raise NotImplementedError()
    async def put(self, *args): # -> bool, json_string | raises NotPuttableException
        raise NotImplementedError()


def is_pos(value) -> None:
    if value < 0:
        raise ValueError("Value must be positive")

def bound_validator(_min, _max) -> Callable[[float], None]:
    def validator(value):
        if value <= _min or value >= _max:
            raise ValueError(f"Value must be between {_min} and {_max}. Value = {value} was given")
    return validator

def type_validator(_type) -> Callable[[type], None]:
    def validator(value):
        if not isinstance(value, _type):
            raise TypeError(f"Value {value} is not instanse of {_type}")
    return validator

def assert_gettable(func):
    @wraps(func)
    def wrap(*args, **kwargs):
        _self = args[0]
        if _self.get_cb is None:
            raise NotGettableParameterException()
        return func(*args, **kwargs)
    return wrap

def assert_settable(func):
    @wraps(func)
    def wrap(*args, **kwargs):
        _self = args[0]
        if _self.set_cb is None:
            raise NotSettableParameterException()
        return func(*args, **kwargs)
    return wrap

def assert_puttable(func):
    @wraps(func)
    async def wrap(*args, **kwargs):
        _self = args[0]
        if _self.put_cb is None:
            raise NotPuttableParameterException()
        return await func(*args, **kwargs)
    return wrap

def validate(func):
    @wraps(func)
    def wrap(*args, **kwargs):
        _self = args[0]
        val_args = args[1:]
        if _self.validators and VALIDATE:
            for validate in _self.validators:
                validate(*val_args, **kwargs)
        return func(*args, **kwargs)
    return wrap

def async_validate(func):
    @wraps(func)
    async def wrap(*args, **kwargs):
        _self = args[0]
        val_args = args[1:]
        if _self.validators and VALIDATE:
            for validate in _self.validators:
                validate(*val_args, **kwargs)
        return await func(*args, **kwargs)
    return wrap


class VirtualParameter:
    def __init__(self, param_type, get_cb=None, set_cb=None, put_cb=None, validators: List[Callable]=None):
        self.param_type = param_type
        self.get_cb = get_cb
        self.set_cb = set_cb
        self.put_cb = put_cb
        self.validators = [type_validator(param_type)] if validators is None else [type_validator(param_type)] + validators

    @assert_gettable
    def get(self):
        return self.get_cb()


    @assert_settable
    @validate
    def set(self, value):
        self.set_cb(value)

    @assert_puttable
    @async_validate
    async def put(self, value):
        if is_coroutine(self.put_cb):
            return await self.put_cb(value)
        else:
            return self.put_cb(value)



class GetVirtualParameter(VirtualParameter):
    def __init__(self, param_type, get_cb):
        super().__init__(param_type, get_cb=get_cb)


class GetSetVirtualParameter(VirtualParameter):
    def __init__(self, param_type, get_cb, set_cb):
        super().__init__(param_type, get_cb=get_cb, set_cb=set_cb)


class PutVirtualParameter(VirtualParameter):
    def __init__(self, param_type, put_cb, validators=None):
        super().__init__(param_type, put_cb=put_cb, validators=validators)


class ValueParameter(VirtualParameter):
    def __init__(self, param_type, initial_value, put_cb=None, validators: List[Callable]=None):
        super().__init__(param_type, get_cb=self._get, set_cb=self._set, put_cb=put_cb, validators=validators)
        type_validator(param_type)(initial_value)
        self.value = initial_value

    def _get(self):
        return self.value

    def _set(self, value):
        self.value = value


class GetSetValueParameter(ValueParameter):
    def __init__(self, param_type, initial_value):
        super().__init__(param_type, initial_value)

class ListParameter(ValueParameter):
    def __init__(self, put_cb=None, validators: List[Callable]=None):
        super().__init__(list, [], put_cb=put_cb, validators=validators)

    async def put_element(self, index, value):
        if self.put_cb is None:
            raise NotPuttableParameterException()
        temp = self.value.copy()
        temp[index] = value
        resp =  await self.put_cb(temp) # Raises Exception if operation was unsuccessful so we don't set self.value.
        self.value = temp
        return resp


def split_path(path_str):
    return path_str.strip("/").split("/")

class XspressParameterTree:
    def __init__(self, tree):
        self.tree = tree

    def get(self, path):
        result =  self._resolve_path(path)
        return self._unroll(result)

    def _unroll(self, tree: Union[dict, VirtualParameter]):
        if isinstance(tree, VirtualParameter):
            return tree.get()
        else:
            return {k: self._unroll(v) for k, v in tree.items()}

    def set(self, path, value):
        result = self._resolve_path(path)
        self._roll(result, value)

    def _roll(self, tree, param_val):
        if isinstance(tree, VirtualParameter) and not isinstance(param_val, dict):
            tree.set(param_val)
        elif isinstance(tree, dict) and isinstance(param_val, dict):
            for k , v in param_val.items():
                self._roll(tree[k], v)
        else:
            raise ValueError("input does is not correct type")
        

    async def put(self, path, value):
        tokens = split_path(path)
        args = (value)
        is_list = False
        if tokens[-1].isdigit():
            is_list = True
            index = int(tokens[-1])
            args = (index, value)
        parameter = self._resolve_path(path)
        if is_list:
            return await parameter.put_element(*args)
        else:
            return await parameter.put(*args)

    def _resolve_path(self, path: str) -> Union[VirtualParameter, ListParameter, dict]:
        tokens = split_path(path)
        if tokens[-1].isdigit(): # disregard index if parameter is a ListParameter
            tokens = tokens[:-1]
        parameter = self.tree
        for token in tokens:
            parameter = parameter[token]
        return parameter