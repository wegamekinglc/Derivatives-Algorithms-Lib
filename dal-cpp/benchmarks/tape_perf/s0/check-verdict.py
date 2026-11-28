import argparse
import csv
import json
from collections import Counter
from itertools import product
from pathlib import Path
from unittest.mock import patch

from collect import require, run_timing
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
        profiles = (("comparison", timing), ("aa", noise))
        for (kind, profile), group, p, batch in product(profiles, GROUPS, range(1, 11), range(1, 21)):
            if (round_, kind, group, p, batch) == omitted:
                continue
            x = profile[int(batch > 18)] if group == TARGET else 100
            first = "B" if (round_ + p + batch) % 2 else "A"
            yield dict(zip(P1_FIELDS, (kind, *group, round_, p, batch, first, GROUPS[group], x, 100), strict=True))


def p3_rows():
    for case, ratios in {"equal": (104, 104), "cross-a": (105, 103), "cross-b": (103, 105), "sustained": (105, 106), "missing": (105, 106)}.items():
        for r, value in enumerate(ratios, 1):
            for p in range(1, 11):
                if case == "missing" and (r, p) == (2, 10):
                    continue
                spread = p - 1
                yield dict(zip(P3_FIELDS, (case, r, p, "candidate" if (p + r) % 2 else "baseline", value + spread, 100 + 4 * spread),
                               strict=True))


def check_confirmation_run(path, rows):
    logs = {}
    fields = ("kind", "case", "directions", "batch", "first", "calls", "a_ns", "b_ns")
    for row in rows:
        scope = "dupire" if row["case"] == "dupire_quote" else "vanilla"
        name = f"r{row['round']}-p{int(row['process']):02d}-{row['kind']}-{scope}"
        logs.setdefault(name, []).append("s0_pair," + ",".join(str(row[c]) for c in fields))
    calls = Counter()
    path.mkdir(parents=True, exist_ok=True)

    def run(name, *_args):
        if name.startswith("verify-"):
            return ""
        r = int(name.split("-", 1)[0][1:])
        if r == 3 and not calls[r]:
            (path / "first-two-rounds.csv").write_bytes((path / "paired.csv").read_bytes())
            (path / "first-two-verdict.json").write_bytes((path / "verdict.json").read_bytes())
        calls[r] += 1
        return "\n".join(logs.get(name, ()))

    with patch("collect.output", return_value=""):
        run_timing(run, path, 3)
    require(calls == {1: 40, 2: 40, 3: 40}, calls)
    pending = json.loads((path / "first-two-verdict.json").read_text())
    require(pending["needs_confirmation"] and pending["selected_rounds"] == [], pending)
    final = json.loads((path / "verdict.json").read_text())
    require(not final["needs_confirmation"], final)
    return pending, final


def check_p1_fixtures(out, flat):
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
        rows = write(out / f"{name}.csv", P1_FIELDS, p1_rows(timing, aa, omitted))
        result = judge_p1(rows)
        require((result["verdict"], result["selected_rounds"], result["needs_confirmation"]) == (verdict, selection, third), name)
        if name == "alternate-first-two":
            group = f"{TARGET[0]}/{TARGET[1]}"
            require(result["attempts"]["1,2"][group] == "unconfirmed", result)
            require(result["rounds"][1]["ratios"][group]["comparison"] == ("1.11", "1.14"), result)
            require(result["rounds"][2]["ratios"][group]["comparison"] == ("1.09", "1.16"), result)
        results[name] = result
    return results


def check_p1_sampler(out, flat):
    empty = [row for row in p1_rows([flat] * 3, [flat] * 3) if row["round"] != 2]
    pending, final = check_confirmation_run(out / "empty-second-round", empty)
    require(pending["rounds"]["2"]["eligible"] is False and pending["rounds"]["2"]["ratios"] == {}, pending)
    require(final["rounds"]["1"]["eligible"] and final["rounds"]["3"]["eligible"], final)
    require(final["verdict"] == "insufficient" and final["selected_rounds"] == [], final)
    first_only = judge_p1([row for row in empty if row["round"] == 1], attempted_rounds=3)
    require(not first_only["needs_confirmation"], first_only)
    return final


def check_duplicate_sampler(out, flat):
    duplicate = list(p1_rows([flat] * 3, [(94, 95), flat, flat]))
    # Replay the reviewer's A/A scenario with an additional copy of the first pair.
    duplicate.insert(1, duplicate[0])
    pending, final = check_confirmation_run(out / "duplicate-pair", duplicate)
    require(any("duplicate pair" in reason for reason in pending["rounds"]["1"]["reasons"]), pending)
    require(final["verdict"] == "pass" and final["selected_rounds"] == [2, 3], final)
    quiet = [dict(row, a_ns=100) if row["kind"] == "aa" else row for row in duplicate]
    require(judge_p1(quiet)["selected_rounds"] == [2, 3], "duplicate sample admitted in quiet round")
    return final


def check_p3_fixtures(out):
    rows = write(out / "first-order.csv", P3_FIELDS, p3_rows())
    result = judge_p3(rows, cases=("unseen",))
    expected = {"equal": "pass", "cross-a": "pass", "cross-b": "pass", "sustained": "regression", "missing": "insufficient", "unseen": "insufficient"}
    require({case: r["verdict"] for case, r in result.items()} == expected, result)
    shuffled = sorted(rows, key=lambda row: int(row["process"]) % 2)
    require(judge_p3(shuffled, cases=("unseen",)) == result, "P3 result depends on CSV order")
    unordered = write(out / "first-order-no-alternation.csv", P3_FIELDS,
                      (dict(row, first="candidate") for row in rows if row["case"] == "sustained"))
    no_alternation = judge_p3(unordered)
    require(no_alternation["sustained"]["verdict"] == "insufficient", no_alternation)
    return result, no_alternation


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    flat = (100, 100)
    results = check_p1_fixtures(args.output, flat)
    first_order, no_alternation = check_p3_fixtures(args.output)
    results["first-order"] = first_order
    results["empty-second-round"] = check_p1_sampler(args.output, flat)
    results["duplicate-pair"] = check_duplicate_sampler(args.output, flat)
    results["first-order-no-alternation"] = no_alternation
    (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(f"{len(results)} synthetic CSV checks passed")


if __name__ == "__main__":
    main()
