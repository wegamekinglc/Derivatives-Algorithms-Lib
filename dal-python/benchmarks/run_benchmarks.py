#!/usr/bin/env python3
"""Run from any directory with a current DAL wheel or explicit build PYTHONPATH."""

from dal_benchmarks.runner import main


if __name__ == "__main__":
    raise SystemExit(main())
