import argparse
import csv
import json
import math
import statistics
from decimal import Decimal


GROUPS = {
    ("black_scholes", 1): 256, ("black_scholes", 3): 256,
    ("conditional_basket", 1): 16, ("conditional_basket", 6): 16,
    ("dupire_quote", 1): 1, ("dupire_quote", 3): 1,
}
P1_FIELDS = ("kind", "case", "directions", "round", "process", "batch", "first", "calls", "a_ns", "b_ns")
P3_FIELDS = ("case", "round", "process", "first", "candidate_ns", "baseline_ns")
P1_LIMIT = (Decimal("1.10"), Decimal("1.15"))
AA_LIMIT = (Decimal("1.04"), Decimal("1.10"))
P3_LIMIT = Decimal("1.04")


def duration(value):
    result = Decimal(value)
    if not result.is_finite() or result <= 0:
        raise ValueError(f"invalid duration {value}")
    return result


def quantiles(samples):
    return statistics.median(samples), sorted(samples)[math.ceil(len(samples) * 0.95) - 1]


def ratios(pairs, symmetric):
    lhs, rhs = zip(*pairs)
    values = list(zip(quantiles(lhs), quantiles(rhs)))
    if symmetric:
        return tuple(max(a, b) / min(a, b) for a, b in values)
    return tuple(a / b for a, b in values)


def p1_group(row, round_number):
    group = row["case"], int(row["directions"])
    kind = row["kind"]
    if round_number not in (1, 2, 3) or kind not in ("aa", "comparison") or group not in GROUPS:
        raise ValueError(f"unexpected P1 row {row}")
    return kind, group


def read_p1_tables(rows, slots):
    rounds, faults = {}, {}
    for row in rows:
        r, process, batch = (int(row[c]) for c in ("round", "process", "batch"))
        kind, group = p1_group(row, r)
        table = rounds.setdefault(r, {}).setdefault((kind, group), {})
        if (process, batch) not in slots:
            raise ValueError(f"unexpected pair {row}")
        if (process, batch) in table:
            faults.setdefault(r, []).append(f"{kind} {group[0]}/{group[1]}: duplicate pair at process {process}, batch {batch}")
            continue
        if row["first"] not in ("A", "B") or int(row["calls"]) != GROUPS[group]:
            raise ValueError(f"unexpected order or batch size {row}")
        table[process, batch] = row["first"], (duration(row["a_ns"]), duration(row["b_ns"]))
    return rounds, faults


def pair_error(table, slots, processes):
    if table.keys() != slots:
        return "incomplete pairs"
    if any(table[p, b][0] == table[p, b - 1][0] for p, b in slots if b > 1):
        return "order did not alternate"
    if any(table[p, 1][0] == table[p - 1, 1][0] for p in range(2, processes + 1)):
        return "process order did not alternate"
    return None


def exceeds(values, limits):
    return tuple(value > limit for value, limit in zip(values, limits, strict=True))


def summarize_p1_round(tables, faults, slots, processes):
    invalid, results = list(faults), {}
    for group in GROUPS:
        group_name = f"{group[0]}/{group[1]}"
        for kind in ("aa", "comparison"):
            table = tables.get((kind, group), {})
            error = pair_error(table, slots, processes)
            if error:
                invalid.append(f"{kind} {group_name}: {error}")
                continue
            aa = kind == "aa"
            values = ratios([pair for _, pair in table.values()], aa)
            results.setdefault(group_name, {})[kind] = tuple(str(x) for x in values)
            if aa and any(exceeds(values, AA_LIMIT)):
                invalid.append(f"A/A {group_name}: percentile noise exceeds limit")
    return {"eligible": not invalid, "reasons": invalid, "ratios": results}


