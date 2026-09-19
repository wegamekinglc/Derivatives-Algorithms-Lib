//
// Created by Codex on 2026/9/14.
//

#include <gtest/gtest.h>

#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/preparation.hpp>
#include <dal/storage/globals.hpp>

#include "bcg_allocation_probe.hpp"
#include "script_test_observers.hpp"

using Dal::Cell_;
using Dal::Date_;
using Dal::DateTime_;
using Dal::Handle_;
using Dal::MarketFixingSnapshot_;
using Dal::Vector_;
namespace Probe = Dal::BcgAllocationProbePrivate_;

namespace {
    template <class E_, class F_> void CheckAllocations(E_* state, const F_& evaluate, size_t payoff, double expected) {
        Probe::Reset_();
        double total = 0.0;
        Probe::Measurement_ measurement;
        for (size_t path = 0; path < 8193; ++path) {
            evaluate(*state);
            total += state->VarVals()[payoff];
        }
        const auto measured = measurement.Finish_();
        ASSERT_NEAR(total / 8193, expected, expected * 1.0e-12);
        ASSERT_TRUE(measured.balanced_);
        ASSERT_FALSE(measured.failedClosed_);
        ASSERT_EQ(measured.allocationRequests_, 0u);
    }
} // namespace

TEST(ScriptObservationAllocationTest, TestProbeCountsTemporaryAllocations) {
    Probe::Reset_();
    Probe::Measurement_ measurement;
    void* ordinary = ::operator new(32);
    ::operator delete(ordinary);
    void* aligned = ::operator new(64, std::align_val_t(64));
    ::operator delete(aligned, std::align_val_t(64));
    const auto measured = measurement.Finish_();
    ASSERT_TRUE(measured.balanced_);
    ASSERT_FALSE(measured.failedClosed_);
    ASSERT_EQ(measured.allocationRequests_, 2u);
}

TEST(ScriptObservationAllocationTest, TestExactAndFuzzyRepeatedPathsAllocateNothing) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const Handle_<MarketFixingSnapshot_> history(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}}}}));
    const Dal::Script::ScriptProductData_ product(
        "", {Cell_("SCALE"), Cell_("K"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
        {"2", "79.95", "x = SCALE * FIX(EQ[DAL196_TEST])",
         "IF FIX(EQ[DAL196_TEST], 2026-09-11) > K:0.2 THEN pay PAYS x + FIX(EQ[DAL196_TEST], 2026-09-15) ELSE pay PAYS 0 END"});
    Dal::Script::ScriptValuationSettings_ bindings;
    for (bool fuzzy : {false, true}) {
        SCOPED_TRACE(fuzzy);
        Dal::Script::MonteCarloSettings_ settings;
        settings.enableAad_ = fuzzy;
        settings.compiled_ = true;
        Dal::AAD::BlackScholes_<double> model(100.0, 0.2, 0.03, 0.01);
        const auto prepared = Dal::Script::PrepareScript(product, &model, bindings, settings, history);
        const auto artifact = prepared.Compile(fuzzy);
        auto compiled = prepared.BuildEvalState<double>();
        Dal::AAD::Scenario_<double> scenario;
        Dal::AAD::AllocatePath(prepared.DefLine(), scenario);
        Dal::AAD::InitializePath(scenario);
        for (auto& sample : scenario) {
            sample.numeraire_ = 2.0;
            for (auto& observation : sample.observations_)
                observation = 120.0;
        }
        Dal::Script::TestSupport::RejectFixingReads_ reject("history read after preparation", "index read after preparation");
        const Dal::Detail::ScopedFixingReadObserver_ guard(&reject);
        const double expected = fuzzy ? 105.0 : 140.0;
        ASSERT_NO_FATAL_FAILURE(
            CheckAllocations(&compiled, [&](auto& state) { artifact.Evaluate(scenario, state); }, prepared.PayOffIdx(), expected));
        if (fuzzy) {
            auto tree = prepared.BuildFuzzyEvaluator<double>(0, settings.smooth_);
            ASSERT_NO_FATAL_FAILURE(
                CheckAllocations(&tree, [&](auto& state) { prepared.Evaluate(scenario, state); }, prepared.PayOffIdx(), expected));
        } else {
            auto tree = prepared.BuildEvaluator<double>();
            ASSERT_NO_FATAL_FAILURE(
                CheckAllocations(&tree, [&](auto& state) { prepared.Evaluate(scenario, state); }, prepared.PayOffIdx(), expected));
        }
    }
}
