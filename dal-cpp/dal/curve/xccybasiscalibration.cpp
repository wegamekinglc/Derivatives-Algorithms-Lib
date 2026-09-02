//
// Created by Codex on 2026/7/13.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <cmath>
#include <memory>
#include <type_traits>
#include <vector>

#include <dal/curve/aadjacobian.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curvejacobian.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/optimization/underdeterminedutils.hpp>
#include <dal/utilities/dictionary.hpp>

namespace Dal {
    namespace {
        DateTime_ ResolveValuationTime(const CrossCurrencyCalibrationSpec_& spec) {
            if (spec.valuationTime_.IsValid()) {
                if (spec.today_.IsValid())
                    REQUIRE(spec.today_ == spec.valuationTime_.Date(), "Cross-currency calibration today must match the explicit valuation date");
                return spec.valuationTime_;
            }
            REQUIRE(spec.today_.IsValid(), "Cross-currency calibration requires today or an explicit valuation time");
            return DateTime_(spec.today_);
        }

        Ccy_ ResolveCollateralCurrency(const CrossCurrencyCalibrationSpec_& spec) {
            return spec.collateralCurrency_.Switch() == Ccy_::Value_::_NOT_SET ? spec.basisPair_.domestic_ : spec.collateralCurrency_;
        }

        void RequireFiniteValues(const Vector_<>& values, const String_& context) {
            for (int i = 0; i < static_cast<int>(values.size()); ++i)
                REQUIRE(std::isfinite(values[i]), context + " has a non-finite value at index " + String::FromInt(i));
        }

        void RequireFiniteValues(const Matrix_<>& values, const String_& context) {
            for (int row = 0; row < values.Rows(); ++row)
                for (int column = 0; column < values.Cols(); ++column)
                    REQUIRE(std::isfinite(values(row, column)),
                            context + " has a non-finite value at row " + String::FromInt(row) + ", column " + String::FromInt(column));
        }

        Vector_<XccyCashflowPlan_> BuildInstrumentPlans(const CrossCurrencyCalibrationSpec_& spec) {
            Vector_<XccyCashflowPlan_> result;
            result.reserve(spec.instruments_.size());
            for (const auto& instrument : spec.instruments_) {
                const auto span = instrument->TimeSpan();
                result.push_back(BuildXccyCashflowPlan(span.first, span.second, instrument->Config()));
            }
            return result;
        }

        Handle_<MarketFixingSnapshot_>
        ResolveFixings(const CrossCurrencyCalibrationSpec_& spec, const DateTime_& valuationTime, const Vector_<XccyCashflowPlan_>& plans) {
            if (spec.fixings_)
                return spec.fixings_;

            Vector_<FixingRequest_> requests;
            for (const auto& plan : plans) {
                const auto required = RequiredHistoricalFixings(plan, valuationTime);
                for (const auto& request : required)
                    requests.push_back(request);
            }
            return SnapshotGlobalFixings(requests);
        }

        CurveDefinition_ BasisDefinition(const CrossCurrencyCalibrationSpec_& spec, const DateTime_& valuationTime) {
            return MakeCurveDefinition(String_("xccy_basis_") + spec.basisPair_.domestic_.String(), spec.basisPair_.domestic_.String(),
                                       CurveParameterization_(CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD),
                                       LogDfScheme_(LogDfScheme_::Value_::LOG_LINEAR), spec.knotDates_, valuationTime.Date(), DayBasis::Act365F());
        }

        void AddEligibilityIssue(
            AnalyticEligibilityReport_* report, AnalyticIneligibilityReason_ reason, int instrumentIndex, int resetIndex, const String_& message) {
            AnalyticEligibilityIssue_ issue;
            issue.reason_ = reason;
            issue.group_ = "staged";
            issue.instrumentIndex_ = instrumentIndex;
            issue.resetIndex_ = resetIndex;
            issue.nativeMessage_ = message;
            report->issues_.push_back(issue);
            report->eligible_ = false;
        }

