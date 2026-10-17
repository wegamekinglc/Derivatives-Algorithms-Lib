//
// Created by Codex on 2026/10/11.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include <dal-cpp/tests/curve/jointquoteriskfixtures.hpp>
#include <dal-excel/src/__ratecurvature.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curvespec.hpp>
#include <dal-public/tests/ratexccycurvaturefixtures.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/platform/initall.hpp>
#include <dal/storage/json.hpp>
#include <dal/time/holidays.hpp>

using namespace Dal;

namespace {
    CurveCalibrationSpec_ SingleSpec(int count = 1) {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2025, 1, 2);
        spec.ccy_ = "USD";
        spec.curveName_ = "rate-curvature";
        spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.tolerance_ = 1.0e-14;
        spec.initialGuess_ = 0.025;
        spec.knotDates_.push_back(spec.today_);
        RateIndexConvention_ index;
        index.dayBasis_ = DayBasis::Act365F();
        index.businessDayConvention_ = BizDayConvention_("Unadjusted");
        index.accrualHolidays_ = Holidays::None();
        for (int year = 1; year <= count; ++year) {
            const auto maturity = Date::AddMonths(spec.today_, 12 * year);
            spec.knotDates_.push_back(maturity);
            spec.instruments_.push_back(Handle_<YCInstrument_>(new Deposit_(spec.today_, spec.today_, maturity, 0.025, index)));
        }
        return spec;
    }

    Handle_<Storable_> Source(const CurveCalibrationSpec_& spec) {
        return Handle_<Storable_>(new StorableCurveCalibrationResult_(CalibrateSingleCurve(spec), spec, {}));
    }

    Handle_<StorableRateCalibrationSnapshot_> Snapshot() {
        Handle_<StorableRateCalibrationSnapshot_> snapshot;
        RateCalibration_New("snapshot", Source(SingleSpec()), &snapshot);
        return snapshot;
    }

    Handle_<Storable_> Trade() {
        const auto spec = SingleSpec();
        RateTradeDefinition_ trade;
        trade.instrumentId_ = "deposit";
        trade.instrumentType_ = RateInstrumentType_::Value_::DEPOSIT;
        trade.tradeDate_ = spec.today_;
        trade.startDate_ = spec.today_;
        trade.maturityDate_ = spec.knotDates_.back();
        trade.currencyOrPair_ = Ccy_("USD");
        DepositTradeTerms_ terms;
        terms.notional_ = 1.0;
        terms.contractRate_ = 0.028;
        terms.discountComponentKey_ = spec.curveName_;
        terms.index_ = static_cast<const Deposit_&>(*spec.instruments_[0]).FloatConvention();
        trade.terms_ = terms;
        return Handle_<Storable_>(new StorableRateTradeDefinition_(trade));
    }

    Handle_<StorableBumpOverAADRequest_> Bumps(int rows = 3) {
        AAD::BumpOverAADRequest_ request;
        request.directions_ = Matrix_<>(rows, 1, 0.5);
        if (rows > 0)
            request.directions_(0, 0) = 1.0;
        if (rows > 1)
            request.directions_(1, 0) = -2.0;
        request.steps_ = Vector_<>(rows, 0.0001);
        return Handle_<StorableBumpOverAADRequest_>(new StorableBumpOverAADRequest_("bumps", request));
    }

    Handle_<StorableRateTradeQuoteCurvatureResult_> Result(const Handle_<StorableBumpOverAADRequest_>& bumps = Bumps()) {
        Handle_<StorableRateTradeQuoteCurvatureResult_> result;
        RateTradeQuoteCurvatureResult_New("result", {Trade()}, Snapshot(), bumps, {}, &result);
        return result;
    }

    template <class F_> void CheckError(F_&& function, std::initializer_list<const char*> fields) {
        try {
            function();
            FAIL() << "Expected input rejection";
        } catch (const Exception_& error) {
            for (const auto* field : fields)
                ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }
} // namespace

