from typing import List
import dal.dal as dal


def Product_New(events_dates: List[any], events: List[str]):
    new_events_dates = []
    for d in events_dates:
        if isinstance(d, dal.Cell_):
            new_events_dates.append(d)
        else:
            new_events_dates.append(dal.Cell_(d))
    return dal.Product_New(new_events_dates, events)