def p1_rounds(rows, processes=10, batches=20, *, attempted_rounds=None):
    slots = {(p, b) for p in range(1, processes + 1) for b in range(1, batches + 1)}
    rounds, faults = read_p1_tables(rows, slots)
    observed_rounds = max(rounds, default=0)
    if attempted_rounds is None:
        attempted_rounds = observed_rounds
    elif not observed_rounds <= attempted_rounds <= 3:
        raise ValueError("attempted rounds must include the observed rounds and stay within the three-round limit")
    return {
        r: summarize_p1_round(rounds.get(r, {}), faults.get(r, ()), slots, processes)
        for r in range(1, attempted_rounds + 1)
    }


def comparison_state(first, second):
    a = exceeds(map(Decimal, first), P1_LIMIT)
    b = exceeds(map(Decimal, second), P1_LIMIT)
    if not (any(a) or any(b)):
        return "pass"
    if any(x and y for x, y in zip(a, b, strict=True)):
        return "fail"
    return "unconfirmed"


def judge_p1(rows, processes=10, batches=20, *, attempted_rounds=None):
    rounds = p1_rounds(rows, processes, batches, attempted_rounds=attempted_rounds)
    attempts = {}
    for first, second in ((1, 2), (2, 3)):
        if not all(rounds.get(r, {}).get("eligible", False) for r in (first, second)):
            continue
        a, b = rounds[first]["ratios"], rounds[second]["ratios"]
        group_states = {group: comparison_state(timing["comparison"], b[group]["comparison"]) for group, timing in a.items()}
        attempts[f"{first},{second}"] = group_states
        if "unconfirmed" not in group_states.values():
            return {"verdict": "fail" if "fail" in group_states.values() else "pass", "selected_rounds": [first, second],
                    "needs_confirmation": False, "rounds": rounds, "attempts": attempts}
    return {"verdict": "insufficient", "selected_rounds": [], "needs_confirmation": 2 in rounds and 3 not in rounds,
            "rounds": rounds, "attempts": attempts}


def read_p3_tables(rows, processes):
    tables = {}
    for row in rows:
        r, p = int(row["round"]), int(row["process"])
        table = tables.setdefault((row["case"], r), {})
        if r not in (1, 2) or not 1 <= p <= processes or p in table or row["first"] not in ("candidate", "baseline"):
            raise ValueError(f"duplicate or unexpected P3 pair {row}")
        table[p] = row["first"], duration(row["candidate_ns"]), duration(row["baseline_ns"])
    return tables


def p3_complete(round_, processes):
    return len(round_) == processes and all(round_[p][0] != round_[p - 1][0] for p in range(2, processes + 1))


def p3_minimum_ratio(round_):
    _, candidate, baseline = zip(*round_.values())
    return min(candidate) / min(baseline)


def p3_summary(rounds, processes):
    if not all(p3_complete(r, processes) for r in rounds):
        return {"verdict": "insufficient", "round_minimum_ratios": []}
    ratios_by_round = [p3_minimum_ratio(r) for r in rounds]
    return {"verdict": "regression" if all(x > P3_LIMIT for x in ratios_by_round) else "pass",
            "round_minimum_ratios": list(map(str, ratios_by_round))}


def judge_p3(rows, processes=10, cases=()):
    tables = read_p3_tables(rows, processes)
    result = {}
    for case in sorted(set(cases) | {case for case, _ in tables}):
        rounds = [tables.get((case, r), {}) for r in (1, 2)]
        result[case] = p3_summary(rounds, processes)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("rule", choices=("p1", "p3"))
    parser.add_argument("csv")
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--attempted-rounds", type=int, choices=range(1, 4),
                        help="P1 replay: number of rounds attempted, including rounds with no samples")
    args = parser.parse_args()
    if args.rule == "p3" and args.attempted_rounds is not None:
        parser.error("--attempted-rounds applies to P1")
    with open(args.csv, newline="") as source:
        rows = list(csv.DictReader(source))
    result = judge_p1(rows, attempted_rounds=args.attempted_rounds) if args.rule == "p1" else judge_p3(rows, cases=args.case)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