TEST(ExcelRateCurvatureTest, TestSnapshotOwnsFinalSpecificationAndDetachedPoint) {
    const auto spec = SingleSpec();
    auto original = std::make_shared<StorableCurveCalibrationResult_>(CalibrateSingleCurve(spec), spec, CurveCalibrationOptions_());
    Handle_<StorableRateCalibrationSnapshot_> snapshot;
    RateCalibration_New("snapshot", Handle_<Storable_>(original), &snapshot);
    ASSERT_EQ(snapshot->val_.Point().size(), 1U);
    ASSERT_DOUBLE_EQ(snapshot->val_.Point()[0], 0.025);
    original->spec_.instruments_.clear();
    Matrix_<Cell_> point;
    RateCalibration_Get_Point(snapshot, &point);
    ASSERT_EQ(point.Rows(), 1);
    ASSERT_EQ(point.Cols(), 1);
    point(0, 0) = 0.5;
    ASSERT_DOUBLE_EQ(snapshot->val_.Point()[0], 0.025);
    Handle_<StorableRateCalibrationSnapshot_> rebuilt;
    RateCalibration_Recalibrate("rebuilt", snapshot, Matrix_<Cell_>(1, 1, Cell_(0.03)), &rebuilt);
    ASSERT_DOUBLE_EQ(rebuilt->val_.Point()[0], 0.03);
    ASSERT_NEAR(rebuilt->val_.Parameters()[0], -std::log(1.03), 1.0e-10);
    ASSERT_EQ(rebuilt->val_.Provenance().Axis().fingerprint_, snapshot->val_.Provenance().Axis().fingerprint_);
}

TEST(ExcelRateCurvatureTest, TestIndependentDepositValueGradientSignedGammaAndCounters) {
    const auto result = Result();
    const auto& risk = result->val_.Curvature();
    const double quote = 0.025, payment = 1.028;
    ASSERT_NEAR(risk.Value(), payment / (1.0 + quote) - 1.0, 1.0e-10);
    ASSERT_NEAR(risk.Gradient()[0], -payment / std::pow(1.0 + quote, 2), 1.0e-10);
    for (int row = 0; row < 3; ++row) {
        const double shift = risk.Steps()[row] * risk.Directions()(row, 0);
        const double expected =
            (-payment / std::pow(1.0 + quote + shift, 2) + payment / std::pow(1.0 + quote - shift, 2)) / (2.0 * risk.Steps()[row]);
        ASSERT_NEAR(risk.HessianProducts()(row, 0), expected, 1.0e-9);
        ASSERT_NEAR(risk.HessianProducts()(row, 0), risk.Directions()(row, 0) * 2.0 * payment / std::pow(1.0 + quote, 3), 4.0e-7);
    }
    ASSERT_EQ(result->val_.Currency(), Ccy_("USD"));
    ASSERT_EQ(risk.Execution().method_, "BumpOverRecalibratedNativeRateAAD");
    ASSERT_EQ(risk.Execution().calibrations_, 7U);
    ASSERT_EQ(risk.Execution().quoteGradientEvaluations_, 7U);
    ASSERT_EQ(risk.Execution().objectiveReverseSweeps_, 7U);
    ASSERT_EQ(risk.Execution().numericPayloadBytes_, AAD::BumpOverAADPayloadBytes(1, 3));
}

TEST(ExcelRateCurvatureTest, TestCopiedSignedWeightsDuplicateTradesAndFixings) {
    const DateTime_ time(Date_(2024, 1, 2), 9, 0);
    const auto history = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"saved", {{time, 0.03}}}}));
    const Handle_<StorableMarketFixingSnapshot_> wrapper(new StorableMarketFixingSnapshot_(history));
    Matrix_<Cell_> weights(1, 2);
    weights(0, 0) = 2.0;
    weights(0, 1) = -0.5;
    Handle_<StorableRateTradeQuoteCurvatureSettings_> settings;
    RateTradeQuoteCurvatureSettings_New("weights", weights, wrapper, &settings);
    weights(0, 0) = 99.0;
    ASSERT_DOUBLE_EQ(settings->val_.weights_[0], 2.0);
    ASSERT_NE(settings->val_.fixings_.get(), history.get());
    Matrix_<Cell_> copied;
    RateTradeQuoteCurvatureSettings_Get_Fixings(settings, &copied);
    ASSERT_EQ(copied.Rows(), 2);
    ASSERT_TRUE(Cell::ToBool(copied(0, 1)));
    ASSERT_EQ(Cell::ToString(copied(1, 0)), "saved");
    ASSERT_DOUBLE_EQ(Cell::ToDouble(copied(1, 2)), 0.03);
    copied(1, 2) = 99.0;
    ASSERT_DOUBLE_EQ(settings->val_.fixings_->Require("saved", time, "test"), 0.03);
    Matrix_<Cell_> spill;
    RateTradeQuoteCurvatureSettings_Get_Weights(settings, &spill);
    spill(0, 0) = 17.0;
    ASSERT_DOUBLE_EQ(settings->val_.weights_[0], 2.0);
    Handle_<StorableRateTradeQuoteCurvatureResult_> result;
    RateTradeQuoteCurvatureResult_New("result", {Trade(), Trade()}, Snapshot(), Bumps(), settings, &result);
    const auto unit = Result();
    ASSERT_NEAR(result->val_.Curvature().Value(), 1.5 * unit->val_.Curvature().Value(), 1.0e-10);
    ASSERT_NEAR(result->val_.Curvature().Gradient()[0], 1.5 * unit->val_.Curvature().Gradient()[0], 1.0e-10);
    ASSERT_NEAR(result->val_.Curvature().HessianProducts()(1, 0), 1.5 * unit->val_.Curvature().HessianProducts()(1, 0), 1.0e-8);
}

