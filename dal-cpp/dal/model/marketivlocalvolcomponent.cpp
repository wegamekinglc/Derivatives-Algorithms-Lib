#include <dal/platform/platform.hpp>

#include <dal/model/marketivlocalvolcomponent.hpp>

namespace Dal::MarketIvLocalVolDetail {
    namespace {
        const double MIN_LOG_SPOT = std::log(std::numeric_limits<double>::denorm_min());
        const double MAX_LOG_SPOT = std::log(std::numeric_limits<double>::max());

        HybridLocalVolEquityData_
        EquityData(const MarketIvMarket_& market, const LocalVolSurfaceData_& surface, const MarketIvLocalVolModelSettings_& settings) {
            ValidateMarketIvLocalVolModelSettings(settings);
            const Handle_<LocalVolSurfaceData_> frozen(new LocalVolSurfaceData_("vol", surface.spots_, surface.times_, surface.vols_));
            return HybridLocalVolEquityData_("equity", settings.index_, settings.currency_, settings.factor_, market.Spot(), 0., frozen,
                                             settings.maxStep_);
        }
    } // namespace

    TermCarryEquity_::TermCarryEquity_(const MarketIvMarket_& market,
                                       const LocalVolSurfaceData_& surface,
                                       const MarketIvLocalVolModelSettings_& settings)
        : delegate_(EquityData(market, surface, settings)), carry_(market), settings_(settings), knots_(carry_.Times()),
          spotRows_(surface.spots_.size()), gridValues_(CheckedMultiply(surface.spots_.size(), surface.times_.size())),
          gridAxes_(CheckedAdd(surface.spots_.size(), surface.times_.size())) {
        knots_.Append(surface.times_);
        std::sort(knots_.begin(), knots_.end());
        knots_.erase(std::unique(knots_.begin(), knots_.end()), knots_.end());
    }

    void TermCarryEquity_::Prepare(const Vector_<>& timeline, const Vector_<>& integratedCarry) {
        if (timeline.empty() || timeline.front() != 0. || integratedCarry.size() != timeline.size() - 1)
            Fail("InvalidMarketIvLocalVolRequest", "Prepare", "/execution/timeline", "zero anchor and one R per interval", "invalid shape");
        CheckLimit(integratedCarry.size(), settings_.maxInternalSteps_, "/settings/max_internal_steps", "Prepare");
        CheckLimit(integratedCarry.size(), settings_.maxSimulationDimension_, "/settings/max_simulation_dimension", "Prepare");
        const size_t steps = integratedCarry.size();
        // Cover old/new columns during re-preparation, pointer arrays, and the delegate's numeric copies.
        const size_t columns = CheckedMultiply(CheckedMultiply(2, CheckedAdd(steps, preparedSteps_)),
                                               CheckedAdd(CheckedMultiply(sizeof(double), spotRows_), sizeof(Vector_<>)));
        const size_t fixed = CheckedMultiply(64, CheckedAdd(gridValues_, CheckedAdd(gridAxes_, knots_.capacity())));
        const size_t workspace = CheckedAdd(columns, CheckedAdd(fixed, CheckedMultiply(128, CheckedAdd(steps, preparedSteps_))));
        CheckLimit(workspace, settings_.workspaceBudgetBytes_, "/settings/workspace_budget_bytes", "Prepare");
        double minPositiveVol = std::numeric_limits<double>::infinity(), maxVol = 0.;
        const auto& nativeParameters = delegate_.Parameters();
        for (size_t i = 2; i < nativeParameters.size(); ++i) {
            const double vol = *nativeParameters[i];
            maxVol = std::max(maxVol, vol);
            if (vol > 0.)
                minPositiveVol = std::min(minPositiveVol, vol);
        }
        Vector_<> drift(integratedCarry.size());
        for (size_t step = 0; step < drift.size(); ++step) {
            const double a = timeline[step], b = timeline[step + 1];
            if (!std::isfinite(a) || !std::isfinite(b) || b <= a || b > settings_.horizon_)
                Fail("InvalidMarketIvLocalVolRequest", "Prepare", "/execution/timeline", "finite increasing times within horizon", ToString(b));
            const double dt = b - a, rootDt = std::sqrt(dt), maxStd = maxVol * rootDt;
            const double minStd = std::isfinite(minPositiveVol) ? minPositiveVol * rootDt : 0.;
            if (dt < std::numeric_limits<double>::min() || !std::isfinite(maxStd * maxStd) ||
                (std::isfinite(minPositiveVol) && minStd * minStd == 0.))
                Fail("MarketIvLocalVolNumericRange", "Prepare", "/execution/steps/" + String_(std::to_string(step)),
                     "normal positive dt and representable variance", ToString(dt));
            const auto expected = carry_.Integrated(a, b);
            if (!std::isfinite(integratedCarry[step]) || std::abs(integratedCarry[step] - expected.rate_) > 2e-12)
                Fail("MarketIvSourceMismatch", "Prepare", "/execution/rate_integral", "same-source logDF endpoint difference",
                     String_(std::to_string(step)));
            drift[step] = integratedCarry[step] - expected.yield_;
        }
        delegate_.Prepare(timeline, drift);
        preparedSteps_ = std::max(preparedSteps_, steps);
    }
    void TermCarryEquity_::ResetState(Vector_<>* state, size_t offset) const { delegate_.ResetState(state, offset); }

    void TermCarryEquity_::Evolve(size_t step,
                                  const Vector_<>& factors,
                                  const Vector_<size_t>& factorSlots,
                                  const double& carryAdjustment,
                                  Vector_<>* state,
                                  size_t stateOffset) const {
        if (carryAdjustment != 0.)
            Fail("InvalidMarketIvLocalVolRequest", "Evaluate", "/execution/carry_adjustment", "deterministic same-source rate", "nonzero adjustment");
        EvolveWithoutCarry(step, factors, factorSlots, state, stateOffset);
    }
    void TermCarryEquity_::EvolveWithoutCarry(
        size_t step, const Vector_<>& factors, const Vector_<size_t>& factorSlots, Vector_<>* state, size_t stateOffset) const {
        delegate_.EvolveWithoutCarry(step, factors, factorSlots, state, stateOffset);
        const double logSpot = (*state)[stateOffset];
        if (!std::isfinite(logSpot) || logSpot <= MIN_LOG_SPOT || logSpot >= MAX_LOG_SPOT)
            static_cast<void>(CheckedExp(logSpot, "/path/spot", "Evaluate"));
    }
    double TermCarryEquity_::Observe(size_t sample, size_t slot, const Vector_<>& state, size_t stateOffset, bool today) const {
        const double result = delegate_.Observe(sample, slot, state, stateOffset, today);
        if (!std::isfinite(result) || result <= 0.)
            Fail("MarketIvLocalVolNumericRange", "Evaluate", "/path/spot", "finite positive spot", "unrepresentable spot");
        return result;
    }
    std::unique_ptr<AAD::HybridComponent_<double>> TermCarryEquity_::Clone() const { return std::make_unique<TermCarryEquity_>(*this); }
} // namespace Dal::MarketIvLocalVolDetail
