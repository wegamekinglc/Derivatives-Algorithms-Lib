//
// Created by Codex on 2026/10/13.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;

namespace {
    struct ScopedAadWorkers_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        explicit ScopedAadWorkers_(size_t threads) { pool_->Start(threads, true); }
        ~ScopedAadWorkers_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    struct AadResultAudit_ {
        size_t axes_ = 0;
        size_t metadataCopies_ = 0;
        bool valid_ = false;
    };

    template <class R_> struct AuditedResult_ : R_ {
        AadResultAudit_* audit_;
        template <class... A_> AuditedResult_(AadResultAudit_* audit, A_&&... args) : R_(std::forward<A_>(args)...), audit_(audit) {}
        AuditedResult_(const AuditedResult_& source) : R_(source), audit_(source.audit_) { ++audit_->metadataCopies_; }
        AuditedResult_(AuditedResult_&&) = default;
    };

    struct AuditedAadObjective_ : Script::Detail::ScalarSimulationObjective_ {
        using Result_ = AuditedResult_<Script::SimResults_>;
        AadResultAudit_* audit_;
        explicit AuditedAadObjective_(AadResultAudit_* audit) : audit_(audit) {}
        [[nodiscard]] Result_ MakeResult(const Vector_<String_>& names) const {
            ++audit_->axes_;
            return {audit_, names};
        }
    };

    struct AuditedWeightedObjective_ : Script::Detail::WeightedSimulationObjective_ {
        using Result_ = AuditedResult_<Script::Detail::WeightedSimResults_>;
        AadResultAudit_* audit_;
        AuditedWeightedObjective_(const Script::WeightedRiskPlan_& plan, AadResultAudit_* audit)
            : WeightedSimulationObjective_(plan), audit_(audit) {}
        [[nodiscard]] Result_ MakeResult(const Vector_<String_>& names) const {
            ++audit_->axes_;
            return {audit_, names, plan_.OutputAxis().size()};
        }
    };

    struct LegacyAadObjective_ {
        using Result_ = Script::SimResults_;
        Script::Detail::ScalarSimulationObjective_ inner_;
        [[nodiscard]] Result_ MakeResult(const Vector_<String_>& names) const { return inner_.MakeResult(names); }
        [[nodiscard]] Script::Detail::ScalarPayoffCollector_ AADCollector(Result_* result) const { return inner_.AADCollector(result); }
        void AccumulateComponents(Result_* result, const Result_& batch) const { inner_.AccumulateComponents(result, batch); }
        [[nodiscard]] AAD::ProfilingArrayStatistics_ ResultArrays(const Result_& result) const { return inner_.ResultArrays(result); }
    };

    struct LegacyTypedAadObjective_ : AuditedAadObjective_ {
        using AuditedAadObjective_::AuditedAadObjective_;
        [[nodiscard]] Script::Detail::ScalarPayoffCollector_ AADCollector(Result_*) const { return {}; }
    };

    struct LegacyInputCollector_ : Script::Detail::ScalarPayoffCollector_ {
        void AccumulateInput(Script::SimResults_* result, size_t position, const AAD::Number_& input, size_t paths) const {
            Script::Detail::ScalarPayoffCollector_::AccumulateInput(result, position, input, paths);
        }
    };

    struct LegacyInputAadObjective_ : AuditedAadObjective_ {
        using AuditedAadObjective_::AuditedAadObjective_;
        template <class R_> [[nodiscard]] LegacyInputCollector_ AADCollector(R_*) const { return {}; }
    };

    struct LegacyProfiledAadObjective_ : AuditedAadObjective_ {
        using AuditedAadObjective_::AuditedAadObjective_;
        [[nodiscard]] AAD::ProfilingArrayStatistics_ ResultArrays(const Result_& result) const {
            return Script::Detail::ProfileArrays(result.risks_);
        }
    };

    struct LegacyReducedAadObjective_ : AuditedAadObjective_ {
        using AuditedAadObjective_::AuditedAadObjective_;
        void AccumulateComponents(Result_*, const Result_&) const {}
    };

    bool ValidAuditedAxis(const Script::SimResults_& result) {
        return result.risks_.size() == 5 && result.names_.size() == 5 && result.results_.size() == 5 && std::isfinite(result["SCALE"]);
    }

    bool ValidAuditedResult(const Script::SimResults_& result, size_t paths) {
        return ValidAuditedAxis(result) && std::isfinite(result.aggregated_) && (paths == 0 ? result.aggregated_ == 0.0 : result.aggregated_ > 0.0) &&
               std::all_of(result.risks_.begin(), result.risks_.end(),
                           [paths](double risk) { return paths == 0 ? risk == 0.0 : std::isfinite(risk); });
    }

