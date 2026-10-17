#define DAL_LSMC_STUDY_NO_MAIN
#include "checkpointstudy.cpp"

namespace Dal::Script::CheckpointStudy {
    struct ProductionMetrics_ {
        size_t bound_ = 0;
        size_t peak_ = 0;
        size_t retainedTape_ = 0;
        size_t fixedReserve_ = 0;
        size_t rawRowReserve_ = 0;
        size_t vectorCapacityPeak_ = 0;
        size_t normals_ = 0;
        size_t steps_ = 0;
        size_t events_ = 0;
        size_t comparedRows_ = 0;
        size_t rejectedRestores_ = 0;
        bool checkpoint_ = false;
    };

    Vector_<ExerciseRegression_> FitProduction(
        const Workload_& workload, AAD::Model_<double>* model, const Vector_<>* constants, ProductionMetrics_* metrics, bool bumped = false) {
        LsmcStorage_ empty;
        LsmcContext_ ctx{workload.prepared_, model, workload.scan_, empty, &workload.hardCompiled_, nullptr, LsmcScrambleKey(false, 17), constants};
        ctx.policyBumpTraining_ = bumped;
        if (workload.options_.schedule_ == "materialized") {
            const auto plan = PlanTrainingMemory(ctx, workload.counts_, workload.options_.events_);
            TrainingAllocationBudget_ budget(
                std::numeric_limits<size_t>::max(),
                TrainingAdd(plan.fixed_, TrainingRawRowsBytes(workload.scan_, workload.counts_.training_ + workload.counts_.validation_, 3)));
            Dal::Detail::OwnedBufferScope_ scope(&budget);
            Work_ work;
            auto result = TrainMaterialized(workload, model, constants, &work);
            metrics->peak_ = std::max(metrics->peak_, budget.PeakCapacityBytes());
            const size_t rawRows = TrainingRawRowsBytes(workload.scan_, workload.counts_.training_ + workload.counts_.validation_, 3);
            metrics->fixedReserve_ = std::max(metrics->fixedReserve_, plan.fixed_);
            metrics->rawRowReserve_ = std::max(metrics->rawRowReserve_, rawRows);
            metrics->vectorCapacityPeak_ = std::max(metrics->vectorCapacityPeak_, budget.PeakCapacityBytes() - plan.fixed_ - rawRows);
            metrics->retainedTape_ = std::max(metrics->retainedTape_, plan.tape_);
            metrics->normals_ += work.normalVectors_;
            metrics->steps_ += work.modelSteps_;
            metrics->events_ += work.scriptEvents_;
            return result;
        }
        LsmcDiagnostics_ diagnostics;
        ctx.trainingDiagnostics_ = &diagnostics;
        auto result = TrainFrozenPolicy(ctx, workload.counts_).regressions_;
        const auto plan = PlanTrainingMemory(ctx, workload.counts_, workload.options_.block_);
        const size_t rawRows = diagnostics.checkpointTraining_
                                   ? plan.rawBlock_
                                   : TrainingRawRowsBytes(workload.scan_, workload.counts_.training_ + workload.counts_.validation_, 3);
        metrics->fixedReserve_ = std::max(metrics->fixedReserve_, plan.fixed_);
        metrics->rawRowReserve_ = std::max(metrics->rawRowReserve_, rawRows);
        metrics->vectorCapacityPeak_ = std::max(metrics->vectorCapacityPeak_, diagnostics.trainingAllocationPeak_ - plan.fixed_ - rawRows);
        metrics->bound_ = std::max(metrics->bound_, diagnostics.trainingAllocationBound_);
        metrics->peak_ = std::max(metrics->peak_, diagnostics.trainingAllocationPeak_);
        metrics->retainedTape_ = std::max(metrics->retainedTape_, diagnostics.retainedTapeBytes_);
        metrics->normals_ += diagnostics.trainingNormalVectors_;
        metrics->steps_ += diagnostics.trainingModelSteps_;
        metrics->events_ += diagnostics.trainingScriptEvents_;
        metrics->checkpoint_ |= diagnostics.checkpointTraining_;
        return result;
    }

