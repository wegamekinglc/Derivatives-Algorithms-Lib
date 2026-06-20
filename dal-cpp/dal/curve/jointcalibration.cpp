//
// Created by dal-implementer on 2026/6/20.
//

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <utility>
#include <vector>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/curvejacobian.hpp>
#include <dal/curve/jointcalibration.hpp>
#include <dal/curve/jointrate.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycctx.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/optimization/underdeterminedutils.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/dictionary.hpp>
#include <dal/utilities/numerics.hpp>

namespace Dal {

    namespace {
        constexpr const char* KEY_MAX_EVALUATIONS = "MAXEVALUATIONS";
        constexpr const char* KEY_MAX_RESTARTS = "MAXRESTARTS";

        // PWL/PWC params-per-knot for the joint path. LOG_DISCOUNT and ZERO_RATE are out of scope
        // for the joint first cut (the spec scopes the joint residual to PIECEWISE_LINEAR_FWD); we
        // REQUIRE(false) on them so a future caller fails loudly at validation rather than silently
        // slicing the wrong number of parameters.
        int ParamsPerKnot(CurveParameterization_ parameterization) {
            switch (parameterization.Switch()) {
            case CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD:
                return 2;
            case CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD:
                return 1;
            case CurveParameterization_::Value_::LOG_DISCOUNT:
                REQUIRE(false, "Joint multi-curve calibration does not support LOG_DISCOUNT in the first cut");
                return 0;
            case CurveParameterization_::Value_::ZERO_RATE:
                REQUIRE(false, "Joint multi-curve calibration does not support ZERO_RATE");
                return 0;
            default:
                REQUIRE(false, "Unknown curve parameterization");
                return 0;
            }
        }

        // Build ONE declaration's curve from its slice of the joint parameter vector. A forward
        // declaration may opt into base layering via decl.baseLayeredOverDiscount_: when set, the
        // caller supplies the discount curve at decl.targetCollateral_ (built earlier in the SAME
        // solve iteration) as `base`, and the forward curve is stored as NewDiscountPWLF(..., base)
        // so the smoother acts on the spread forward f_abs - f_ois -- matching the staged path's
        // ApplyStageDefaults base layering. When base is empty (the default, including every
        // discount declaration and every baseless forward declaration), the curve is a raw PWL/PWC
        // with no base handle; the IBOR residual rows still read yc.Forward off the assembled
        // CurveBlock_ and route discounting to the discount declaration's slice.
        //
        // Base-layered forward curves carry OIS sensitivity through their base handle (B-new-2 fixed
        // for the opt-in path): a bump-and-reprice consumer who reads the standalone forward handle
        // sees the OIS delta flow through. The baseless representation remains available for callers
        // who prefer a self-contained forward curve and re-price through the assembled CurveBlock_.
        Handle_<DiscountCurve_> BuildDeclarationCurve(const JointCurveDeclaration_& decl,
                                                      const String_& ccy,
                                                      const DayBasis_& dayCount,
                                                      const Vector_<>& xSlice,
                                                      const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>()) {
            switch (decl.parameterization_.Switch()) {
            case CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD: {
                const int nKnots = static_cast<int>(decl.knotDates_.size());
                REQUIRE(static_cast<int>(xSlice.size()) == 2 * nKnots, "Joint PWL parameter slice length must equal 2 * nKnots");
                Vector_<> fLeft(nKnots);
                Vector_<> fRight(nKnots);
                for (int i = 0; i < nKnots; ++i) {
                    fLeft[i] = xSlice[2 * i];
                    fRight[i] = xSlice[2 * i + 1];
                }
                return Handle_<DiscountCurve_>(NewDiscountPWLF(decl.curveName_, ccy, PiecewiseLinear_(decl.knotDates_, fLeft, fRight), base));
            }
            case CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD: {
                REQUIRE(static_cast<int>(xSlice.size()) == static_cast<int>(decl.knotDates_.size()),
                        "Joint PWC parameter slice length must equal nKnots");
                REQUIRE(base.IsEmpty(), "Joint multi-curve calibration: base layering is not supported on PIECEWISE_CONSTANT_FWD declarations");
                return Handle_<DiscountCurve_>(NewDiscountPWC(decl.curveName_, ccy, PiecewiseConstant_(decl.knotDates_, xSlice)));
            }
            default:
                REQUIRE(false, "Joint multi-curve calibration: unsupported parameterization at curve build");
                return Handle_<DiscountCurve_>();
            }
        }

        // The Phase A float-convention accessor (Deposit/FRA/Future/Swap), or nullptr if the
        // instrument has no projection convention to inspect. Mirrors the eligibility helper in
        // calibration.cpp but is read-only here (used by the forward-projection validator).
        const RateIndexConvention_* FloatConventionOf(const YCInstrument_& inst) {
            if (const auto* deposit = dynamic_cast<const Deposit_*>(&inst))
                return &deposit->FloatConvention();
            if (const auto* fra = dynamic_cast<const FRA_*>(&inst))
                return &fra->FloatConvention();
            if (const auto* future = dynamic_cast<const Future_*>(&inst))
                return &future->FloatConvention();
            if (const auto* swap = dynamic_cast<const Swap_*>(&inst))
                return &swap->FloatConvention();
            return nullptr;
        }

        // B-new-1 validator: every forward declaration's instruments must route their fixing
        // through the forward curve (useProjectionCurve_ == true). A mis-conventioned instrument
        // would silently leave the forward curve unconstrained by data (BAR-C fails by percent-level
        // with no principled diagnostic). Returns the offending instrument name, or empty if OK.
        String_ ForwardDeclarationOffendingInstrument(const JointCurveDeclaration_& decl) {
            for (const auto& inst : decl.instruments_) {
                const RateIndexConvention_* conv = FloatConventionOf(*inst);
                if (!conv || !conv->useProjectionCurve_)
                    return inst->Name();
            }
            return String_();
        }