    template <class O_ = AuditedAadObjective_>
    AadResultAudit_ AuditAadResultConstruction(bool compiled, size_t paths = 17, bool prepared = false, bool expired = false) {
        ScopedAadWorkers_ workers(4);
        const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 1));
        const Script::ScriptProductData_ data("audit", {Cell_(String_("SCALE")), Cell_(expired ? Date_(2025, 12, 31) : Date_(2027, 1, 1))},
                                              {"1.2", expired ? "payoff PAYS SCALE" : "payoff PAYS SCALE * SPOT()"});
        const Handle_<ModelData_> model(new BSModelData_("model", 100.0, 0.2, 0.05, 0.02));
        AadResultAudit_ audit;
        const O_ objective(&audit);
        const auto evaluate = [&](const auto& product, int depth) {
            const auto result = Script::MCAADSimulationWithObjective(product, model, paths, "sobol", false, compiled, depth, 0.01, objective);
            audit.valid_ = ValidAuditedResult(result, expired ? 0 : paths);
        };
        if (prepared) {
            auto metadata = CreateModel<double>(model);
            Script::ScriptValuationSettings_ valuation;
            valuation.evaluationDate_ = Date_(2026, 1, 1);
            Script::MonteCarloSettings_ simulation;
            simulation.enableAad_ = true;
            simulation.compiled_ = compiled;
            simulation.smooth_ = 0.01;
            evaluate(Script::PrepareScript(data, metadata.get(), valuation, simulation), -1);
        } else {
            Script::ScriptProduct_ product(data.Dates(), data.EventTexts());
            evaluate(product, static_cast<int>(product.PreProcess(true, false)));
        }
        return audit;
    }

    template <class O_> void AssertLegacyAadStorageFallback() {
        for (const bool compiled : {false, true}) {
            const auto audit = AuditAadResultConstruction<O_>(compiled);
            ASSERT_TRUE(audit.valid_);
            ASSERT_EQ(audit.axes_, 1);
            ASSERT_EQ(audit.metadataCopies_, 4);
        }
    }

    void AssertSameAadResults(const Script::SimResults_& actual, const Script::SimResults_& expected) {
        ASSERT_EQ(actual.aggregated_, expected.aggregated_);
        ASSERT_EQ(actual.names_, expected.names_);
        ASSERT_EQ(actual.results_, expected.results_);
        ASSERT_EQ(actual.risks_.size(), expected.risks_.size());
        for (size_t risk = 0; risk < actual.risks_.size(); ++risk)
            ASSERT_EQ(actual.risks_[risk], expected.risks_[risk]);
    }

    Script::ScriptProduct_ ScalarAuditProduct(const String_& payoff = "payoff") {
        Script::ScriptProduct_ product({Cell_(String_("SCALE")), Cell_(Date_(2027, 1, 1))},
                                       {"1.2", "a = SCALE * SPOT() b = SPOT() * SPOT() c = SCALE payoff PAYS a + b + c"}, payoff);
        product.PreProcess(true, false);
        return product;
    }

    void AssertChunkProfileCounts(const AAD::ProfilingData_& statistics, const Script::Detail::ChunkPlan_& chunks) {
        ASSERT_EQ(statistics.taskGroups_.size(), 1u);
        ASSERT_EQ(statistics.taskGroups_.front().size(), std::min(ThreadPool_::GetInstance()->NumThreads(), chunks.BatchCount()));
        uint64_t prefix = 0;
        for (const auto& task : statistics.taskGroups_.front()) {
            ASSERT_TRUE(task.complete_);
            ASSERT_EQ(task.Phase(AAD::AADProfilingPhase_::Value_::WORKER_INIT).calls_, 1u);
            prefix += task.Phase(AAD::AADProfilingPhase_::Value_::REVERSE_PREFIX).calls_;
        }
        ASSERT_EQ(prefix, chunks.BatchCount());
    }

    void AssertWeightedAadPartials(bool compiled, size_t paths, size_t threads) {
        ScopedAadWorkers_ workers(threads);
        const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 1));
        const Handle_<ModelData_> model(new BSModelData_("model", 10.0, 0.2, 0.05, 0.02));
        auto metadata = CreateModel<double>(model);
        const auto product = ScalarAuditProduct();
        Vector_<Script::RiskCoordinate_> inputs;
        for (size_t input = 0; input < metadata->Parameters().size(); ++input)
            inputs.push_back({"model:" + String_(std::to_string(input)), metadata->ParameterLabels()[input], "model", input,
                              *metadata->Parameters()[input], "model-coordinate", std::nullopt, 1.0});
        inputs.push_back({"constant:0", "SCALE", "constant", 0, 1.2, "script-number", std::nullopt, 1.0});
        Script::WeightedRiskRequest_ request;
        request.selection_.outputs_ = Vector_<String_>{"output:0", "output:1", "output:2"};
        request.weights_ = Vector_<>{1.25, -0.4, 0.0};
        const auto plan = Script::PlanWeightedRiskRequest(product, inputs, Date_(2026, 1, 1), request, true);
        AadResultAudit_ audit;
        const AuditedWeightedObjective_ objective(plan, &audit);
        AAD::ProfilingData_ statistics;
        std::optional<AAD::ProfilingScope_> profile;
        if (AAD::ProfilingAvailable())
            profile.emplace(&statistics);
        const auto result = Script::MCAADSimulationWithObjective(product, model, paths, "sobol", false, compiled, 0, 0.01, objective);
        profile.reset();
        if (AAD::ProfilingAvailable()) {
            ASSERT_TRUE(statistics.complete_);
            const Script::Detail::ChunkPlan_ chunks(paths);
            const size_t slots = Script::Detail::ChunkPartialCapacity(chunks.BatchCount(), (inputs.size() + 4) * sizeof(double));
            ASSERT_EQ(statistics.memory_.resultArrays_.liveBytes_, (slots + 1) * (inputs.size() + 3) * sizeof(double));
            ASSERT_GE(statistics.memory_.resultArrays_.capacityBytes_, statistics.memory_.resultArrays_.liveBytes_);
            ASSERT_NO_FATAL_FAILURE(AssertChunkProfileCounts(statistics, chunks));
        }
        ASSERT_EQ(audit.axes_, 1);
        ASSERT_EQ(audit.metadataCopies_, 0);
        ASSERT_EQ(result.componentSums_.size(), 3);
        ASSERT_EQ(result.names_, Vector::Join(metadata->ParameterLabels(), product.ConstVarNames()));
        ASSERT_EQ(result.risks_.size(), inputs.size());
        double expectedValue = 0.0;
        Vector_<> expectedRisks(inputs.size(), 0.0);
        const Vector_<String_> components{"a", "b", "c"};
        for (size_t component = 0; component < components.size(); ++component) {
            const auto scalar = ScalarAuditProduct(components[component]);
            const auto reference = Script::MCAADSimulation(scalar, model, paths, "sobol", false, compiled, 0, 0.01);
            ASSERT_NEAR(result.componentSums_[component], reference.aggregated_, 1.0e-10);
            expectedValue += (*request.weights_)[component] * reference.aggregated_;
            for (size_t input = 0; input < inputs.size(); ++input)
                expectedRisks[input] += (*request.weights_)[component] * reference.risks_[input];
        }
        ASSERT_NEAR(result.aggregated_, expectedValue, 1.0e-10);
        for (size_t input = 0; input < inputs.size(); ++input) {
            ASSERT_NEAR(result.risks_[input], expectedRisks[input], 1.0e-10);
            ASSERT_EQ(result[result.names_[input]], result.risks_[input]);
        }
    }
} // namespace

