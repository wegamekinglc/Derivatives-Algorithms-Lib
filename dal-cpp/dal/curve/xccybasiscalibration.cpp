#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <cmath>
#include <memory>
#include <type_traits>
#include <vector>

#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/jointcalibration_internal.hpp>
#include <dal/curve/xccybasiscalibration.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/curve/xccyscript_internal.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/optimization/underdeterminedutils.hpp>
#include <dal/storage/json.hpp>
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

        void ValidateStagedRoute(const CurveBlock_& block,
                                 const RateIndexConvention_& index,
                                 int instrumentIndex,
                                 const String_& leg,
                                 AnalyticEligibilityReport_* report) {
            if (!block.HasDiscount(index.collateral_)) {
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::DISCOUNT_ROUTE_MISSING, leg, -1, instrumentIndex, -1,
                                    leg + " discount route is absent");
            }
            if (index.useProjectionCurve_ && !block.HasForward(index.forecastTenor_)) {
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::PROJECTION_ROUTE_MISSING, leg, -1, instrumentIndex, -1,
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
                    if (!JointCalibrationInternal::ValidResetDomesticMapping(plan, reset)) {
                        AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::RESET_MAPPING_INVALID, "staged", -1, instrumentIndex, reset,
                                            "reset event does not map consecutively from the second domestic period");
                    }
                }
                return;
            default:
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::NOTIONAL_MODE_UNSUPPORTED, "staged", -1, instrumentIndex, -1,
                                    "typed pricing does not support the notional mode");
            }
        }

        void ValidateStagedPlan(const CrossCurrencyCalibrationSpec_& spec,
                                const XccyCashflowPlan_& plan,
                                int instrumentIndex,
                                AnalyticEligibilityReport_* report) {
            if (!(plan.config_.pair_ == spec.basisPair_)) {
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::PAIR_CURRENCY_MISMATCH, "staged", -1, instrumentIndex, -1,
                                    "instrument pair does not match the calibration pair");
            }
            if (plan.domesticPeriods_.empty() || plan.foreignPeriods_.empty()) {
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::COUPON_PLAN_EMPTY, "staged", -1, instrumentIndex, -1,
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

            TypedBlockStorage_() = default;
            // Vector_ reallocation must move rather than fall back to a deleted copy.
            TypedBlockStorage_(TypedBlockStorage_&&) noexcept = default;
            TypedBlockStorage_& operator=(TypedBlockStorage_&&) noexcept = default;
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
            // The domestic/foreign blocks depend only on each plan's conventions.
            // Build each numeric family lazily so a finite-difference calibration
            // does not pay for AAD blocks (and vice versa).
            mutable Vector_<TypedBlockStorage_<double>> domesticBlocks_;
            mutable Vector_<TypedBlockStorage_<double>> foreignBlocks_;
            mutable Vector_<TypedBlockStorage_<AAD::Number_>> domesticAadBlocks_;
            mutable Vector_<TypedBlockStorage_<AAD::Number_>> foreignAadBlocks_;
            mutable bool doubleBlocksBuilt_ = false;
            mutable bool aadBlocksBuilt_ = false;
            CurveDefinition_ basisDefinition_;
            CurveJacobianMode_ jacobianMode_;
            double bumpSize_;
            FdScheme_ fdScheme_;
            int* evaluationCount_;

            template <class T_>
            void BuildBlocks(Vector_<TypedBlockStorage_<T_>>& domestic, Vector_<TypedBlockStorage_<T_>>& foreign) const {
                domestic.reserve(plans_.size());
                foreign.reserve(plans_.size());
                for (const auto& plan : plans_) {
                    const auto& convention = plan.config_.convention_;
                    domestic.push_back(BuildTypedBlock<T_>(*domesticBlock_, convention.domesticIndex_));
                    foreign.push_back(BuildTypedBlock<T_>(*foreignBlock_, convention.foreignIndex_));
                }
            }

            template <class T_> void EnsureBlocks() const {
                if constexpr (std::is_same_v<T_, double>) {
                    if (doubleBlocksBuilt_)
                        return;
                    BuildBlocks(domesticBlocks_, foreignBlocks_);
                    doubleBlocksBuilt_ = true;
                } else {
                    if (aadBlocksBuilt_)
                        return;
                    BuildBlocks(domesticAadBlocks_, foreignAadBlocks_);
                    aadBlocksBuilt_ = true;
                }
            }

            template <class T_> const TypedBlockStorage_<T_>& DomesticBlock(size_t i) const {
                if constexpr (std::is_same_v<T_, double>)
                    return domesticBlocks_[i];
                else
                    return domesticAadBlocks_[i];
            }

            template <class T_> const TypedBlockStorage_<T_>& ForeignBlock(size_t i) const {
                if constexpr (std::is_same_v<T_, double>)
                    return foreignBlocks_[i];
                else
                    return foreignAadBlocks_[i];
            }

            template <class T_> Vector_<T_> Residuals(const Vector_<T_>& parameters) const {
                EnsureBlocks<T_>();
                const auto basis = BuildDiscountCurveT<T_>(basisDefinition_, parameters);
                Vector_<T_> result(plans_.size());
                for (int i = 0; i < static_cast<int>(plans_.size()); ++i) {
                    const auto& plan = plans_[i];
                    const auto& domestic = DomesticBlock<T_>(static_cast<size_t>(i));
                    const auto& foreign = ForeignBlock<T_>(static_cast<size_t>(i));

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
                                      CurveJacobianMode_ jacobianMode,
                                      int* evaluationCount)
                : valuationTime_(valuationTime), pair_(spec.basisPair_), collateralCurrency_(collateralCurrency),
                  domesticBlock_(spec.domesticCurveBlock_), foreignBlock_(spec.foreignCurveBlock_), fxSpot_(spec.fxSpot_), fixings_(fixings),
                  plans_(plans), basisDefinition_(basisDefinition), jacobianMode_(jacobianMode),
                  bumpSize_(spec.solveMode_ == CurveSolveMode_::Value_::EXACT ? 1.0e-6 : 1.0e-4),
                  fdScheme_(spec.solveMode_ == CurveSolveMode_::Value_::EXACT ? FdScheme_::CENTRAL : FdScheme_::FORWARD),
                  evaluationCount_(evaluationCount) {
                marketRates_.reserve(spec.instruments_.size());
                for (const auto& instrument : spec.instruments_)
                    marketRates_.push_back(instrument->MarketRate());
            }

            [[nodiscard]] double BumpSize() const override { return bumpSize_; }
            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
                ++*evaluationCount_;
                return Residuals<double>(x);
            }

            void Gradient(const Vector_<>& x, const Vector_<>& f, Matrix_<>* jacobian) const override {
                FiniteDifferenceGradient(*this, fdScheme_, bumpSize_, x, f, [&](const Vector_<>& parameters) { return Residuals<double>(parameters); },
                                         jacobian);
            }

            [[nodiscard]] std::unique_ptr<Underdetermined::Jacobian_> Gradient(const Vector_<>& x, const Vector_<>&) const override {
                if (jacobianMode_ != CurveJacobianMode_::Value_::ANALYTIC)
                    return nullptr;
                return TapeResidualJacobian(x,
                                            [&](const Vector_<Dal::AAD::Number_>& parameters) { return Residuals<Dal::AAD::Number_>(parameters); });
            }
        };

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
                                                                  CurveSolveOutput_* solve) {
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
            // The solver clears a requested inverse that fails its mapping verification;
            // report that fail-closed outcome with the joint driver's token.
            if (solve->hasEffJacobianInverse_ && diagnostics.effJacobianInverse_.Empty())
                diagnostics.effJacobianInverseAvailability_ = "not_available_for_mapping";
            RequireFiniteValues(fxForwardCurve.forwards_, "Cross-currency calibration FX forwards");
            return CrossCurrencyCalibrationResult_(market, basisCurves, fxForwardCurve, diagnostics);
        }
    } // namespace

    namespace XccyInstrumentInternal {
        using namespace CalibrationInternal;

        CalibrationDiagnostic_ Row(const CalibrationObservation_& row, CalibrationDiagnostic_ d = {}) {
            d.observationId_ = row.Id();
            d.instrumentId_ = row.Instrument()->Id();
            d.quoteId_ = row.Quote()->Id();
            d.measureId_ = row.MeasureId();
            return d;
        }
        std::optional<double> Finite(double value) { return std::isfinite(value) ? std::optional<double>(value) : std::nullopt; }
        std::optional<double> At(const Vector_<>& values, size_t i) { return i < values.size() ? Finite(values[i]) : std::nullopt; }
        Vector_<> FiniteValues(const Vector_<>& values) {
            return std::all_of(values.begin(), values.end(), [](double x) { return std::isfinite(x); }) ? values : Vector_<>();
        }
        void ReportSummary(const XccyInstrumentCalibrationResult_& r, const String_& status, CalibrationReportSummary_* summary) {
            auto& s = *summary;
            s.domain_ = "Xccy";
            s.status_ = status;
            s.mode_ = r.solveMode_.String();
            s.terminationReason_ = r.termination_.empty() ? std::nullopt : std::optional<String_>(r.termination_);
            s.evaluations_ = r.evaluations_;
            s.jacobianEvaluations_ = r.gradients_;
            s.fitWeight_ = r.fitWeight_ ? Finite(*r.fitWeight_) : std::nullopt;
            s.fitNormTolerance_ = r.fitNormTolerance_ ? Finite(*r.fitNormTolerance_) : std::nullopt;
            if (!r.modelMeasures_.empty()) {
                s.fitWithinTolerance_ = r.fitWithinTolerance_;
                s.scaledL2_ = Finite(r.scaledNorm_);
                s.scaledInfinity_ = Finite(r.maxAbsScaledResidual_);
                const auto& unit = r.axes_.observations_.front()->Quote()->Unit();
                if (std::all_of(r.axes_.observations_.begin(), r.axes_.observations_.end(),
                                [&](const auto& row) { return row->Quote()->Unit() == unit; }))
                    s.rawL2_ = Finite(r.rawNorm_);
            }
        }
        void ReportObservations(const XccyInstrumentCalibrationResult_& r, CalibrationReportSnapshot_* snapshot) {
            auto& data = *snapshot;
            data.observations_.clear();
            for (size_t i = 0; i < r.axes_.observations_.size(); ++i) {
                const auto& o = *r.axes_.observations_[i];
                CalibrationReportObservation_ row;
                row.instrument_ = o.Instrument();
                row.observationId_ = o.Id();
                row.quoteId_ = o.Quote()->Id();
                row.quoteIndex_ = r.axes_.quoteIndex_[i];
                row.measureId_ = o.MeasureId();
                row.unit_ = o.Quote()->Unit().Token();
                row.representation_ = o.Quote()->Unit().kind_ == CalibrationUnitKind_::Value_::Price ? "Price" : "Scalar";
                row.target_ = o.Quote()->Value();
                row.errorScale_ = o.ErrorScale();
                row.modelValue_ = At(r.modelMeasures_, i);
                row.rawResidual_ = At(r.rawResiduals_, i);
                row.scaledResidual_ = At(r.scaledResiduals_, i);
                if (row.scaledResidual_)
                    row.fitRowPassed_ = std::abs(*row.scaledResidual_) <= 1.;
                row.backend_ = "FastDeterministic";
                row.derivativeMethod_ = r.derivativeMethod_.String();
                if (i < data.plans_.size()) {
                    row.planIdentity_ = data.plans_[i].planIdentity_.value_or("");
                    data.plans_[i].derivativeMethod_ = row.derivativeMethod_;
                }
                data.observations_.push_back(row);
            }
        }
        void ReportParameters(const XccyInstrumentCalibrationResult_& r, CalibrationReportSnapshot_* snapshot) {
            auto& data = *snapshot;
            data.parameters_.clear();
            data.objective_.nativeLayout_.clear();
            for (size_t i = 0; i < r.freeParameters_.size(); ++i) {
                const auto& p = r.freeParameters_[i];
                const String_ id = "Basis:" + String_(p.component_.String()) + ":" + Date::ToString(p.date_);
                data.parameters_.push_back({id, "Rate", At(r.initialParameters_, i), At(r.parameters_, i), At(r.parameterScales_, i), {}, {}, {}});
                data.objective_.nativeLayout_.push_back(id);
            }
            data.objective_.theta0_ = FiniteValues(r.initialParameters_);
            data.objective_.parameterScales_ = FiniteValues(r.parameterScales_);
        }
        std::optional<double> Penalty(const XccyInstrumentCalibrationResult_& r) {
            const auto n = r.parameters_.size();
            if (r.initialParameters_.size() != n || r.nativeWeights_.Rows() != static_cast<int>(n))
                return std::nullopt;
            long double value = 0.;
            for (size_t i = 0; i < n; ++i)
                for (size_t j = 0; j < n; ++j)
                    value += .5L * (static_cast<long double>(r.parameters_[i]) - r.initialParameters_[i]) *
                             r.nativeWeights_(static_cast<int>(i), static_cast<int>(j)) *
                             (static_cast<long double>(r.parameters_[j]) - r.initialParameters_[j]);
            return Finite(static_cast<double>(value));
        }
        void ReportObjective(const XccyInstrumentCalibrationResult_& r, CalibrationReportSnapshot_* snapshot) {
            auto& data = *snapshot;
            data.objective_.lambda_ = data.summary_.fitWeight_;
            data.objective_.eta_ = data.summary_.fitNormTolerance_;
            if (r.nativeWeights_.Rows() > 0) {
                data.objective_.weightMatrix_ = r.nativeWeights_;
                data.objective_.weightProvenance_ = "NativeXccyBasisSmoothingMetric";
            }
            if (r.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE && r.fitWeight_ && !r.modelMeasures_.empty()) {
                data.summary_.dataObjective_ = Finite(static_cast<double>(.5L * *r.fitWeight_ * r.scaledNorm_ * r.scaledNorm_));
                data.summary_.penaltyObjective_ = Penalty(r);
                if (data.summary_.dataObjective_ && data.summary_.penaltyObjective_)
                    data.summary_.objective_ = Finite(*data.summary_.dataObjective_ + *data.summary_.penaltyObjective_);
            }
        }
        CalibrationReportSnapshot_ Report(const XccyInstrumentCalibrationResult_& r, const String_& status) {
            auto data = r.report_ ? r.report_->Data() : CalibrationReportSnapshot_();
            ReportSummary(r, status, &data.summary_);
            ReportObservations(r, &data);
            ReportParameters(r, &data);
            ReportObjective(r, &data);
            if (r.quoteJacobian_.Rows() > 0)
                data.quoteJacobian_ = r.quoteJacobian_;
            if (r.xccySpec_) {
                data.execution_.valuationDate_ = r.xccySpec_->context_.valuationTime_.Date();
                data.execution_.todayFixingPolicy_ = "NativeXccyTimestamp";
            }
            return data;
        }
        [[noreturn]] void Failure(XccyInstrumentCalibrationResult_ result, CalibrationDiagnostic_ d) {
            if (!result.axes_.observations_.empty()) {
                const auto& row = *result.axes_.observations_.front();
                d.observationId_ = d.observationId_.value_or(row.Id());
                d.instrumentId_ = d.instrumentId_.value_or(row.Instrument()->Id());
                d.quoteId_ = d.quoteId_.value_or(row.Quote()->Id());
                d.measureId_ = d.measureId_.value_or(row.MeasureId());
            }
            if (d.message_.empty())
                d.message_ = d.code_ + ": " + d.field_.value_or("/solver") + "; " + d.expected_.value_or("valid XCCY calibration");
            result.termination_ = d.code_;
            auto failure = std::make_shared<CalibrationFailure_>();
            failure->diagnostic_ = d;
            failure->report_.reset(new CalibrationReport_(d.stage_, result.axes_, {d}, Report(result, "Failed")));
            result.report_ = failure->report_;
            if (result.curve_)
                failure->candidate_.reset(new XccyInstrumentCalibrationCandidate_(std::move(result)));
            throw CalibrationError_(__FILE__, __LINE__, __func__, failure);
        }
        Handle_<DiscountCurve_> FreezeCurve(const Handle_<DiscountCurve_>& curve) {
            Check(bool(curve), "CalibrationContextMismatch", "/market/routes", "nonnull explicit curve");
            Vector_<const YCComponent_*> components;
            curve->Poll(&components);
            YCComponent_::substitutions_t replacements;
            for (auto i = components.rbegin(); i != components.rend(); ++i)
                replacements[*i].reset((*i)->Clone((*i)->Name(), replacements).release());
            const auto copy = handle_cast<DiscountCurve_>(replacements.at(curve.get()));
            Check(bool(copy), "CalibrationContextMismatch", "/market/routes", "cloned discount curve");
            return copy;
        }
        Handle_<CurveBlock_> Freeze(const Handle_<CurveBlock_>& block, const String_& field) {
            Check(bool(block), "CalibrationContextMismatch", field, "explicit frozen currency block");
            Check(HasAct365FLiborBasis(block->LiborBasis()), "CalibrationContextMismatch", field + "/libor_basis", "ACT_365F native mapping");
            std::map<CollateralType_, Handle_<DiscountCurve_>> discounts;
            std::map<PeriodLength_, Handle_<DiscountCurve_>> forwards;
            for (const auto& [key, curve] : block->DiscountCurves())
                discounts[key] = FreezeCurve(curve);
            for (const auto& [key, curve] : block->ForwardCurves())
                forwards[key] = FreezeCurve(curve);
            return Handle_<CurveBlock_>(new CurveBlock_(block->Name(), block->ccy_.String(), discounts, forwards, block->LiborBasis()));
        }
        Json_ BackgroundIdentity(const CurveBlock_& block) {
            auto discounts = Json_::Array(), forwards = Json_::Array();
            for (const auto& [key, curve] : block.DiscountCurves())
                discounts.array_.push_back(Obj({{"collateral", Str(key.String())}, {"curve", Parse(JSON::WriteString(*curve))}}));
            for (const auto& [key, curve] : block.ForwardCurves())
                forwards.array_.push_back(Obj({{"tenor", Str(key.String())}, {"curve", Parse(JSON::WriteString(*curve))}}));
            return Obj({{"discounts", discounts}, {"projections", forwards}});
        }
        String_ FixingsIdentity(const Vector_<PreparedCalibrationInstrument_>& plans) {
            MarketFixingSnapshot_::values_t values;
            for (const auto& plan : plans)
                for (const auto& [index, history] : std::get<CalibrationXccy_>(plan.Descriptor())->fixings_->Values())
                    for (const auto& [time, value] : history) {
                        const auto inserted = values[index].emplace(time, value);
                        Check(inserted.second || inserted.first->second == value, "CalibrationContextMismatch", "/context/fixings",
                              "consistent frozen history across the basket");
                    }
            auto entries = Json_::Array();
            for (const auto& [index, history] : values)
                for (const auto& [time, value] : history)
                    entries.array_.push_back(Obj(
                        {{"index", Str(index)}, {"date", Str(Date::ToString(time.Date()))}, {"fraction", Num(time.Frac())}, {"value", Num(value)}}));
            return Fingerprint(entries);
        }
        CurveDefinition_ Definition(const XccyInstrumentCalibrationSpec_& spec) {
            return MakeCurveDefinition(String_("xccy_basis_") + spec.context_.pair_.domestic_.String(), spec.context_.pair_.domestic_.String(),
                                       CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, LogDfScheme_::Value_::LOG_LINEAR, spec.knotDates_,
                                       spec.context_.valuationTime_.Date(), DayBasis::Act365F());
        }
        void Validate(const XccyInstrumentCalibrationSpec_& spec, const CurveInstrumentCalibrationOptions_& options) {
            XccyScriptInternal::ContextIdentity(spec.context_, nullptr, {});
            Check(!spec.observations_.empty(), "InvalidCalibrationObservation", "/observations", "nonempty ordered XCCY observations");
            Check(spec.domesticCurveBlock_ && spec.foreignCurveBlock_, "CalibrationContextMismatch", "/market", "both explicit currency blocks");
            Check(spec.domesticCurveBlock_->ccy_ == spec.context_.pair_.domestic_ && spec.foreignCurveBlock_->ccy_ == spec.context_.pair_.foreign_,
                  "CalibrationContextMismatch", "/market", "blocks matching the explicit pair");
            Check(std::isfinite(spec.fxSpot_) && spec.fxSpot_ > 0., "CalibrationContextMismatch", "/fx_spot", "finite positive USD per EUR spot");
            Check(!spec.knotDates_.empty() && spec.knotDates_.front() > spec.context_.valuationTime_.Date(), "InvalidCalibrationPlan", "/knot_dates",
                  "future strictly increasing basis knots");
            for (size_t i = 1; i < spec.knotDates_.size(); ++i)
                Check(spec.knotDates_[i] > spec.knotDates_[i - 1], "InvalidCalibrationPlan", "/knot_dates", "strictly increasing basis knots");
            XccyScriptInternal::Admit({options.evaluation_, options.jacobianMode_}, {});
            Check(options.jacobianMode_ != CalibrationDerivative_::Value_::None, "DerivativeUnavailable", "/options/jacobian_mode", "AAD or FD");
            Check(std::isfinite(options.differenceStep_) && options.differenceStep_ > 0., "InvalidCalibrationPlan", "/options/difference_step",
                  "finite positive FD step");
        }
        template <class T_> TypedBlockStorage_<T_> Background(const CurveBlock_& source) {
            TypedBlockStorage_<T_> result;
            for (const auto& [key, curve] : source.DiscountCurves())
                result.block_.discountCurves[key] = AddPassiveCurve<T_>(*curve, &result);
            for (const auto& [key, curve] : source.ForwardCurves())
                result.block_.forwardCurves[key] = AddPassiveCurve<T_>(*curve, &result);
            return result;
        }
        template <class T_>
        Vector_<T_> Measures(const XccyInstrumentCalibrationSpec_& spec,
                             const CurveDefinition_& definition,
                             const Vector_<PreparedCalibrationInstrument_>& plans,
                             const Vector_<T_>& parameters) {
            const auto basis = BuildDiscountCurveT<T_>(definition, parameters);
            auto domestic = Background<T_>(*spec.domesticCurveBlock_), foreign = Background<T_>(*spec.foreignCurveBlock_);
            const XccyMarketView_<T_> market{spec.context_.valuationTime_,
                                             spec.context_.pair_,
                                             spec.context_.collateralCurrency_,
                                             T_(spec.fxSpot_),
                                             &domestic.block_,
                                             &foreign.block_,
                                             basis.get()};
            Vector_<T_> values;
            for (size_t i = 0; i < plans.size(); ++i) {
                try {
                    const auto& descriptor = *std::get<CalibrationXccy_>(plans[i].Descriptor());
                    Check(XccyScriptInternal::ContextIdentity(spec.context_, descriptor.fixings_.get(), {}) == plans[i].ContextIdentity(),
                          "CalibrationContextMismatch", "/context", "unchanged frozen conventions and history");
                    values.push_back(EvaluateXccyScript(plans[i], market));
                } catch (const CalibrationError_& error) {
                    const auto d = Row(*spec.observations_[i], error.Failure().diagnostic_);
                    Fail(d.code_, d.field_.value_or("/market"), d.expected_.value_or("valid XCCY measure"), d.actual_.value_or(""), d);
                }
            }
            return values;
        }
        double Norm(const Vector_<>& values) {
            double norm = 0.;
            for (double value : values)
                norm = std::hypot(norm, value);
            return norm;
        }
        Vector_<> Native(const Vector_<>& z, const Vector_<>& initial, const Vector_<>& scales) {
            auto theta = initial;
            for (size_t j = 0; j < theta.size(); ++j)
                theta[j] += z[j] * scales[j];
            return theta;
        }
        // Calibration assembly mutates its large result buffers in place.
        void Residuals(XccyInstrumentCalibrationResult_* result) {
            for (size_t i = 0; i < result->modelMeasures_.size(); ++i) {
                const auto& row = *result->axes_.observations_[i];
                const double raw = result->modelMeasures_[i] - row.Quote()->Value(), scaled = raw / row.ErrorScale();
                Check(std::isfinite(raw) && std::isfinite(scaled), "CalibrationNumericalFailure", "/residuals", "finite residuals", "", Row(row));
                result->rawResiduals_.push_back(raw);
                result->scaledResiduals_.push_back(scaled);
                result->maxAbsScaledResidual_ = std::max(result->maxAbsScaledResidual_, std::abs(scaled));
            }
            result->rawNorm_ = Norm(result->rawResiduals_);
            result->scaledNorm_ = Norm(result->scaledResiduals_);
            Check(std::isfinite(result->scaledNorm_), "CalibrationNumericalFailure", "/residual_norms", "finite scaled norm");
        }
        std::pair<double, double> DifferencePoints(double theta, double step, const CurveFreeParameter_& label) {
            const double lower = theta - step, upper = theta + step;
            CalibrationDiagnostic_ d;
            d.stage_ = CalibrationStage_::Value_::Evaluate;
            d.parameterKey_ = "Basis:" + String_(label.component_.String()) + ":" + Date::ToString(label.date_);
            Check(std::isfinite(upper - lower) && lower < theta && theta < upper, "CalibrationNumericalFailure", "/options/difference_step",
                  "finite resolvable two-sided perturbations", NumberText(step), d);
            return {lower, upper};
        }
        Matrix_<> DifferenceJacobian(const XccyInstrumentCalibrationSpec_& spec,
                                     const CurveDefinition_& definition,
                                     const Vector_<PreparedCalibrationInstrument_>& plans,
                                     const Vector_<>& parameters,
                                     double step) {
            Matrix_<> jacobian(static_cast<int>(plans.size()), static_cast<int>(parameters.size()));
            const auto labels = DescribeCurveFreeParameters(definition);
            for (size_t j = 0; j < parameters.size(); ++j) {
                auto up = parameters, down = parameters;
                const auto [lower, upper] = DifferencePoints(parameters[j], step, labels[j]);
                up[j] = upper;
                down[j] = lower;
                const auto plus = Measures(spec, definition, plans, up), minus = Measures(spec, definition, plans, down);
                for (size_t i = 0; i < plans.size(); ++i)
                    jacobian(static_cast<int>(i), static_cast<int>(j)) = (plus[i] - minus[i]) / (upper - lower);
            }
            return jacobian;
        }
        Matrix_<> NativeJacobian(const XccyInstrumentCalibrationSpec_& spec,
                                 const CurveDefinition_& definition,
                                 const Vector_<PreparedCalibrationInstrument_>& plans,
                                 const Vector_<>& parameters,
                                 const CurveInstrumentCalibrationOptions_& options) {
            if (options.jacobianMode_ == CalibrationDerivative_::Value_::FiniteDifference)
                return DifferenceJacobian(spec, definition, plans, parameters, options.differenceStep_);
            const auto jacobian =
                TapeResidualJacobian(parameters, [&](const Vector_<AAD::Number_>& p) { return Measures(spec, definition, plans, p); });
            return static_cast<const XCurveJacobian_&>(*jacobian).j_;
        }
    } // namespace XccyInstrumentInternal

    PreparedXccyInstruments_::PreparedXccyInstruments_(const XccyInstrumentCalibrationSpec_& spec, const CurveInstrumentCalibrationOptions_& options)
        : spec_(spec), options_(options), axes_(BuildCalibrationObservationAxes(spec.observations_)) {
        using namespace XccyInstrumentInternal;
        try {
            Validate(spec_, options_);
            spec_.domesticCurveBlock_ = Freeze(spec_.domesticCurveBlock_, "/market/domestic");
            spec_.foreignCurveBlock_ = Freeze(spec_.foreignCurveBlock_, "/market/foreign");
            definition_ = Definition(spec_);
            for (const auto& row : axes_.observations_) {
                CalibrationPreparationContext_ context;
                context.xccy_ = std::make_shared<const XccyScriptContext_>(spec_.context_);
                try {
                    plans_.push_back(
                        PrepareCalibrationInstrument(row->Instrument(), row->MeasureId(), context, {options.evaluation_, options.jacobianMode_}));
                } catch (const CalibrationError_& error) {
                    Failure(Describe(), Row(*row, error.Failure().diagnostic_));
                }
            }
            frozenSpec_ = std::make_shared<const XccyInstrumentCalibrationSpec_>(spec_);
            planIdentity_ = Fingerprint(Obj({{"adapter", Str("dal.staged-xccy-instruments/1")},
                                             {"domestic", BackgroundIdentity(*spec_.domesticCurveBlock_)},
                                             {"foreign", BackgroundIdentity(*spec_.foreignCurveBlock_)},
                                             {"spot", Num(spec_.fxSpot_)},
                                             {"knots", Array(spec_.knotDates_, [](const auto& d) { return Str(Date::ToString(d)); })},
                                             {"plans", Array(plans_, [](const auto& p) { return Str(p.PlanIdentity()); })},
                                             {"observations", Array(axes_.observations_,
                                                                    [](const auto& r) {
                                                                        return Obj({{"id", Str(r->Id())},
                                                                                    {"quote", Str(r->Quote()->Fingerprint())},
                                                                                    {"scale", Num(r->ErrorScale())}});
                                                                    })},
                                             {"derivative", Str(options_.jacobianMode_.String())},
                                             {"step", Num(options_.differenceStep_)}}));
        } catch (const CalibrationError_& error) {
            Failure(Describe(), error.Failure().diagnostic_);
        } catch (const std::exception& error) {
            CalibrationDiagnostic_ d;
            d.code_ = "InvalidCalibrationPlan";
            d.stage_ = CalibrationStage_::Value_::Prepare;
            d.message_ = d.code_ + ": " + String_(error.what());
            Failure(Describe(), d);
        }
    }
    XccyInstrumentCalibrationResult_ PreparedXccyInstruments_::Describe() const {
        using namespace XccyInstrumentInternal;
        XccyInstrumentCalibrationResult_ result;
        result.axes_ = axes_;
        result.definition_ = definition_;
        if (!definition_.nodeDates_.empty())
            result.freeParameters_ = DescribeCurveFreeParameters(definition_);
        result.xccySpec_ = frozenSpec_;
        result.planIdentity_ = planIdentity_;
        CalibrationReportSnapshot_ data;
        if (frozenSpec_) {
            data.execution_.fixingSnapshotIdentity_ = FixingsIdentity(plans_);
            data.execution_.routeIdentities_ = {Fingerprint(BackgroundIdentity(*spec_.domesticCurveBlock_)),
                                                Fingerprint(BackgroundIdentity(*spec_.foreignCurveBlock_))};
        }
        for (size_t i = 0; i < plans_.size(); ++i) {
            const auto& p = plans_[i];
            const auto& instrument = *p.Instrument();
            result.capabilities_.push_back(p.Capability());
            CalibrationReportPlan_ row;
            row.observationId_ = axes_.observations_[i]->Id();
            row.measureId_ = p.Measure().id_;
            row.ruleId_ = p.Capability().ruleId_;
            row.backend_ = p.Capability().backend_.String();
            row.derivativeMethod_ = "None";
            row.sourceIdentity_ = instrument.Definition()->SourceIdentity();
            row.definitionIdentity_ = instrument.Definition()->DefinitionIdentity();
            row.bindingIdentity_ = instrument.BindingIdentity();
            row.compilerIdentity_ = p.CompilerIdentity();
            row.contextIdentity_ = p.ContextIdentity();
            row.planIdentity_ = p.PlanIdentity();
            data.plans_.push_back(row);
        }
        result.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Prepare, axes_, {}, data));
        return result;
    }
    XccyInstrumentCalibrationResult_ PreparedXccyInstruments_::Evaluate(const Vector_<>& parameters, bool derivatives) const {
        using namespace XccyInstrumentInternal;
        auto result = Describe();
        try {
            result.modelMeasures_ = Measures(spec_, definition_, plans_, parameters);
            Residuals(&result);
            result.parameters_ = parameters;
            result.curve_ = Handle_<DiscountCurve_>(BuildDiscountCurveT<double>(definition_, parameters));
            result.candidateIdentity_ =
                Fingerprint(Obj({{"plan", Str(planIdentity_)}, {"parameters", Array(parameters, [](double x) { return Num(x); })}}));
            if (derivatives) {
                const bool aad = options_.jacobianMode_ != CalibrationDerivative_::Value_::FiniteDifference;
                result.derivativeMethod_ = aad ? CalibrationDerivative_::Value_::AAD : CalibrationDerivative_::Value_::FiniteDifference;
                result.quoteJacobian_ = NativeJacobian(spec_, definition_, plans_, parameters, options_);
                RequireFiniteValues(result.quoteJacobian_, "raw XCCY Jacobian");
            }
            result.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Evaluate, axes_, {}, Report(result, "Evaluated")));
            return result;
        } catch (const CalibrationError_& error) {
            Failure(std::move(result), error.Failure().diagnostic_);
        } catch (const std::exception& error) {
            CalibrationDiagnostic_ d;
            d.code_ = "CalibrationNumericalFailure";
            d.stage_ = CalibrationStage_::Value_::Evaluate;
            d.message_ = d.code_ + ": " + String_(error.what());
            Failure(std::move(result), d);
        }
    }

    std::unique_ptr<Underdetermined::Jacobian_> PreparedXccyInstruments_::Jacobian(const Vector_<>& parameters) const {
        if (options_.jacobianMode_ == CalibrationDerivative_::Value_::FiniteDifference)
            return nullptr;
        return std::make_unique<XCurveJacobian_>(XccyInstrumentInternal::NativeJacobian(spec_, definition_, plans_, parameters, options_));
    }

    namespace XccyInstrumentInternal {
        class FitFunction_ final : public Underdetermined::Function_ {
            const PreparedXccyInstruments_& plan_;
            Vector_<> initial_, scales_;

        public:
            FitFunction_(const PreparedXccyInstruments_& plan, const Vector_<>& initial, const Vector_<>& scales)
                : plan_(plan), initial_(initial), scales_(scales) {}
            Vector_<> F(const Vector_<>& z) const override { return plan_.Evaluate(Native(z, initial_, scales_), false).rawResiduals_; }
            std::unique_ptr<Underdetermined::Jacobian_> Gradient(const Vector_<>& z, const Vector_<>&) const override {
                auto result = plan_.Jacobian(Native(z, initial_, scales_));
                if (result) {
                    auto& j = static_cast<XCurveJacobian_&>(*result).j_;
                    for (int i = 0; i < j.Rows(); ++i)
                        for (int k = 0; k < j.Cols(); ++k)
                            j(i, k) *= scales_[k];
                }
                return result;
            }
        };
        void ApproximateControls(const CurveInstrumentSolver_& s) {
            if (s.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE) {
                Check(s.fitWeight_ && std::isfinite(*s.fitWeight_) && *s.fitWeight_ > 0., "InvalidCalibrationObjective", "/solver/fit_weight",
                      "explicit finite positive dimensionless lambda");
                Check(s.fitNormTolerance_ && std::isfinite(*s.fitNormTolerance_) && *s.fitNormTolerance_ > 0., "InvalidCalibrationObjective",
                      "/solver/fit_norm_tolerance", "explicit finite positive dimensionless eta");
            }
        }
        void ObjectiveControls(const CurveInstrumentSolver_& s) {
            Token(s.solveMode_, "InvalidCalibrationObjective", "/solver/mode");
            ApproximateControls(s);
            Check(s.maxEvaluations_ > 0 && s.maxRestarts_ > 0, "InvalidCalibrationObjective", "/solver/budget", "positive budgets");
            Check(std::isfinite(s.smoothingWeight_) && s.smoothingWeight_ > 0., "InvalidCalibrationObjective", "/solver/smoothing_weight",
                  "finite positive native metric");
        }
        // Calibration assembly mutates its large result buffers in place.
        void InitialCoordinates(const CurveInstrumentSolver_& s, XccyInstrumentCalibrationResult_* r) {
            const size_t n = r->freeParameters_.size();
            r->initialParameters_ = s.initialGuessPerNode_.empty() ? Vector_<>(n, s.initialGuess_) : s.initialGuessPerNode_;
            Check(r->initialParameters_.size() == n, "InvalidCalibrationObjective", "/solver/initial_guess_per_node", "one value per basis knot");
            for (double x : r->initialParameters_)
                Check(std::isfinite(x), "InvalidCalibrationObjective", "/solver/initial_guess", "finite initial parameters");
            r->parameterScales_ = s.parameterScales_.empty() ? Vector_<>(n, 1.) : s.parameterScales_;
            Check(r->parameterScales_.size() == n, "InvalidCalibrationObjective", "/solver/parameter_scales", "one scale per basis knot");
            for (double x : r->parameterScales_)
                Check(std::isfinite(x) && x > 0., "InvalidCalibrationObjective", "/solver/parameter_scales", "finite positive parameter scales");
        }
        // Calibration assembly mutates its large result buffers in place.
        std::unique_ptr<Sparse::TriDiagonal_> Solver(const CurveInstrumentSolver_& s, XccyInstrumentCalibrationResult_* r) {
            r->solveMode_ = s.solveMode_;
            r->fitWeight_ = s.fitWeight_;
            r->fitNormTolerance_ = s.fitNormTolerance_;
            ObjectiveControls(s);
            InitialCoordinates(s, r);
            const size_t n = r->freeParameters_.size();
            Vector_<Date_> dates;
            for (const auto& p : r->freeParameters_)
                dates.push_back(p.date_);
            auto weights = BuildCurveCalibrationWeights(dates, 1, s.smoothingWeight_);
            r->nativeWeights_.Resize(static_cast<int>(n), static_cast<int>(n));
            r->weights_.Resize(static_cast<int>(n), static_cast<int>(n));
            for (int i = 0; i < static_cast<int>(n); ++i)
                for (int j = 0; j <= i; ++j) {
                    const double native = (*weights)(i, j), value = native * r->parameterScales_[i] * r->parameterScales_[j];
                    Check(std::isfinite(value) && (native == 0. || value != 0.), "InvalidCalibrationObjective", "/solver/weights",
                          "representable symmetric S'W_theta*S");
                    r->nativeWeights_(i, j) = r->nativeWeights_(j, i) = native;
                    r->weights_(i, j) = r->weights_(j, i) = value;
                    if (i - j <= 1) {
                        weights->Set(i, j, value);
                        weights->Set(j, i, value);
                    }
                }
            r->objectiveIdentity_ = Fingerprint(Obj({{"plan", Str(r->planIdentity_)},
                                                     {"mode", Str(r->solveMode_.String())},
                                                     {"theta0", Array(r->initialParameters_, [](double x) { return Num(x); })},
                                                     {"scales", Array(r->parameterScales_, [](double x) { return Num(x); })},
                                                     {"smoothing_weight", Num(s.smoothingWeight_)},
                                                     {"lambda", Optional(r->fitWeight_, [](double x) { return Num(x); })},
                                                     {"eta", Optional(r->fitNormTolerance_, [](double x) { return Num(x); })}}));
            return weights;
        }
        [[noreturn]] void FailedFit(const Underdetermined::FitResult_& solved, XccyInstrumentCalibrationResult_ result) {
            CalibrationDiagnostic_ d;
            d.code_ = solved.termination_;
            d.stage_ = CalibrationStage_::Value_::Solve;
            d.field_ = "/solver";
            d.expected_ = "finite scaled infinity <=1 (EXACT), scaled L2 <=eta (APPROXIMATE)";
            d.actual_ = "scaled L2=" + NumberText(result.scaledNorm_) + "; evaluations=" + OrdinalText(solved.evaluations_);
            if (solved.cause_)
                try {
                    std::rethrow_exception(solved.cause_);
                } catch (const CalibrationError_& error) {
                    d = error.Failure().diagnostic_;
                } catch (const std::exception& error) {
                    d.message_ = d.code_ + ": " + String_(error.what());
                }
            Failure(std::move(result), d);
        }
        // Calibration assembly mutates its large result buffers in place.
        void FitState(const PreparedXccyInstruments_& plan,
                      const Underdetermined::FitResult_& solved,
                      const CurveInstrumentCalibrationOptions_& options,
                      XccyInstrumentCalibrationResult_* result) {
            result->evaluations_ = solved.evaluations_;
            result->gradients_ = solved.gradients_;
            result->termination_ = solved.termination_;
            result->fitWithinTolerance_ = solved.fitWithinTolerance_;
            if (solved.parameters_.empty())
                return;
            result->coordinates_ = solved.parameters_;
            const auto values = plan.Evaluate(Native(solved.parameters_, result->initialParameters_, result->parameterScales_),
                                              options.computeQuoteJacobian_ && solved.fitWithinTolerance_);
            result->parameters_ = values.parameters_;
            result->curve_ = values.curve_;
            result->modelMeasures_ = values.modelMeasures_;
            result->rawResiduals_ = values.rawResiduals_;
            result->scaledResiduals_ = values.scaledResiduals_;
            result->rawNorm_ = values.rawNorm_;
            result->scaledNorm_ = values.scaledNorm_;
            result->maxAbsScaledResidual_ = values.maxAbsScaledResidual_;
            result->candidateIdentity_ = values.candidateIdentity_;
            result->report_ = values.report_;
            result->quoteJacobian_ = values.quoteJacobian_;
            result->derivativeMethod_ = values.derivativeMethod_;
            result->diagnosticEvaluations_ = 1;
            result->diagnosticGradients_ = options.computeQuoteJacobian_ && solved.fitWithinTolerance_ ? 1 : 0;
        }
    } // namespace XccyInstrumentInternal

    XccyInstrumentCalibrationCandidate_::XccyInstrumentCalibrationCandidate_(XccyInstrumentCalibrationResult_ result)
        : Storable_("XccyInstrumentCalibrationCandidate", "candidate"), result_(std::move(result)) {}
    void XccyInstrumentCalibrationCandidate_::Write(Archive::Store_&) const {
        CalibrationInternal::Fail("CalibrationPersistenceUnsupported", "/candidate", "session-owned XCCY candidate");
    }
    XccyInstrumentCalibrationResult_ CalibrateXccyInstruments(const XccyInstrumentCalibrationSpec_& spec,
                                                              const CurveInstrumentCalibrationOptions_& options) {
        using namespace XccyInstrumentInternal;
        XccyInstrumentCalibrationResult_ result;
        try {
            result.axes_ = BuildCalibrationObservationAxes(spec.observations_);
            const PreparedXccyInstruments_ plan(spec, options);
            result = plan.Describe();
            const auto weights = Solver(spec.solver_, &result);
            const FitFunction_ function(plan, result.initialParameters_, result.parameterScales_);
            Vector_<> scales;
            for (const auto& row : result.axes_.observations_)
                scales.push_back(row->ErrorScale());
            Dictionary_ dict;
            dict.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(spec.solver_.maxEvaluations_)));
            dict.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(spec.solver_.maxRestarts_)));
            Underdetermined::FitSettings_ settings;
            settings.exact_ = spec.solver_.solveMode_ == CurveSolveMode_::Value_::EXACT;
            settings.fitWeight_ = spec.solver_.fitWeight_;
            settings.fitNormTolerance_ = spec.solver_.fitNormTolerance_;
            settings.differenceStep_ = options.differenceStep_;
            const auto solved = Underdetermined::Fit(function, Vector_<>(result.initialParameters_.size(), 0.), scales, *weights,
                                                     UnderdeterminedControls_(dict), settings);
            FitState(plan, solved, options, &result);
            if (!solved.fitWithinTolerance_)
                FailedFit(solved, result);
            auto data = Report(result, "Succeeded");
            data.summary_.converged_ = true;
            result.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Solve, result.axes_, {}, data));
            return result;
        } catch (const CalibrationError_& error) {
            if (error.Failure().report_ && !result.report_)
                result.report_ = error.Failure().report_;
            Failure(std::move(result), error.Failure().diagnostic_);
        } catch (const std::exception& error) {
            CalibrationDiagnostic_ d;
            d.code_ = "CalibrationNumericalFailure";
            d.stage_ = CalibrationStage_::Value_::Solve;
            d.message_ = d.code_ + ": " + String_(error.what());
            Failure(std::move(result), d);
        }
    }

    AnalyticEligibilityReport_ ValidateCrossCurrencyAnalyticEligibility(const CrossCurrencyCalibrationSpec_& spec) {
        AnalyticEligibilityReport_ report;
        if (spec.domesticCurveBlock_ && !HasAct365FLiborBasis(spec.domesticCurveBlock_->LiborBasis())) {
            AddEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::LIBOR_BASIS_UNSUPPORTED, "domestic", -1, -1, -1,
                                "domestic libor basis must be ACT_365F");
        }
        if (spec.foreignCurveBlock_ && !HasAct365FLiborBasis(spec.foreignCurveBlock_->LiborBasis())) {
            AddEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::LIBOR_BASIS_UNSUPPORTED, "foreign", -1, -1, -1,
                                "foreign libor basis must be ACT_365F");
        }
        for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i) {
            if (!spec.instruments_[i]) {
                AddEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::CASHFLOW_PLAN_UNSUPPORTED, "staged", -1, i, -1,
                                    "empty XCCY instrument");
                continue;
            }
            try {
                const auto span = spec.instruments_[i]->TimeSpan();
                const XccyCashflowPlan_ plan = BuildXccyCashflowPlan(span.first, span.second, spec.instruments_[i]->Config());
                ValidateStagedPlan(spec, plan, i, &report);
            } catch (const std::exception& error) {
                AddEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::CASHFLOW_PLAN_UNSUPPORTED, "staged", -1, i, -1, String_(error.what()));
            }
        }
        return report;
    }

    CrossCurrencyCalibrationResult_ CalibrateCrossCurrencyMarket(const CrossCurrencyCalibrationSpec_& spec,
                                                                 const CrossCurrencyCalibrationOptions_& options) {
        RecordCurveCalibrationInvocation();
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

        int evaluationCount = 0;
        XccyBasisCalibrationFunc_ func(spec, valuationTime, collateralCurrency, fixings, plans, basisDefinition, options.jacobianMode_,
                                       &evaluationCount);
        CurveSolveOutput_ solve =
            RunCurveCalibration(func, guess, tolerance, spec.solveMode_ == CurveSolveMode_::Value_::EXACT, options.computeEffJacobianInverse_,
                                options.computeForwardJacobian_ && options.jacobianMode_ == CurveJacobianMode_::Value_::ANALYTIC, spec.fitTolerance_,
                                *weights, spec.maxEvaluations_, spec.maxRestarts_);
        CrossCurrencyCalibrationResult_ result =
            AssembleCalibrationResult(spec, valuationTime, collateralCurrency, fixings, basisDefinition, func, options, &solve);
        const double convergenceBound = spec.solveMode_ == CurveSolveMode_::Value_::EXACT ? 10.0 * spec.tolerance_ : 10.0 * spec.fitTolerance_;
        if (!ResidualsWithinBar(result.diagnostics_.residuals_, convergenceBound)) {
            const String_ pairName = String_(spec.basisPair_.domestic_.String()) + "/" + spec.basisPair_.foreign_.String();
            const String_ message = "Cross-currency calibration failed to converge for pair " + pairName + ": " +
                                    NonConvergenceStats(result.diagnostics_.residuals_, evaluationCount, false);
            THROW2(message, Underdetermined::ConvergenceError_);
        }
        return result;
    }
} // namespace Dal
