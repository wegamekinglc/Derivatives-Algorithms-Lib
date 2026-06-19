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
#include <dal/curve/jointcalibration.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
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

        // Build ONE declaration's curve from its slice of the joint parameter vector. Forward
        // declarations are stored as raw PWL/PWC curves with NO base handle (the spec's "no base
        // concept"): their IBOR residual rows read yc.Forward(tenor, collateral) off the assembled
        // CurveBlock_ (which routes discounting to the discount declaration's curve), so the forecast
        // fixings are forced onto the forward curve while discounting flows through the OIS slice.
        // The stored forward curve therefore carries no OIS sensitivity (B-new-2 caveat): a bump-
        // and-reprice consumer must re-price through the assembled CurveBlock_ rather than the
        // standalone forward handle. This matches the approved API (no baseCurve_ field) and the
        // Non-Goals (no AAD risk work in the first cut).
        Handle_<DiscountCurve_>
        BuildDeclarationCurve(const JointCurveDeclaration_& decl, const String_& ccy, const DayBasis_& dayCount, const Vector_<>& xSlice) {
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
                return Handle_<DiscountCurve_>(NewDiscountPWLF(decl.curveName_, ccy, PiecewiseLinear_(decl.knotDates_, fLeft, fRight)));
            }
            case CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD: {
                REQUIRE(static_cast<int>(xSlice.size()) == static_cast<int>(decl.knotDates_.size()),
                        "Joint PWC parameter slice length must equal nKnots");
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
            for (const auto& decl : spec.curves_) {
                if (decl.calibrateDiscountCurve_) {
                    hasDiscount = true;
                    REQUIRE(producedCollaterals.insert(decl.targetCollateral_).second,
                            String_("Joint multi-curve calibration has duplicate discount collateral ") + decl.targetCollateral_.String());
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
            mutable int evaluationCount_ = 0;

        public:
            JointResidualFunction_(const JointMultiCurveCalibrationSpec_& spec, const std::vector<CurveSlot_>& slots)
                : spec_(&spec), slots_(&slots), ccy_(spec.ccy_), dayCount_(spec.liborBasis_) {}

            [[nodiscard]] int EvaluationCount() const { return evaluationCount_; }

            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
                ++evaluationCount_;
                std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves;
                std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves;
                for (const auto& slot : *slots_) {
                    const JointCurveDeclaration_& decl = spec_->curves_[slot.curveIndex];
                    Vector_<> xSlice(slot.nParams);
                    for (int j = 0; j < slot.nParams; ++j)
                        xSlice[j] = x[slot.paramOffset + j];
                    const Handle_<DiscountCurve_> curve = BuildDeclarationCurve(decl, ccy_, dayCount_, xSlice);
                    if (decl.calibrateDiscountCurve_)
                        discountCurves[decl.targetCollateral_] = curve;
                    else
                        forwardCurves[decl.targetTenor_] = curve;
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

            // Bumped Jacobian only. No AAD in the first cut (Non-Goals); the solver dense-bumps.
            [[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>&, const Vector_<>&) const override { return nullptr; }
        };

        // Run the joint solver (EXACT via Find, APPROXIMATE via Approximate) and return the solved x.
        Vector_<> RunJointSolver(const JointMultiCurveCalibrationSpec_& spec,
                                 const JointResidualFunction_& func,
                                 const Vector_<>& guess,
                                 const Vector_<>& tol,
                                 const Sparse::TriDiagonal_& weights) {
            Dictionary_ ctrlDict;
            ctrlDict.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(spec.maxEvaluations_)));
            ctrlDict.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(spec.maxRestarts_)));
            UnderdeterminedControls_ controls(ctrlDict);

            if (spec.solveMode_ == CurveSolveMode_::Value_::EXACT) {
                std::unique_ptr<Sparse::SymmetricDecomposition_> wDecomp(weights.DecomposeSymmetric());
                return Underdetermined::Find(func, guess, tol, *wDecomp, controls);
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

        JointResidualFunction_ func(spec, slots);
        const Vector_<> solved = RunJointSolver(spec, func, guess, tol, *weights);

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
        // Assemble the solved curves into the result maps and a CurveBlock_ for diagnostics.
        std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves;
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves;
        for (const auto& slot : slots) {
            const JointCurveDeclaration_& decl = spec.curves_[slot.curveIndex];
            Vector_<> xSlice(slot.nParams);
            for (int j = 0; j < slot.nParams; ++j)
                xSlice[j] = solved[slot.paramOffset + j];
            const Handle_<DiscountCurve_> curve = BuildDeclarationCurve(decl, spec.ccy_, spec.liborBasis_, xSlice);
            if (decl.calibrateDiscountCurve_)
                discountCurves[decl.targetCollateral_] = curve;
            else
                forwardCurves[decl.targetTenor_] = curve;
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

        if (!converged)
            ThrowNonConvergence(func, finalResiduals);
        result.converged_ = true;
        return result;
    }

} // namespace Dal
