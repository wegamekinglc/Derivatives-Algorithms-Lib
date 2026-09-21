"""Shared setup for the benchmark and comparison test suites."""

from pathlib import Path
import sys

# Make dal_benchmarks/ and dal_comparisons/ importable without installing them.
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
