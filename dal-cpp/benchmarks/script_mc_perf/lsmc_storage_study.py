#!/usr/bin/env python3
"""Run the prespecified reduced-storage-precision research matrix."""

import argparse
from pathlib import Path

from lsmc_storage_run import run_storage_profile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--profile", choices=("accuracy", "corner"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--retry-failed", action="store_true")
    arguments = parser.parse_args()
    run_storage_profile(arguments.image, arguments.profile, arguments.output, arguments.timeout, arguments.retry_failed)


if __name__ == "__main__":
    main()
