#!/usr/bin/env python3
"""Validate the interpreter behind a DAL Python local environment."""

import argparse
from pathlib import Path
import platform
import sys


SUPPORTED_MINORS = ("3.9", "3.10", "3.11", "3.12", "3.13")
SUPPORTED_RANGE = ">=3.9,<3.14"


def validate_interpreter(
    entry_point, environment, requested, implementation, version, remediation
):
    observed = "%s %s.%s" % (implementation, version[0], version[1])
    observed_minor = "%s.%s" % (version[0], version[1])
    reason = None
    if implementation != "CPython":
        reason = "requires CPython"
    elif observed_minor not in SUPPORTED_MINORS:
        reason = "requires CPython %s" % SUPPORTED_RANGE
    elif requested is not None and observed_minor != requested:
        reason = "--python requested %s" % requested
    if reason is not None:
        raise ValueError(
            "%s: environment %r uses %s but %s; %s"
            % (entry_point, str(Path(environment)), observed, reason, remediation)
        )
    return observed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--entry-point", required=True)
    parser.add_argument("--environment", type=Path, required=True)
    parser.add_argument("--requested", choices=SUPPORTED_MINORS)
    parser.add_argument("--remediation", required=True)
    args = parser.parse_args()
    try:
        observed = validate_interpreter(
            args.entry_point,
            args.environment,
            args.requested,
            platform.python_implementation(),
            sys.version_info[:2],
            args.remediation,
        )
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    print("%s: using %s from %s" % (args.entry_point, observed, args.environment))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
