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
                                       LogDfScheme_(LogDfScheme_::Value_::LOG_LINEAR), spec.knotDates_, valuationTime.Date(), DayBasis_("ACT_365F"));
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
            REQUIRE(!spec.knotDates_.empty(), "Cross-currency calibration requires at least one basis knot date");
            REQUIRE(spec.smoothingWeight_ > 0.0, "Cross-currency calibration smoothing weight must be positive");
            REQUIRE(spec.tolerance_ > 0.0, "Cross-currency calibration tolerance must be positive");
            REQUIRE(spec.fitTolerance_ > 0.0, "Cross-currency calibration fit tolerance must be positive");
            REQUIRE(spec.maxEvaluations_ > 0, "Cross-currency calibration max evaluations must be positive");
            REQUIRE(spec.maxRestarts_ > 0, "Cross-currency calibration max restarts must be positive");
            REQUIRE(std::isfinite(spec.initialGuess_), "Cross-currency calibration initial guess must be finite");
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
            [[nodiscard]] PassiveDiscountCurve_* Clone(const String_&, const YCComponent_::substitutions_t&) const override {
                return new PassiveDiscountCurve_(*source_);
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
                  plans_(plans), basisDefinition_(basisDefinition), jacobianMode_(jacobianMode) {
                marketRates_.reserve(spec.instruments_.size());
                for (const auto& instrument : spec.instruments_)
                    marketRates_.push_back(instrument->MarketRate());
            }

            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override { return Residuals<double>(x); }

            [[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>& x, const Vector_<>&) const override {
                if (jacobianMode_ != CurveJacobianMode_::Value_::ANALYTIC)
                    return nullptr;
                auto* tape = Dal::AAD::Tape();
                TapeGuard_ guard(tape);
                Vector_<Dal::AAD::Number_> parameters = RegisterCurveParameters(x);
                Dal::AAD::NewRecording(*tape);
                Vector_<Dal::AAD::Number_> residuals = Residuals<Dal::AAD::Number_>(parameters);
                return new XCurveJacobian_(HarvestCurveJacobian(*tape, parameters, residuals));
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
                                             const Sparse::TriDiagonal_& weights,
                                             const UnderdeterminedControls_& controls) {
            XccyBasisSolveResult_ result;
            result.approximate_ = spec.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE;
            const bool wantForwardJacobian =
                options.computeForwardJacobian_ && options.jacobianMode_ == CurveJacobianMode_::Value_::ANALYTIC && !result.approximate_;
            if (result.approximate_) {
                result.parameters_ = Underdetermined::Approximate(func, guess, tolerance, spec.fitTolerance_, weights, controls);
                return result;
            }

            result.hasEffJacobianInverse_ = options.computeEffJacobianInverse_;
            std::unique_ptr<Sparse::SymmetricDecomposition_> decomposition(weights.DecomposeSymmetric());
            result.parameters_ = Underdetermined::Find(func, guess, tolerance, *decomposition, controls,
                                                       result.hasEffJacobianInverse_ ? &result.effJacobianInverse_ : nullptr,
                                                       wantForwardJacobian ? &result.forwardJacobian_ : nullptr);
            return result;
        }

        CrossCurrencyCalibrationDiagnostics_ BuildDiagnostics(const CrossCurrencyCalibrationSpec_& spec,
                                                              const XccyBasisCalibrationFunc_& func,
                                                              const Vector_<>& parameters,
                                                              bool usedApproximateFit,
                                                              const Matrix_<>* effJacobianInverse) {
            CrossCurrencyCalibrationDiagnostics_ result;
            result.usedApproximateFit_ = usedApproximateFit;
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
                                                                  XccyBasisSolveResult_* solve) {
            Handle_<DiscountCurve_> basisCurve(BuildDiscountCurveUniqueT<double>(basisDefinition, solve->parameters_).release());
            CrossCurrencyMarket_ market(spec.domesticCurveBlock_, spec.foreignCurveBlock_, spec.fxSpot_, valuationTime, collateralCurrency, fixings);
            market.SetBasisCurve(basisCurve);

            std::map<CurrencyPair_, Handle_<DiscountCurve_>> basisCurves;
            basisCurves[spec.basisPair_] = basisCurve;
            CrossCurrencyFxForwardCurve_ fxForwardCurve = BuildFxForwardCurve(spec.basisPair_, spec.knotDates_, market, spec.fxForwardCollateral_);
            const Matrix_<>* effJacobianInverse = solve->hasEffJacobianInverse_ ? &solve->effJacobianInverse_ : nullptr;
            CrossCurrencyCalibrationDiagnostics_ diagnostics =
                BuildDiagnostics(spec, func, solve->parameters_, solve->approximate_, effJacobianInverse);
            diagnostics.jacobian_ = std::move(solve->forwardJacobian_);
            return CrossCurrencyCalibrationResult_(market, basisCurves, fxForwardCurve, diagnostics);
        }
    } // namespace

    CrossCurrencyCalibrationResult_ CalibrateCrossCurrencyMarket(const CrossCurrencyCalibrationSpec_& spec,
                                                                 const CrossCurrencyCalibrationOptions_& options) {
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

        Vector_<> guess(parameterCount, spec.initialGuess_);
        Vector_<> tolerance(spec.instruments_.size(), spec.tolerance_);
        Vector_<DateTime_> knotDateTimes;
        knotDateTimes.reserve(spec.knotDates_.size());
        for (const auto& date : spec.knotDates_)
            knotDateTimes.push_back(DateTime_(date));
        std::unique_ptr<Sparse::TriDiagonal_> weights(Underdetermined::WeightsPWC(knotDateTimes, spec.smoothingWeight_));

        constexpr const char* KEY_MAX_EVALUATIONS = "MAXEVALUATIONS";
        constexpr const char* KEY_MAX_RESTARTS = "MAXRESTARTS";
        Dictionary_ controlsDictionary;
        controlsDictionary.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(spec.maxEvaluations_)));
        controlsDictionary.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(spec.maxRestarts_)));
        UnderdeterminedControls_ controls(controlsDictionary);

        XccyBasisCalibrationFunc_ func(spec, valuationTime, collateralCurrency, fixings, plans, basisDefinition, options.jacobianMode_);
        XccyBasisSolveResult_ solve = SolveXccyBasis(spec, options, func, guess, tolerance, *weights, controls);
        return AssembleCalibrationResult(spec, valuationTime, collateralCurrency, fixings, basisDefinition, func, &solve);
    }
} // namespace Dal