        // Mirrors calibration.cpp's OrderInstruments: sort by (maturity, start, name) so the
        // residual vector the solver sees is in a deterministic, monotone order. The per-curve
        // diagnostics preserve this order (instrumentNames_[i] aligns with residuals_[i]).
        Vector_<Handle_<YCInstrument_>> OrderInstruments(const Vector_<Handle_<YCInstrument_>>& instruments) {
            auto ordered = instruments;
            std::sort(ordered.begin(), ordered.end(), [](const Handle_<YCInstrument_>& lhs, const Handle_<YCInstrument_>& rhs) {
                const auto lhsSpan = lhs->TimeSpan();
                const auto rhsSpan = rhs->TimeSpan();
                if (lhsSpan.second != rhsSpan.second)
                    return lhsSpan.second < rhsSpan.second;
                if (lhsSpan.first != rhsSpan.first)
                    return lhsSpan.first < rhsSpan.first;
                return lhs->Name() < rhs->Name();
            });
            return ordered;
        }

        // Per-curve slice metadata: where this declaration's free parameters live in the joint x
        // vector, and where its instrument residuals live in the stacked residual vector.
        struct CurveSlot_ {
            int curveIndex;
            int paramOffset;
            int nParams;
            int residualOffset;
            int nInstruments;
            Vector_<Handle_<YCInstrument_>> instruments;
            Vector_<Handle_<YCInstrument_::Rate_>> rates;
            Vector_<> marketRates;
            Vector_<Date_> knotDates;
            int paramsPerKnot;
            double smoothingWeight;
        };

        // Entry-point validation per the api-note Error Cases table. Throws on the first violation
        // with a message naming the offending input. Runs BEFORE the residual function, the
        // CurveBlock_, and the solver are constructed so mis-routings surface as clear entry errors
        // rather than late confusing throws deep inside the first residual evaluation.
        std::vector<CurveSlot_> ValidateAndBuildSlots(const JointMultiCurveCalibrationSpec_& spec) {
            REQUIRE(!spec.curves_.empty(), "Joint multi-curve calibration requires at least one curve declaration");
            REQUIRE(spec.tolerance_ > 0.0, "Joint multi-curve calibration tolerance must be positive");
            REQUIRE(spec.fitTolerance_ > 0.0, "Joint multi-curve calibration fit tolerance must be positive");
            REQUIRE(spec.maxEvaluations_ > 0, "Joint multi-curve calibration max evaluations must be positive");
            REQUIRE(spec.maxRestarts_ > 0, "Joint multi-curve calibration max restarts must be positive");

            // Produced discount collaterals (for B-new-3) and duplicate-key checks.
            std::set<CollateralType_> producedCollaterals;
            std::set<PeriodLength_> producedTenors;
            bool hasDiscount = false;
            for (int di = 0; di < static_cast<int>(spec.curves_.size()); ++di) {
                const JointCurveDeclaration_& decl = spec.curves_[di];
                if (decl.calibrateDiscountCurve_) {
                    hasDiscount = true;
                    REQUIRE(producedCollaterals.insert(decl.targetCollateral_).second,
                            String_("Joint multi-curve calibration has duplicate discount collateral ") + decl.targetCollateral_.String());
                    REQUIRE(!decl.baseLayeredOverDiscount_,
                            String_("Joint curve declaration ") + String::FromInt(di) + " sets baseLayeredOverDiscount_ on a discount-curve "
                                      "declaration - base layering applies only to forward declarations");
                } else {
                    REQUIRE(producedTenors.insert(decl.targetTenor_).second,
                            String_("Joint multi-curve calibration has duplicate forward tenor ") + decl.targetTenor_.String());
                }
            }
            REQUIRE(hasDiscount, "Joint multi-curve calibration requires at least one discount-curve declaration");

            std::vector<CurveSlot_> slots;
            slots.reserve(spec.curves_.size());
            int paramOffset = 0;
            int residualOffset = 0;
            for (int i = 0; i < static_cast<int>(spec.curves_.size()); ++i) {
                const JointCurveDeclaration_& decl = spec.curves_[i];
                REQUIRE(!decl.instruments_.empty(), String_("Joint curve declaration ") + String::FromInt(i) + " requires at least one instrument");
                REQUIRE(!decl.knotDates_.empty(), String_("Joint curve declaration ") + String::FromInt(i) + " requires at least one knot date");
                for (int k = 1; k < static_cast<int>(decl.knotDates_.size()); ++k) {
                    REQUIRE(decl.knotDates_[k] > decl.knotDates_[k - 1], String_("Joint curve declaration ") + String::FromInt(i) +
                                                                             " knot dates must be strictly increasing - knot " + String::FromInt(k) +
                                                                             " = " + Date::ToString(decl.knotDates_[k]) + " not greater than knot " +
                                                                             String::FromInt(k - 1) + " = " + Date::ToString(decl.knotDates_[k - 1]));
                }
                REQUIRE(decl.knotDates_.front() > spec.today_,
                        String_("Joint curve declaration ") + String::FromInt(i) + " knot dates must be after today");
                REQUIRE(decl.smoothingWeight_ > 0.0, String_("Joint curve declaration ") + String::FromInt(i) + " smoothing weight must be positive");

                if (!decl.calibrateDiscountCurve_) {
                    REQUIRE(decl.targetTenor_ != PeriodLength_(),
                            String_("Forward-curve declaration ") + String::FromInt(i) + " requires a target tenor");
                    // Base layering is supported on PWL forward declarations only. The base resolves
                    // from the discount curve at decl.targetCollateral_; B-new-3 below already
                    // guarantees that collateral is produced by a discount declaration in this spec.
                    if (decl.baseLayeredOverDiscount_) {
                        REQUIRE(decl.parameterization_ == CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD,
                                String_("Joint curve declaration ") + String::FromInt(i) + " requests base layering on a non-PWL forward "
                                          "declaration - use PIECEWISE_LINEAR_FWD with baseLayeredOverDiscount_");
                    }
                    // B-new-3: forward declaration's discount collateral must be produced by some discount declaration.
                    REQUIRE(
                        producedCollaterals.count(decl.targetCollateral_) > 0,
                        String_("Joint curve declaration ") + String::FromInt(i) + " target collateral " + decl.targetCollateral_.String() +
                            " is not produced by any discount-curve declaration in this spec - add a discount declaration with targetCollateral_ = " +
                            decl.targetCollateral_.String());
                    // B-new-1: every forward instrument must project its fixing through the forward curve.
                    const String_ offender = ForwardDeclarationOffendingInstrument(decl);
                    REQUIRE(offender.empty(), String_("Joint curve declaration ") + String::FromInt(i) +
                                                  " is a forward-curve declaration but instrument " + offender +
                                                  " has useProjectionCurve_ = false (RateIndexConvention_) - its fixing routes to yc.Discount(" +
                                                  decl.targetCollateral_.String() +
                                                  ") and leaves the forward curve unconstrained by data; construct IBOR instruments via "
                                                  "Ccy::Conventions::LiborIndex()(ccy) so useProjectionCurve_ defaults to true");
                }

                const int paramsPerKnot = ParamsPerKnot(decl.parameterization_);
                const int nKnots = static_cast<int>(decl.knotDates_.size());
                const int nParams = paramsPerKnot * nKnots;
                const auto ordered = OrderInstruments(decl.instruments_);

                CurveSlot_ slot;
                slot.curveIndex = i;
                slot.paramOffset = paramOffset;
                slot.nParams = nParams;
                slot.residualOffset = residualOffset;
                slot.nInstruments = static_cast<int>(ordered.size());
                slot.instruments = ordered;
                slot.knotDates = decl.knotDates_;
                slot.paramsPerKnot = paramsPerKnot;
                slot.smoothingWeight = decl.smoothingWeight_;
                slot.rates.reserve(ordered.size());
                slot.marketRates.reserve(ordered.size());
                for (const auto& inst : ordered) {
                    slot.rates.push_back(inst->Precompute(Handle_<YieldCurve_>()));
                    slot.marketRates.push_back(inst->MarketRate());
                }
                slots.push_back(std::move(slot));
                paramOffset += nParams;
                residualOffset += slot.nInstruments;
            }
            return slots;
        }