        void AddGroupedEligibilityIssue(AnalyticEligibilityReport_* report,
                                        AnalyticIneligibilityReason_ reason,
                                        const String_& group,
                                        int instrumentIndex,
                                        int resetIndex,
                                        const String_& message) {
            AnalyticEligibilityIssue_ issue;
            issue.reason_ = reason;
            issue.group_ = group;
            issue.instrumentIndex_ = instrumentIndex;
            issue.resetIndex_ = resetIndex;
            issue.nativeMessage_ = message;
            report->issues_.push_back(issue);
            report->eligible_ = false;
        }

        void ValidateStagedRoute(const CurveBlock_& block,
                                 const RateIndexConvention_& index,
                                 int instrumentIndex,
                                 const String_& leg,
                                 AnalyticEligibilityReport_* report) {
            if (!block.HasDiscount(index.collateral_)) {
                AddGroupedEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::DISCOUNT_ROUTE_MISSING, leg, instrumentIndex, -1,
                                           leg + " discount route is absent");
            }
            if (index.useProjectionCurve_ && !block.HasForward(index.forecastTenor_)) {
                AddGroupedEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::PROJECTION_ROUTE_MISSING, leg, instrumentIndex, -1,
                                           leg + " projection route is absent");
            }
        }

        void ValidateStagedResetMappings(const XccyCashflowPlan_& plan, int instrumentIndex, AnalyticEligibilityReport_* report) {
            switch (plan.config_.notionalMode_.Switch()) {
            case XccyNotionalMode_::Value_::FIXED:
                return;
            case XccyNotionalMode_::Value_::RESETTABLE:
            case XccyNotionalMode_::Value_::MARK_TO_MARKET:
                for (int reset = 0; reset < static_cast<int>(plan.resets_.size()); ++reset) {
                    if (plan.resets_[reset].domesticPeriodIndex_ != reset + 1 ||
                        plan.resets_[reset].domesticPeriodIndex_ >= static_cast<int>(plan.domesticPeriods_.size())) {
                        AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::RESET_MAPPING_INVALID, instrumentIndex, reset,
                                            "reset event does not map consecutively from the second domestic period");
                    }
                }
                return;
            default:
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::NOTIONAL_MODE_UNSUPPORTED, instrumentIndex, -1,
                                    "typed pricing does not support the notional mode");
            }
        }

        void ValidateStagedPlan(const CrossCurrencyCalibrationSpec_& spec,
                                const XccyCashflowPlan_& plan,
                                int instrumentIndex,
                                AnalyticEligibilityReport_* report) {
            if (!(plan.config_.pair_ == spec.basisPair_)) {
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::PAIR_CURRENCY_MISMATCH, instrumentIndex, -1,
                                    "instrument pair does not match the calibration pair");
            }
            if (plan.domesticPeriods_.empty() || plan.foreignPeriods_.empty()) {
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::COUPON_PLAN_EMPTY, instrumentIndex, -1,
                                    "typed pricing requires coupon periods on both legs");
            }
            ValidateStagedResetMappings(plan, instrumentIndex, report);
            if (spec.domesticCurveBlock_)
                ValidateStagedRoute(*spec.domesticCurveBlock_, plan.config_.convention_.domesticIndex_, instrumentIndex, "domestic", report);
            if (spec.foreignCurveBlock_)
                ValidateStagedRoute(*spec.foreignCurveBlock_, plan.config_.convention_.foreignIndex_, instrumentIndex, "foreign", report);
        }

        void ValidateSpec(const CrossCurrencyCalibrationSpec_& spec, const DateTime_& valuationTime, const Ccy_& collateralCurrency) {
            REQUIRE(spec.domesticCurveBlock_, "Cross-currency calibration requires a domestic curve block");
            REQUIRE(spec.foreignCurveBlock_, "Cross-currency calibration requires a foreign curve block");
            REQUIRE(std::isfinite(spec.fxSpot_) && spec.fxSpot_ > 0.0, "Cross-currency calibration requires a positive finite FX spot");
            REQUIRE(spec.basisPair_.domestic_ == spec.domesticCurveBlock_->ccy_,
                    "Cross-currency calibration basis pair domestic currency must match the domestic curve block");
            REQUIRE(spec.basisPair_.foreign_ == spec.foreignCurveBlock_->ccy_,
                    "Cross-currency calibration basis pair foreign currency must match the foreign curve block");
            REQUIRE(collateralCurrency == spec.basisPair_.domestic_, "Cross-currency calibration supports domestic-currency collateral only");
            REQUIRE(!spec.instruments_.empty(), "Cross-currency calibration requires at least one instrument");
            for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i) {
                REQUIRE(spec.instruments_[i], "Cross-currency calibration has an empty XCCY instrument at index " + String::FromInt(i));
                REQUIRE(std::isfinite(spec.instruments_[i]->MarketRate()),
                        "Cross-currency calibration instrument market rate at index " + String::FromInt(i) + " must be finite");
            }
            REQUIRE(!spec.knotDates_.empty(), "Cross-currency calibration requires at least one basis knot date");
            REQUIRE(std::isfinite(spec.smoothingWeight_) && spec.smoothingWeight_ > 0.0,
                    "Cross-currency calibration smoothing weight must be finite and positive");
            REQUIRE(std::isfinite(spec.tolerance_) && spec.tolerance_ > 0.0, "Cross-currency calibration tolerance must be finite and positive");
            REQUIRE(std::isfinite(spec.fitTolerance_) && spec.fitTolerance_ > 0.0,
                    "Cross-currency calibration fit tolerance must be finite and positive");
            REQUIRE(spec.maxEvaluations_ > 0, "Cross-currency calibration max evaluations must be positive");
            REQUIRE(spec.maxRestarts_ > 0, "Cross-currency calibration max restarts must be positive");
            REQUIRE(std::isfinite(spec.initialGuess_), "Cross-currency calibration initial guess must be finite");
            for (int i = 0; i < static_cast<int>(spec.initialGuessPerNode_.size()); ++i)
                REQUIRE(std::isfinite(spec.initialGuessPerNode_[i]),
                        "Cross-currency calibration per-node initial guess at index " + String::FromInt(i) + " must be finite");
            REQUIRE(spec.knotDates_.front() > valuationTime.Date(), "Cross-currency calibration basis knot dates must be after the valuation date");
            for (int i = 1; i < static_cast<int>(spec.knotDates_.size()); ++i)
                REQUIRE(spec.knotDates_[i] > spec.knotDates_[i - 1], "Cross-currency calibration basis knot dates must be strictly increasing");
        }

        template <class T_> class PassiveDiscountCurve_ : public Tape::DiscountCurve_<T_> {
            const Dal::DiscountCurve_* source_;

        public:
            explicit PassiveDiscountCurve_(const Dal::DiscountCurve_& source)
                : Tape::DiscountCurve_<T_>(source.Name(), source.ccy_.String()), source_(&source) {}

            T_ operator()(const Date_& from, const Date_& to) const override { return T_((*source_)(from, to)); }
            void Poll(Vector_<const YCComponent_*>* all) const override { all->push_back(this); }
            void Poll(std::map<const YCComponent_*, Handle_<YCComponent_>>*) const override {}
            [[nodiscard]] std::unique_ptr<YCComponent_> Clone(const String_&, const YCComponent_::substitutions_t&) const override {
                return std::make_unique<PassiveDiscountCurve_>(*source_);
            }
            void Write(Archive::Store_&) const override {}
        };

        template <class T_> struct TypedBlockStorage_ {
            Tape::JointCurveBlock_<T_> block_;
            std::vector<std::unique_ptr<PassiveDiscountCurve_<T_>>> owned_;
        };

        template <class T_> const Tape::DiscountCurve_<T_>* AddPassiveCurve(const DiscountCurve_& source, TypedBlockStorage_<T_>* storage) {
            if constexpr (std::is_same_v<T_, double>) {
                return &source;
            } else {
                storage->owned_.emplace_back(new PassiveDiscountCurve_<T_>(source));
                return storage->owned_.back().get();
            }
        }

        template <class T_> TypedBlockStorage_<T_> BuildTypedBlock(const CurveBlock_& source, const RateIndexConvention_& convention) {
            TypedBlockStorage_<T_> result;
            result.block_.discountCurves[convention.collateral_] = AddPassiveCurve<T_>(source.Discount(convention.collateral_), &result);
            if (convention.useProjectionCurve_)
                result.block_.forwardCurves[convention.forecastTenor_] =
                    AddPassiveCurve<T_>(source.Forward(convention.forecastTenor_, convention.collateral_), &result);
            return result;
        }

        class XccyBasisCalibrationFunc_ : public Underdetermined::Function_ {
            DateTime_ valuationTime_;
            CurrencyPair_ pair_;
            Ccy_ collateralCurrency_;
            Handle_<CurveBlock_> domesticBlock_;
            Handle_<CurveBlock_> foreignBlock_;
            double fxSpot_;
            Handle_<MarketFixingSnapshot_> fixings_;
            Vector_<XccyCashflowPlan_> plans_;
            Vector_<> marketRates_;
            CurveDefinition_ basisDefinition_;
            CurveJacobianMode_ jacobianMode_;
            double bumpSize_;

            template <class T_> Vector_<T_> Residuals(const Vector_<T_>& parameters) const {
                const auto basis = BuildDiscountCurveT<T_>(basisDefinition_, parameters);
                Vector_<T_> result(plans_.size());
                for (int i = 0; i < static_cast<int>(plans_.size()); ++i) {
                    const auto& plan = plans_[i];
                    const auto& convention = plan.config_.convention_;
                    auto domestic = BuildTypedBlock<T_>(*domesticBlock_, convention.domesticIndex_);
                    auto foreign = BuildTypedBlock<T_>(*foreignBlock_, convention.foreignIndex_);

                    XccyMarketView_<T_> market;
                    market.valuationTime_ = valuationTime_;
                    market.pair_ = pair_;
                    market.collateralCurrency_ = collateralCurrency_;
                    market.fxSpot_ = T_(fxSpot_);
                    market.domestic_ = &domestic.block_;
                    market.foreign_ = &foreign.block_;
                    market.basis_ = basis.get();
                    result[i] = PriceXccyParSpread<T_>(plan, market, *fixings_) - T_(marketRates_[i]);
                }
                return result;
            }

        public:
            XccyBasisCalibrationFunc_(const CrossCurrencyCalibrationSpec_& spec,
                                      const DateTime_& valuationTime,
                                      const Ccy_& collateralCurrency,
                                      const Handle_<MarketFixingSnapshot_>& fixings,
                                      const Vector_<XccyCashflowPlan_>& plans,
                                      const CurveDefinition_& basisDefinition,
                                      CurveJacobianMode_ jacobianMode)
                : valuationTime_(valuationTime), pair_(spec.basisPair_), collateralCurrency_(collateralCurrency),
                  domesticBlock_(spec.domesticCurveBlock_), foreignBlock_(spec.foreignCurveBlock_), fxSpot_(spec.fxSpot_), fixings_(fixings),
                  plans_(plans), basisDefinition_(basisDefinition), jacobianMode_(jacobianMode),
                  bumpSize_(spec.solveMode_ == CurveSolveMode_::Value_::EXACT ? 1.0e-6 : 1.0e-4) {
                marketRates_.reserve(spec.instruments_.size());
                for (const auto& instrument : spec.instruments_)
                    marketRates_.push_back(instrument->MarketRate());
            }

            [[nodiscard]] double BumpSize() const override { return bumpSize_; }
            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override { return Residuals<double>(x); }

            void Gradient(const Vector_<>& x, const Vector_<>& f, Matrix_<>* jacobian) const override {
                if (bumpSize_ != 1.0e-6) {
                    Underdetermined::Function_::Gradient(x, f, jacobian);
                    return;
                }
                CentralDifferenceJacobian(
                    x, static_cast<int>(f.size()), bumpSize_, [&](const Vector_<>& parameters) { return Residuals<double>(parameters); }, jacobian);
            }

            [[nodiscard]] std::unique_ptr<Underdetermined::Jacobian_> Gradient(const Vector_<>& x, const Vector_<>&) const override {
                if (jacobianMode_ != CurveJacobianMode_::Value_::ANALYTIC)
                    return nullptr;
                auto* tape = Dal::AAD::Tape();
                TapeGuard_ guard(tape);
                Vector_<Dal::AAD::Number_> parameters = RegisterCurveParameters(x);
                Dal::AAD::NewRecording(*tape);
                Vector_<Dal::AAD::Number_> residuals = Residuals<Dal::AAD::Number_>(parameters);
                return std::make_unique<XCurveJacobian_>(HarvestCurveJacobian(*tape, parameters, residuals));
            }
        };

        struct XccyBasisSolveResult_ {
            Vector_<> parameters_;
            Matrix_<> effJacobianInverse_;
            Matrix_<> forwardJacobian_;
            bool approximate_ = false;
            bool hasEffJacobianInverse_ = false;
        };

        XccyBasisSolveResult_ SolveXccyBasis(const CrossCurrencyCalibrationSpec_& spec,
                                             const CrossCurrencyCalibrationOptions_& options,
                                             const XccyBasisCalibrationFunc_& func,
                                             const Vector_<>& guess,
                                             const Vector_<>& tolerance,
                                             const Sparse::TriDiagonal_& weights) {
            XccyBasisSolveResult_ result;
            result.approximate_ = spec.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE;
            result.hasEffJacobianInverse_ = !result.approximate_ && options.computeEffJacobianInverse_;
            const bool wantForwardJacobian =
                !result.approximate_ && options.computeForwardJacobian_ && options.jacobianMode_ == CurveJacobianMode_::Value_::ANALYTIC;
            result.parameters_ = RunCurveSolver(func, guess, tolerance, !result.approximate_, spec.fitTolerance_, weights, spec.maxEvaluations_,
                                                spec.maxRestarts_, result.hasEffJacobianInverse_ ? &result.effJacobianInverse_ : nullptr,
                                                wantForwardJacobian ? &result.forwardJacobian_ : nullptr);
            return result;
        }

        CrossCurrencyCalibrationDiagnostics_ BuildDiagnostics(const CrossCurrencyCalibrationSpec_& spec,
                                                              const CrossCurrencyCalibrationOptions_& options,
                                                              const XccyBasisCalibrationFunc_& func,
                                                              const Vector_<>& parameters,
                                                              bool usedApproximateFit,
                                                              const Matrix_<>* effJacobianInverse) {
            CrossCurrencyCalibrationDiagnostics_ result;
            result.usedApproximateFit_ = usedApproximateFit;
            result.parameterKnotDates_ = spec.knotDates_;
            result.residualTolerance_ = spec.tolerance_;
            result.jacobianScaling_ = "unscaled";
            result.effJacobianInverseScaling_ = "solver_scaled";
            result.jacobianAvailability_ =
                !options.computeForwardJacobian_
                    ? String_("not_requested")
                    : String_(usedApproximateFit || options.jacobianMode_ != CurveJacobianMode_::Value_::ANALYTIC ? "not_available_for_mode"
                                                                                                                  : "available");
            result.effJacobianInverseAvailability_ =
                !options.computeEffJacobianInverse_ ? String_("not_requested") : String_(usedApproximateFit ? "not_available_for_mode" : "available");
            result.residuals_ = func.F(parameters);
            for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i) {
                result.instrumentNames_.push_back(spec.instruments_[i]->Name());
                result.marketRates_.push_back(spec.instruments_[i]->MarketRate());
                result.modelRates_.push_back(result.marketRates_.back() + result.residuals_[i]);
            }
            const ResidualStats_ stats = ResidualStats(result.residuals_);
            result.maxAbsResidual_ = stats.maxAbsResidual_;
            result.rmsResidual_ = stats.rmsResidual_;
            if (effJacobianInverse)
                result.effJacobianInverse_ = *effJacobianInverse;
            return result;
        }

        CrossCurrencyFxForwardCurve_ BuildFxForwardCurve(const CurrencyPair_& pair,
                                                         const Vector_<Date_>& dates,
                                                         const CrossCurrencyMarket_& market,
                                                         const CollateralType_& collateral) {
            CrossCurrencyFxForwardCurve_ result;
            result.pair_ = pair;
            result.dates_ = dates;
            for (const auto& date : dates)
                result.forwards_.push_back(market.FxForward(market.Today(), date, collateral));
            return result;
        }

        CrossCurrencyCalibrationResult_ AssembleCalibrationResult(const CrossCurrencyCalibrationSpec_& spec,
                                                                  const DateTime_& valuationTime,
                                                                  const Ccy_& collateralCurrency,
                                                                  const Handle_<MarketFixingSnapshot_>& fixings,
                                                                  const CurveDefinition_& basisDefinition,
                                                                  const XccyBasisCalibrationFunc_& func,
                                                                  const CrossCurrencyCalibrationOptions_& options,
                                                                  XccyBasisSolveResult_* solve) {
            Handle_<DiscountCurve_> basisCurve(BuildDiscountCurveUniqueT<double>(basisDefinition, solve->parameters_).release());
            CrossCurrencyMarket_ market(spec.domesticCurveBlock_, spec.foreignCurveBlock_, spec.fxSpot_, valuationTime, collateralCurrency, fixings);
            market.SetBasisCurve(basisCurve);

            std::map<CurrencyPair_, Handle_<DiscountCurve_>> basisCurves;
            basisCurves[spec.basisPair_] = basisCurve;
            CrossCurrencyFxForwardCurve_ fxForwardCurve = BuildFxForwardCurve(spec.basisPair_, spec.knotDates_, market, spec.fxForwardCollateral_);
            const Matrix_<>* effJacobianInverse = solve->hasEffJacobianInverse_ ? &solve->effJacobianInverse_ : nullptr;
            CrossCurrencyCalibrationDiagnostics_ diagnostics =
                BuildDiagnostics(spec, options, func, solve->parameters_, solve->approximate_, effJacobianInverse);
            diagnostics.jacobian_ = std::move(solve->forwardJacobian_);
            RequireFiniteValues(diagnostics.marketRates_, "Cross-currency calibration market rates");
            RequireFiniteValues(diagnostics.modelRates_, "Cross-currency calibration model rates");
            RequireFiniteValues(diagnostics.residuals_, "Cross-currency calibration residuals");
            RequireFiniteResidualStats(ResidualStats_{diagnostics.maxAbsResidual_, diagnostics.rmsResidual_},
                                       "Cross-currency calibration diagnostics");
            RequireFiniteValues(diagnostics.jacobian_, "Cross-currency calibration forward Jacobian");
            RequireFiniteValues(diagnostics.effJacobianInverse_, "Cross-currency calibration effective inverse Jacobian");
            RequireFiniteValues(fxForwardCurve.forwards_, "Cross-currency calibration FX forwards");
            return CrossCurrencyCalibrationResult_(market, basisCurves, fxForwardCurve, diagnostics);
        }
    } // namespace

    AnalyticEligibilityReport_ ValidateCrossCurrencyAnalyticEligibility(const CrossCurrencyCalibrationSpec_& spec) {
        AnalyticEligibilityReport_ report;
        if (spec.domesticCurveBlock_ && !HasAct365FLiborBasis(spec.domesticCurveBlock_->LiborBasis())) {
            AddGroupedEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::LIBOR_BASIS_UNSUPPORTED, "domestic", -1, -1,
                                       "domestic libor basis must be ACT_365F");
        }
        if (spec.foreignCurveBlock_ && !HasAct365FLiborBasis(spec.foreignCurveBlock_->LiborBasis())) {
            AddGroupedEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::LIBOR_BASIS_UNSUPPORTED, "foreign", -1, -1,
                                       "foreign libor basis must be ACT_365F");
        }
        for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i) {
            if (!spec.instruments_[i]) {
                AddEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::CASHFLOW_PLAN_UNSUPPORTED, i, -1, "empty XCCY instrument");
                continue;
            }
            try {
                const auto span = spec.instruments_[i]->TimeSpan();
                const XccyCashflowPlan_ plan = BuildXccyCashflowPlan(span.first, span.second, spec.instruments_[i]->Config());
                ValidateStagedPlan(spec, plan, i, &report);
            } catch (const std::exception& error) {
                AddEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::CASHFLOW_PLAN_UNSUPPORTED, i, -1, String_(error.what()));
            }
        }
        return report;
    }

    CrossCurrencyCalibrationResult_ CalibrateCrossCurrencyMarket(const CrossCurrencyCalibrationSpec_& spec,
                                                                 const CrossCurrencyCalibrationOptions_& options) {
        ++g_curveCalibrationInvocationCount;
        const DateTime_ valuationTime = ResolveValuationTime(spec);
        const Ccy_ collateralCurrency = ResolveCollateralCurrency(spec);
        ValidateSpec(spec, valuationTime, collateralCurrency);
        REQUIRE(options.jacobianMode_ == CurveJacobianMode_::Value_::ANALYTIC || options.jacobianMode_ == CurveJacobianMode_::Value_::BUMPED,
                "Cross-currency calibration requires ANALYTIC or BUMPED Jacobian mode");
        const Vector_<XccyCashflowPlan_> plans = BuildInstrumentPlans(spec);
        const Handle_<MarketFixingSnapshot_> fixings = ResolveFixings(spec, valuationTime, plans);

        const CurveDefinition_ basisDefinition = BasisDefinition(spec, valuationTime);
        const int parameterCount = BuildCurveParameterLayout(basisDefinition).parameterCount_;
        REQUIRE(parameterCount == static_cast<int>(spec.knotDates_.size()),
                "Cross-currency basis calibration requires one piecewise-constant parameter per knot");
        if (!spec.initialGuessPerNode_.empty())
            REQUIRE(static_cast<int>(spec.initialGuessPerNode_.size()) == parameterCount,
                    "Cross-currency calibration initialGuessPerNode_ length must equal its parameter count");

        Vector_<> guess = spec.initialGuessPerNode_.empty() ? Vector_<>(parameterCount, spec.initialGuess_) : spec.initialGuessPerNode_;
        Vector_<> tolerance(spec.instruments_.size(), spec.tolerance_);
        Vector_<DateTime_> knotDateTimes;
        knotDateTimes.reserve(spec.knotDates_.size());
        for (const auto& date : spec.knotDates_)
            knotDateTimes.push_back(DateTime_(date));
        std::unique_ptr<Sparse::TriDiagonal_> weights(Underdetermined::WeightsPWC(knotDateTimes, spec.smoothingWeight_));

        XccyBasisCalibrationFunc_ func(spec, valuationTime, collateralCurrency, fixings, plans, basisDefinition, options.jacobianMode_);
        XccyBasisSolveResult_ solve = SolveXccyBasis(spec, options, func, guess, tolerance, *weights);
        return AssembleCalibrationResult(spec, valuationTime, collateralCurrency, fixings, basisDefinition, func, options, &solve);
    }
} // namespace Dal
