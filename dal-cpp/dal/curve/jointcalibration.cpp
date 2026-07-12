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
#include <set>
#include <utility>
#include <vector>

namespace Dal {

    namespace {
        constexpr const char* KEY_MAX_EVALUATIONS = "MAXEVALUATIONS";
        constexpr const char* KEY_MAX_RESTARTS = "MAXRESTARTS";

        // See docs/methodology/yield_curve_jacobian.md §Joint Multi-Curve Analytic Jacobian.
        String_ ForwardDeclarationOffendingInstrument(const JointCurveDeclaration_& decl) {
            for (const auto& inst : decl.instruments_) {
                const RateIndexConvention_* conv = FloatConventionOf(*inst);
                if (!conv || !conv->useProjectionCurve_)
                    return inst->Name();
            }
            return String_();
        }

        struct CurveSlot_ {
            int curveIndex;
            int paramOffset;
            int nParams;
            int residualOffset;
            int nInstruments;
            Vector_<Handle_<YCInstrument_>> instruments;
            Vector_<Handle_<YCInstrument_::Rate_>> rates;
            Vector_<> marketRates;
            double smoothingWeight;
            CurveDefinition_ definition;
            CurveParameterLayout_ layout;
        };

        template <class T_> Vector_<T_> SliceParameters(const Vector_<T_>& parameters, const CurveSlot_& slot) {
            Vector_<T_> result(slot.nParams);
            for (int i = 0; i < slot.nParams; ++i)
                result[i] = parameters[slot.paramOffset + i];
            return result;
        }

        std::pair<std::set<CollateralType_>, std::set<PeriodLength_>> DedupCollateralsAndTenors(const JointMultiCurveCalibrationSpec_& spec) {
            REQUIRE(!spec.curves_.empty(), "Joint multi-curve calibration requires at least one curve declaration");
            REQUIRE(spec.tolerance_ > 0.0 && spec.fitTolerance_ > 0.0, "Joint multi-curve calibration tolerances must be positive");
            REQUIRE(spec.maxEvaluations_ > 0 && spec.maxRestarts_ > 0, "Joint multi-curve calibration iteration caps must be positive");
            std::set<CollateralType_> collaterals;
            std::set<PeriodLength_> tenors;
            bool hasDiscount = false;
            for (int di = 0; di < static_cast<int>(spec.curves_.size()); ++di) {
                const JointCurveDeclaration_& decl = spec.curves_[di];
                if (decl.calibrateDiscountCurve_) {
                    hasDiscount = true;
                    REQUIRE(collaterals.insert(decl.targetCollateral_).second,
                            String_("Duplicate discount collateral ") + decl.targetCollateral_.String());
                    REQUIRE(!decl.baseLayeredOverDiscount_,
                            String_("Declaration ") + String::FromInt(di) + " base-layered on a discount declaration");
                } else {
                    REQUIRE(tenors.insert(decl.targetTenor_).second, String_("Duplicate forward tenor ") + decl.targetTenor_.String());
                }
            }
            REQUIRE(hasDiscount, "Joint multi-curve calibration requires at least one discount-curve declaration");
            return {std::move(collaterals), std::move(tenors)};
        }