        // Block-diagonal smoothing matrix: one tridiagonal block per curve, zero off-block by
        // construction. Each block is built by Underdetermined::SelfCouplePWC at the curve's
        // parameter offset, mirroring BuildCurveCalibrationWeights in calibration.cpp but
        // accumulated into a single full-joint-size Sparse::TriDiagonal_.
        std::unique_ptr<Sparse::TriDiagonal_> BuildJointSmoothing(const std::vector<CurveSlot_>& slots) {
            int totalParams = 0;
            for (const auto& slot : slots)
                totalParams += slot.nParams;
            auto weights = std::make_unique<Sparse::TriDiagonal_>(totalParams);
            for (const auto& slot : slots) {
                Vector_<DateTime_> expandedKnots;
                expandedKnots.reserve(slot.knotDates.size() * slot.paramsPerKnot);
                for (const auto& knot : slot.knotDates)
                    for (int i = 0; i < slot.paramsPerKnot; ++i)
                        expandedKnots.push_back(DateTime_(knot));
                Underdetermined::SelfCouplePWC(weights.get(), expandedKnots, slot.smoothingWeight, slot.paramOffset);
            }
            return weights;
        }

        // Build the per-declaration initial-guess slice. Falls back to spec.initialGuess_ flat when
        // the declaration leaves initialGuessPerNode_ empty.
        Vector_<> BuildGuessSlice(const JointMultiCurveCalibrationSpec_& spec, const JointCurveDeclaration_& decl, int nParams) {
            if (!decl.initialGuessPerNode_.empty()) {
                REQUIRE(static_cast<int>(decl.initialGuessPerNode_.size()) == nParams,
                        String_("Joint curve declaration ") + decl.curveName_ + " initialGuessPerNode_ length must equal its parameter count");
                return decl.initialGuessPerNode_;
            }
            return Vector_<>(nParams, spec.initialGuess_);
        }

        // RAII tape scope: Clear on entry and on exit (exception-safe), single-threaded contract.
        // Verbatim from calibration.cpp:268-280 (Phase A); factored inline here because the joint
        // path is the only second consumer and a shared header for two consumers is premature.
        struct TapeGuard_ {
            Dal::AAD::Tape_* t_;
            explicit TapeGuard_(Dal::AAD::Tape_* t) : t_(t) { Dal::AAD::Clear(*t_); }
            ~TapeGuard_() {
                try {
                    Dal::AAD::Clear(*t_);
                } catch (...) {
                    // swallow; we are unwinding
                }
            }
            TapeGuard_(const TapeGuard_&) = delete;
            TapeGuard_& operator=(const TapeGuard_&) = delete;
        };

