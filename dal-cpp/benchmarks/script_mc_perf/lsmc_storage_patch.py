"""Exact-source overlays for complete, isolated storage experiment builds."""

import hashlib
import re

from lsmc_study_support import CONTROL_MACRO, controlled_source

CORE = "dal-cpp/dal/script/lsmc.cpp"
API = "dal-cpp/dal/script/lsmc.hpp"
ROWS = "dal-cpp/dal/script/visitor/evalstate.hpp"
SAMPLE = "dal-cpp/benchmarks/script_mc_perf/lsmctrainingstudy.cpp"
SOURCE_HASHES = {
    CORE: "e53a2c25001f46498c3797864f4c1fa184b83c8c9320b9c11fb3d41e688ccb82",
    API: "b63da56f108901fa12cff1901f3059be60d2163e7cbe9c5c3469b7d5cca32278",
    ROWS: "117eb18dae677af7e7024821e704e8207c4b66497cf909b673b590477062c8ce",
    SAMPLE: "55d36a9fbee88eb2e7740d7aa61b8571612bab3eecf733037306a91c9ca55a4c",
}
HEADER = "benchmarks/script_mc_perf/lsmcstoragestudy.hpp"


def replace_exact(source, before, after, count=1):
    if source.count(before) != count:
        raise ValueError(f"unrecognized storage overlay site: {before}")
    return source.replace(before, after)


def rows_source(source, bits):
    source = replace_exact(source, "#pragma once", f'#pragma once\n\n#include "../../../{HEADER}"')
    pattern = r"    class LsmcRows_ \{[\s\S]*?\n    \};"
    matches = re.findall(pattern, source)
    if len(matches) != 1:
        raise ValueError("unrecognized storage row definition")
    alias = f"    using LsmcRows_ = StudyRows_<std::conditional_t<{bits} == 32, float, double>>;"
    return replace_exact(source, matches[0], alias)


def core_source(source, bits, anchor):
    source = controlled_source(source, anchor)
    source = replace_exact(source, "const double* x_;", "StudyReadRow_ x_;")
    source = replace_exact(source, "std::array<const double*, 3> features_{};", "std::array<StudyReadRow_, 3> features_{};")
    source = replace_exact(source, "const double* h_ = nullptr;", "StudyReadRow_ h_ = nullptr;", 2)
    source = replace_exact(source, "void RequireFiniteIncluded(const double* values,", "template <class R_> void RequireFiniteIncluded(R_ values,")
    for name in ("pays", "discount", "x"):
        source = replace_exact(source, f"const double* {name}_ = nullptr;", f"StudyReadRow_ {name}_ = nullptr;")
    for name in ("pays", "discount", "h", "x"):
        source = replace_exact(source, f"const double* {name} = eventRows.{name}_;", f"const StudyReadRow_ {name} = eventRows.{name}_;")
    source = replace_exact(source, "const double* pendingH = pending.h_;", "const StudyReadRow_ pendingH = pending.h_;")
    source = replace_exact(source, "HoldingValue(const double* pays,", "HoldingValue(StudyReadRow_ pays,")
    source = replace_exact(source, "InRegressionSet(const char* cond, const double* h,", "InRegressionSet(const char* cond, StudyReadRow_ h,")
    return replace_exact(source, "Detail::ProfileElements(count, count, sizeof(double))",
                         f"Detail::ProfileElements(count, count, sizeof(std::conditional_t<{bits} == 32, float, double>))")


def storage_sources(sources, bits, anchor):
    if type(bits) is not int or bits not in (32, 64):
        raise ValueError("storage width must be 32 or 64")
    if set(sources) != set(SOURCE_HASHES):
        raise ValueError("storage overlay requires exactly four complete native inputs")
    for name, expected in SOURCE_HASHES.items():
        if hashlib.sha256(sources[name].encode()).hexdigest() != expected:
            raise ValueError(f"unrecognized complete native source: {name}")
    api = replace_exact(sources[API], "#pragma once", f'#pragma once\n\n#include "../../{HEADER}"')
    api = replace_exact(api, "std::array<const double*, 3> features_{};", "std::array<StudyReadRow_, 3> features_{};")
    sample = replace_exact(sources[SAMPLE], 'WriteText(writer, "record", "lsmc_training_sample");',
                           f'''WriteText(writer, "record", "lsmc_training_sample");
        writer->Key("storage_bits");
        writer->Uint({bits});
        writer->Key("peak_training_row_bytes");
        writer->Uint64(StudyPeakRowBytes());
        writer->Key("live_training_row_bytes");
        writer->Uint64(StudyCurrentRowBytes());''')
    define = f"#define {CONTROL_MACRO} {anchor}\n"
    return {CORE: define + core_source(sources[CORE], bits, anchor), API: api,
            ROWS: rows_source(sources[ROWS], bits), SAMPLE: define + sample}