    void VerifyProductionRows(const Workload_& workload, ProductionMetrics_* metrics) {
        Work_ excluded;
        const auto expected = Materialize(workload, workload.model_.get(), nullptr, workload.counts_.training_, 0, &excluded);
        LsmcStorage_ empty;
        LsmcContext_ ctx{workload.prepared_,        workload.model_.get(), workload.scan_, empty, &workload.hardCompiled_, nullptr,
                         LsmcScrambleKey(false, 17)};
        PassiveTrainingWorkspace_ workspace(ctx);
        TrainingCheckpoints_ checkpoints(ctx, workload.counts_.training_, 0, workload.options_.block_);
        workspace.Capture(&checkpoints);
        for (size_t first = 0; first < workload.options_.events_; first += workload.options_.block_) {
            const size_t last = std::min(workload.options_.events_, first + workload.options_.block_);
            const auto slice = CheckpointSlice(workload.scan_, first, last);
            auto actual = MakeStorage(slice, workload.counts_.training_, 3, false);
            workspace.Record(slice, workload.counts_.training_, 0, first, last, &actual, &checkpoints);
            CompareRows(actual, expected, slice, workload.scan_, first, last, &excluded);
        }
        metrics->comparedRows_ = excluded.rowValuesCompared_;
        checkpoints.parameters_[0] += 1.0;
        bool rejected = false;
        try {
            checkpoints.RequireIdentity(ctx, 0);
        } catch (const Exception_&) {
            rejected = true;
        }
        REQUIRE(rejected, "production restore must reject a different model point");
        ++metrics->rejectedRestores_;
        checkpoints.parameters_[0] -= 1.0;
        checkpoints.constants_[0] += 1.0;
        rejected = false;
        try {
            checkpoints.RequireIdentity(ctx, 0);
        } catch (const Exception_&) {
            rejected = true;
        }
        REQUIRE(rejected, "production restore must reject a different constant point");
        ++metrics->rejectedRestores_;
        checkpoints.constants_[0] -= 1.0;
        checkpoints.stream_ = 0;
        rejected = false;
        try {
            checkpoints.RequireIdentity(ctx, 0);
        } catch (const Exception_&) {
            rejected = true;
        }
        REQUIRE(rejected, "production restore must reject a different stream");
        ++metrics->rejectedRestores_;
        checkpoints.stream_ = ctx.scrambleKey_;
        ++checkpoints.states_[0].model_.next_;
        rejected = false;
        auto storage = MakeStorage(workload.scan_, workload.counts_.training_, 3, false);
        try {
            workspace.Record(workload.scan_, workload.counts_.training_, 0, 0, workload.options_.events_, &storage, &checkpoints);
        } catch (const Exception_&) {
            rejected = true;
        }
        REQUIRE(rejected, "production restore must reject a different boundary");
        ++metrics->rejectedRestores_;
    }

    Vector_<ExerciseRegression_> FitProductionPoint(const Workload_& workload, size_t input, double bump, ProductionMetrics_* metrics) {
        if (input < 4) {
            auto model = workload.model_->Clone();
            *model->Parameters()[input] += bump;
            model->Init(workload.prepared_.TimeLine(), workload.prepared_.DefLine());
            return FitProduction(workload, model.get(), nullptr, metrics, true);
        }
        auto constants = workload.prepared_.Product().ConstVarValues();
        constants[input - 4] += bump;
        return FitProduction(workload, workload.model_.get(), &constants, metrics, true);
    }

    double ProductionPolicyValue(const Workload_& workload, const Vector_<ExerciseRegression_>& policy) {
        return ValueFuzzyPolicy(workload.prepared_, workload.modelData_, workload.scan_, policy, &workload.prepared_.CompiledProgram(true),
                                BatchPlan_(workload.options_.pricing_, 1), workload.counts_, workload.options_.pricing_);
    }

