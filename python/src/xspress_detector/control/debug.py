import inspect

def debug_method():
    frame = inspect.stack()[1][0]
    method_name = frame.f_code.co_name
    arg_names = frame.f_code.co_varnames
    f_locals = frame.f_locals
    meta = {}
    params = ({k : v for k, v in f_locals.items() if k in arg_names})
    meta["method"] = params["self"].__class__.__name__ + f".{method_name}"
    params.pop("self")
    meta["params"] = params
    return f"Method called -> {meta}"