TEST(ExcelRateCurvatureTest, TestDetachedResultGettersBaseAndCompleteQuotePlan) {
    const auto result = Result();
    Matrix_<Cell_> cells;
    for (const auto getter :
         {RateTradeQuoteCurvatureResult_Get_Point, RateTradeQuoteCurvatureResult_Get_Gradient, RateTradeQuoteCurvatureResult_Get_Directions,
          RateTradeQuoteCurvatureResult_Get_Steps, RateTradeQuoteCurvatureResult_Get_HessianProducts}) {
        getter(result, &cells);
        ASSERT_GT(cells.Rows(), 0);
        cells(0, 0) = 88.0;
    }
    ASSERT_DOUBLE_EQ(result->val_.Curvature().Point()[0], 0.025);
    ASSERT_DOUBLE_EQ(result->val_.Curvature().Directions()(0, 0), 1.0);
    RateTradeQuoteCurvatureResult_Get_Shape(result, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 3.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 1)), 1.0);
    RateTradeQuoteCurvatureResult_Get_Execution(result, &cells);
    ASSERT_EQ(cells.Rows(), 7);
    ASSERT_EQ(Cell::ToString(cells(0, 1)), "BumpOverRecalibratedNativeRateAAD");
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(1, 1)), 7.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(4, 1)), double(AAD::BumpOverAADPayloadBytes(1, 3)));
    ASSERT_GE(Cell::ToDouble(cells(5, 1)), Cell::ToDouble(cells(6, 1)));
    double value = 0.0;
    String_ currency;
    RateTradeQuoteCurvatureResult_Get_Value(result, &value);
    RateTradeQuoteCurvatureResult_Get_Currency(result, &currency);
    ASSERT_DOUBLE_EQ(value, result->val_.Curvature().Value());
    ASSERT_EQ(currency, "USD");
    Handle_<StorableRateCalibrationSnapshot_> base;
    RateTradeQuoteCurvatureResult_Get_BaseCalibration(result, &base);
    Handle_<StorableCalibrationRiskPlan_> plan;
    RateCalibration_Get_QuotePlan(base, &plan);
    ASSERT_EQ(plan->val_.CompleteInputAxis().size(), 1U);
    ASSERT_FALSE(plan->val_.CompleteInputAxis()[0].value_.has_value());
    ASSERT_EQ(plan->val_.CompleteInputAxis()[0].label_, base->val_.Provenance().Axis().quotes_[0].displayName_);
    RateCalibration_Get_Parameters(base, &cells);
    ASSERT_NEAR(Cell::ToDouble(cells(0, 0)), -std::log(1.025), 1.0e-10);
}

