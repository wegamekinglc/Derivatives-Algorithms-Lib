"""Add immutable scalar strike/time coordinates to the frozen request."""

import argparse
import json
from pathlib import Path

import mpmath as mp

from qualify import MATURITIES
from reference import RATIOS, carry


def prepare(request):
    with mp.workdps(90):
        for fixture in request["fixtures"]:
            cases = []
            for time in MATURITIES:
                forward = carry(mp.mpf(time), fixture["flat"])[0]
                cases.append(
                    dict(
                        time=repr(time),
                        strikes=[
                            mp.nstr(forward * mp.mpf(ratio), 90) for ratio in RATIOS
                        ],
                    )
                )
            fixture["mc_cases"] = cases
    return request


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("request", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(
        json.dumps(prepare(json.loads(args.request.read_text())), indent=2) + "\n",
        encoding="utf-8",
    )
