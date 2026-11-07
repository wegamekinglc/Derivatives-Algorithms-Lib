#pragma once

#include <dal/math/pde/earlyexercisepde.hpp>

namespace Dal::PDE::EarlyExercisePdeInternal {
    struct PlanStep_ {
        double target_, source_, theta_;
        int carry_;
        bool damped_;
        std::optional<Date_> date_;
    };
    struct PlanCounts_ {
        int steps_ = 0, base_ = 0, damping_ = 0, upwind_ = 0;
    };
    struct PlanLayout_ {
        PlanCounts_ counts_;
        size_t payloadBytes_, workspaceBytes_, planBytes_, canonicalWorkspaceBytes_;
    };
    struct RollbackControl_ {
        bool failFirstSolve_ = false, extraFirstSolve_ = false, nonfinitePrice_ = false;
    };
    // Counts and envelopes only: no owning request, geometry or canonical buffer.
    [[nodiscard]] PlanLayout_ PreviewPlan(const EarlyExercisePdeRequest_&, const Vector_<EarlyExercisePdeMarket_>& extraMarkets = {});

    // Internal owning geometry; execution consumes it without replanning.
    class Plan_ {
        EarlyExercisePdeRequest_ request_;
        Vector_<double> grid_;
        Vector_<EarlyExercisePdeTimeNode_> timeGrid_;
        Vector_<PlanStep_> steps_;
        PlanCounts_ counts_;
        int basePlannedSteps_ = 0;
        double epsilon_ = 0.0;
        String_ id_;
        size_t canonicalBytes_ = 0, storageBytes_ = 0, canonicalWorkspaceBytes_ = 0, peakTemporaryBytes_ = 0;
        Plan_() = default;
        friend Plan_ BuildPlan(const EarlyExercisePdeRequest_&, const Vector_<EarlyExercisePdeMarket_>&);
        friend struct ResultAccess_;

    public:
        [[nodiscard]] const EarlyExercisePdeRequest_& Request() const { return request_; }
        [[nodiscard]] const Vector_<double>& Grid() const { return grid_; }
        [[nodiscard]] const Vector_<EarlyExercisePdeTimeNode_>& TimeGrid() const { return timeGrid_; }
        [[nodiscard]] const Vector_<PlanStep_>& Steps() const { return steps_; }
        [[nodiscard]] const PlanCounts_& Counts() const { return counts_; }
        [[nodiscard]] int BasePlannedSteps() const { return basePlannedSteps_; }
        [[nodiscard]] double Upper() const { return *request_.settings_.upper_; }
        [[nodiscard]] double Epsilon() const { return epsilon_; }
        [[nodiscard]] const String_& Id() const { return id_; }
        [[nodiscard]] size_t CanonicalBytes() const { return canonicalBytes_; }
        [[nodiscard]] size_t StorageBytes() const { return storageBytes_; }
        [[nodiscard]] size_t CanonicalWorkspaceBytes() const { return canonicalWorkspaceBytes_; }
        [[nodiscard]] size_t PeakTemporaryBytes() const { return peakTemporaryBytes_; }
    };

    // Base is always included. Extra markets must have the same dated carry layout.
    [[nodiscard]] Plan_ BuildPlan(const EarlyExercisePdeRequest_& base, const Vector_<EarlyExercisePdeMarket_>& extraMarkets = {});
    [[nodiscard]] EarlyExercisePdeResult_ Rollback(const EarlyExercisePdeRequest_& scenario, const Plan_& plan);
    [[nodiscard]] EarlyExercisePdeResult_ RollbackForRisk(const EarlyExercisePdeRequest_&, const Plan_&, const RollbackControl_* = nullptr);
} // namespace Dal::PDE::EarlyExercisePdeInternal
