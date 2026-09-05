//
// Created by dal-implementer on 2026/9/5.
//
// Quote-risk provenance benchmarks: the Build*QuoteRiskProvenance entry points exposed to the
// public, Python and Excel surfaces (PRs #325-#332). Every calibration consumer pays this once
// per calibration — axis/ coordinate assembly, JCS-canonicalized curve JSON comparison, the
// SHA-256 state fingerprint over spec + result + market, and the effective-inverse copy.
// rate_risk_perf times the aggregation that consumes provenances; this suite times producing
// them, plus the per-component state probe the aggregator uses for staleness detection.

#include <dal/benchmarks/bench.hpp>
#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/curve/quoteriskprovenance_internal.hpp>
#include <dal/platform/platform.hpp>

#include "../rate_risk_perf/quoteriskbenchfixtures.hpp"

using namespace Dal;

namespace {
    constexpr int kWarmup = 3;
    constexpr int kRepeats = 20;

    template <class Materials_, class Build_> void RunBuildCase(const char* name, const Materials_& materials, Build_&& build) {
        double sink = 0.0;
        const auto result = Bench::Run(
            name,
            [&]() {
                const RateQuoteRiskProvenance_ provenance = build(materials);
                REQUIRE(provenance.Available(), "Quote-risk provenance benchmark produced an unavailable provenance");
                sink += static_cast<double>(provenance.State().fingerprint_.size());
            },
            kWarmup, kRepeats);
        Bench::Print(result);
        Bench::DoNotOptimize(&sink);
    }

    template <class Materials_> void RunProbeCase(const char* name, const Materials_& materials, const Vector_<String_>& componentKeys) {
        double sink = 0.0;
        const auto result = Bench::Run(
            name,
            [&]() {
                for (const auto& key : componentKeys) {
                    const RateQuoteRiskComponentState_ state = CurrentRateQuoteRiskComponentState(key, materials.market_);
                    REQUIRE(!state.fingerprint_.empty(), "Quote-risk state probe found an unbound component");
                    sink += static_cast<double>(state.fingerprint_.size());
                }
            },
            kWarmup, kRepeats);
        Bench::Print(result);
        Bench::DoNotOptimize(&sink);
    }
} // namespace

int main() {
    const auto analytic = CurveJacobianMode_(CurveJacobianMode_::Value_::ANALYTIC);
    const auto single8 = RateRiskPerf::MakeSingleCurveProvenanceMaterials(8, analytic);
    const auto single16 = RateRiskPerf::MakeSingleCurveProvenanceMaterials(16, analytic);
    const auto joint8 = RateRiskPerf::MakeJointXccyProvenanceMaterials(8, analytic);
    const auto joint16 = RateRiskPerf::MakeJointXccyProvenanceMaterials(16, analytic);
    const auto staged8 = RateRiskPerf::MakeStagedXccyProvenanceMaterials(8, analytic);
    const auto staged16 = RateRiskPerf::MakeStagedXccyProvenanceMaterials(16, analytic);

    Bench::PrintHeader();

    RunBuildCase("Quote risk provenance single build (8 quotes)", single8, [](const auto& m) {
        return BuildSingleCurveQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_);
    });
    RunBuildCase("Quote risk provenance single build (16 quotes)", single16, [](const auto& m) {
        return BuildSingleCurveQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_);
    });
    RunBuildCase("Quote risk provenance joint build (3x8 quotes)", joint8, [](const auto& m) {
        return BuildJointXccyQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_);
    });
    RunBuildCase("Quote risk provenance joint build (3x16 quotes)", joint16, [](const auto& m) {
        return BuildJointXccyQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_);
    });
    RunBuildCase("Quote risk provenance staged build (8 quotes)", staged8, [](const auto& m) {
        return BuildStagedXccyBasisQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_);
    });
    RunBuildCase("Quote risk provenance staged build (16 quotes)", staged16, [](const auto& m) {
        return BuildStagedXccyBasisQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_);
    });

    RunProbeCase("Quote risk state probe single (16 quotes)", single16, Vector_<String_>{"discount"});
    Vector_<String_> jointComponents;
    for (int i = 0; i < 5; ++i)
        jointComponents.push_back("joint-quote-bench-" + String::FromInt(i));
    RunProbeCase("Quote risk state probe joint (5 components x 16 quotes)", joint16, jointComponents);
    return 0;
}