TEST(ScriptTest, TestAadTreeConstructsRiskAxisOnce) {
    const auto audit = AuditAadResultConstruction(false);
    ASSERT_TRUE(audit.valid_);
    ASSERT_EQ(audit.axes_, 1);
}

TEST(ScriptTest, TestAadCompiledConstructsRiskAxisOnce) {
    const auto audit = AuditAadResultConstruction(true);
    ASSERT_TRUE(audit.valid_);
    ASSERT_EQ(audit.axes_, 1);
}

TEST(ScriptTest, TestAadTreeWorkersDoNotCopyRiskMetadata) {
    const auto audit = AuditAadResultConstruction(false);
    ASSERT_TRUE(audit.valid_);
    ASSERT_EQ(audit.metadataCopies_, 0);
}

TEST(ScriptTest, TestAadCompiledWorkersDoNotCopyRiskMetadata) {
    const auto audit = AuditAadResultConstruction(true);
    ASSERT_TRUE(audit.valid_);
    ASSERT_EQ(audit.metadataCopies_, 0);
}

TEST(ScriptTest, TestAadPreparedResultsShareOneRiskAxis) {
    for (const bool compiled : {false, true}) {
        const auto audit = AuditAadResultConstruction(compiled, 17, true);
        ASSERT_TRUE(audit.valid_);
        ASSERT_EQ(audit.axes_, 1);
        ASSERT_EQ(audit.metadataCopies_, 0);
    }
}

TEST(ScriptTest, TestAadZeroPathsKeepOneLabeledZeroResult) {
    for (const bool prepared : {false, true}) {
        for (const bool compiled : {false, true}) {
            const auto audit = AuditAadResultConstruction(compiled, 0, prepared);
            ASSERT_TRUE(audit.valid_);
            ASSERT_EQ(audit.axes_, 1);
            ASSERT_EQ(audit.metadataCopies_, 0);
        }
    }
}

