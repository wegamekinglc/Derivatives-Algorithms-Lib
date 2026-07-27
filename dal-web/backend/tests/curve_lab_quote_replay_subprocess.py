"""Fresh-process helper for canonical quote restart tests."""

from __future__ import annotations

import json
import sys

from app.services.quote_canonicalization import (
    apply_exact_decimal_bump,
    replay_canonical_quote,
)


def main() -> int:
    payload = sys.stdin.buffer.read()
    restored = replay_canonical_quote(payload)
    result = {
        "financial": json.loads(restored.financial_bytes()),
        "raw_bumped": apply_exact_decimal_bump(
            restored.raw_quote,
            restored.exact_risk_raw_bump,
        ),
        "normalized_bumped": apply_exact_decimal_bump(
            restored.normalized_quote,
            restored.normalized_risk_bump,
        ),
    }
    sys.stdout.write(json.dumps(result, separators=(",", ":"), sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
