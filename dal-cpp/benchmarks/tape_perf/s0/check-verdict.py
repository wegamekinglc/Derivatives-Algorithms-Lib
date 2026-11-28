import argparse
import csv
import json
from pathlib import Path

from verdict import GROUPS, P1_FIELDS, P3_FIELDS, judge_p1, judge_p3


TARGET = ("conditional_basket", 6)


def write(path, fields, rows):
    with path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fields)
        writer.writeheader()
        writer.writerows(rows)
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def p1_rows(compare, aa, omitted=None):
    for round_, (timing, noise) in enumerate(zip(compare, aa, strict=True), 1):
        for kind, profile in (("comparison", timing), ("aa", noise)):
            for case, directions in GROUPS:
                for p in range(1, 11):
                    for batch in range(1, 21):
                        if (round_, kind, (case, directions), p, batch) == omitted:
                            continue
                        x = profile[0 if batch <= 18 else 1] if (case, directions) == TARGET else 100
                        yield dict(zip(P1_FIELDS, (kind, case, directions, round_, p, batch, "B" if (round_ + p + batch) % 2 else "A",
                                                   GROUPS[(case, directions)], x, 100), strict=True))


def p3_rows():
    for case, ratios in {"equal": (104, 104), "cross-a": (105, 103), "cross-b": (103, 105), "sustained": (105, 106), "missing": (105, 106)}.items():
        for r, value in enumerate(ratios, 1):
            for p in range(1, 11):
                if case == "missing" and (r, p) == (2, 10):
                    continue
                spread = p - 1
                yield dict(zip(P3_FIELDS, (case, r, p, "candidate" if (p + r) % 2 else "baseline", value + spread, 100 + 4 * spread),
                               strict=True))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    flat = (100, 100)
    alt1, alt2 = (111, 114), (109, 116)
    fixtures = {
        "equality": ([(110, 115)] * 2, [(104, 110)] * 2, None, "pass", [1, 2], False),
        "noise-symmetric": ([flat, flat, flat], [(94, 95), flat, flat], None, "pass", [2, 3], False),
        "bad-middle": ([flat, flat, flat], [flat, (104, 111), flat], None, "insufficient", [], False),
        "missing-pair": ([flat, flat], [flat, flat], (1, "comparison", TARGET, 10, 20), "insufficient", [], True),
        "alternate-first-two": ([alt1, alt2], [flat, flat], None, "insufficient", [], True),
        "alternate-confirmed": ([alt1, alt2, alt2], [flat] * 3, None, "fail", [2, 3], False),
        "alternate-unconfirmed": ([alt1, alt2, alt1], [flat] * 3, None, "insufficient", [], False),
        "pass-versus-fail": ([flat, alt2, alt2], [flat] * 3, None, "fail", [2, 3], False),
        "sustained-first-two": ([alt1, alt1], [flat, flat], None, "fail", [1, 2], False),
    }
    results = {}
    for name, (timing, aa, omitted, verdict, selection, third) in fixtures.items():
        rows = write(args.output / f"{name}.csv", P1_FIELDS, p1_rows(timing, aa, omitted))
        result = judge_p1(rows)
        assert (result["verdict"], result["selected_rounds"], result["needs_confirmation"]) == (verdict, selection, third), name
        if name == "alternate-first-two":
            group = f"{TARGET[0]}/{TARGET[1]}"
            assert result["attempts"]["1,2"][group] == "unconfirmed"
            assert result["rounds"][1]["ratios"][group]["comparison"] == ("1.11", "1.14")
            assert result["rounds"][2]["ratios"][group]["comparison"] == ("1.09", "1.16")
        results[name] = result
    rows = write(args.output / "first-order.csv", P3_FIELDS, p3_rows())
    result = judge_p3(rows, cases=("unseen",))
    assert {case: r["verdict"] for case, r in result.items()} == {
        "equal": "pass", "cross-a": "pass", "cross-b": "pass", "sustained": "regression", "missing": "insufficient", "unseen": "insufficient",
    }
    results["first-order"] = result
    (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(f"{len(results)} synthetic CSV checks passed")


if __name__ == "__main__":
    main()