        // Templated PWL_FWD curve builder (joint analogue of calibration.cpp:239-256
        // BuildDiscountCurveT<T_>). Constructs a Tape::DiscountPWLF_<T_> from the declaration's
        // fLeft/fRight parameter slice. Baseless when base is empty; base-layered (B_ = T_) when a
        // base handle is supplied (Gap 4 -- the base adjoints propagate through the reverse sweep).
        template <class T_, class B_ = Tape::DiscountCurve_<double>>
        std::shared_ptr<Tape::DiscountPWLF_<T_, B_>>
        BuildDeclarationCurveT(const JointCurveDeclaration_& decl,
                               const String_& ccy,
                               const Vector_<Date_>& knotDates,
                               const Vector_<T_>& fLeftT,
                               const Vector_<T_>& fRightT,
                               const Handle_<B_>& base = Handle_<B_>()) {
            REQUIRE(decl.parameterization_ == CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD,
                    "JointResidualFunction_::BuildDeclarationCurveT: only PIECEWISE_LINEAR_FWD is supported on the AAD path");
            return std::make_shared<Tape::DiscountPWLF_<T_, B_>>(decl.curveName_, ccy, knotDates, fLeftT, fRightT, base);
        }

        // Per-instrument joint eligibility (FR3 (e)/(g)). Returns true if the instrument can be
        // priced on the AAD tape; on fall-through emits a NOTICE naming the instrument and the
        // failing condition. NEVER throws (FR6). The tradeDate == knot 0 check (FR3 (i)) is done at
        // the declaration level in JointSpecEligibleForAnalyticJacobian (the instrument does not
        // carry its declaration's knot dates).
        bool InstrumentEligibleForAnalyticJacobian(const YCInstrument_& inst, bool onDiscountDeclaration) {
            const RateIndexConvention_* convPtr = FloatConventionOf(inst);
            REQUIRE(convPtr, "InstrumentEligibleForAnalyticJacobian: internal error -- type check should have caught this");
            const RateIndexConvention_& conv = *convPtr;
            if (dynamic_cast<const OISSwap_*>(&inst) || dynamic_cast<const Swap_*>(&inst) || dynamic_cast<const Deposit_*>(&inst)
                || dynamic_cast<const FRA_*>(&inst) || dynamic_cast<const Future_*>(&inst)) {
                // Type is fine. Check projection-vs-declaration consistency (FR3 (g)): a
                // discount-declaration instrument must NOT project (useProjectionCurve_ == false),
                // so its fixing routes to the discount curve and the AAD path binds a single curve.
                // OIS-discount-slice instruments (OIS overnight index has useProjectionCurve_ == false)
                // are the canonical discount-eligible case.
                if (onDiscountDeclaration && conv.useProjectionCurve_) {
                    const String_ msg = String_("Joint AAD Jacobian requires discount-declaration instruments to forecast off "
                                                "the discount curve; instrument '")
                                        + inst.Name() + "' projects (useProjectionCurve_ == true), falling back to bumped";
                    NOTICE(msg);
                    return false;
                }
                return true;
            }
            {
                const String_ msg = String_("Joint AAD Jacobian has no templated rate for instrument '") + inst.Name()
                                    + "' (type " + inst.Name() + ") in this declaration; falling back to bumped";
                NOTICE(msg);
            }
            return false;
        }

        // Per-spec joint eligibility (FR3). Pure query over ctor-stored state, NEVER throws,
        // returns true iff EVERY clause holds. Evaluated once and cached on JointResidualFunction_
        // (Phase A H1 once-per-call NOTICE budget).
        bool JointSpecEligibleForAnalyticJacobian(const JointMultiCurveCalibrationSpec_* spec,
                                                   const std::vector<CurveSlot_>* slots) {
            REQUIRE(spec && slots, "JointSpecEligibleForAnalyticJacobian: null spec/slots");
            for (int d = 0; d < static_cast<int>(slots->size()); ++d) {
                const CurveSlot_& slot = (*slots)[d];
                const JointCurveDeclaration_& decl = spec->curves_[slot.curveIndex];
                if (decl.parameterization_ != CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD) {
                    const String_ msg = String_("Joint AAD Jacobian requires CurveParameterization_::PIECEWISE_LINEAR_FWD on "
                                                "every declaration; declaration ")
                                        + String::FromInt(d) + " has " + decl.parameterization_.String()
                                        + "; falling back to bumped";
                    NOTICE(msg);
                    return false;
                }
                // (j): liborBasis_ == ACT_365F (the DAYS_PER_YEAR denominator the templated PWL
                // forward curve assumes).
                if (spec->liborBasis_.String() != String_("ACT_365F")) {
                    const String_ msg = String_("Joint AAD Jacobian requires liborBasis_ == ACT_365F (the DAYS_PER_YEAR "
                                                "denominator the templated PWL-forward curve assumes); spec has ")
                                        + spec->liborBasis_.String() + "; falling back to bumped";
                    NOTICE(msg);
                    return false;
                }
                // (e)/(f)/(g): instrument type + projection-vs-declaration consistency.
                const bool onDiscountDecl = decl.calibrateDiscountCurve_;
                for (int i = 0; i < slot.nInstruments; ++i) {
                    if (!InstrumentEligibleForAnalyticJacobian(*slot.instruments[i], onDiscountDecl))
                        return false;
                }
            }
            return true;
        }

        // The joint residual function. Builds EVERY declaration's curve from its slice of x,
        // assembles them into ONE CurveBlock_, and returns the stacked residuals of every
        // instrument. The CurveBlock_ routes IBOR discounting to the OIS slice of x automatically
        // (the post-2008 routing in curveblock.cpp) -- the one cross-curve coupling the joint
        // system has under interpretation (a).
        class JointResidualFunction_ : public Underdetermined::Function_ {
            const JointMultiCurveCalibrationSpec_* spec_;
            const std::vector<CurveSlot_>* slots_;
            String_ ccy_;
            DayBasis_ dayCount_;
            CurveJacobianMode_ jacobianMode_;
            mutable int evaluationCount_ = 0;
            enum class Eligibility_ { Unknown, Eligible, Ineligible };
            mutable Eligibility_ cachedEligibility_ = Eligibility_::Unknown;

