//
// Created by Codex on 2026/10/09.
//

#include <algorithm>
#include <cmath>
#include <limits>

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/segmentedmontecarlo.hpp>
#include <dal/script/simulation.hpp>

namespace Dal::Script {
    namespace {
        constexpr size_t SEGMENTED_BATCH_SIZE = 32;
        constexpr size_t MAX_SEGMENTED_LANES = 64;
        struct Resources_ {
            size_t tape_ = 0;
            size_t checkpoints_ = 0;
            size_t cleanup_ = 0;
        };

        struct Sum_ {
            double value_ = 0.0;
            Vector_<> gradient_;
            explicit Sum_(size_t parameters) : gradient_(parameters, 0.0) {}
        };

        struct Lane_ {
            std::unique_ptr<Random_> random_;
            Vector_<> gaussian_;
            Sum_ batch_;
            Resources_ resources_{};
            Lane_(size_t parameters, size_t dimension) : gaussian_(dimension), batch_(parameters) {}
        };

        void ValidateRange(size_t paths, size_t dimension, const SegmentedMonteCarloSettings_& settings) {
            REQUIRE2(paths > 0, "SegmentedMonteCarlo: pathCount must be positive", ScriptError_);
            constexpr size_t MAX_SIZE = std::numeric_limits<size_t>::max();
            REQUIRE2(settings.firstPath_ <= MAX_SIZE - paths, "SegmentedMonteCarlo: firstPath + pathCount overflows size_t", ScriptError_);
            const size_t end = settings.firstPath_ + paths;
            if (settings.rsg_ == "sobol" && dimension != 0) {
                REQUIRE2(end <= std::numeric_limits<std::uint32_t>::max(), "SegmentedMonteCarlo: Sobol path range exceeds 32-bit directions",
                         ScriptError_);
            } else {
                REQUIRE2(dimension == 0 || end <= MAX_SIZE / dimension, "SegmentedMonteCarlo: normal draw range overflows size_t", ScriptError_);
            }
            REQUIRE2(dimension <= static_cast<size_t>(std::numeric_limits<int>::max()),
                     "SegmentedMonteCarlo: driver dimension exceeds generator/bridge integer range", ScriptError_);
        }

        void AccumulateRisk(Sum_* sum, double value, const Vector_<>& gradient) {
            sum->value_ += value;
            REQUIRE2(std::isfinite(sum->value_), "SegmentedMonteCarlo: nonfinite price sum", ScriptError_);
            for (size_t i = 0; i < gradient.size(); ++i) {
                sum->gradient_[i] += gradient[i];
                REQUIRE2(std::isfinite(sum->gradient_[i]), "SegmentedMonteCarlo: nonfinite gradient sum", ScriptError_);
            }
        }

        void Capture(const Resources_& path, Resources_* peak) {
            peak->tape_ = std::max(peak->tape_, path.tape_);
            peak->checkpoints_ = std::max(peak->checkpoints_, path.checkpoints_);
            peak->cleanup_ = std::max(peak->cleanup_, path.cleanup_);
        }

        Vector_<Lane_> MakeLanes(size_t count, size_t parameters, size_t dimension, std::unique_ptr<Random_> prototype) {
            using AAD::SegmentedPathDetail::Product;
            (void)Product(count, sizeof(Lane_));
            (void)Product(Product(count, parameters), sizeof(double));
            (void)Product(Product(count, dimension), sizeof(double));
            Vector_<Lane_> result;
            result.reserve(count);
            for (size_t i = 0; i < count; ++i)
                result.emplace_back(parameters, dimension);
            for (size_t i = 1; i < count; ++i)
                result[i].random_ = prototype ? prototype->Clone() : nullptr;
            result[0].random_ = std::move(prototype);
            return result;
        }

