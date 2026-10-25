"""Generate the frozen three-row cashflow reference, independently of DAL."""

import argparse
import hashlib
import importlib.util
import json
from datetime import date
from pathlib import Path

import mpmath as mp


class Terms:
    def __init__(self, terms=()):
        self.terms = list(terms)

    def __add__(self, other):
        if other == 0:
            return self
        return Terms(self.terms + other.terms)

    __radd__ = __add__

    def __neg__(self):
        return self * -1

    def __sub__(self, other):
        return self + -other

    def __mul__(self, weight):
        return Terms([value * weight for value in self.terms])

    __rmul__ = __mul__

    def __truediv__(self, other):
        return sum(self.terms) / sum(other.terms)


def load_oracle(path):
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != "fee518f156894ada3b9deee48b10714feccf925583112b06c95b97ed22675fab":
        raise ValueError(f"Unexpected independent oracle identity: {digest}")
    spec = importlib.util.spec_from_file_location("independent_cashflows", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def cashflows(oracle, case, target):
    class RecordingEngine(oracle.Engine):
        def expect(self, exponent):
            return Terms([super().expect(exponent)])

    columns = []
    for column in range(3):
        inputs = list(map(oracle.number, case["inputs"]))
        inputs[3:6] = [oracle.Dual(oracle.number(x), int(i == column)) for i, x in enumerate(target)]
        columns.append(oracle.oracle(RecordingEngine(case, inputs))[:2])
    terms = []
    for quantity in range(2):
        for row, value in enumerate(columns[0][quantity].terms):
            passive = oracle.val(value)
            loads = []
            for column in columns:
                active = column[quantity].terms[row]
                derivative = active.d if isinstance(active, oracle.Dual) else 0
                slope = derivative / passive
                loads.append(mp.mpf(0) if abs(slope) < mp.mpf("1e-60") else slope)
            terms.append((quantity == 0, passive, loads))
    return terms


def generate(oracle, cases, precision):
    with mp.workdps(precision):
        target = [-.003 * (date.fromisoformat(d) - date(2026, 10, 21)).days / 365.
                  for d in ("2027-11-08", "2028-05-08", "2028-11-08")]
        lines = ["#pragma once", "", "#include <array>", "#include <vector>", "",
                 "namespace Dal::TestXccyRecovery {",
                 "    struct Term_ { bool annuity_; long double weight_; std::array<long double, 3> load_; };",
                 "    inline const std::array<std::vector<Term_>, 3>& Cashflows() {",
                 "        static const std::array<std::vector<Term_>, 3> rows{{"]
        for case in cases[24:27]:
            lines.append("            {")
            for annuity, weight, loads in cashflows(oracle, case, target):
                numbers = [mp.nstr(x, 30) + "L" for x in (weight, *loads)]
                lines.append("                {" + str(annuity).lower() + ", " + numbers[0] + ", {" + ", ".join(numbers[1:]) + "}},")
            lines.append("            },")
        lines.extend(["        }};", "        return rows;", "    }", "} // namespace Dal::TestXccyRecovery", ""])
        return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--oracle", type=Path, required=True)
    parser.add_argument("--cases", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    oracle = load_oracle(args.oracle)
    if hashlib.sha256(args.cases.read_bytes()).hexdigest() != "66bdba1c05ebe30dea4aa42eeb92d376a8d3fffd5744e15eba233d88d68fb03b":
        raise ValueError("Unexpected frozen cashflow input identity")
    cases = json.loads(args.cases.read_text())
    low, high = (generate(oracle, cases, precision) for precision in (65, 85))
    if low != high:
        raise ValueError("Independent coefficient refinement did not stabilize")
    args.output.write_text(high)


if __name__ == "__main__":
    main()