            void EvaluateEligibilityOnce() const {
                if (cachedEligibility_ != Eligibility_::Unknown)
                    return;
                cachedEligibility_ = JointSpecEligibleForAnalyticJacobian(spec_, slots_) ? Eligibility_::Eligible : Eligibility_::Ineligible;
            }

            [[nodiscard]] bool Eligible() const {
                EvaluateEligibilityOnce();
                return cachedEligibility_ == Eligibility_::Eligible;
            }

            [[nodiscard]] Underdetermined::Jacobian_* AnalyticJacobian(const Vector_<>& x, const Vector_<>& /*f*/) const;

        public:
            JointResidualFunction_(const JointMultiCurveCalibrationSpec_& spec,
                                   const std::vector<CurveSlot_>& slots,
                                   CurveJacobianMode_ jacobianMode)
                : spec_(&spec), slots_(&slots), ccy_(spec.ccy_), dayCount_(spec.liborBasis_), jacobianMode_(jacobianMode) {}

            [[nodiscard]] int EvaluationCount() const { return evaluationCount_; }

            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
                ++evaluationCount_;
                std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves;
                std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves;
                // Two-pass build so a base-layered forward declaration resolves its base from a
                // discount curve built in the SAME solve iteration, regardless of declaration order.
                // Pass 1: discount declarations (they never carry a base).
                for (const auto& slot : *slots_) {
                    const JointCurveDeclaration_& decl = spec_->curves_[slot.curveIndex];
                    if (!decl.calibrateDiscountCurve_)
                        continue;
                    Vector_<> xSlice(slot.nParams);
                    for (int j = 0; j < slot.nParams; ++j)
                        xSlice[j] = x[slot.paramOffset + j];
                    discountCurves[decl.targetCollateral_] = BuildDeclarationCurve(decl, ccy_, dayCount_, xSlice);
                }
                // Pass 2: forward declarations; base resolves from the now-populated discountCurves
                // when the declaration opts into base layering, else empty.
                for (const auto& slot : *slots_) {
                    const JointCurveDeclaration_& decl = spec_->curves_[slot.curveIndex];
                    if (decl.calibrateDiscountCurve_)
                        continue;
                    Vector_<> xSlice(slot.nParams);
                    for (int j = 0; j < slot.nParams; ++j)
                        xSlice[j] = x[slot.paramOffset + j];
                    Handle_<DiscountCurve_> base;
                    if (decl.baseLayeredOverDiscount_)
                        base = discountCurves.at(decl.targetCollateral_);
                    forwardCurves[decl.targetTenor_] = BuildDeclarationCurve(decl, ccy_, dayCount_, xSlice, base);
                }
                CurveBlock_ yc("joint", ccy_, discountCurves, forwardCurves, dayCount_);

                int totalResiduals = 0;
                for (const auto& slot : *slots_)
                    totalResiduals += slot.nInstruments;
                Vector_<> residuals(totalResiduals);
                int offset = 0;
                for (const auto& slot : *slots_) {
                    for (int i = 0; i < slot.nInstruments; ++i)
                        residuals[offset + i] = (*slot.rates[i])(yc)-slot.marketRates[i];
                    offset += slot.nInstruments;
                }
                return residuals;
            }

            // Engage the analytic Jacobian IFF the mode is ANALYTIC AND the cached eligibility
            // verdict is Eligible. Otherwise return nullptr and the solver dense-bumps. Mirrors
            // Phase A's YieldCurveCalibrationFunc_::Gradient at calibration.cpp:368-389.
            [[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>& x, const Vector_<>& f) const override {
                if (jacobianMode_ != CurveJacobianMode_::Value_::ANALYTIC)
                    return nullptr;
                EvaluateEligibilityOnce();
                if (cachedEligibility_ == Eligibility_::Eligible)
                    return AnalyticJacobian(x, f);
                return nullptr;
            }
        };