        std::vector<CurveSlot_> ValidateAndBuildSlots(const JointMultiCurveCalibrationSpec_& spec) {
            const auto [producedCollaterals, producedTenors] = DedupCollateralsAndTenors(spec);

            std::vector<CurveSlot_> slots;
            slots.reserve(spec.curves_.size());
            int paramOffset = 0;
            int residualOffset = 0;
            for (int i = 0; i < static_cast<int>(spec.curves_.size()); ++i) {
                const JointCurveDeclaration_& decl = spec.curves_[i];
                REQUIRE(!decl.instruments_.empty(), String_("Declaration ") + String::FromInt(i) + " requires at least one instrument");
                REQUIRE(!decl.knotDates_.empty(), String_("Declaration ") + String::FromInt(i) + " requires at least one knot date");
                for (int k = 1; k < static_cast<int>(decl.knotDates_.size()); ++k)
                    REQUIRE(decl.knotDates_[k] > decl.knotDates_[k - 1],
                            String_("Declaration ") + String::FromInt(i) + " knot dates must be strictly increasing");
                REQUIRE(decl.knotDates_.front() > spec.today_, String_("Declaration ") + String::FromInt(i) + " knot dates must be after today");
                REQUIRE(decl.smoothingWeight_ > 0.0, String_("Declaration ") + String::FromInt(i) + " smoothing weight must be positive");

                if (!decl.calibrateDiscountCurve_) {
                    REQUIRE(decl.targetTenor_ != PeriodLength_(), String_("Forward declaration ") + String::FromInt(i) + " requires a target tenor");
                    REQUIRE(producedCollaterals.count(decl.targetCollateral_) > 0,
                            String_("Declaration ") + String::FromInt(i) + " target collateral " + decl.targetCollateral_.String() +
                                " is not produced by any discount-curve declaration in this spec");
                    const String_ offender = ForwardDeclarationOffendingInstrument(decl);
                    REQUIRE(offender.empty(), String_("Declaration ") + String::FromInt(i) + " forward instrument " + offender +
                                                  " has useProjectionCurve_ = false and leaves the forward curve unconstrained");
                }

                const CurveDefinition_ definition = MakeCurveDefinition(decl.curveName_, spec.ccy_, decl.parameterization_, decl.logDfScheme_,
                                                                        decl.knotDates_, spec.today_, spec.liborBasis_);
                const CurveParameterLayout_ layout = BuildCurveParameterLayout(definition);
                const int nParams = layout.parameterCount_;
                const auto ordered = OrderInstruments(decl.instruments_);
                CurveSlot_ slot;
                slot.curveIndex = i;
                slot.paramOffset = paramOffset;
                slot.nParams = nParams;
                slot.residualOffset = residualOffset;
                slot.nInstruments = static_cast<int>(ordered.size());
                slot.instruments = ordered;
                slot.smoothingWeight = decl.smoothingWeight_;
                slot.definition = definition;
                slot.layout = layout;
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

        std::unique_ptr<Sparse::TriDiagonal_> BuildJointSmoothing(const std::vector<CurveSlot_>& slots) {
            int totalParams = 0;
            for (const auto& slot : slots)
                totalParams += slot.nParams;
            auto weights = std::make_unique<Sparse::TriDiagonal_>(totalParams);
            for (const auto& slot : slots) {
                Vector_<DateTime_> expandedKnots;
                expandedKnots.reserve(slot.layout.parameterCount_);
                const int firstFreeNode = slot.layout.pinnedAnchor_ ? 1 : 0;
                for (int node = firstFreeNode; node < slot.layout.storageNodeCount_; ++node)
                    for (int parameter = 0; parameter < slot.layout.paramsPerDeclaredKnot_; ++parameter)
                        expandedKnots.push_back(DateTime_(slot.definition.nodeDates_[node]));
                REQUIRE(static_cast<int>(expandedKnots.size()) == slot.layout.parameterCount_,
                        "Joint curve smoothing dates must match the curve parameter layout");
                Underdetermined::SelfCouplePWC(weights.get(), expandedKnots, slot.smoothingWeight, slot.paramOffset);
            }
            return weights;
        }

        Vector_<> BuildGuessSlice(const JointMultiCurveCalibrationSpec_& spec, const JointCurveDeclaration_& decl, int nParams) {
            if (!decl.initialGuessPerNode_.empty()) {
                REQUIRE(static_cast<int>(decl.initialGuessPerNode_.size()) == nParams,
                        String_("Joint curve declaration ") + decl.curveName_ + " initialGuessPerNode_ length must equal its parameter count");
                return decl.initialGuessPerNode_;
            }
            return Vector_<>(nParams, spec.initialGuess_);
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
                const JointCurveDeclaration_& decl = spec->curves_[slot.curveIndex];
                if (decl.parameterization_ == CurveParameterization_::Value_::ZERO_RATE) {
                    NOTICE("Joint AAD Jacobian does not support the unimplemented ZERO_RATE parameterization; falling back to bumped");
                    return false;
                }
                const bool onDiscountDecl = decl.calibrateDiscountCurve_;
                for (int i = 0; i < slot.nInstruments; ++i) {
                    if (!InstrumentEligibleForAnalyticJacobian(*slot.instruments[i], onDiscountDecl))
                        return false;
                }
            }
            return true;
        }

        class JointResidualFunction_ : public Underdetermined::Function_ {
            const JointMultiCurveCalibrationSpec_* spec_;
            const std::vector<CurveSlot_>* slots_;
            String_ ccy_;
            DayBasis_ dayCount_;
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
                : spec_(&spec), slots_(&slots), ccy_(spec.ccy_), dayCount_(spec.liborBasis_), jacobianMode_(jacobianMode) {}

            [[nodiscard]] int EvaluationCount() const { return evaluationCount_; }

            CurveBlock_ BuildCurvesFromX(const Vector_<>& x) const {
                std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves;
                std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves;
                for (const auto& slot : *slots_) {
                    const JointCurveDeclaration_& decl = spec_->curves_[slot.curveIndex];
                    if (!decl.calibrateDiscountCurve_)
                        continue;
                    discountCurves[decl.targetCollateral_] =
                        Handle_<DiscountCurve_>(BuildDiscountCurveUniqueT<double>(slot.definition, SliceParameters(x, slot)).release());
                }
                for (const auto& slot : *slots_) {
                    const JointCurveDeclaration_& decl = spec_->curves_[slot.curveIndex];
                    if (decl.calibrateDiscountCurve_)
                        continue;
                    Handle_<DiscountCurve_> base;
                    if (decl.baseLayeredOverDiscount_)
                        base = discountCurves.at(decl.targetCollateral_);
                    forwardCurves[decl.targetTenor_] =
                        Handle_<DiscountCurve_>(BuildDiscountCurveUniqueT<double>(slot.definition, SliceParameters(x, slot), base).release());
                }
                return CurveBlock_("joint", ccy_, discountCurves, forwardCurves, dayCount_);
            }

            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
                ++evaluationCount_;
                const CurveBlock_ yc = BuildCurvesFromX(x);
                int totalResiduals = 0;
                for (const auto& slot : *slots_)
                    totalResiduals += slot.nInstruments;
                Vector_<> residuals(totalResiduals);
                int offset = 0;
                for (const auto& slot : *slots_) {
                    for (int i = 0; i < slot.nInstruments; ++i)
                        residuals[offset + i] = (*slot.rates[i])(yc) - slot.marketRates[i];
                    offset += slot.nInstruments;
                }
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

            [[nodiscard]] std::map<PeriodLength_, std::shared_ptr<Tape::DiscountCurve_<Dal::AAD::Number_>>>
            BuildTemplatedCurves(const Vector_<Dal::AAD::Number_>& parameters,
                                 std::map<CollateralType_, std::shared_ptr<Tape::DiscountCurve_<Dal::AAD::Number_>>>& discountStorage) const {
                std::map<PeriodLength_, std::shared_ptr<Tape::DiscountCurve_<Dal::AAD::Number_>>> forwardStorage;
                for (const auto& slot : *slots_) {
                    const JointCurveDeclaration_& decl = spec_->curves_[slot.curveIndex];
                    if (decl.calibrateDiscountCurve_)
                        discountStorage[decl.targetCollateral_] =
                            BuildDiscountCurveT<Dal::AAD::Number_>(slot.definition, SliceParameters(parameters, slot));
                }
                for (const auto& slot : *slots_) {
                    const JointCurveDeclaration_& decl = spec_->curves_[slot.curveIndex];
                    if (decl.calibrateDiscountCurve_)
                        continue;
                    if (decl.baseLayeredOverDiscount_) {
                        Handle_<Tape::DiscountCurve_<Dal::AAD::Number_>> base(discountStorage.at(decl.targetCollateral_));
                        forwardStorage[decl.targetTenor_] = BuildDiscountCurveT<Dal::AAD::Number_, Tape::DiscountCurve_<Dal::AAD::Number_>>(
                            slot.definition, SliceParameters(parameters, slot), base);
                    } else {
                        forwardStorage[decl.targetTenor_] =
                            BuildDiscountCurveT<Dal::AAD::Number_>(slot.definition, SliceParameters(parameters, slot));
                    }
                }
                return forwardStorage;
            }

            [[nodiscard]] Handle_<Tape::Rate_<Dal::AAD::Number_>> DiscountRateT(const YCInstrument_& inst) const {
                // Caller (ComputeTemplatedResiduals) has already filtered to supported types via
                // InstrumentEligibleForAnalyticJacobian; VisitRate returns an empty handle on miss.
                return VisitRate(
                    inst, [](const Deposit_& d) { return d.PrecomputeT<Dal::AAD::Number_>(); },
                    [](const FRA_& f) { return f.PrecomputeT<Dal::AAD::Number_>(); },
                    [](const Future_& fu) { return fu.PrecomputeT<Dal::AAD::Number_>(); },
                    [](const Swap_& s) { return s.PrecomputeT<Dal::AAD::Number_>(); });
            }

            [[nodiscard]] Vector_<Dal::AAD::Number_> ComputeTemplatedResiduals(const Tape::JointCurveBlock_<Dal::AAD::Number_>& block) const {
                int totalResiduals = 0;
                for (const auto& slot : *slots_)
                    totalResiduals += slot.nInstruments;
                Vector_<Dal::AAD::Number_> residuals(totalResiduals);
                int offset = 0;
                for (int d = 0; d < static_cast<int>(slots_->size()); ++d) {
                    const CurveSlot_& slot = (*slots_)[d];
                    for (int i = 0; i < slot.nInstruments; ++i) {
                        const auto& inst = *slot.instruments[i];
                        const RateIndexConvention_& conv = *FloatConventionOf(inst);
                        if (conv.useProjectionCurve_)
                            residuals[offset + i] =
                                (*Tape::ProjectionRateAt<Dal::AAD::Number_>(inst))(block) - static_cast<double>(slot.marketRates[i]);
                        else
                            residuals[offset + i] = (*DiscountRateT(inst))(Tape::YCCtx_<Dal::AAD::Number_>(block.Discount(conv.collateral_))) -
                                                    static_cast<double>(slot.marketRates[i]);
                    }
                    offset += slot.nInstruments;
                }
                return residuals;
            }
        };

        Underdetermined::Jacobian_* JointResidualFunction_::AnalyticJacobian(const Vector_<>& x, const Vector_<>& /*f*/) const {
            auto* tape = Dal::AAD::Tape();
            TapeGuard_ guard(tape);

            Vector_<Dal::AAD::Number_> parameters = RegisterCurveParameters(x);
            Dal::AAD::NewRecording(*tape);

            std::map<CollateralType_, std::shared_ptr<Tape::DiscountCurve_<Dal::AAD::Number_>>> discountStorage;
            const auto forwardStorage = BuildTemplatedCurves(parameters, discountStorage);

            Tape::JointCurveBlock_<Dal::AAD::Number_> block;
            for (const auto& [collateral, curve] : discountStorage)
                block.discountCurves[collateral] = curve.get();
            for (const auto& [tenor, curve] : forwardStorage)
                block.forwardCurves[tenor] = curve.get();

            Vector_<Dal::AAD::Number_> residuals = ComputeTemplatedResiduals(block);
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
            for (int i = 0; i < slot.nInstruments; ++i) {
                const double modelRate = (*slot.rates[i])(solvedBlock);
                const double marketRate = slot.marketRates[i];
                diag.instrumentNames_.push_back(slot.instruments[i]->Name());
                diag.marketRates_.push_back(marketRate);
                diag.modelRates_.push_back(modelRate);
                diag.residuals_.push_back(modelRate - marketRate);
            }
            const ResidualStats_ stats = ResidualStats(diag.residuals_);
            diag.maxAbsResidual_ = stats.maxAbsResidual_;
            diag.rmsResidual_ = stats.rmsResidual_;
            return diag;
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
                const Vector_<> sl = BuildGuessSlice(spec, spec.curves_[s.curveIndex], s.nParams);
                for (int j = 0; j < s.nParams; ++j)
                    g[off + j] = sl[j];
                off += s.nParams;
            }
            return g;
        }

