"""Extract the already qualified independent M3 value controls; no new oracle run."""

import argparse
from decimal import Decimal, localcontext
import hashlib
import json
from pathlib import Path


def literal(value):
    return f"{Decimal(value):.30e}L"


def generate(gate_path):
    raw = gate_path.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == GATE_SHA256
    records = json.loads(raw)
    lines = ["#pragma once", "", "#include <array>", "", "namespace Dal::TestXccyExecution {",
             "    struct Expectation_ {", "        const char* mode_;", "        bool foreign_, receive_, zero_, nonflat_;",
             "        std::array<std::array<long double, 4>, 4> quantities_;", "    };",
             "    inline const auto& Expectations() {", "        static const std::array<Expectation_, 30> result{{"]
    for record in records:
        identity = record["id"]
        if "synthetic" in identity or identity.endswith(("small", "zero-a", "fx-only")):
            continue
        mode, leg, direction, *scenario = identity.split("-")
        flags = [leg == "foreign", direction == "receive", scenario[0] == "zero", "nonflat" in scenario]
        lines.append('            {"' + mode + '", ' + ", ".join(str(flag).lower() for flag in flags) + ", {{")
        for value in record["values"]:
            replay = abs(Decimal(value["wide"][1]) - Decimal(value["wide"][0]))
            data = [value["oracle"][2], value["e_oracle"], value["wide"][1], replay]
            lines.append("                {{" + ", ".join(literal(x) for x in data) + "}},")
        lines.append("            }}},")
    lines += ["        }};", "        return result;", "    }", "} // namespace Dal::TestXccyExecution", ""]
    return "\n".join(lines)


GATE_SHA256 = "0cabea620823b1b9c061ccae69df61251d9d41b3b0cd4f90baba17791e680d97"

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gate", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    with localcontext() as context:
        context.prec = 100
        arguments.output.write_text(generate(arguments.gate), encoding="utf-8")