    SimResults_ ProductionResult(const Workload_& workload, ProductionMetrics_* metrics) {
        const auto policy = FitProduction(workload, workload.model_.get(), nullptr, metrics);
        Vector_<size_t> counts;
        auto result = Price(workload, policy, workload.options_.mode_ != "hard", &counts);
        if (workload.options_.mode_ != "retrained")
            return result;
        const auto& constants = workload.prepared_.Product().ConstVarValues();
        for (size_t input = 0; input < 4 + constants.size(); ++input) {
            const double value = input < 4 ? *workload.model_->Parameters()[input] : constants[input - 4];
            const double step = workload.prepared_.Simulation().lsmcPolicyBumpRelative_ * std::max(1.0, std::fabs(value));
            const auto upPolicy = FitProductionPoint(workload, input, step, metrics);
            const double up = ProductionPolicyValue(workload, upPolicy);
            const auto downPolicy = FitProductionPoint(workload, input, -step, metrics);
            const double down = ProductionPolicyValue(workload, downPolicy);
            result.risks_[input] += (up - down) / (2.0 * step);
        }
        return result;
    }

    void RunProduction(const Options_& options) {
        const auto start = std::chrono::steady_clock::now();
        const Workload_ workload(options, options.schedule_ != "materialized");
        const auto* initialTape = AAD::NativeThread::retainedTape;
        ProductionMetrics_ metrics;
        const auto result = ProductionResult(workload, &metrics);
        const size_t postPricingTape = RetainedTrainingTapeBytes();
        if (options.mode_ == "hard")
            REQUIRE(AAD::NativeThread::retainedTape == initialTape, "passive training must not create an AAD tape");
        if (options.schedule_ == "verify") {
            VerifyProductionRows(workload, &metrics);
            Work_ excluded;
            const auto reference = TrainMaterialized(workload, workload.model_.get(), nullptr, &excluded);
            const auto actual = FitProduction(workload, workload.model_.get(), nullptr, &metrics);
            ComparePolicy(actual, reference);
            Vector_<size_t> actualCounts, expectedCounts;
            CompareResult(Price(workload, actual, false, &actualCounts), Price(workload, reference, false, &expectedCounts));
            REQUIRE(actualCounts == expectedCounts, "production exercise counts differ");
            if (options.mode_ != "hard")
                CompareResult(result, MCLsmcAadSimulation(workload.prepared_, workload.modelData_, options.pricing_));
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << std::setprecision(17) << "{\"schedule\":\"" << options.schedule_ << "\",\"mode\":\"" << options.mode_
                  << "\",\"checkpoint\":" << (metrics.checkpoint_ ? "true" : "false") << ",\"seconds\":" << seconds
                  << ",\"allocation_bound_bytes\":" << metrics.bound_ << ",\"allocation_peak_bytes\":" << metrics.peak_
                  << ",\"retained_tape_bytes\":" << metrics.retainedTape_ << ",\"normal_vectors\":" << metrics.normals_
                  << ",\"post_pricing_tape_bytes\":" << postPricingTape << ",\"fixed_reserve_bytes\":" << metrics.fixedReserve_
                  << ",\"raw_row_reserve_bytes\":" << metrics.rawRowReserve_ << ",\"vector_capacity_peak_bytes\":" << metrics.vectorCapacityPeak_
                  << ",\"model_steps\":" << metrics.steps_ << ",\"script_events\":" << metrics.events_
                  << ",\"compared_rows\":" << metrics.comparedRows_ << ",\"rejected_restores\":" << metrics.rejectedRestores_
                  << ",\"pv\":" << result.aggregated_ / static_cast<double>(options.pricing_) << ",\"risks\":[";
        for (size_t j = 0; j < result.risks_.size(); ++j) {
            if (j)
                std::cout << ',';
            std::cout << result.risks_[j];
        }
        std::cout << "]}\n";
    }
} // namespace Dal::Script::CheckpointStudy

int main(int argc, char** argv) {
    try {
        Dal::RegisterAll_::Init();
        Dal::ThreadPool_::GetInstance()->Start(1, true);
        Dal::Script::CheckpointStudy::RunProduction(Dal::Script::CheckpointStudy::ReadOptions(argc, argv));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
