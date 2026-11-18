// Quote-risk provenance benchmarks: the Build*QuoteRiskProvenance entry points exposed to the
// public, Python and Excel surfaces (PRs #325-#332). Every calibration consumer pays this once
// per calibration — axis/ coordinate assembly, JCS-canonicalized curve JSON comparison, the
// SHA-256 state fingerprint over spec + result + market, and the effective-inverse copy.
// rate_risk_perf times the aggregation that consumes provenances; this suite times producing
// them, plus the per-component state probe the aggregator uses for staleness detection.

#include <cmath>
#include <memory>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/curve/quoteriskprovenance_internal.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/canonicaljson_internal.hpp>

#include "../../tests/curve/jointquoteriskfixtures.hpp"
#include "../rate_risk_perf/quoteriskbenchfixtures.hpp"
#include "dupirecurvatureperf.hpp"
#include "marketivperf.hpp"
#include "scriptcurveperf.hpp"

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

    void CheckProfileInverse(const RateQuoteRiskProvenance_& source, const RateQuoteRiskProvenance_& retained) {
        const auto& inverse = source.EffectiveInverse();
        REQUIRE(inverse.Rows() == retained.EffectiveInverse().Rows() && inverse.Cols() == retained.EffectiveInverse().Cols(),
                "State profile inverse shape mismatch");
        for (int row = 0; row < inverse.Rows(); ++row)
            for (int col = 0; col < inverse.Cols(); ++col)
                REQUIRE(std::isfinite(inverse(row, col)) && inverse(row, col) == retained.EffectiveInverse()(row, col),
                        "State profile inverse mismatch");
    }

    void CheckProfileSource(const RateQuoteRiskProvenance_& source, const RateQuoteRiskProvenance_& retained, const RatePricingMarket_& market) {
        RequireLiveQuoteRiskState(source, "quote_risk_perf.source");
        REQUIRE(source.Available() && retained.Available() && source.CalibrationRecord().empty() && !retained.CalibrationRecord().empty(),
                "State profile retain or availability mismatch");
        REQUIRE(source.Axis().fingerprint_ == retained.Axis().fingerprint_ && source.State().fingerprint_ == retained.State().fingerprint_ &&
                    CanonicalJson::FingerprintCanonical(retained.CalibrationRecord()) == source.State().fingerprint_,
                "State profile retained identity mismatch");
        CheckProfileInverse(source, retained);
        REQUIRE(!source.State().components_.empty(), "State profile has no components");
        for (const auto& component : source.State().components_)
            REQUIRE(CurrentRateQuoteRiskComponentState(component.componentKey_, market, source.State().scheme_).fingerprint_ ==
                        component.fingerprint_,
                    "State profile component mismatch");
        std::cout << "STATE_PREFLIGHT kind=" << source.Kind() << " scheme=" << source.State().scheme_ << " quotes=" << source.Axis().quotes_.size()
                  << " components=" << source.State().components_.size() << " components_verified=" << source.State().components_.size()
                  << " record_bytes=" << retained.CalibrationRecord().size() << " axis=" << source.Axis().fingerprint_
                  << " state=" << source.State().fingerprint_ << " timed_retain=false\n";
    }

    void RunBoundProbeCase(const char* name, const RateQuoteRiskProvenance_& source, const RatePricingMarket_& market) {
        double sink = 0.;
        Bench::Print(Bench::Run(
            name,
            [&] {
                for (const auto& component : source.State().components_) {
                    const auto state = CurrentRateQuoteRiskComponentState(component.componentKey_, market, source.State().scheme_);
                    REQUIRE(state.fingerprint_ == component.fingerprint_, "State profile component mismatch");
                    sink += static_cast<double>(state.fingerprint_.size());
                }
            },
            kWarmup, kRepeats));
        Bench::DoNotOptimize(&sink);
    }

    int RunStateProfile(int argc, char** argv) {
        if (argc > 3 || (argc == 3 && std::string(argv[2]) != "--preflight")) {
            std::cerr << "usage: quote_risk_perf --state-profile [--preflight]\n";
            return 2;
        }
        try {
            using Generic_ = RateRiskPerf::ProvenanceMaterials_<JointMultiCurveCalibrationSpec_, JointMultiCurveCalibrationOptions_,
                                                                JointMultiCurveCalibrationResult_>;
            Generic_ generic;
            generic.spec_ = JointQuoteRiskFixtures::Spec(5, 3, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
            generic.options_.computeEffJacobianInverse_ = true;
            generic.calibration_ = std::make_shared<JointMultiCurveCalibrationResult_>(CalibrateJointMultiCurve(generic.spec_, generic.options_));
            generic.market_ = JointQuoteRiskFixtures::Market(generic.spec_, *generic.calibration_);
            generic.config_ = JointQuoteRiskFixtures::Config(3);
            const auto buildGeneric = [](const auto& m) {
                return BuildJointMultiCurveQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_);
            };
            const auto genericSource = buildGeneric(generic);
            auto retainedGeneric = generic;
            retainedGeneric.config_.retainCalibrationRecord_ = true;
            CheckProfileSource(genericSource, buildGeneric(retainedGeneric), generic.market_);
            const auto staged = RateRiskPerf::MakeStagedXccyProvenanceMaterials(16, CurveJacobianMode_::Value_::ANALYTIC);
            const auto stagedSource =
                BuildStagedXccyBasisQuoteRiskProvenance(staged.spec_, *staged.calibration_, staged.options_, staged.market_, staged.config_);
            auto retainedConfig = staged.config_;
            retainedConfig.retainCalibrationRecord_ = true;
            CheckProfileSource(
                stagedSource,
                BuildStagedXccyBasisQuoteRiskProvenance(staged.spec_, *staged.calibration_, staged.options_, staged.market_, retainedConfig),
                staged.market_);
            if (argc == 3)
                return 0;
            const int calibrations = CurveCalibrationInvocationCount();
            Bench::PrintHeader();
            RunBuildCase("Quote risk provenance generic build (N5, informational)", generic, buildGeneric);
            RunBoundProbeCase("Quote risk state probe generic (3 components, informational)", genericSource, generic.market_);
            RunBoundProbeCase("Quote risk state probe staged (16 quotes, informational)", stagedSource, staged.market_);
            REQUIRE(CurveCalibrationInvocationCount() == calibrations, "State profile timer recalibrated");
            return 0;
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
    }
} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--state-profile")
        return RunStateProfile(argc, argv);
    if (argc == 2) {
        const int script = RunScriptCurveQuoteRiskMode(argv[1]);
        if (script >= 0)
            return script;
    }
    if (argc > 1 && std::string(argv[1]) == "--market-iv")
        return RunMarketIvBenchmarks();
    if (argc > 1)
        return RunDupireQuoteCurvatureBenchmarks(argc, argv);
    const auto analytic = CurveJacobianMode_(CurveJacobianMode_::Value_::ANALYTIC);
    const auto single8 = RateRiskPerf::MakeSingleCurveProvenanceMaterials(8, analytic);
    const auto single16 = RateRiskPerf::MakeSingleCurveProvenanceMaterials(16, analytic);
    const auto joint8 = RateRiskPerf::MakeJointXccyProvenanceMaterials(8, analytic);
    const auto joint16 = RateRiskPerf::MakeJointXccyProvenanceMaterials(16, analytic);
    const auto staged8 = RateRiskPerf::MakeStagedXccyProvenanceMaterials(8, analytic);
    const auto staged16 = RateRiskPerf::MakeStagedXccyProvenanceMaterials(16, analytic);

    Bench::PrintHeader();

    RunBuildCase("Quote risk provenance single build (8 quotes)", single8,
                 [](const auto& m) { return BuildSingleCurveQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_); });
    RunBuildCase("Quote risk provenance single build (16 quotes)", single16,
                 [](const auto& m) { return BuildSingleCurveQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_); });
    RunBuildCase("Quote risk provenance joint build (3x8 quotes)", joint8,
                 [](const auto& m) { return BuildJointXccyQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_); });
    RunBuildCase("Quote risk provenance joint build (3x16 quotes)", joint16,
                 [](const auto& m) { return BuildJointXccyQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_); });
    RunBuildCase("Quote risk provenance staged build (8 quotes)", staged8,
                 [](const auto& m) { return BuildStagedXccyBasisQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_); });
    RunBuildCase("Quote risk provenance staged build (16 quotes)", staged16,
                 [](const auto& m) { return BuildStagedXccyBasisQuoteRiskProvenance(m.spec_, *m.calibration_, m.options_, m.market_, m.config_); });

    RunProbeCase("Quote risk state probe single (16 quotes)", single16, Vector_<String_>{"discount"});
    Vector_<String_> jointComponents;
    for (int i = 0; i < 5; ++i)
        jointComponents.push_back("joint-quote-bench-" + String::FromInt(i));
    RunProbeCase("Quote risk state probe joint (5 components x 16 quotes)", joint16, jointComponents);
    return RunDupireQuoteCurvatureBenchmarks(argc, argv);
}
