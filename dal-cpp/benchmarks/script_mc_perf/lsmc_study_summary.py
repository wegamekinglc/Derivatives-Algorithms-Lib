"""Conditional paired pricing uncertainty and observed training dispersion."""

from collections import defaultdict
import hashlib
import itertools
import json
import statistics

from lsmc_study_data import risk_vector
from lsmc_study_support import paired_difference


def policy_fingerprint(record):
    fitted = [{key: value for key, value in event.items() if key != "exercise_rate"} for event in record["policy"]]
    encoded = json.dumps(fitted, sort_keys=True, allow_nan=False, separators=(",", ":"))
    return hashlib.sha256(encoded.encode()).hexdigest()


def index_observations(observations):
    indexed, axes, policies, mode_prices = {}, {}, defaultdict(list), defaultdict(dict)
    for job, record in observations:
        key = (job.case, job.mode, job.training, job.training_seed, job.pricing_seed)
        if key in indexed:
            raise ValueError("duplicate study observation")
        indexed[key] = record
        names = tuple(record["risk_names"])
        if job.case in axes and axes[job.case] != names:
            raise ValueError("inconsistent ordered risk axes")
        axes[job.case] = names
        policies[key[:-1]].append(record)
        mode_prices[(job.case, job.training, job.training_seed, job.pricing_seed)][job.mode] = record["pv"]
    if not indexed or any(set(values) != {"frozen", "retrained"} or len(set(values.values())) != 1 for values in mode_prices.values()):
        raise ValueError("missing mode or policy-risk modes disagree on price")
    return indexed, axes, policies


def policy_groups(policies):
    result = []
    for (case, mode, training, seed), records in sorted(policies.items()):
        fingerprints = {policy_fingerprint(record) for record in records}
        if len(fingerprints) != 1:
            raise ValueError("fitted policy changed across pricing groups")
        rates = [event["exercise_rate"] for record in records for event in record["policy"]]
        result.append(dict(case=case, mode=mode, training=training, training_seed=seed,
                           pricing_groups=len(records), policy_sha256=fingerprints.pop(),
                           exercise_rate_min=min(rates), exercise_rate_max=max(rates)))
    return result


def check_matrix(indexed, case):
    keys = [key for key in indexed if key[0] == case]
    modes, budgets, training, pricing = (sorted({key[index] for key in keys}) for index in range(1, 5))
    expected = {(case, *key) for key in itertools.product(modes, budgets, training, pricing)}
    if len(training) < 2 or len(pricing) < 2 or set(keys) != expected:
        raise ValueError("missing paired budget/training/pricing observations")
    return modes, budgets, training, pricing


def diagnostic_vector(record):
    return {"hard_PV": record["hard_pv"], "fuzzy_minus_hard": record["pv"] - record["hard_pv"]}


def coordinate_summary(indexed, case, mode, budget, reference, training, pricing, vector):
    result = defaultdict(list)
    for seed in training:
        values = [vector(indexed[(case, mode, budget, seed, group)]) for group in pricing]
        references = [vector(indexed[(case, mode, reference, seed, group)]) for group in pricing]
        for coordinate in values[0]:
            paired = paired_difference((row[coordinate] for row in values), (row[coordinate] for row in references))
            result[coordinate].append(dict(training_seed=seed, **paired))
    return {name: dispersion(rows) for name, rows in result.items()}


def dispersion(rows):
    differences = [row["difference"] for row in rows]
    observed = dict(mean_difference=statistics.fmean(differences), sd_difference=statistics.stdev(differences),
                    min_difference=min(differences), max_difference=max(differences),
                    max_conditional_pricing_se=max(row["pricing_se"] for row in rows))
    return dict(by_training_seed=rows, observed_training_means=observed)


def summarize(observations):
    indexed, axes, policies = index_observations(observations)
    groups = policy_groups(policies)
    comparisons = []
    for case in sorted(axes):
        modes, budgets, training, pricing = check_matrix(indexed, case)
        for mode, budget in itertools.product(modes, budgets):
            arguments = (indexed, case, mode, budget, max(budgets), training, pricing)
            comparisons.append(dict(case=case, mode=mode, training=budget, reference_training=max(budgets),
                                    training_seeds=training, pricing_seeds=pricing,
                                    coordinates=coordinate_summary(*arguments, risk_vector),
                                    diagnostics=coordinate_summary(*arguments, diagnostic_vector)))
    return dict(default_training_budget="unchanged: pricing path count", accuracy_acceptance_defined=False,
                interpretation="Largest budget is a reference, not truth. Pricing SE is conditional on the fitted policy. "
                               "Training dispersion is observed across paired means and includes residual pricing noise; "
                               "shared pricing groups across training seeds preclude treating all observations as independent.",
                samples=len(indexed), comparisons=comparisons, policy_groups=groups)
