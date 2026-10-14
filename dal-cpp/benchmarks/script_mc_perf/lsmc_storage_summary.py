"""Compare storage widths at identical training budgets and random addresses."""

from collections import defaultdict
import itertools

from lsmc_study_data import risk_vector
from lsmc_study_summary import check_matrix, diagnostic_vector, dispersion, index_observations, policy_groups
from lsmc_study_support import paired_difference


def coordinates(record):
    if len(record["risk_names"]) != len(record["risks"]):
        raise ValueError("cropped risk vector in storage comparison")
    return dict(risk_vector(record), **diagnostic_vector(record))


def paired_coordinates(values, references, seed):
    result = {}
    for coordinate in values[0]:
        paired = paired_difference((row[coordinate] for row in values), (row[coordinate] for row in references))
        result[coordinate] = dict(training_seed=seed, **paired)
    return result


def group_coordinates(indexed, keys):
    return [coordinates(indexed[key]) for key in keys]


def width_coordinates(reduced, native, case, mode, budget, training, pricing):
    rows = defaultdict(list)
    for seed in training:
        keys = [(case, mode, budget, seed, group) for group in pricing]
        paired = paired_coordinates(group_coordinates(reduced, keys), group_coordinates(native, keys), seed)
        for coordinate, result in paired.items():
            rows[coordinate].append(result)
    return {coordinate: dispersion(values) for coordinate, values in rows.items()}


def policy_changes(reduced, native):
    fields = ("selected_degree", "rank", "eligible_paths", "solver", "fallback", "degenerate")
    changed = defaultdict(int)
    max_rate_difference = 0.0
    for key, record in reduced.items():
        events, references = record["policy"], native[key]["policy"]
        if len(events) != len(references):
            raise ValueError("storage widths have different exercise-date counts")
        for event, reference in zip(events, references):
            for field in fields:
                if event.get(field) != reference.get(field):
                    changed[field] += 1
            max_rate_difference = max(max_rate_difference, abs(event["exercise_rate"] - reference["exercise_rate"]))
    return dict(changed_event_observations=dict(changed), max_absolute_exercise_rate_difference=max_rate_difference)


def summarize_storage(reduced_observations, native_observations):
    reduced, axes, reduced_policies = index_observations(reduced_observations)
    native, native_axes, native_policies = index_observations(native_observations)
    if set(reduced) != set(native) or axes != native_axes:
        raise ValueError("storage widths must have identical jobs and ordered risk axes")
    groups = dict(binary32=policy_groups(reduced_policies), native_binary64=policy_groups(native_policies))
    comparisons = []
    for case in sorted(axes):
        modes, budgets, training, pricing = check_matrix(reduced, case)
        for mode, budget in itertools.product(modes, budgets):
            values = width_coordinates(reduced, native, case, mode, budget, training, pricing)
            comparisons.append(dict(case=case, mode=mode, training=budget, training_seeds=training,
                                    pricing_seeds=pricing, coordinates=values))
    return dict(samples_per_width=len(reduced), comparison="binary32 overlay minus native binary64 at equal training budget",
                accuracy_acceptance_defined=False, default_storage="unchanged: native double", comparisons=comparisons,
                policy_groups=groups, policy_changes=policy_changes(reduced, native))