        void EvaluateBatch(const BlackScholesSegmentedPath_& kernel,
                           const Vector_<>& parameters,
                           const AAD::SegmentedPathSettings_& settings,
                           const PathBatch_& batch,
                           Lane_* lane) {
            lane->batch_.value_ = 0.0;
            for (double& value : lane->batch_.gradient_)
                value = 0.0;
            lane->resources_ = {};
            if (lane->random_)
                lane->random_->SkipNormalTo(batch.firstPath_);
            for (size_t i = 0; i < batch.pathCount_; ++i) {
                if (lane->random_)
                    lane->random_->FillNormal(&lane->gaussian_);
                const auto path = kernel.Evaluate(parameters, lane->gaussian_, settings);
                AccumulateRisk(&lane->batch_, path.Value(), path.Gradient());
                const auto& usage = path.Execution();
                Capture({usage.peakTapeBytes_, usage.checkpointBytes_, usage.cleanupReserveBytes_}, &lane->resources_);
            }
        }

        SegmentedMonteCarloExecution_ Execution(size_t paths, const SegmentedMonteCarloSettings_& settings) {
            SegmentedMonteCarloExecution_ result;
            result.pathCount_ = paths;
            result.firstPath_ = settings.firstPath_;
            result.batches_ = paths / SEGMENTED_BATCH_SIZE + static_cast<size_t>(paths % SEGMENTED_BATCH_SIZE != 0);
            result.segmentSteps_ = settings.path_.segmentSteps_;
            result.rsg_ = settings.rsg_;
            result.useBb_ = settings.useBb_;
            result.scrambleKey_ = settings.scrambleKey_;
            return result;
        }
    } // namespace

    SegmentedMonteCarloResult_ EvaluateBlackScholesSegmentedMonteCarlo(const BlackScholesSegmentedPath_& kernel,
                                                                       const Vector_<>& parameters,
                                                                       size_t pathCount,
                                                                       const SegmentedMonteCarloSettings_& settings) {
        AAD::RequireRecordingModeChangeAllowed();
        kernel.ValidateRequest(parameters, settings.path_);
        const SegmentedMonteCarloSettings_ request = settings;
        ValidateRange(pathCount, kernel.SimDim(), request);
        auto prototype = CreateRNG(request.rsg_, kernel.SimDim(), request.useBb_, request.scrambleKey_);
        const Vector_<> inputs = parameters;
        auto execution = Execution(pathCount, request);
        ThreadPool_* pool = ThreadPool_::GetInstance();
        execution.lanes_ = std::min({MAX_SEGMENTED_LANES, execution.batches_, pool->NumThreads()});
        REQUIRE2(execution.lanes_ > 0, "SegmentedMonteCarlo: thread pool must have a positive lane count", ScriptError_);
        auto lanes = MakeLanes(execution.lanes_, inputs.size(), kernel.SimDim(), std::move(prototype));
        Sum_ total(inputs.size());
        Resources_ resources{};
        for (size_t first = 0; first < execution.batches_;) {
            const size_t count = std::min(lanes.size(), execution.batches_ - first);
            SimulationTaskGroup_ tasks(pool, count);
            for (size_t slot = 0; slot < count; ++slot) {
                const size_t relative = (first + slot) * SEGMENTED_BATCH_SIZE;
                const PathBatch_ batch{request.firstPath_ + relative, std::min(SEGMENTED_BATCH_SIZE, pathCount - relative)};
                Lane_* lane = &lanes[slot];
                tasks.Spawn([&, batch, lane]() {
                    EvaluateBatch(kernel, inputs, request.path_, batch, lane);
                    return true;
                });
            }
            tasks.Complete();
            for (size_t slot = 0; slot < count; ++slot) {
                AccumulateRisk(&total, lanes[slot].batch_.value_, lanes[slot].batch_.gradient_);
                Capture(lanes[slot].resources_, &resources);
            }
            first += count;
        }
        execution.maxPathTapeBytes_ = resources.tape_;
        execution.maxPathCheckpointBytes_ = resources.checkpoints_;
        execution.maxPathCleanupReserveBytes_ = resources.cleanup_;
        const double divisor = static_cast<double>(pathCount);
        for (double& value : total.gradient_)
            value /= divisor;
        return {total.value_ / divisor, std::move(total.gradient_), kernel.ParameterLabels(), std::move(execution)};
    }
} // namespace Dal::Script
