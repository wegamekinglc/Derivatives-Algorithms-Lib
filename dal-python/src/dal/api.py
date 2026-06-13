from . import dal as _bindings


def Product_New(events_dates: list, events: list[str]):
    wrapped = [d if isinstance(d, _bindings.Cell_) else _bindings.Cell_(d) for d in events_dates]
    return _bindings.Product_New(wrapped, events)