        // The AAD reverse-sweep Jacobian (FR2, FR5). Recording contract (the ONE order that works on
        // all four backends): Clear -> RegisterIndependent (per free parameter, every declaration's
        // 2 * nKnots PWL params, NO anchor exclusion) -> NewRecording -> forward eval (build every
        // Number_-typed Tape::DiscountPWLF_<Number_>, baseless or base-layered, assemble the
        // JointCurveBlock_<Number_> + per-collateral YCCtx_<Number_> map) -> per residual row
        // {ZeroAdjoints -> Adjoint(residuals[i]) = 1.0 -> PropagateToStart -> harvest}.
        Underdetermined::Jacobian_* JointResidualFunction_::AnalyticJacobian(const Vector_<>& x, const Vector_<>& /*f*/) const {
            auto* tape = Dal::AAD::Tape();
            TapeGuard_ guard(tape);

            // 1. Register EVERY declaration's 2 * nKnots free parameters as independents.
            //    PWL_FWD has NO anchor exclusion -- every knot (including knot 0) is free. The
            //    column map is per-declaration: solver column (slot.paramOffset + 2k) maps to
            //    declaration d's fLeftT[k] and (slot.paramOffset + 2k+1) to fRightT[k].
            std::vector<std::pair<Vector_<Dal::AAD::Number_>, Vector_<Dal::AAD::Number_>>> fwdParamsPerDecl(slots_->size());
            for (int d = 0; d < static_cast<int>(slots_->size()); ++d) {
                const CurveSlot_& slot = (*slots_)[d];
                const int nKnots = static_cast<int>(slot.knotDates.size());
                auto& paramsPair = fwdParamsPerDecl[d];
                auto& fLeftT = paramsPair.first;
                auto& fRightT = paramsPair.second;
                fLeftT.Resize(nKnots);
                fRightT.Resize(nKnots);
                for (int k = 0; k < nKnots; ++k) {
                    Dal::AAD::RegisterIndependent(fLeftT[k], x[slot.paramOffset + 2 * k]);
                    Dal::AAD::RegisterIndependent(fRightT[k], x[slot.paramOffset + 2 * k + 1]);
                }
            }

            // 2. Open the recording AFTER registering the independents and BEFORE the forward pass.
            Dal::AAD::NewRecording(*tape);

            // 3. Two-pass templated curve build. Pass 1: discount declarations as
            //    Tape::DiscountPWLF_<Number_> (baseless), stored as shared_ptr so base-layered
            //    forward declarations in pass 2 can share ownership of their base (Gap 4, critique S6).
            std::map<CollateralType_, std::shared_ptr<Tape::DiscountCurve_<Dal::AAD::Number_>>> discountStorage;
            std::map<PeriodLength_, std::shared_ptr<Tape::DiscountCurve_<Dal::AAD::Number_>>> forwardStorage;
            std::vector<std::shared_ptr<Tape::DiscountCurve_<Dal::AAD::Number_>>> curveStorage;
            curveStorage.reserve(slots_->size());
            for (int d = 0; d < static_cast<int>(slots_->size()); ++d) {
                const CurveSlot_& slot = (*slots_)[d];
                const JointCurveDeclaration_& decl = spec_->curves_[slot.curveIndex];
                if (!decl.calibrateDiscountCurve_)
                    continue;
                const auto& [fLeftT, fRightT] = fwdParamsPerDecl[d];
                auto dc = BuildDeclarationCurveT<Dal::AAD::Number_>(decl, ccy_, slot.knotDates, fLeftT, fRightT);
                // Implicit shared_ptr<DiscountPWLF_<Number_>> -> shared_ptr<DiscountCurve_<Number_>>
                // upcast (DiscountPWLF_<T_> inherits DiscountCurve_<T_> via CurveWithBase_<...>).
                discountStorage[decl.targetCollateral_] = std::move(dc);
                curveStorage.push_back(discountStorage.at(decl.targetCollateral_));
            }
            for (int d = 0; d < static_cast<int>(slots_->size()); ++d) {
                const CurveSlot_& slot = (*slots_)[d];
                const JointCurveDeclaration_& decl = spec_->curves_[slot.curveIndex];
                if (decl.calibrateDiscountCurve_)
                    continue;
                const auto& [fLeftT, fRightT] = fwdParamsPerDecl[d];
                if (decl.baseLayeredOverDiscount_) {
                    // Gap 4: Number_-typed base sharing ownership of the discount curve (critique S6).
                    // The base is a Tape::DiscountPWLF_<Number_, DiscountCurve_<Number_>> whose
                    // adjoints propagate through the reverse sweep into the OIS discount-curve free
                    // nodes (the load-bearing Option-B generalization -- Phase A's DiscountLogDF_<T_>
                    // uses a double base treated as constant).
                    Handle_<Tape::DiscountCurve_<Dal::AAD::Number_>> base(discountStorage.at(decl.targetCollateral_));
                    auto fc = BuildDeclarationCurveT<Dal::AAD::Number_, Tape::DiscountCurve_<Dal::AAD::Number_> >(
                        decl, ccy_, slot.knotDates, fLeftT, fRightT, base);
                    forwardStorage[decl.targetTenor_] = std::move(fc);
                } else {
                    auto fc = BuildDeclarationCurveT<Dal::AAD::Number_>(decl, ccy_, slot.knotDates, fLeftT, fRightT);
                    forwardStorage[decl.targetTenor_] = std::move(fc);
                }
                curveStorage.push_back(forwardStorage.at(decl.targetTenor_));
            }

            // 4. Assemble BOTH routing contexts:
            //    (a) JointCurveBlock_<Number_> for the IBOR projection slice (forecast != discount).
            //    (b) per-collateral YCCtx_<Number_> map for the OIS-discount slice (forecast ==
            //        discount == that collateral's discount curve) -- the OIS slice rides the
            //        inherited Swap_::PrecomputeT<Number_> machinery bound to a single curve.
            Tape::JointCurveBlock_<Dal::AAD::Number_> block;
            for (const auto& [collateral, curve] : discountStorage)
                block.discountCurves[collateral] = curve.get();
            for (const auto& [tenor, curve] : forwardStorage)
                block.forwardCurves[tenor] = curve.get();

            // 5. Compute Number_-typed stacked residuals. For each instrument:
            //    - OIS-discount slice (useProjectionCurve_ == false, including OISSwap_): route
            //      through the inherited PrecomputeT<Number_> + YCCtx_<Number_> bound to the discount
            //      curve at the instrument's collateral. forecast == discount == that curve.
            //    - IBOR projection slice (useProjectionCurve_ == true): route through
            //      PrecomputeProjectionT<Number_> + JointCurveBlock_<Number_>. forecast resolves to
            //      the forward declaration's curve at (forecastTenor_, collateral_).
            int totalResiduals = 0;
            for (const auto& slot : *slots_)
                totalResiduals += slot.nInstruments;
            Vector_<Dal::AAD::Number_> residuals(totalResiduals);
            int offset = 0;
            for (int d = 0; d < static_cast<int>(slots_->size()); ++d) {
                const CurveSlot_& slot = (*slots_)[d];
                for (int i = 0; i < slot.nInstruments; ++i) {
                    const auto& inst = *slot.instruments[i];
                    const RateIndexConvention_* convPtr = FloatConventionOf(inst);
                    REQUIRE(convPtr, "JointResidualFunction_::AnalyticJacobian: internal error -- eligibility should have rejected this instrument type");
                    const RateIndexConvention_& conv = *convPtr;
                    if (conv.useProjectionCurve_) {
                        // IBOR projection slice: JointRate_<Number_> + JointCurveBlock_<Number_>.
                        auto rateT = Tape::ProjectionRateAt<Dal::AAD::Number_>(inst);
                        residuals[offset + i] = (*rateT)(block) - static_cast<double>(slot.marketRates[i]);
                    } else {
                        // OIS-discount slice: inherited Rate_<Number_> + YCCtx_<Number_> bound to the
                        // discount curve at the instrument's collateral. The overnight index has
                        // useProjectionCurve_ == false so forecast == discount == that curve.
                        Handle_<Tape::Rate_<Dal::AAD::Number_>> rateT;
                        const auto* instPtr = slot.instruments[i].get();
                        if (dynamic_cast<const Deposit_*>(instPtr))
                            rateT = static_cast<const Deposit_*>(instPtr)->PrecomputeT<Dal::AAD::Number_>();
                        else if (dynamic_cast<const FRA_*>(instPtr))
                            rateT = static_cast<const FRA_*>(instPtr)->PrecomputeT<Dal::AAD::Number_>();
                        else if (dynamic_cast<const Future_*>(instPtr))
                            rateT = static_cast<const Future_*>(instPtr)->PrecomputeT<Dal::AAD::Number_>();
                        else if (dynamic_cast<const Swap_*>(instPtr))
                            rateT = static_cast<const Swap_*>(instPtr)->PrecomputeT<Dal::AAD::Number_>();
                        const Tape::DiscountCurve_<Dal::AAD::Number_>& discountCurve = block.Discount(conv.collateral_);
                        const Tape::YCCtx_<Dal::AAD::Number_> ctx(discountCurve);
                        residuals[offset + i] = (*rateT)(ctx) - static_cast<double>(slot.marketRates[i]);
                    }
                }
                offset += slot.nInstruments;
            }

            // 6. Dense Jacobian (totalResiduals x totalFreeParams), single-result reverse sweep.
            const int nCols = static_cast<int>(x.size());
            Matrix_<> j(totalResiduals, nCols, 0.0);
            for (int i = 0; i < totalResiduals; ++i) {
                Dal::AAD::ZeroAdjoints(*tape);
                Dal::AAD::Adjoint(residuals[i]) = 1.0;
                Dal::AAD::PropagateToStart(*tape);
                for (int d = 0; d < static_cast<int>(slots_->size()); ++d) {
                    const CurveSlot_& slot = (*slots_)[d];
                    const int nKnots = static_cast<int>(slot.knotDates.size());
                    const auto& [fLeftT, fRightT] = fwdParamsPerDecl[d];
                    for (int k = 0; k < nKnots; ++k) {
                        j(i, slot.paramOffset + 2 * k) = Dal::AAD::Value(Dal::AAD::Adjoint(fLeftT[k]));
                        j(i, slot.paramOffset + 2 * k + 1) = Dal::AAD::Value(Dal::AAD::Adjoint(fRightT[k]));
                    }
                }
            }
            return new XCurveJacobian_(std::move(j));
        }

