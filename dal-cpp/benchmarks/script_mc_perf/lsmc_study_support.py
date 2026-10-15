"""Source admission, exact block layouts and paired study summaries."""

import hashlib
import math
import re
import statistics

MAX_INT = 2**31 - 1
MAX_SOBOL_PATHS = 2**32 - 1
CONTROL_MACRO = "DAL_LSMC_STUDY_PRICING_OFFSET"
FUNCTIONS = {
    "LsmcPathCounts": ("PathCounts_", "89c20bcccd037c38e529817ebf1343c44e2de64edebe4406cccfa5316ce42a0a"),
    "TrainFrozenPolicy": ("TrainingOutcome_", "f69f27dec1d7c935cd9bf05c886004891bf57cb75e50ff527563835ae1d68c48"),
    "RunForwardPhase": ("void", "a7fd42e6aa3a2bdebbf614400f3535296647b92cdc43dbf77396e9539efec8fe"),
}


def count(value, name, minimum=1, maximum=MAX_INT):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"invalid {name}: expected an integer in [{minimum}, {maximum}]")
    return value


def controlled_source(source, anchor):
    count(anchor, "training anchor")
    for name, (result_type, expected) in FUNCTIONS.items():
        pattern = rf"        {result_type} {name}\([^\n]*\) \{{[\s\S]*?\n        \}}"
        matches = re.findall(pattern, source)
        if len(matches) != 1 or hashlib.sha256(matches[0].encode()).hexdigest() != expected:
            raise ValueError(f"unrecognized native source shape: {name}")
    original_return = "            return {training, validation, training + validation, replicates};"
    replacement = f'''            const size_t studyAnchor = static_cast<size_t>({CONTROL_MACRO});
            REQUIRE2(studyAnchor >= training && studyAnchor <= MAX_SOBOL_PATHS && validation <= MAX_SOBOL_PATHS - studyAnchor &&
                         nPaths <= (MAX_SOBOL_PATHS - studyAnchor - validation) / replicates,
                     "InvalidStudyPathCount: anchor must follow training and all held-out blocks must fit Sobol", ScriptError_);
            return {{training, validation, studyAnchor + validation, replicates}};'''
    original_validation = "RunForwardPhase(validationCtx, BatchPlan_(counts.validation_, 1), counts.training_);"
    fixed_validation = f"RunForwardPhase(validationCtx, BatchPlan_(counts.validation_, 1), static_cast<size_t>({CONTROL_MACRO}), counts.training_);"
    if source.count(original_return) != 1 or source.count(original_validation) != 1:
        raise ValueError("expected exactly two native address sites")
    # Sobol addresses use the fixed anchor; cached validation rows remain adjacent to training rows.
    cache_sites = (
        ("size_t pathOffset = 0) {", "size_t pathOffset = 0, size_t gaussianOffset = 0) {"),
        ("REQUIRE(pathOffset <= ctx.gaussians_->Paths() && batchPlan.PathCount() <= ctx.gaussians_->Paths() - pathOffset,",
         "REQUIRE(gaussianOffset <= ctx.gaussians_->Paths() && batchPlan.PathCount() <= ctx.gaussians_->Paths() - gaussianOffset,"),
        ("ctx.gaussianOffset_ = pathOffset;", "ctx.gaussianOffset_ = gaussianOffset;"),
    )
    for before, after in cache_sites:
        if source.count(before) != 1:
            raise ValueError("expected exactly one native Gaussian offset site")
        source = source.replace(before, after)
    return source.replace(original_return, replacement).replace(original_validation, fixed_validation)


def block_layout(anchor, training, validation, pricing, replicates):
    count(anchor, "training anchor")
    count(pricing, "pricing paths", maximum=MAX_SOBOL_PATHS)
    training = pricing if training is None else training
    count(training, "training paths", maximum=MAX_SOBOL_PATHS)
    count(validation, "validation paths", minimum=0)
    count(replicates, "pricing replicates", minimum=2)
    if training > anchor or anchor + validation + replicates * pricing > MAX_SOBOL_PATHS:
        raise ValueError("overlapping or overflowing held-out Sobol blocks")
    first = anchor + validation
    return {
        "training": (0, training),
        "validation": (anchor, first),
        "pricing": tuple((first + index * pricing, first + (index + 1) * pricing) for index in range(replicates)),
    }


def mean_and_se(values):
    values = tuple(values)
    if len(values) < 2 or not all(type(value) in (int, float) and math.isfinite(value) for value in values):
        raise ValueError("at least two finite numeric observations are required")
    return statistics.fmean(values), statistics.stdev(values) / math.sqrt(len(values))


def paired_difference(values, reference):
    values, reference = tuple(values), tuple(reference)
    if len(values) != len(reference):
        raise ValueError("paired observations have different extents")
    value_mean, _ = mean_and_se(values)
    reference_mean, _ = mean_and_se(reference)
    difference, error = mean_and_se(left - right for left, right in zip(values, reference))
    return {
        "mean": value_mean,
        "reference_mean": reference_mean,
        "difference": difference,
        "pricing_se": error,
        "relative_difference": difference / abs(reference_mean) if reference_mean else None,
        "scaled_difference": difference / max(1.0, abs(value_mean), abs(reference_mean)),
    }