TEST(ScriptTest, TestAadExpiredPreparedResultsKeepOneLabeledZeroResult) {
    for (const bool compiled : {false, true}) {
        const auto audit = AuditAadResultConstruction(compiled, 17, true, true);
        ASSERT_TRUE(audit.valid_);
        ASSERT_EQ(audit.axes_, 1);
        ASSERT_EQ(audit.metadataCopies_, 0);
    }
}

TEST(ScriptTest, TestAadLegacyTypedCollectorKeepsCompatibleWorkerStorage) { AssertLegacyAadStorageFallback<LegacyTypedAadObjective_>(); }

TEST(ScriptTest, TestAadLegacyTypedInputHarvesterKeepsCompatibleWorkerStorage) { AssertLegacyAadStorageFallback<LegacyInputAadObjective_>(); }

TEST(ScriptTest, TestAadLegacyTypedProfilerKeepsCompatibleWorkerStorage) { AssertLegacyAadStorageFallback<LegacyProfiledAadObjective_>(); }

TEST(ScriptTest, TestAadLegacyTypedReducerKeepsCompatibleWorkerStorage) { AssertLegacyAadStorageFallback<LegacyReducedAadObjective_>(); }

TEST(ScriptTest, TestAadLegacyObjectiveNeedsNoNumericFactory) {
    ScopedAadWorkers_ workers(1);
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 1));
    const Handle_<ModelData_> model(new BSModelData_("model", 10.0, 0.2, 0.05, 0.02));
    const auto product = ScalarAuditProduct();
    for (const bool compiled : {false, true}) {
        const auto result = Script::MCAADSimulationWithObjective(product, model, 17, "sobol", false, compiled, 0, 0.01, LegacyAadObjective_{});
        const auto reference = Script::MCAADSimulation(product, model, 17, "sobol", false, compiled, 0, 0.01);
        AssertSameAadResults(result, reference);
    }
}

TEST(ScriptTest, TestAadDirectBatchRetainsExplicitCollectorTemplateArguments) {
    ScopedAadWorkers_ workers(1);
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 1));
    const Handle_<ModelData_> model(new BSModelData_("model", 10.0, 0.2, 0.05, 0.02));
    auto metadata = CreateModel<double>(model);
    const auto product = ScalarAuditProduct();
    const String_ method = "sobol";
    const Script::Detail::AADBatchSettings_ settings{
        method, false, 0, 0.01, 17, metadata->Parameters().size(), product.ConstVarNames().size(), product.PayOffIdx()};
    for (const bool compiled : {false, true}) {
        const auto program = compiled ? std::optional<Script::ScriptCompiled_>(product.Compile(true)) : std::nullopt;
        Script::SimResults_ result(Vector::Join(metadata->ParameterLabels(), product.ConstVarNames()));
        Script::Detail::EvaluateAADBatch<Script::ScriptProduct_, Script::Detail::ScalarPayoffCollector_>(product, model, settings, program, {0, 17},
                                                                                                         &result);
        const auto reference = Script::MCAADSimulation(product, model, 17, method, false, compiled, 0, 0.01);
        AssertSameAadResults(result, reference);
    }
}

TEST(ScriptTest, TestAadLegacyAggregateRetainsLabelsAndNormalizedRisks) {
    const Vector_<String_> names{"spot", "SCALE"};
    Vector_<Script::SimResults_> batches(2, Script::SimResults_(names));
    batches[0].aggregated_ = 13.0;
    batches[0].risks_ = {0.25, -0.125};
    batches[1].aggregated_ = -3.0;
    batches[1].risks_ = {-0.125, 0.5};
    const auto result = Script::Detail::AggregateAADResults(names, batches);
    ASSERT_EQ(result.aggregated_, 10.0);
    ASSERT_EQ(result.names_, names);
    ASSERT_EQ(result["SPOT"], 0.125);
    ASSERT_EQ(result["scale"], 0.375);
    ASSERT_THROW(static_cast<void>(result["missing"]), ScriptError_);
}

TEST(ScriptTest, TestAadWeightedPartialsMatchIndependentScalarObjectives) {
    for (const size_t threads : {1, 4}) {
        for (const bool compiled : {false, true})
            AssertWeightedAadPartials(compiled, 17, threads);
    }
}

TEST(ScriptTest, TestAadWeightedZeroPathsRetainLabelsAndComponents) {
    for (const bool compiled : {false, true})
        AssertWeightedAadPartials(compiled, 0, 4);
}