TEST(ExcelRateCurvatureTest, TestZeroDirectionsAndBlankOptionalSettingsRetainLogicalShape) {
    const auto result = Result(Bumps(0));
    ASSERT_EQ(result->val_.Curvature().HessianProducts().Rows(), 0);
    ASSERT_EQ(result->val_.Curvature().HessianProducts().Cols(), 1);
    ASSERT_EQ(result->val_.Curvature().Execution().calibrations_, 1U);
    Matrix_<Cell_> cells;
    RateTradeQuoteCurvatureResult_Get_HessianProducts(result, &cells);
    ASSERT_EQ(cells.Rows(), 1);
    ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
    RateTradeQuoteCurvatureResult_Get_Shape(result, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 0.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 1)), 1.0);
    Handle_<StorableRateTradeQuoteCurvatureSettings_> settings;
    RateTradeQuoteCurvatureSettings_New("unit", Matrix_<Cell_>(1, 1), {}, &settings);
    ASSERT_TRUE(settings->val_.weights_.empty());
    Matrix_<Cell_> history;
    RateTradeQuoteCurvatureSettings_Get_Fixings(settings, &history);
    ASSERT_EQ(history.Rows(), 1);
    ASSERT_FALSE(Cell::ToBool(history(0, 1)));
}

TEST(ExcelRateCurvatureTest, TestSeparateNumericAndRecordingCapsPreservePriorResultAndRecover) {
    const auto snapshot = Snapshot();
    auto result = Result();
    const auto previous = result;
    auto request = Bumps()->val_;
    request.numericPayloadBudgetBytes_ = AAD::BumpOverAADPayloadBytes(1, 3) - 1;
    Handle_<StorableBumpOverAADRequest_> limited(new StorableBumpOverAADRequest_("limit", request));
    ASSERT_THROW(RateTradeQuoteCurvatureResult_New("failure", {Trade()}, snapshot, limited, {}, &result), Exception_);
    ASSERT_EQ(result, previous);
    request.numericPayloadBudgetBytes_ = AAD::BumpOverAADPayloadBytes(1, 3);
    request.recordingCapacityBudgetBytes_ = 0;
    limited.reset(new StorableBumpOverAADRequest_("limit", request));
    ASSERT_THROW(RateTradeQuoteCurvatureResult_New("failure", {Trade()}, snapshot, limited, {}, &result), Exception_);
    ASSERT_EQ(result, previous);
    request.recordingCapacityBudgetBytes_ = 64U * 1024U * 1024U;
    limited.reset(new StorableBumpOverAADRequest_("limit", request));
    RateTradeQuoteCurvatureResult_New("recovery", {Trade()}, snapshot, limited, {}, &result);
    ASSERT_NE(result, previous);
    ASSERT_NEAR(result->val_.Curvature().HessianProducts()(0, 0), previous->val_.Curvature().HessianProducts()(0, 0), 1.0e-10);
}

TEST(ExcelRateCurvatureTest, TestMalformedVectorsNullFixingsAndRowsRejectWithContext) {
    Handle_<StorableRateTradeQuoteCurvatureSettings_> settings;
    for (const auto& cell : {Cell_(true), Cell_("2"), Cell_(Date_(2025, 1, 2)), Cell_(), Cell_(std::numeric_limits<double>::infinity())}) {
        Matrix_<Cell_> values(1, 2, Cell_(1.0));
        values(0, 1) = cell;
        CheckError([&] { RateTradeQuoteCurvatureSettings_New("bad", values, {}, &settings); }, {"weights", "row=1", "column=2"});
    }
    ASSERT_THROW(RateTradeQuoteCurvatureSettings_New("bad", Matrix_<Cell_>(2, 2, Cell_(1.0)), {}, &settings), Exception_);
    const Handle_<StorableMarketFixingSnapshot_> nullHistory(new StorableMarketFixingSnapshot_({}));
    ASSERT_THROW(RateTradeQuoteCurvatureSettings_New("bad", {}, nullHistory, &settings), Exception_);
    const auto snapshot = Snapshot();
    Handle_<StorableRateTradeQuoteCurvatureResult_> result;
    CheckError([&] { RateTradeQuoteCurvatureResult_New("bad", {Trade(), Source(SingleSpec())}, snapshot, Bumps(), {}, &result); },
               {"trades", "row=2"});
    CheckError([&] { RateTradeQuoteCurvatureResult_New("bad", {Trade(), {}}, snapshot, Bumps(), {}, &result); }, {"trades", "row=2"});
    ASSERT_THROW(RateTradeQuoteCurvatureResult_New("bad", {}, snapshot, Bumps(), {}, &result), Exception_);
    auto copied = snapshot;
    ASSERT_THROW(RateCalibration_Recalibrate("bad", snapshot, Matrix_<Cell_>(1, 1, Cell_(true)), &copied), Exception_);
    ASSERT_EQ(copied, snapshot);
}

