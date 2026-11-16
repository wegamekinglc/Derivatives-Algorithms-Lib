"""Recompute A/1 statistical stopping decisions from all 16 raw blocks."""

import numpy as np


def batch_statistics(row):
    means = np.array(row["batch_means"])
    actual = np.array(row["estimates"])
    if (
        means.shape != (16, 80)
        or actual.shape != (80, 2)
        or not np.all(np.isfinite(means))
    ):
        raise ValueError("Missing or nonfinite batch evidence")
    expected = np.column_stack((means.mean(axis=0), means.std(axis=0, ddof=1) / 4))
    if not np.allclose(actual, expected, rtol=1e-12, atol=1e-11):
        raise ValueError("Batch mean or SE mismatch")
    return expected


def validate_identity(mc):
    if mc["sampling"] != "FORMAL" or mc["seed"] != 1024 or mc["dmax"] != 8728:
        raise ValueError("Unqualified A/1 sampling identity")


def validate_tier(index, tier):
    if (
        tier["paths_per_batch"] != 2 ** (16 + index)
        or index > 4
        or len(tier["groups"]) != 14
    ):
        raise ValueError("Nonregistered tier/group")


def tier_sufficient(tier):
    sufficient = True
    for group, row in enumerate(tier["groups"]):
        expected = batch_statistics(row)
        sufficient = sufficient and bool(
            np.max(expected[:, 1]) <= (0.01 if group < 8 else 0.005)
        )
    return sufficient


def verify(mc):
    validate_identity(mc)
    records = []
    for index, tier in enumerate(mc["tiers"]):
        validate_tier(index, tier)
        sufficient = tier_sufficient(tier)
        if sufficient != tier["se_sufficient"] or (
            index < len(mc["tiers"]) - 1 and sufficient
        ):
            raise ValueError("Stopping decision violates SE-only protocol")
        records.append(
            dict(paths_per_batch=tier["paths_per_batch"], se_sufficient=sufficient)
        )
    if not records:
        raise ValueError("Missing sampling tiers")
    return dict(status="PASS", tiers=records, degrees_of_freedom=15)
