// Generic same-currency joint calibration Excel construction and result views.

#include "__jointcalibration_test_api.hpp"
#include "__settingskeys.hpp"

#include <cmath>
#include <dal/utilities/dictionary.hpp>

// clang-format off
/*IF--------------------------------------------------------------------------
public JointCurveDeclaration_New
    Declare one curve in a generic same-currency joint calibration
&inputs
name is string
    Display name; duplicate names are distinguished by declaration index
instruments is handle[]
    Yield-curve calibration instrument handles
knotDates is date[]
    Native curve knot dates
calibrateDiscountCurve is boolean
    True for a discount slot, false for a forward tenor slot
collateral is string
    Discount collateral slot, including the base slot for layered forwards
tenor is string
    Forward tenor such as 3M; empty for discount declarations
&optional
settings is cell[][]
    &$.Cols() == 2 || $.Empty()\must have two columns (key, value)
    Keys: parameterization, logDfScheme, smoothingWeight, baseLayeredOverDiscount
initialGuessPerNode is number[]
    Optional initial guess in native parameter order
&outputs
declaration is handle StorableJointCurveDeclaration
    Declaration for JOINTMULTICURVECALIBRATIONSPEC.NEW
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JointMultiCurveCalibrationSpec_New
    Build a generic same-currency joint calibration spec in declaration order
&inputs
today is date
    Calibration date
currency is string
    One currency for all declarations
declarations is handle[]
    Ordered JOINTCURVEDECLARATION.NEW handles
&optional
settings is cell[][]
    &$.Cols() == 2 || $.Empty()\must have two columns (key, value)
    Keys: liborBasis, tolerance, fitTolerance, initialGuess, maxEvaluations, maxRestarts, solveMode
&outputs
spec is handle StorableJointMultiCurveCalibrationSpec
    Spec for CALIBRATE.JOINTMULTICURVE
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Calibrate_JointMultiCurve
    Calibrate all declared curves in one joint solve
&inputs
spec is handle StorableJointMultiCurveCalibrationSpec
    Generic joint spec
&optional
settings is cell[][]
    &$.Cols() == 2 || $.Empty()\must have two columns (key, value)
    Keys: jacobianMode (ANALYTIC|BUMPED), computeJacobianAtSolution (true), computeEffJacobianInverse (false). EXACT inverse requests may select a different underdetermined solution; see the fixed initial-Jacobian chart contract.
&outputs
result is handle StorableJointMultiCurveCalibrationResult
    Owning result with the spec and options retained for JOINTMULTICURVEQUOTERISKPROVENANCE.NEW
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JointMultiCurveCalibrationResult_Get_Curve
    Extract one calibrated discount or forward curve by declaration index
&inputs
result is handle StorableJointMultiCurveCalibrationResult
    Generic joint calibration result
curveIndex is integer
    Zero-based declaration index, matching curve:0, curve:1, and so on
&outputs
curve is handle StorableDiscountCurve
    Owning curve handle usable by RATEPRICINGMARKET.NEW
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JointMultiCurveCalibrationResult_Get
    Read a generic joint calibration matrix or diagnostic
&inputs
result is handle StorableJointMultiCurveCalibrationResult
    Generic joint calibration result
attribute is string
    Including jacobian, effJacobianInverse, parameterRanges, residualRanges, residualInstrumentOrdinals, marketRates, modelRates, residuals. See the result-field reference for scalar and inverse metadata selectors.
&outputs
value is cell[][]
    Matrices preserve native axes. Ranges have columns curveIndex, offset, size (all zero-based). Vectors follow the complete residual order.
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    namespace {
        Dictionary_ JointSettings(const Matrix_<Cell_>& cells, const Vector_<String_>& validKeys) {
            REQUIRE(cells.Empty() || cells.Cols() == 2, "Joint settings must have two columns");
            Dictionary_ result;
            for (int row = 0; row < cells.Rows(); ++row) {
                const auto key = Cell::ToString(cells(row, 0));
                RequireKnownSettingsKey(key, validKeys);
                result.Insert(key, cells(row, 1));
            }
            return result;
        }

        double JointNumber(const Cell_& value) {
            const double number = Cell::ToDouble(value);
            REQUIRE(std::isfinite(number), "Joint numeric settings must be finite");
            return number;
        }

        void ApplyDeclarationSettings(const Matrix_<Cell_>& cells, JointCurveDeclaration_* declaration) {
            for (const auto& [key, value] : JointSettings(cells, {"parameterization", "logDfScheme", "smoothingWeight", "baseLayeredOverDiscount"})) {
                if (key == "parameterization")
                    declaration->parameterization_ = CurveParameterization_(Cell::ToString(value));
                else if (key == "logDfScheme")
                    declaration->logDfScheme_ = LogDfScheme_(Cell::ToString(value));
                else if (key == "smoothingWeight")
                    declaration->smoothingWeight_ = JointNumber(value);
                else {
                    REQUIRE(Cell::IsBool(value), "baseLayeredOverDiscount must be boolean");
                    declaration->baseLayeredOverDiscount_ = Cell::ToBool(value);
                }
            }
        }

        void ApplySpecSettings(const Matrix_<Cell_>& cells, JointMultiCurveCalibrationSpec_* spec) {
            const auto settings =
                JointSettings(cells, {"liborBasis", "tolerance", "fitTolerance", "initialGuess", "maxEvaluations", "maxRestarts", "solveMode"});
            for (const auto& [key, value] : settings) {
                if (key == "liborBasis")
                    spec->liborBasis_ = DayBasis_(Cell::ToString(value));
                else if (key == "solveMode")
                    spec->solveMode_ = CurveSolveMode_(Cell::ToString(value));
                else if (key == "maxEvaluations" || key == "maxRestarts") {
                    REQUIRE(Cell::IsInt(value), "Joint evaluation limits must be integers");
                    (key == "maxEvaluations" ? spec->maxEvaluations_ : spec->maxRestarts_) = Cell::ToInt(value);
                } else if (key == "tolerance")
                    spec->tolerance_ = JointNumber(value);
                else if (key == "fitTolerance")
                    spec->fitTolerance_ = JointNumber(value);
                else
                    spec->initialGuess_ = JointNumber(value);
            }
        }

        JointMultiCurveCalibrationOptions_ JointOptions(const Matrix_<Cell_>& cells) {
            JointMultiCurveCalibrationOptions_ options;
            for (const auto& [key, value] : JointSettings(cells, {"jacobianMode", "computeJacobianAtSolution", "computeEffJacobianInverse"})) {
                if (key == "jacobianMode")
                    options.jacobianMode_ = CurveJacobianMode_(Cell::ToString(value));
                else {
                    REQUIRE(Cell::IsBool(value), "Joint inverse and Jacobian requests must be boolean");
                    (key == "computeJacobianAtSolution" ? options.computeJacobianAtSolution_ : options.computeEffJacobianInverse_) =
                        Cell::ToBool(value);
                }
            }
            return options;
        }

        Matrix_<Cell_> JointMatrix(const Matrix_<>& source) {
            Matrix_<Cell_> result(source.Rows(), source.Cols());
            for (int row = 0; row < source.Rows(); ++row)
                for (int col = 0; col < source.Cols(); ++col)
                    result(row, col) = Cell_(source(row, col));
            return result;
        }

        Matrix_<Cell_> JointRanges(const Vector_<JointCurveCalibrationRange_>& ranges) {
            Matrix_<Cell_> result(static_cast<int>(ranges.size()), 3);
            for (int row = 0; row < static_cast<int>(ranges.size()); ++row) {
                result(row, 0) = Cell_(static_cast<double>(ranges[row].curveIndex_));
                result(row, 1) = Cell_(static_cast<double>(ranges[row].offset_));
                result(row, 2) = Cell_(static_cast<double>(ranges[row].size_));
            }
            return result;
        }

        Cell_ JointScalar(const JointMultiCurveCalibrationResult_& result, const String_& attribute) {
            if (attribute == "converged")
                return Cell_(result.converged_);
            if (attribute == "solverEvaluations")
                return Cell_(static_cast<double>(result.solverEvaluations_));
            if (attribute == "jointMaxAbsResidual")
                return Cell_(result.jointMaxAbsResidual_);
            if (attribute == "jointRmsResidual")
                return Cell_(result.jointRmsResidual_);
            if (attribute == "effJacobianInverseScaling")
                return Cell_(result.effJacobianInverseScaling_);
            if (attribute == "effJacobianInverseAvailability")
                return Cell_(result.effJacobianInverseAvailability_);
            if (attribute == "effJacobianInverseMapping")
                return Cell_(result.effJacobianInverseMapping_);
            if (attribute == "jacobianModeUsed")
                return Cell_(result.jacobianModeUsed_);
            THROW("Unknown generic joint result attribute: " + attribute);
        }
    } // namespace

    void JointCurveDeclaration_New(const String_& name,
                                   const Vector_<Handle_<Storable_>>& instruments,
                                   const Vector_<Date_>& knotDates,
                                   bool calibrateDiscountCurve,
                                   const String_& collateral,
                                   const String_& tenor,
                                   const Matrix_<Cell_>& settings,
                                   const Vector_<>& initialGuessPerNode,
                                   Handle_<StorableJointCurveDeclaration_>* declaration) {
        JointCurveDeclaration_ value;
        value.curveName_ = name;
        value.knotDates_ = knotDates;
        value.calibrateDiscountCurve_ = calibrateDiscountCurve;
        value.targetCollateral_ = CollateralType_(collateral);
        value.targetTenor_ = tenor.empty() ? PeriodLength_() : PeriodLength_(tenor);
        value.initialGuessPerNode_ = initialGuessPerNode;
        for (const auto& handle : instruments) {
            const auto instrument = handle_cast<StorableYCInstrument_>(handle);
            REQUIRE(instrument && instrument->val_, "Joint declarations require yield-curve instrument handles");
            value.instruments_.push_back(instrument->val_);
        }
        ApplyDeclarationSettings(settings, &value);
        declaration->reset(new StorableJointCurveDeclaration_(value));
    }

    void JointMultiCurveCalibrationSpec_New(const Date_& today,
                                            const String_& currency,
                                            const Vector_<Handle_<Storable_>>& declarations,
                                            const Matrix_<Cell_>& settings,
                                            Handle_<StorableJointMultiCurveCalibrationSpec_>* spec) {
        JointMultiCurveCalibrationSpec_ value;
        value.today_ = today;
        value.ccy_ = currency;
        for (const auto& handle : declarations) {
            const auto declaration = handle_cast<StorableJointCurveDeclaration_>(handle);
            REQUIRE(declaration, "Joint specs require joint curve declaration handles");
            value.curves_.push_back(declaration->val_);
        }
        ApplySpecSettings(settings, &value);
        spec->reset(new StorableJointMultiCurveCalibrationSpec_(value));
    }

    void Calibrate_JointMultiCurve(const Handle_<StorableJointMultiCurveCalibrationSpec_>& spec,
                                   const Matrix_<Cell_>& settings,
                                   Handle_<StorableJointMultiCurveCalibrationResult_>* result) {
        REQUIRE(spec, "Invalid generic joint spec handle");
        const auto options = JointOptions(settings);
        result->reset(new StorableJointMultiCurveCalibrationResult_(CalibrateJointMultiCurveBundle(spec->val_, options), spec->val_, options));
    }

    void JointMultiCurveCalibrationResult_Get_Curve(const Handle_<StorableJointMultiCurveCalibrationResult_>& result,
                                                    int curveIndex,
                                                    Handle_<StorableDiscountCurve_>* curve) {
        REQUIRE(result, "Invalid generic joint result handle");
        REQUIRE(curveIndex >= 0 && curveIndex < static_cast<int>(result->spec_.curves_.size()), "Joint curve index is out of range");
        const auto& declaration = result->spec_.curves_[curveIndex];
        curve->reset(new StorableDiscountCurve_(declaration.calibrateDiscountCurve_ ? result->val_.discountCurves_.at(declaration.targetCollateral_)
                                                                                    : result->val_.forwardCurves_.at(declaration.targetTenor_)));
    }

    void JointMultiCurveCalibrationResult_Get(const Handle_<StorableJointMultiCurveCalibrationResult_>& result,
                                              const String_& attribute,
                                              Matrix_<Cell_>* value) {
        REQUIRE(result, "Invalid generic joint result handle");
        if (attribute == "jacobian")
            *value = JointMatrix(result->val_.jacobianAtSolution_);
        else if (attribute == "effJacobianInverse")
            *value = JointMatrix(result->val_.effJacobianInverse_);
        else if (attribute == "parameterRanges")
            *value = JointRanges(result->val_.parameterRanges_);
        else if (attribute == "residualRanges")
            *value = JointRanges(result->val_.residualRanges_);
        else if (attribute == "residualInstrumentOrdinals") {
            Vector_<> ordinals(result->val_.residualInstrumentOrdinals_.begin(), result->val_.residualInstrumentOrdinals_.end());
            *value = AsCellColumn(ordinals);
        } else if (attribute == "marketRates" || attribute == "modelRates" || attribute == "residuals") {
            Vector_<> numbers;
            for (const auto& diagnostic : result->val_.diagnostics_) {
                const auto& source = attribute == "marketRates"  ? diagnostic.marketRates_
                                     : attribute == "modelRates" ? diagnostic.modelRates_
                                                                 : diagnostic.residuals_;
                for (double number : source)
                    numbers.push_back(number);
            }
            *value = AsCellColumn(numbers);
        } else {
            value->Resize(1, 1);
            (*value)(0, 0) = JointScalar(result->val_, attribute);
        }
    }

    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_JointCurveDeclaration_New_public.inc>
#include <dal-excel/auto/MG_JointMultiCurveCalibrationSpec_New_public.inc>
#include <dal-excel/auto/MG_Calibrate_JointMultiCurve_public.inc>
#include <dal-excel/auto/MG_JointMultiCurveCalibrationResult_Get_Curve_public.inc>
#include <dal-excel/auto/MG_JointMultiCurveCalibrationResult_Get_public.inc>
#endif
    // clang-format on
} // namespace Dal