TEST(ExcelRateCurvatureTest, TestAllFourRetainedCalibrationFactoriesAndFullAxes) {
    const auto jointSpec = JointQuoteRiskFixtures::Spec(4);
    const auto stagedSpec = RateXccyCurvatureFixtures::StagedSpec();
    const auto xccySpec = RateXccyCurvatureFixtures::JointSpec();
    const Vector_<Handle_<Storable_>> sources{
        Source(SingleSpec()), Handle_<Storable_>(new StorableJointMultiCurveCalibrationResult_(CalibrateJointMultiCurve(jointSpec), jointSpec, {})),
        Handle_<Storable_>(new StorableCrossCurrencyCalibrationResult_(CalibrateCrossCurrencyMarket(stagedSpec), stagedSpec, {}, {})),
        Handle_<Storable_>(new StorableJointXccyCalibrationResult_(CalibrateJointXccyMarket(xccySpec), xccySpec, {}))};
    for (const auto& source : sources) {
        Handle_<StorableRateCalibrationSnapshot_> snapshot;
        RateCalibration_New("family", source, &snapshot);
        Handle_<StorableCalibrationRiskPlan_> plan;
        RateCalibration_Get_QuotePlan(snapshot, &plan);
        ASSERT_EQ(plan->val_.CompleteInputAxis().size(), snapshot->val_.Point().size());
        ASSERT_EQ(snapshot->val_.Parameters().size(), snapshot->val_.Point().size());
        for (size_t column = 0; column < snapshot->val_.Point().size(); ++column) {
            ASSERT_FALSE(plan->val_.CompleteInputAxis()[column].value_.has_value());
            ASSERT_EQ(plan->val_.CompleteInputAxis()[column].label_, snapshot->val_.Provenance().Axis().quotes_[column].displayName_);
            ASSERT_EQ(*plan->val_.CompleteInputAxis()[column].blockKey_, snapshot->val_.Provenance().Axis().quotes_[column].blockKey_);
            ASSERT_EQ(plan->val_.CompleteInputAxis()[column].ordinal_, column);
        }
    }
}

TEST(ExcelRateCurvatureTest, TestNullUnsupportedSourcesNamesAndArchivesPreserveOutputs) {
    RegisterAll_::Init();
    const auto snapshot = Snapshot();
    auto copied = snapshot;
    const Handle_<Storable_> spec(new StorableCurveCalibrationSpec_(SingleSpec()));
    ASSERT_THROW(RateCalibration_New("bad", {}, &copied), Exception_);
    ASSERT_THROW(RateCalibration_New("bad", spec, &copied), Exception_);
    ASSERT_THROW(RateCalibration_New(String_(std::string("bad\0name", 8)), Source(SingleSpec()), &copied), Exception_);
    ASSERT_EQ(copied, snapshot);
    Matrix_<Cell_> cells(1, 1, Cell_(9.0));
    ASSERT_THROW(RateCalibration_Get_Point({}, &cells), Exception_);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 9.0);
    ASSERT_THROW(JSON::WriteString(*snapshot), Exception_);
    const auto result = Result();
    ASSERT_THROW(JSON::WriteString(*result), Exception_);
    Handle_<StorableRateTradeQuoteCurvatureSettings_> settings;
    RateTradeQuoteCurvatureSettings_New("unit", {}, {}, &settings);
    ASSERT_THROW(JSON::WriteString(*settings), Exception_);
}

