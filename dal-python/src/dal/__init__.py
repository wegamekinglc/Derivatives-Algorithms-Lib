# -*- coding: utf-8 -*-

# Import the C extension first to avoid circular imports when dal.py
# executes "from . import _dal" during package initialization.
from . import _dal  # noqa: F401
from .dal import *
from .api import *

__author__ = 'The Derivatives Algorithms Group'
__email__ = 'wegamekinglc@hotmail.com'
__version__ = "2026.8.11"
