#pragma once

#include <dal/model/hybrid.hpp>
#include <dal/model/marketivlocalvolinternal.hpp>

namespace Dal::MarketIvLocalVolDetail {
    class TermCarryEquity_ final : public AAD::HybridComponent_<double> {
        AAD::HybridLocalVolEquity_<double> delegate_;
        Carry_ carry_;
        MarketIvLocalVolModelSettings_ settings_;
        Vector_<> knots_;
        Vector_<double*> parameters_;
        Vector_<String_> labels_;
        size_t spotRows_, gridValues_, gridAxes_, preparedSteps_ = 0;

    public:
        TermCarryEquity_(const MarketIvMarket_& market,
                         const LocalVolSurfaceData_& surface,
                         const MarketIvLocalVolModelSettings_& settings,
                         const Vector_<>& extraKnots = {});
        [[nodiscard]] const String_& Name() const override { return delegate_.Name(); }
        [[nodiscard]] const String_& Currency() const override { return delegate_.Currency(); }
        [[nodiscard]] size_t StateDim() const override { return delegate_.StateDim(); }
        [[nodiscard]] size_t FactorDim() const override { return delegate_.FactorDim(); }
        [[nodiscard]] const Vector_<String_>& FactorNames() const override { return delegate_.FactorNames(); }
        [[nodiscard]] const Vector_<String_>& ObservableNames() const override { return delegate_.ObservableNames(); }
        [[nodiscard]] const Vector_<double*>& Parameters() const override { return parameters_; }
        [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return labels_; }
        [[nodiscard]] double MaxStep() const override { return settings_.maxStep_; }
        [[nodiscard]] Vector_<> TimelineKnots() const override { return knots_; }
        void Prepare(const Vector_<>& timeline, const Vector_<>& integratedCarry) override;
        void ResetState(Vector_<>* state, size_t offset) const override;
        void Evolve(size_t step,
                    const Vector_<>& factors,
                    const Vector_<size_t>& factorSlots,
                    const double& carryAdjustment,
                    Vector_<>* state,
                    size_t stateOffset) const override;
        void EvolveWithoutCarry(
            size_t step, const Vector_<>& factors, const Vector_<size_t>& factorSlots, Vector_<>* state, size_t stateOffset) const override;
        [[nodiscard]] double Observe(size_t sample, size_t slot, const Vector_<>& state, size_t stateOffset, bool today) const override;
        [[nodiscard]] std::unique_ptr<AAD::HybridComponent_<double>> Clone() const override;
    };
} // namespace Dal::MarketIvLocalVolDetail
