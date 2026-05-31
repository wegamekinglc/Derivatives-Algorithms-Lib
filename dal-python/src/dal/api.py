from . import dal as _swig


def Product_New(events_dates: list, events: list[str]):
    wrapped = [d if isinstance(d, _swig.Cell_) else _swig.Cell_(d) for d in events_dates]
    return _swig.Product_New(wrapped, events)