        std::pair<std::map<CollateralType_, Handle_<DiscountCurve_>>, std::map<PeriodLength_, Handle_<DiscountCurve_>>>
        BuildSolvedCurves(const JointMultiCurveCalibrationSpec_& spec, const std::vector<CurveSlot_>& slots, const Vector_<>& solved) {
            std::map<CollateralType_, Handle_<DiscountCurve_>> dc;
            std::map<PeriodLength_, Handle_<DiscountCurve_>> fc;
            for (const auto& s : slots) {
                const auto& d = spec.curves_[s.curveIndex];
                if (!d.calibrateDiscountCurve_)
                    continue;
                dc[d.targetCollateral_] =
                    Handle_<DiscountCurve_>(BuildDiscountCurveUniqueT<double>(s.definition, SliceParameters(solved, s)).release());
            }
            for (const auto& s : slots) {
                const auto& d = spec.curves_[s.curveIndex];
                if (d.calibrateDiscountCurve_)
                    continue;
                Handle_<DiscountCurve_> b;
                if (d.baseLayeredOverDiscount_)
                    b = dc.at(d.targetCollateral_);
                fc[d.targetTenor_] =
                    Handle_<DiscountCurve_>(BuildDiscountCurveUniqueT<double>(s.definition, SliceParameters(solved, s), b).release());
            }
            return {std::move(dc), std::move(fc)};
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
                const JointCurveCalibrationDiagnostics_ diag = BuildCurveDiagnostics(spec.curves_[s.curveIndex], s, solvedBlock, usedApprox);
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
            totalParams += slot.nParams;
            totalResiduals += slot.nInstruments;
        }

        const Vector_<> guess = BuildInitialGuess(spec, slots, totalParams);

        const Vector_<> tol(totalResiduals, spec.tolerance_);
        std::unique_ptr<Sparse::TriDiagonal_> weights = BuildJointSmoothing(slots);
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
