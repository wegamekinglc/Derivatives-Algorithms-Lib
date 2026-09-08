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
            REQUIRE(std::isfinite(spec.tolerance_) && std::isfinite(spec.fitTolerance_) && spec.tolerance_ > 0.0 && spec.fitTolerance_ > 0.0,
                    "Joint multi-curve calibration tolerances must be positive and finite");
            REQUIRE(spec.maxEvaluations_ > 0 && spec.maxRestarts_ > 0, "Joint multi-curve calibration iteration caps must be positive");
            return JointCalibrationInternal::ValidateAndBuildSlots(InternalSpec(spec));
        }

        Vector_<>
        BuildGuessSlice(const JointMultiCurveCalibrationSpec_& spec, const JointCurveDeclaration_& decl, const CurveDefinition_& definition) {
            return JointCalibrationInternal::BuildGuessSlice(decl, definition, spec.initialGuess_,
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
            if (!HasAct365FLiborBasis(spec->liborBasis_)) {
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
            bool quoteRiskRequested_;
            int* evaluationCount_;
            AnalyticEligibility_ analyticEligibility_ = AnalyticEligibility_::Value_::UNKNOWN;

            [[nodiscard]] std::unique_ptr<Underdetermined::Jacobian_> AnalyticJacobian(const Vector_<>& x, const Vector_<>& /*f*/) const;

        public:
            JointResidualFunction_(const JointMultiCurveCalibrationSpec_& spec,
                                   const std::vector<CurveSlot_>& slots,
                                   CurveJacobianMode_ jacobianMode,
                                   bool quoteRiskRequested,
                                   int* evaluationCount)
                : spec_(&spec), slots_(&slots), jacobianMode_(jacobianMode), quoteRiskRequested_(quoteRiskRequested),
                  evaluationCount_(evaluationCount) {
                REQUIRE(evaluationCount_, "JointResidualFunction_: null evaluation count");
                if (jacobianMode_ == CurveJacobianMode_::Value_::ANALYTIC)
                    analyticEligibility_ = JointSpecEligibleForAnalyticJacobian(spec_, slots_) ? AnalyticEligibility_::Value_::ELIGIBLE
                                                                                               : AnalyticEligibility_::Value_::INELIGIBLE;
            }

            Handle_<CurveBlock_> BuildCurvesFromX(const Vector_<>& x) const {
                return JointCalibrationInternal::BuildCurveBlock(InternalSpec(*spec_), *slots_, x);
            }

            String_ JacobianModeUsed() const {
                return jacobianMode_ == CurveJacobianMode_::Value_::ANALYTIC && analyticEligibility_ == AnalyticEligibility_::Value_::ELIGIBLE
                           ? "ANALYTIC"
                           : "BUMPED";
            }

            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
                ++*evaluationCount_;
                const Handle_<CurveBlock_> yc = BuildCurvesFromX(x);
                int totalResiduals = 0;
                for (const auto& slot : *slots_)
                    totalResiduals += slot.nInstruments_;
                Vector_<> residuals(totalResiduals);
                JointCalibrationInternal::AppendDoubleResiduals(*slots_, *yc, &residuals);
                return residuals;
            }

            // Returns AAD-tape Jacobian when ANALYTIC + eligible; nullptr otherwise (solver dense-bumps).
            [[nodiscard]] std::unique_ptr<Underdetermined::Jacobian_> Gradient(const Vector_<>& x, const Vector_<>& f) const override {
                if (jacobianMode_ != CurveJacobianMode_::Value_::ANALYTIC)
                    return nullptr;
                if (analyticEligibility_ == AnalyticEligibility_::Value_::ELIGIBLE)
                    return AnalyticJacobian(x, f);
                return nullptr;
            }

            void Gradient(const Vector_<>& parameters, const Vector_<>& residuals, Matrix_<>* jacobian) const override {
                if (!quoteRiskRequested_) {
                    Underdetermined::Function_::Gradient(parameters, residuals, jacobian);
                    return;
                }
                CentralDifferenceJacobian(
                    parameters, static_cast<int>(residuals.size()), 1.0e-6, [&](const Vector_<>& bumped) { return F(bumped); }, jacobian);
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

        std::unique_ptr<Underdetermined::Jacobian_> JointResidualFunction_::AnalyticJacobian(const Vector_<>& x, const Vector_<>& /*f*/) const {
            auto* tape = Dal::AAD::Tape();
            TapeGuard_ guard(tape);

            Vector_<Dal::AAD::Number_> parameters = RegisterCurveParameters(x);
            Dal::AAD::NewRecording(*tape);

            const auto storage = JointCalibrationInternal::BuildTypedCurveBlock<Dal::AAD::Number_>(InternalSpec(*spec_), *slots_, parameters);
            Vector_<Dal::AAD::Number_> residuals = ComputeTemplatedResiduals(storage.block_);
            return std::make_unique<XCurveJacobian_>(HarvestCurveJacobian(*tape, parameters, residuals));
        }

        Vector_<> RunJointSolver(const JointMultiCurveCalibrationSpec_& spec,
                                 const JointResidualFunction_& func,
                                 const Vector_<>& guess,
                                 const Vector_<>& tol,
                                 const Sparse::TriDiagonal_& weights,
                                 Matrix_<>* optEffectiveInverse,
                                 Matrix_<>* optFwdJacAtSolution = nullptr) {
            return RunCurveSolver(func, guess, tol, spec.solveMode_ == CurveSolveMode_::Value_::EXACT, spec.fitTolerance_, weights,
                                  spec.maxEvaluations_, spec.maxRestarts_, optEffectiveInverse, optFwdJacAtSolution);
        }

        Matrix_<> NativeJointJacobian(const Underdetermined::Function_& function, const Vector_<>& parameters, const Vector_<>& residuals) {
            Matrix_<> result;
            if (auto analytic = function.Gradient(parameters, residuals)) {
                result.Resize(analytic->Rows(), analytic->Columns());
                Vector_<> direction(analytic->Columns(), 0.0);
                for (int column = 0; column < analytic->Columns(); ++column) {
                    direction[column] = 1.0;
                    const auto values = analytic->MultiplyLeft(direction);
                    for (int row = 0; row < analytic->Rows(); ++row)
                        result(row, column) = values[row];
                    direction[column] = 0.0;
                }
            } else {
                function.Gradient(parameters, residuals, &result);
            }
            return result;
        }

        Matrix_<> JointInitialDirections(const Underdetermined::Function_& function, const Vector_<>& guess, const Sparse::TriDiagonal_& weights) {
            const auto jacobian = NativeJointJacobian(function, guess, function.F(guess));
            const auto decomposition = weights.DecomposeSymmetric();
            Matrix_<> directions(jacobian.Cols(), jacobian.Rows());
            for (int quote = 0; quote < jacobian.Rows(); ++quote) {
                Vector_<> row(jacobian.Cols());
                for (int parameter = 0; parameter < jacobian.Cols(); ++parameter)
                    row[parameter] = jacobian(quote, parameter);
                Vector_<> direction;
                decomposition->Solve(row, &direction);
                for (int parameter = 0; parameter < jacobian.Cols(); ++parameter)
                    directions(parameter, quote) = direction[parameter];
            }
            return directions;
        }

        class JointInitialChart_ : public Underdetermined::Function_ {
            const JointResidualFunction_& native_;
            const Vector_<>& origin_;
            const Matrix_<>& directions_;

        public:
            JointInitialChart_(const JointResidualFunction_& native, const Vector_<>& origin, const Matrix_<>& directions)
                : native_(native), origin_(origin), directions_(directions) {}

            Vector_<> Parameters(const Vector_<>& coordinates) const {
                Vector_<> result;
                Matrix::Multiply(directions_, coordinates, &result);
                for (int index = 0; index < static_cast<int>(result.size()); ++index)
                    result[index] += origin_[index];
                return result;
            }

            Vector_<> F(const Vector_<>& coordinates) const override { return native_.F(Parameters(coordinates)); }

            std::unique_ptr<Underdetermined::Jacobian_> Gradient(const Vector_<>& coordinates, const Vector_<>& residuals) const override {
                const auto nativeJacobian = NativeJointJacobian(native_, Parameters(coordinates), residuals);
                Matrix_<> chartJacobian;
                Matrix::Multiply(nativeJacobian, directions_, &chartJacobian);
                return std::make_unique<XCurveJacobian_>(std::move(chartJacobian));
            }
        };

        Vector_<> RunJointInitialChartSolver(const JointMultiCurveCalibrationSpec_& spec,
                                             const JointResidualFunction_& function,
                                             const Vector_<>& guess,
                                             const Vector_<>& tolerance,
                                             const Sparse::TriDiagonal_& weights,
                                             Matrix_<>* inverse,
                                             Matrix_<>* forward) {
            const auto directions = JointInitialDirections(function, guess, weights);
            const JointInitialChart_ chart(function, guess, directions);
            Sparse::TriDiagonal_ chartWeights(directions.Cols());
            for (int index = 0; index < directions.Cols(); ++index)
                chartWeights.Add(index, index, 1.0);
            Matrix_<> chartInverse;
            const auto coordinates = RunCurveSolver(chart, Vector_<>(directions.Cols(), 0.0), tolerance, true, spec.fitTolerance_, chartWeights,
                                                    spec.maxEvaluations_, spec.maxRestarts_, &chartInverse);
            const auto solved = chart.Parameters(coordinates);
            Matrix::Multiply(directions, chartInverse, inverse);
            if (forward && function.JacobianModeUsed() == "ANALYTIC")
                *forward = NativeJointJacobian(function, solved, function.F(solved));
            return solved;
        }

        void AppendResultRanges(const JointMultiCurveCalibrationSpec_& spec,
                                const std::vector<CurveSlot_>& slots,
                                JointMultiCurveCalibrationResult_* result) {
            for (const auto& slot : slots) {
                result->parameterRanges_.push_back({slot.curveIndex_, slot.paramOffset_, slot.nParams_});
                result->residualRanges_.push_back({slot.curveIndex_, slot.residualOffset_, slot.nInstruments_});
                const auto& original = spec.curves_[slot.curveIndex_].instruments_;
                Vector_<bool> used(original.size(), false);
                for (const auto& instrument : slot.instruments_) {
                    int ordinal = 0;
                    while (ordinal < static_cast<int>(original.size()) && (used[ordinal] || original[ordinal] != instrument))
                        ++ordinal;
                    REQUIRE(ordinal < static_cast<int>(original.size()), "Joint residual instrument is missing from its declaration");
                    used[ordinal] = true;
                    result->residualInstrumentOrdinals_.push_back(ordinal);
                }
            }
        }

        bool ValidEffectiveMapping(const Underdetermined::Function_& function,
                                   const Vector_<>& solved,
                                   const Vector_<>& residuals,
                                   const Vector_<>& tolerance,
                                   const Matrix_<>& inverse) {
            auto jacobian = function.Gradient(solved, residuals);
            if (!jacobian) {
                Matrix_<> dense;
                function.Gradient(solved, residuals, &dense);
                jacobian = std::make_unique<XCurveJacobian_>(std::move(dense));
            }
            jacobian->DivideRows(tolerance);
            for (int column = 0; column < inverse.Cols(); ++column) {
                Vector_<> direction(inverse.Rows());
                for (int row = 0; row < inverse.Rows(); ++row)
                    direction[row] = inverse(row, column);
                const auto mapped = jacobian->MultiplyLeft(direction);
                for (int row = 0; row < static_cast<int>(mapped.size()); ++row)
                    if (!std::isfinite(mapped[row]) || std::abs(mapped[row] - (row == column ? 1.0 : 0.0)) > 1.0e-7)
                        return false;
            }
            return !inverse.Empty();
        }

        [[noreturn]] void ThrowNonConvergence(int evaluationCount, const Vector_<>& residuals) {
            THROW("Joint multi-curve calibration failed to converge: " + NonConvergenceStats(residuals, evaluationCount));
        }

        // ---- CalibrateJointMultiCurve assembly helpers ----

        Vector_<> BuildInitialGuess(const JointMultiCurveCalibrationSpec_& spec, const std::vector<CurveSlot_>& slots, int totalParams) {
            Vector_<> g(totalParams);
            int off = 0;
            for (const auto& s : slots) {
                const Vector_<> sl = BuildGuessSlice(spec, spec.curves_[s.curveIndex_], s.definition_);
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
        RecordCurveCalibrationInvocation();
        const std::vector<CurveSlot_> slots = ValidateAndBuildSlots(spec);

        int totalParams = 0, totalResiduals = 0;
        for (const auto& slot : slots) {
            totalParams += slot.nParams_;
            totalResiduals += slot.nInstruments_;
        }

        const Vector_<> guess = BuildInitialGuess(spec, slots, totalParams);

        const Vector_<> tol(totalResiduals, spec.tolerance_);
        std::unique_ptr<Sparse::TriDiagonal_> weights = JointCalibrationInternal::BuildJointSmoothing(slots);
        int evaluationCount = 0;
        JointResidualFunction_ func(spec, slots, options.jacobianMode_,
                                    options.computeEffJacobianInverse_ && spec.solveMode_ == CurveSolveMode_::Value_::EXACT, &evaluationCount);
        Matrix_<> fwdJacAtSolution;
        Matrix_<> effectiveInverse;
        const bool initialChart =
            options.computeEffJacobianInverse_ && spec.solveMode_ == CurveSolveMode_::Value_::EXACT && totalParams > totalResiduals;
        const Vector_<> solved =
            initialChart ? RunJointInitialChartSolver(spec, func, guess, tol, *weights, &effectiveInverse,
                                                      options.computeJacobianAtSolution_ ? &fwdJacAtSolution : nullptr)
                         : RunJointSolver(spec, func, guess, tol, *weights, options.computeEffJacobianInverse_ ? &effectiveInverse : nullptr,
                                          options.computeJacobianAtSolution_ ? &fwdJacAtSolution : nullptr);

        const Vector_<> finalResiduals = func.F(solved);
        const bool converged = ResidualsWithinBar(finalResiduals, 10.0 * spec.fitTolerance_);

        auto [discountCurves, forwardCurves] = BuildSolvedCurves(spec, slots, solved);
        const CurveBlock_ solvedBlock("joint", spec.ccy_, discountCurves, forwardCurves, spec.liborBasis_);

        JointMultiCurveCalibrationResult_ result =
            AssembleResult(spec, slots, solvedBlock, discountCurves, forwardCurves, totalResiduals, evaluationCount, std::move(fwdJacAtSolution));
        if (!converged)
            ThrowNonConvergence(evaluationCount, finalResiduals);
        result.converged_ = true;
        result.effJacobianInverse_ = std::move(effectiveInverse);
        result.effJacobianInverseAvailability_ = !options.computeEffJacobianInverse_
                                                     ? "not_requested"
                                                     : (spec.solveMode_ == CurveSolveMode_::Value_::EXACT ? "available" : "not_available_for_mode");
        if (result.effJacobianInverseAvailability_ == "available" &&
            !ValidEffectiveMapping(func, solved, finalResiduals, tol, result.effJacobianInverse_)) {
            result.effJacobianInverse_.Clear();
            result.effJacobianInverseAvailability_ = "not_available_for_mapping";
        }
        result.jacobianModeUsed_ = func.JacobianModeUsed();
        result.effJacobianInverseMapping_ = initialChart ? "initial_jacobian_chart" : "local_weighted";
        result.solverEvaluations_ = evaluationCount;
        AppendResultRanges(spec, slots, &result);
        return result;
    }

} // namespace Dal