        // Run the joint solver (EXACT via Find, APPROXIMATE via Approximate) and return the solved x.
        Vector_<> RunJointSolver(const JointMultiCurveCalibrationSpec_& spec,
                                 const JointResidualFunction_& func,
                                 const Vector_<>& guess,
                                 const Vector_<>& tol,
                                 const Sparse::TriDiagonal_& weights,
                                 Matrix_<>* optFwdJacAtSolution = nullptr) {
            Dictionary_ ctrlDict;
            ctrlDict.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(spec.maxEvaluations_)));
            ctrlDict.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(spec.maxRestarts_)));
            UnderdeterminedControls_ controls(ctrlDict);

            if (spec.solveMode_ == CurveSolveMode_::Value_::EXACT) {
                std::unique_ptr<Sparse::SymmetricDecomposition_> wDecomp(weights.DecomposeSymmetric());
                return Underdetermined::Find(func, guess, tol, *wDecomp, controls, nullptr, optFwdJacAtSolution);
            }
            return Underdetermined::Approximate(func, guess, tol, spec.fitTolerance_, weights, controls);
        }

        // Assemble the per-curve + joint diagnostics from the solved x. Mirrors BuildDiagnostics in
        // calibration.cpp: modelRates via a final CurveBlock_ read, residuals = model - market.
        JointCurveCalibrationDiagnostics_
        BuildCurveDiagnostics(const JointCurveDeclaration_& decl, const CurveSlot_& slot, const CurveBlock_& solvedBlock, bool usedApproximateFit) {
            JointCurveCalibrationDiagnostics_ diag;
            diag.curveName_ = decl.curveName_;
            diag.curveIndex_ = slot.curveIndex;
            diag.usedApproximateFit_ = usedApproximateFit;
            diag.instrumentNames_.reserve(slot.nInstruments);
            diag.marketRates_.reserve(slot.nInstruments);
            diag.modelRates_.reserve(slot.nInstruments);
            diag.residuals_.reserve(slot.nInstruments);
            double sqResidual = 0.0;
            for (int i = 0; i < slot.nInstruments; ++i) {
                const double modelRate = (*slot.rates[i])(solvedBlock);
                const double marketRate = slot.marketRates[i];
                const double residual = modelRate - marketRate;
                diag.instrumentNames_.push_back(slot.instruments[i]->Name());
                diag.marketRates_.push_back(marketRate);
                diag.modelRates_.push_back(modelRate);
                diag.residuals_.push_back(residual);
                diag.maxAbsResidual_ = std::max(diag.maxAbsResidual_, std::fabs(residual));
                sqResidual += residual * residual;
            }
            diag.rmsResidual_ = slot.nInstruments == 0 ? 0.0 : std::sqrt(sqResidual / slot.nInstruments);
            return diag;
        }

        // On non-convergence the joint residual norm exceeds the fit tolerance floor. Report the
        // failing solve and the residual norm so the caller can see how far off convergence is.
        [[noreturn]] void ThrowNonConvergence(const JointResidualFunction_& func, const Vector_<>& residuals) {
            double maxAbs = 0.0;
            double sq = 0.0;
            for (const double r : residuals) {
                maxAbs = std::max(maxAbs, std::fabs(r));
                sq += r * r;
            }
            const double rms = residuals.empty() ? 0.0 : std::sqrt(sq / residuals.size());
            THROW(String_("Joint multi-curve calibration failed to converge: maxAbsResidual = ") + String::FromDouble(maxAbs) +
                  ", rmsResidual = " + String::FromDouble(rms) + " after " + String::FromInt(func.EvaluationCount()) + " evaluations");
        }
    } // namespace

    JointMultiCurveCalibrationResult_ CalibrateJointMultiCurve(const JointMultiCurveCalibrationSpec_& spec) {
        return CalibrateJointMultiCurve(spec, JointMultiCurveCalibrationOptions_());
    }

    JointMultiCurveCalibrationResult_
    CalibrateJointMultiCurve(const JointMultiCurveCalibrationSpec_& spec, const JointMultiCurveCalibrationOptions_& options) {
        const std::vector<CurveSlot_> slots = ValidateAndBuildSlots(spec);

        int totalParams = 0;
        int totalResiduals = 0;
        for (const auto& slot : slots) {
            totalParams += slot.nParams;
            totalResiduals += slot.nInstruments;
        }

        Vector_<> guess(totalParams);
        int guessOffset = 0;
        for (const auto& slot : slots) {
            const JointCurveDeclaration_& decl = spec.curves_[slot.curveIndex];
            const Vector_<> slice = BuildGuessSlice(spec, decl, slot.nParams);
            for (int j = 0; j < slot.nParams; ++j)
                guess[guessOffset + j] = slice[j];
            guessOffset += slot.nParams;
        }

        const Vector_<> tol(totalResiduals, spec.tolerance_);
        std::unique_ptr<Sparse::TriDiagonal_> weights = BuildJointSmoothing(slots);

        JointResidualFunction_ func(spec, slots, options.jacobianMode_);
        Matrix_<> fwdJacAtSolution;
        const Vector_<> solved = RunJointSolver(spec, func, guess, tol, *weights, &fwdJacAtSolution);

        // Convergence check: the solver returns the best-found x, but APPROXIMATE can leave a
        // smoothing-vs-fit residual floor; re-evaluate and confirm every residual is within the
        // loose 10x fitTolerance_ bar (BAR-A's precondition). If not, throw naming the norm.
        const Vector_<> finalResiduals = func.F(solved);
        const double barA = 10.0 * spec.fitTolerance_;
        bool converged = true;
        for (const double r : finalResiduals)
            if (std::fabs(r) > barA) {
                converged = false;
                break;
            }

        JointMultiCurveCalibrationResult_ result;
        // Assemble the solved curves into the result maps and a CurveBlock_ for diagnostics. Two-pass
        // for the same reason as F: a base-layered forward curve must resolve its base from a solved
        // discount curve, independent of declaration order.
        std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves;
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves;
        for (const auto& slot : slots) {
            const JointCurveDeclaration_& decl = spec.curves_[slot.curveIndex];
            if (!decl.calibrateDiscountCurve_)
                continue;
            Vector_<> xSlice(slot.nParams);
            for (int j = 0; j < slot.nParams; ++j)
                xSlice[j] = solved[slot.paramOffset + j];
            discountCurves[decl.targetCollateral_] = BuildDeclarationCurve(decl, spec.ccy_, spec.liborBasis_, xSlice);
        }
        for (const auto& slot : slots) {
            const JointCurveDeclaration_& decl = spec.curves_[slot.curveIndex];
            if (decl.calibrateDiscountCurve_)
                continue;
            Vector_<> xSlice(slot.nParams);
            for (int j = 0; j < slot.nParams; ++j)
                xSlice[j] = solved[slot.paramOffset + j];
            Handle_<DiscountCurve_> base;
            if (decl.baseLayeredOverDiscount_)
                base = discountCurves.at(decl.targetCollateral_);
            forwardCurves[decl.targetTenor_] = BuildDeclarationCurve(decl, spec.ccy_, spec.liborBasis_, xSlice, base);
        }
        const CurveBlock_ solvedBlock("joint", spec.ccy_, discountCurves, forwardCurves, spec.liborBasis_);

        const bool usedApproximateFit = spec.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE;
        double jointMaxAbs = 0.0;
        double jointSq = 0.0;
        for (const auto& slot : slots) {
            const JointCurveDeclaration_& decl = spec.curves_[slot.curveIndex];
            const JointCurveCalibrationDiagnostics_ diag = BuildCurveDiagnostics(decl, slot, solvedBlock, usedApproximateFit);
            jointMaxAbs = std::max(jointMaxAbs, diag.maxAbsResidual_);
            for (const double r : diag.residuals_)
                jointSq += r * r;
            result.diagnostics_.push_back(diag);
        }
        result.discountCurves_ = std::move(discountCurves);
        result.forwardCurves_ = std::move(forwardCurves);
        result.jointMaxAbsResidual_ = jointMaxAbs;
        result.jointRmsResidual_ = totalResiduals == 0 ? 0.0 : std::sqrt(jointSq / totalResiduals);
        result.solverEvaluations_ = func.EvaluationCount();
        result.jacobianAtSolution_ = std::move(fwdJacAtSolution);

        if (!converged)
            ThrowNonConvergence(func, finalResiduals);
        result.converged_ = true;
        return result;
    }

} // namespace Dal