TEST(ExcelRateCurvatureTest, TestZeroWeightInvalidTradeAndWeightCountRemainAdmitted) {
    const auto snapshot = Snapshot();
    const auto original = Result();
    auto result = original;
    Handle_<StorableRateTradeQuoteCurvatureSettings_> settings;
    RateTradeQuoteCurvatureSettings_New("zero", Matrix_<Cell_>(1, 1, Cell_(0.0)), {}, &settings);
    auto invalid = static_cast<const StorableRateTradeDefinition_&>(*Trade()).val_;
    std::get<DepositTradeTerms_>(invalid.terms_).notional_ = -1.0;
    const Handle_<Storable_> malformed(new StorableRateTradeDefinition_(invalid));
    ASSERT_THROW(RateTradeQuoteCurvatureResult_New("bad", {malformed}, snapshot, Bumps(), settings, &result), Exception_);
    ASSERT_EQ(result, original);
    RateTradeQuoteCurvatureSettings_New("mismatch", Matrix_<Cell_>(1, 2, Cell_(1.0)), {}, &settings);
    ASSERT_THROW(RateTradeQuoteCurvatureResult_New("bad", {Trade()}, snapshot, Bumps(), settings, &result), Exception_);
    ASSERT_EQ(result, original);
}

TEST(ExcelRateCurvatureTest, TestBumpedSolveFailurePreservesResultAndAllowsRecovery) {
    const auto snapshot = Snapshot();
    auto result = Result();
    const auto original = result;
    auto request = Bumps(1)->val_;
    request.steps_[0] = 2.0;
    const Handle_<StorableBumpOverAADRequest_> failure(new StorableBumpOverAADRequest_("failure", request));
    CheckError([&] { RateTradeQuoteCurvatureResult_New("bad", {Trade()}, snapshot, failure, {}, &result); }, {"direction=0", "stage=calibration"});
    ASSERT_EQ(result, original);
    RateTradeQuoteCurvatureResult_New("recovered", {Trade()}, snapshot, Bumps(), {}, &result);
    ASSERT_NEAR(result->val_.Curvature().Value(), original->val_.Curvature().Value(), 1.0e-10);
}

TEST(ExcelRateCurvatureTest, TestPassiveQueriesAndRejectedNestedWorkPreserveCallerGraphAndSeed) {
    const auto source = Source(SingleSpec());
    auto snapshot = Snapshot();
    const auto originalSnapshot = snapshot;
    auto result = Result();
    const auto originalResult = result;
    const auto request = Bumps();
#ifdef _WIN32
    Excel::ScriptTestInitialize(1);
    double adjoint = 0.0;
    Excel::ScriptTestWithRecording(
        [&](size_t nodes) {
#else
    AAD::Clear(*AAD::Tape());
    const auto mode = AAD::SetNumResultsForAAD(true, 3);
    AAD::RecordingScope_ recording;
    AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    AAD::Number_ square = input * input;
    AAD::NativeOperations_::SetSeed(square, 7.0, 2);
    const auto nodes = AAD::Tape()->nodes_.Size();
#endif
            Matrix_<Cell_> cells;
            RateCalibration_Get_Point(snapshot, &cells);
            RateCalibration_Get_Parameters(snapshot, &cells);
            Handle_<StorableCalibrationRiskPlan_> plan;
            RateCalibration_Get_QuotePlan(snapshot, &plan);
            for (const auto getter :
                 {RateTradeQuoteCurvatureResult_Get_Point, RateTradeQuoteCurvatureResult_Get_Gradient, RateTradeQuoteCurvatureResult_Get_Directions,
                  RateTradeQuoteCurvatureResult_Get_Steps, RateTradeQuoteCurvatureResult_Get_HessianProducts, RateTradeQuoteCurvatureResult_Get_Shape,
                  RateTradeQuoteCurvatureResult_Get_Execution})
                getter(result, &cells);
            ASSERT_THROW(RateCalibration_New("nested", source, &snapshot), Exception_);
            ASSERT_THROW(RateCalibration_Recalibrate("nested", snapshot, Matrix_<Cell_>(1, 1, Cell_(0.03)), &snapshot), Exception_);
            ASSERT_THROW(RateTradeQuoteCurvatureResult_New("nested", {Trade()}, snapshot, request, {}, &result), Exception_);
            ASSERT_EQ(snapshot, originalSnapshot);
            ASSERT_EQ(result, originalResult);
#ifdef _WIN32
            ASSERT_EQ(Excel::ScriptTestTapeNodeCount(), nodes);
        },
        false, &adjoint);
    ASSERT_DOUBLE_EQ(adjoint, 4.0);
#else
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), nodes);
    ASSERT_EQ(AAD::Tape()->numAdj_, 3);
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(square, 2), 7.0);
    recording.FinishRecording();
    recording.Reverse();
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input, 2), 28.0);
    recording.Close();
#endif
}
