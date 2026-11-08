"""P3 detached getter measurements; run against the selected build-tree package."""

import gc
import json
from time import perf_counter_ns
import tracemalloc

import dal
import _dal_pde_risk_test as native


def request(two_carry):
    today, maturity = dal.Date_(2026, 10, 18), dal.Date_(2027, 10, 18)
    dates = [dal.Date_(2027, 4, 18) if two_carry else dal.Date_(2027, 3, 4), maturity]
    contract = dal.EarlyExercisePdeContract_(today, maturity, 100.0, "PUT", "BERMUDAN", exercise_dates=dates)
    rows = [(dal.Date_(2027, 3, 4), .02, .01), (maturity, .06, .03)] if two_carry else [(maturity, .05, 0.0)]
    market = dal.EarlyExercisePdeMarket_(100.0, .2, [dal.EarlyExercisePdeCarrySegment_(*row) for row in rows])
    settings = dal.EarlyExercisePdeSettings_(upper=400.0, max_actual_steps=200000, max_work_units=1000000000,
        numeric_payload_budget_bytes=1073741824, workspace_budget_bytes=1073741824)
    risk = dal.EarlyExercisePdeRiskSettings_(max_work_units=10000000000,
        numeric_payload_budget_bytes=2147483648, workspace_budget_bytes=2147483648)
    return dal.EarlyExercisePdeRiskRequest_(dal.EarlyExercisePdeRequest_(contract, market, settings=settings), risk=risk)


def counters(result):
    e = result.execution
    return e.work_units, e.total_solves, e.numeric_payload_bytes


def copies(result, reads):
    count = 0
    for _ in range(reads):
        risks, scenarios = result.risks, result.scenarios
        count += len(risks) + len(scenarios)
        del risks, scenarios
    return count


def validate_projection(result, computed, before, records, reference, reads, expected_rows):
    if records != reference["records"] or records != reads * expected_rows:
        raise RuntimeError("projection record count drift")
    if counters(result) != before or counters(computed) != before:
        raise RuntimeError("projection native counters/payload changed")
    if (reference["work"], reference["solves"], reference["payload"]) != before:
        raise RuntimeError("native copy control counters/payload mismatch")


def main():
    for two_carry, name in ((False, "Bermudan17"), (True, "TwoCarry25")):
        r = request(two_carry)
        result = dal.EarlyExercisePdeRiskResult_New(r)
        before = counters(result)
        expected_rows = 32 if two_carry else 22
        for reads in (1, 10, 100, 1000):
            reference = native._benchmark_copy(r, reads)
            start = perf_counter_ns()
            computed = dal.EarlyExercisePdeRiskResult_New(r)
            compute_ns = perf_counter_ns() - start
            start = perf_counter_ns()
            records = copies(result, reads)
            copy_ns = perf_counter_ns() - start
            gc.collect()
            tracemalloc.start()
            copies(result, reads)
            live, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
            validate_projection(result, computed, before, records, reference, reads, expected_rows)
            print(json.dumps(dict(case=name, reads=reads, records=records, binding_compute_ns=compute_ns,
                binding_copy_ns=copy_ns, binding_incremental_ns=compute_ns + copy_ns - reference["compute_ns"] - reference["copy_ns"],
                python_traced_live_bytes=live, python_traced_peak_bytes=peak, native=reference,
                work=before[0], solves=before[1], payload=before[2])), flush=True)


if __name__ == "__main__":
    main()
