//
// Created by dal-implementer on 2026/6/20.
//

#include <algorithm>
#include <cmath>
#include <dal/curve/aadjacobian.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/curvejacobian.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/jointcalibration.hpp>
#include <dal/curve/jointcalibration_internal.hpp>
#include <dal/curve/jointrate.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycctx.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/optimization/underdeterminedutils.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/dictionary.hpp>
#include <dal/utilities/numerics.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace Dal {

    namespace {
        constexpr const char* KEY_MAX_EVALUATIONS = "MAXEVALUATIONS";
        constexpr const char* KEY_MAX_RESTARTS = "MAXRESTARTS";

        using JointCalibrationInternal::CurveSlot_;

        template <class T_> Vector_<T_> SliceParameters(const Vector_<T_>& parameters, const CurveSlot_& slot) {
            return JointCalibrationInternal::SliceParameters(parameters, slot);
        }

        JointCalibrationInternal::CurveCollectionSpec_ InternalSpec(const JointMultiCurveCalibrationSpec_& spec) {
            JointCalibrationInternal::CurveCollectionSpec_ result;
            result.today_ = spec.today_;
            result.ccy_ = spec.ccy_;
            result.liborBasis_ = spec.liborBasis_;
            result.curves_ = &spec.curves_;
            result.context_ = "Joint multi-curve calibration";
            result.declarationLabel_ = "Declaration";
            return result;
        }

        std::vector<CurveSlot_> ValidateAndBuildSlots(const JointMultiCurveCalibrationSpec_& spec) {
            REQUIRE(spec.tolerance_ > 0.0 && spec.fitTolerance_ > 0.0, "Joint multi-curve calibration tolerances must be positive");
            REQUIRE(spec.maxEvaluations_ > 0 && spec.maxRestarts_ > 0, "Joint multi-curve calibration iteration caps must be positive");
            return JointCalibrationInternal::ValidateAndBuildSlots(InternalSpec(spec));
        }

        Vector_<> BuildGuessSlice(const JointMultiCurveCalibrationSpec_& spec, const JointCurveDeclaration_& decl, int nParams) {
            return JointCalibrationInternal::BuildGuessSlice(decl, nParams, spec.initialGuess_,
                                                             String_("Joint curve declaration ") + decl.curveName_);
        }

        [[nodiscard]] bool IsSupportedInstrumentType(const YCInstrument_& inst) {
            // OISSwap_ inherits Swap_, so VisitRate routes it to the Swap_ arm.
            return VisitRate(
                inst, [](const Deposit_&) { return true; }, [](const FRA_&) { return true; }, [](const Future_&) { return true; },
                [](const Swap_&) { return true; });
        }

        bool InstrumentEligibleForAnalyticJacobian(const YCInstrument_& inst, bool onDiscountDeclaration) {
            const RateIndexConvention_* convPtr = FloatConventionOf(inst);
            if (!convPtr) {
                NOTICE("Joint AAD Jacobian: unsupported instrument type (no float convention); falling back to bumped");
                return false;
            }
            const RateIndexConvention_& conv = *convPtr;
            if (IsSupportedInstrumentType(inst)) {
                // See docs/methodology/yield_curve_jacobian.md §Joint Multi-Curve Analytic Jacobian.
                if (onDiscountDeclaration && conv.useProjectionCurve_) {
                    const String_ msg = String_("Joint AAD Jacobian requires discount-declaration instruments to forecast off "
                                                "the discount curve; instrument '") +
                                        inst.Name() + "' projects (useProjectionCurve_ == true), falling back to bumped";
                    NOTICE(msg);
                    return false;
                }
                return true;
            }
            {
                const String_ msg = String_("Joint AAD Jacobian has no templated rate for instrument '") + inst.Name() +
                                    "' in this declaration; falling back to bumped";
                NOTICE(msg);
            }
            return false;
        }

        bool JointSpecEligibleForAnalyticJacobian(const JointMultiCurveCalibrationSpec_* spec, const std::vector<CurveSlot_>* slots) {
            REQUIRE(spec && slots, "JointSpecEligibleForAnalyticJacobian: null spec/slots");
            if (spec->liborBasis_.String() != String_("ACT_365F")) {
                NOTICE("Joint AAD Jacobian requires liborBasis_ == ACT_365F; falling back to bumped");
                return false;
            }
            for (int d = 0; d < static_cast<int>(slots->size()); ++d) {
                const CurveSlot_& slot = (*slots)[d];
                const JointCurveDeclaration_& decl = spec->curves_[slot.curveIndex_];
                const bool onDiscountDecl = decl.calibrateDiscountCurve_;
                for (int i = 0; i < slot.nInstruments_; ++i) {
                    if (!InstrumentEligibleForAnalyticJacobian(*slot.instruments_[i], onDiscountDecl))
                        return false;
                }
            }
            return true;
        }

        class JointResidualFunction_ : public Underdetermined::Function_ {
            const JointMultiCurveCalibrationSpec_* spec_;
            const std::vector<CurveSlot_>* slots_;
            CurveJacobianMode_ jacobianMode_;
            mutable int evaluationCount_ = 0;
            mutable AnalyticEligibility_ cachedEligibility_ = AnalyticEligibility_::Value_::UNKNOWN;

            void EvaluateEligibilityOnce() const {
                if (cachedEligibility_ != AnalyticEligibility_::Value_::UNKNOWN)
                    return;
                cachedEligibility_ = JointSpecEligibleForAnalyticJacobian(spec_, slots_) ? AnalyticEligibility_::Value_::ELIGIBLE
                                                                                         : AnalyticEligibility_::Value_::INELIGIBLE;
            }

            [[nodiscard]] bool Eligible() const {
                EvaluateEligibilityOnce();
                return cachedEligibility_ == AnalyticEligibility_::Value_::ELIGIBLE;
            }

            [[nodiscard]] Underdetermined::Jacobian_* AnalyticJacobian(const Vector_<>& x, const Vector_<>& /*f*/) const;

        public:
            JointResidualFunction_(const JointMultiCurveCalibrationSpec_& spec, const std::vector<CurveSlot_>& slots, CurveJacobianMode_ jacobianMode)
                : spec_(&spec), slots_(&slots), jacobianMode_(jacobianMode) {}

            [[nodiscard]] int EvaluationCount() const { return evaluationCount_; }

            Handle_<CurveBlock_> BuildCurvesFromX(const Vector_<>& x) const {
                return JointCalibrationInternal::BuildCurveBlock(InternalSpec(*spec_), *slots_, x);
            }

            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
                ++evaluationCount_;
                const Handle_<CurveBlock_> yc = BuildCurvesFromX(x);
                int totalResiduals = 0;
                for (const auto& slot : *slots_)
                    totalResiduals += slot.nInstruments_;
                Vector_<> residuals(totalResiduals);
                JointCalibrationInternal::AppendDoubleResiduals(*slots_, *yc, &residuals);
                return residuals;
            }

            // Returns AAD-tape Jacobian when ANALYTIC + eligible; nullptr otherwise (solver dense-bumps).
            [[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>& x, const Vector_<>& f) const override {
                if (jacobianMode_ != CurveJacobianMode_::Value_::ANALYTIC)
                    return nullptr;
                EvaluateEligibilityOnce();
                if (cachedEligibility_ == AnalyticEligibility_::Value_::ELIGIBLE)
                    return AnalyticJacobian(x, f);
                return nullptr;
            }

            [[nodiscard]] Vector_<Dal::AAD::Number_> ComputeTemplatedResiduals(const Tape::JointCurveBlock_<Dal::AAD::Number_>& block) const {
                int totalResiduals = 0;
                for (const auto& slot : *slots_)
                    totalResiduals += slot.nInstruments_;
                Vector_<Dal::AAD::Number_> residuals(totalResiduals);
                JointCalibrationInternal::AppendTemplatedResiduals(*slots_, block, &residuals);
                return residuals;
            }
        };

        Underdetermined::Jacobian_* JointResidualFunction_::AnalyticJacobian(const Vector_<>& x, const Vector_<>& /*f*/) const {
            auto* tape = Dal::AAD::Tape();
            TapeGuard_ guard(tape);

            Vector_<Dal::AAD::Number_> parameters = RegisterCurveParameters(x);
            Dal::AAD::NewRecording(*tape);

            const auto storage = JointCalibrationInternal::BuildTypedCurveBlock<Dal::AAD::Number_>(InternalSpec(*spec_), *slots_, parameters);
            Vector_<Dal::AAD::Number_> residuals = ComputeTemplatedResiduals(storage.block_);
            return new XCurveJacobian_(HarvestCurveJacobian(*tape, parameters, residuals));
        }

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

        [[noreturn]] void ThrowNonConvergence(const JointResidualFunction_& func, const Vector_<>& residuals) {
            const ResidualStats_ stats = ResidualStats(residuals);
            THROW(String_("Joint multi-curve calibration failed to converge: maxAbsResidual = ") + String::FromDouble(stats.maxAbsResidual_) +
                  ", rmsResidual = " + String::FromDouble(stats.rmsResidual_) + " after " + String::FromInt(func.EvaluationCount()) + " evaluations");
        }

        // ---- CalibrateJointMultiCurve assembly helpers ----

        Vector_<> BuildInitialGuess(const JointMultiCurveCalibrationSpec_& spec, const std::vector<CurveSlot_>& slots, int totalParams) {
            Vector_<> g(totalParams);
            int off = 0;
            for (const auto& s : slots) {
                const Vector_<> sl = BuildGuessSlice(spec, spec.curves_[s.curveIndex_], s.nParams_);
                for (int j = 0; j < s.nParams_; ++j)
                    g[off + j] = sl[j];
                off += s.nParams_;
            }
            return g;
        }

        std::pair<std::map<CollateralType_, Handle_<DiscountCurve_>>, std::map<PeriodLength_, Handle_<DiscountCurve_>>>
        BuildSolvedCurves(const JointMultiCurveCalibrationSpec_& spec, const std::vector<CurveSlot_>& slots, const Vector_<>& solved) {
            JointCalibrationInternal::CurveMaps_ maps = JointCalibrationInternal::BuildCurveMaps(InternalSpec(spec), slots, solved);
            return {std::move(maps.discountCurves_), std::move(maps.forwardCurves_)};
        }

        JointMultiCurveCalibrationResult_ AssembleResult(const JointMultiCurveCalibrationSpec_& spec,
                                                         const std::vector<CurveSlot_>& slots,
                                                         const CurveBlock_& solvedBlock,
                                                         std::map<CollateralType_, Handle_<DiscountCurve_>>& discountCurves,
                                                         std::map<PeriodLength_, Handle_<DiscountCurve_>>& forwardCurves,
                                                         int totalResiduals,
                                                         int evalCount,
                                                         Matrix_<>&& fwdJac) {
            JointMultiCurveCalibrationResult_ result;
            const bool usedApprox = spec.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE;
            double jointMaxAbs = 0.0, jointSq = 0.0;
            for (const auto& s : slots) {
                const JointCurveCalibrationDiagnostics_ diag =
                    JointCalibrationInternal::BuildCurveDiagnostics(spec.curves_[s.curveIndex_], s, solvedBlock, usedApprox);
                jointMaxAbs = std::max(jointMaxAbs, diag.maxAbsResidual_);
                for (const double r : diag.residuals_)
                    jointSq += r * r;
                result.diagnostics_.push_back(diag);
            }
            result.discountCurves_ = std::move(discountCurves);
            result.forwardCurves_ = std::move(forwardCurves);
            result.jointMaxAbsResidual_ = jointMaxAbs;
            result.jointRmsResidual_ = totalResiduals ? std::sqrt(jointSq / totalResiduals) : 0.0;
            result.solverEvaluations_ = evalCount;
            result.jacobianAtSolution_ = std::move(fwdJac);
            return result;
        }
    } // namespace

    JointMultiCurveCalibrationResult_ CalibrateJointMultiCurve(const JointMultiCurveCalibrationSpec_& spec) {
        return CalibrateJointMultiCurve(spec, JointMultiCurveCalibrationOptions_());
    }

    JointMultiCurveCalibrationResult_ CalibrateJointMultiCurve(const JointMultiCurveCalibrationSpec_& spec,
                                                               const JointMultiCurveCalibrationOptions_& options) {
        const std::vector<CurveSlot_> slots = ValidateAndBuildSlots(spec);

        int totalParams = 0, totalResiduals = 0;
        for (const auto& slot : slots) {
            totalParams += slot.nParams_;
            totalResiduals += slot.nInstruments_;
        }

        const Vector_<> guess = BuildInitialGuess(spec, slots, totalParams);

        const Vector_<> tol(totalResiduals, spec.tolerance_);
        std::unique_ptr<Sparse::TriDiagonal_> weights = JointCalibrationInternal::BuildJointSmoothing(slots);
        JointResidualFunction_ func(spec, slots, options.jacobianMode_);
        Matrix_<> fwdJacAtSolution;
        const Vector_<> solved = RunJointSolver(spec, func, guess, tol, *weights, options.computeJacobianAtSolution_ ? &fwdJacAtSolution : nullptr);

        const Vector_<> finalResiduals = func.F(solved);
        const double barA = 10.0 * spec.fitTolerance_;
        bool converged = true;
        for (const double r : finalResiduals) {
            if (std::fabs(r) > barA) {
                converged = false;
                break;
            }
        }

        auto [discountCurves, forwardCurves] = BuildSolvedCurves(spec, slots, solved);
        const CurveBlock_ solvedBlock("joint", spec.ccy_, discountCurves, forwardCurves, spec.liborBasis_);

        JointMultiCurveCalibrationResult_ result = AssembleResult(spec, slots, solvedBlock, discountCurves, forwardCurves, totalResiduals,
                                                                  func.EvaluationCount(), std::move(fwdJacAtSolution));
        if (!converged)
            ThrowNonConvergence(func, finalResiduals);
        result.converged_ = true;
        return result;
    }

} // namespace Dal
