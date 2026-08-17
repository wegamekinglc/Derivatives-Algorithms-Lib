# -*- coding: utf-8 -*-
"""Visualize a script product with the width-aware Unicode tree dump.

``Product_DebugTree`` renders the parsed product as a human-friendly tree:
statements collapse to inline math while they fit the width budget
(``width``, default 125) and expand into box-drawing branches otherwise;
``if`` branches are marked with ``▶`` (then) / ``▷`` (else) and fuzzy
comparisons show their smoothing as ``⟨ε=0.1⟩``. ``ascii=True`` switches to
a pure-ASCII fallback for constrained consoles.

Unicode output needs a UTF-8 terminal (on Windows, Windows Terminal or
``chcp 65001``). Machine consumers should use ``Product_DebugJson`` instead.
"""

import sys

from dal import *

if sys.platform == "win32":
    #  Render the box-drawing glyphs on a UTF-8 capable Windows console
    sys.stdout.reconfigure(encoding="utf-8")

event_dates = [
    # macro definition for `BARRIER` and `STRIKE` constants
    "BARRIER",
    "STRIKE",
    # initialization
    Date_(2022, 9, 25),
    # monitor periods -- quarterly over one year keeps the demo output compact
    "START: 2022-09-25\nEND: 2023-09-25\nFREQ: 3M",
    # final payoff
    Date_(2023, 9, 25),
]
events = [
    "150.00",
    "120.00",
    "alive = 1",
    "IF spot() > BARRIER:0.1 THEN alive = 0 END",
    "IF spot() > BARRIER:0.1 THEN alive = 0 END uoc pays alive * MAX(spot() - STRIKE, 0.0)",
]

EvaluationDate_Set(Date_(2022, 9, 25))
product = Product_New(event_dates, events)

print("=== Unicode tree (width = 125) ===")
print(Product_DebugTree(product))

print("=== Unicode tree (width = 40) ===")
print(Product_DebugTree(product, width=40))

print("=== ASCII tree (width = 40) ===")
print(Product_DebugTree(product, ascii=True, width=40))
