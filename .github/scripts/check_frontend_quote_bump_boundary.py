#!/usr/bin/env python3
"""Keep quote-bump arithmetic on the backend side of the API boundary."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FRONTEND = ROOT / "dal-web" / "frontend" / "src"
RISK_SURFACES = (
    FRONTEND / "components" / "QuoteBumpPanel.tsx",
    FRONTEND / "pages" / "CurveRun.tsx",
    *(FRONTEND / "curves").glob("*.ts"),
    *(FRONTEND / "curves").glob("*.tsx"),
)
AUTHORING_SURFACES = (
    FRONTEND / "components" / "CurveLabQuoteAuthoring.tsx",
    FRONTEND / "curves" / "curveLabRegistry.ts",
    FRONTEND / "api" / "client.ts",
)

PROHIBITED = (
    (
        re.compile(r"\bresidual_tolerance\b"),
        "frontend quote-bump code must not divide by residual_tolerance",
    ),
    (
        re.compile(r"\beffective_inverse\s*(?:\?\.|\.)\s*values\b"),
        "frontend quote-bump code must not read effective-inverse values",
    ),
    (
        re.compile(r"\b(?:multiply|matmul|matrixMultiply)\s*\(", re.IGNORECASE),
        "frontend quote-bump code must not perform matrix multiplication",
    ),
)
PROHIBITED_AUTHORING = (
    (
        re.compile(r"\b(?:Number|parseFloat|parseInt)\s*\("),
        "Curve Lab authoring must not convert financial strings to binary numbers",
    ),
    (
        re.compile(r"(?:/\s*100\b|\*\s*100\b)"),
        "Curve Lab authoring must delegate percent/price transforms to the exact-decimal adapter",
    ),
)


def main() -> int:
    violations: list[str] = []
    for path in RISK_SURFACES:
        source = path.read_text(encoding="utf-8")
        for pattern, message in PROHIBITED:
            for match in pattern.finditer(source):
                line = source.count("\n", 0, match.start()) + 1
                violations.append(
                    f"{path.relative_to(ROOT)}:{line}: {message}"
                )
    for path in AUTHORING_SURFACES:
        source = path.read_text(encoding="utf-8")
        for pattern, message in PROHIBITED_AUTHORING:
            for match in pattern.finditer(source):
                line = source.count("\n", 0, match.start()) + 1
                violations.append(
                    f"{path.relative_to(ROOT)}:{line}: {message}"
                )
    if violations:
        raise SystemExit("\n".join(violations))
    print(
        "frontend quote-bump boundary: "
        f"{len(RISK_SURFACES)} risk and {len(AUTHORING_SURFACES)} authoring surfaces clean"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
