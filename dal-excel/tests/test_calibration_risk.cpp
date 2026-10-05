//
// Created by Codex on 2026/10/5.
//

#include <gtest/gtest.h>

#include <limits>
#include <string>

#include <dal-excel/src/__calibrationrisk.hpp>
#include <dal-excel/src/__curvepricing_test_api.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/storage/json.hpp>

#include <script_test_observers.hpp>
#include <tests/curve/jointxccyquoteriskfixtures.hpp>

#include <dal-excel/src/__calibrationinput.hpp>

namespace {
    using namespace Dal;

    class FlatIVS_ final : public AAD::IVS_ {
        double vol_;

    public:
        explicit FlatIVS_(double vol = 0.2) : IVS_(100.0, 0.05, 0.02), vol_(vol) {}
        double ImpliedVol(double, double) const override { return vol_; }
    };

    DupireCalibrationSnapshot_ FrozenCalibration(double vol = 0.2) {
        const FlatIVS_ base(vol);
        return CalibrateDupireWithRisk(base, {{75.0, 105.0, 135.0}, {0.4, 1.2}, Matrix_<>(3, 2, 0.0), {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5});
    }

    void AssertMatrix(const Matrix_<>& actual, const Matrix_<>& expected) {
        ASSERT_EQ(actual.Rows(), expected.Rows());
        ASSERT_EQ(actual.Cols(), expected.Cols());
        for (int row = 0; row < actual.Rows(); ++row)
            for (int col = 0; col < actual.Cols(); ++col)
                ASSERT_EQ(actual(row, col), expected(row, col));
    }

    template <class F_> void AssertError(F_ action, const char* field) {
        try {
            action();
            FAIL() << "Expected error for " << field;
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }

    Handle_<StorableCalibrationPullback_> DupireBoundary(double vol = 0.2) {
        Handle_<StorableCalibrationPullback_> result;
        CalibrationPullback_New("dupire", Handle_<Storable_>(new StorableDupireCalibration_("source", FrozenCalibration(vol))), &result);
        return result;
    }

    Handle_<StorableRateQuoteRiskProvenance_> CapturedCurve(const JointMultiCurveCalibrationSpec_& spec,
                                                            const JointMultiCurveCalibrationResult_& calibrated,
                                                            const JointMultiCurveCalibrationOptions_& options,
                                                            const RatePricingMarket_& market,
                                                            const String_& id = "excel-common") {
        const Handle_<StorableJointMultiCurveCalibrationResult_> source(new StorableJointMultiCurveCalibrationResult_(calibrated, spec, options));
        const Handle_<StorableRatePricingMarket_> input(new StorableRatePricingMarket_(market));
        Vector_<String_> blocks, components;
        for (int block = 0; block < static_cast<int>(spec.curves_.size()); ++block) {
            blocks.push_back("curve:" + String::FromInt(block));
            components.push_back(JointQuoteRiskFixtures::BlockKey(block));
        }
        Handle_<StorableRateQuoteRiskProvenance_> result;
        JointMultiCurveQuoteRiskProvenance_New(source, id, blocks, components, input, Cell_(true), &result);
        return result;
    }

    Handle_<StorableCalibrationPullback_> CurveBoundary(const String_& id = "excel-common") {
        const auto spec = JointQuoteRiskFixtures::Spec();
        const auto options = JointXccyQuoteRiskFixtures::Options();
        const auto calibrated = CalibrateJointMultiCurve(spec, options);
        Handle_<StorableCalibrationPullback_> result;
        CalibrationPullback_New(
            "curve", Handle_<Storable_>(CapturedCurve(spec, calibrated, options, JointQuoteRiskFixtures::Market(spec, calibrated), id)), &result);
        return result;
    }

    Matrix_<>
    PricedJointGradient(const Vector_<RateTradeDefinition_>& trades, const RatePricingMarket_& market, const RateQuoteRiskProvenance_& provenance) {
        Vector_<String_> keys;
        const auto& ranges = provenance.Axis().parameterRanges_;
        for (const auto& range : ranges)
            keys.push_back(provenance.ComponentKeyByParameterBlock().at(range.blockKey_));
        const auto cells = RateCashflowPricingInternal::JointNodeSensitivitiesBatch(trades, market, keys);
        REQUIRE(cells.size() == trades.size() * ranges.size(), "Incomplete joint gradients");
        Matrix_<> gradient(static_cast<int>(provenance.Axis().parameters_.size()), 1, 0.0);
        for (size_t index = 0; index < cells.size(); ++index) {
            const auto block = index % ranges.size();
            const auto& range = ranges[block];
            const auto& cell = cells[index];
            REQUIRE(cell.componentKey_ == keys[block], "Joint gradient component order differs");
            if (!cell.result_.eligible_) {
                REQUIRE(cell.result_.reason_ == "TRADE_DOES_NOT_DEPEND_ON_COMPONENT", "Joint gradient failed: " + cell.result_.reason_);
                continue;
            }
            REQUIRE(cell.result_.gradient_.size() == static_cast<size_t>(range.size_), "Joint gradient width differs");
            for (int ordinal = 0; ordinal < range.size_; ++ordinal)
                gradient(range.offset_ + ordinal, 0) += cell.result_.gradient_[ordinal];
        }
        return gradient;
    }

    void AssertPricedPortfolio(const Vector_<RateTradeDefinition_>& trades,
                               const RatePricingMarket_& market,
                               const Handle_<StorableRateQuoteRiskProvenance_>& provenance) {
        Handle_<StorableCalibrationPullback_> boundary;
        CalibrationPullback_New("portfolio", Handle_<Storable_>(provenance), &boundary);
        Handle_<StorableCalibrationParameterAdjoints_> parameters;
        CalibrationParameterAdjoints_New("priced", boundary, PricedJointGradient(trades, market, *provenance->val_), &parameters);
        Handle_<StorableCalibrationQuoteRisk_> result;
        CalibrationQuoteRisk_New("quotes", boundary, parameters, {}, &result);
        const auto legacy = AggregateRatePortfolioQuoteRisk(trades, market, {*provenance->val_});
        ASSERT_TRUE(legacy.provenanceFailures_.empty());
        ASSERT_EQ(legacy.buckets_.size(), provenance->val_->Axis().quotes_.size());
        const auto& quotes = result->val_.TotalAdjoints();
        ASSERT_EQ(quotes.Rows(), static_cast<int>(legacy.buckets_.size()));
        ASSERT_EQ(quotes.Cols(), 1);
        for (int row = 0; row < quotes.Rows(); ++row) {
            ASSERT_EQ(quotes(row, 0), legacy.buckets_[row].dPvDDecimalQuote_);
            ASSERT_EQ(quotes(row, 0) * 1e-4, legacy.buckets_[row].dv01_);
        }
    }

    struct RejectGetterWork_ {
        Script::TestSupport::RejectFixingReads_ history_;
        Script::TestSupport::RejectSubmissions_ workers_;
        Detail::FixingReadObserver_* previousHistory_ = Excel::ScriptTestFixingObserver();
        Script::Detail::SimulationObserver_* previousWorkers_ = Excel::ScriptTestSimulationObserver();
        RejectGetterWork_() {
            Excel::ScriptTestFixingObserver() = &history_;
            Excel::ScriptTestSimulationObserver() = &workers_;
        }
        ~RejectGetterWork_() {
            Excel::ScriptTestFixingObserver() = previousHistory_;
            Excel::ScriptTestSimulationObserver() = previousWorkers_;
        }
    };
} // namespace

TEST(ExcelCalibrationRiskTest, TestFrozenDupireCommonHandlesOwnSeedsAndMatchTypedRisk) {
    using namespace Dal;
    const auto frozen = FrozenCalibration();
    Handle_<Storable_> input(new StorableDupireCalibration_("source", frozen));
    Handle_<StorableCalibrationPullback_> boundary;
    CalibrationPullback_New("common", input, &boundary);
    Matrix_<> nodes(9, 2, -0.25), quotes(3, 2, 0.125);
    Handle_<StorableCalibrationParameterAdjoints_> parameters;
    Handle_<StorableCalibrationDirectQuoteAdjoints_> direct;
    CalibrationParameterAdjoints_New("parameters", boundary, nodes, &parameters);
    CalibrationDirectQuoteAdjoints_New("direct", boundary, quotes, &direct);
    const auto reference = PullbackDupireCalibration(frozen, {frozen, nodes}, DupireDirectQuoteAdjoints_{frozen, quotes});
    nodes(0, 0) = quotes(0, 0) = -999.0;
    input.reset();
    Handle_<StorableCalibrationQuoteRisk_> result;
    CalibrationQuoteRisk_New("result", boundary, parameters, direct, &result);
    Matrix_<> values;
    CalibrationQuoteRisk_Get_Adjoints(result, "calibration", &values);
    ASSERT_NO_FATAL_FAILURE(AssertMatrix(values, reference.CalibrationAdjoints()));
    CalibrationQuoteRisk_Get_Adjoints(result, "direct", &values);
    ASSERT_NO_FATAL_FAILURE(AssertMatrix(values, reference.DirectAdjoints()));
    CalibrationQuoteRisk_Get_Adjoints(result, {}, &values);
    ASSERT_NO_FATAL_FAILURE(AssertMatrix(values, reference.TotalAdjoints()));
    values(0, 0) = -999.0;
    CalibrationQuoteRisk_Get_Adjoints(result, "total", &values);
    ASSERT_NO_FATAL_FAILURE(AssertMatrix(values, reference.TotalAdjoints()));
    Handle_<StorableCalibrationPullback_> projection;
    CalibrationQuoteRisk_Get_Calibration(result, &projection);
    ASSERT_TRUE(projection->val_.Matches(boundary->val_));
    Handle_<Storable_> source;
    CalibrationPullback_Get_Source(projection, &source);
    const auto typed = handle_cast<StorableDupireCalibration_>(source);
    ASSERT_TRUE(typed);
    ASSERT_TRUE(typed->val_.Matches(frozen));
    Matrix_<Cell_> metadata;
    CalibrationQuoteRisk_Get_Provenance(result, &metadata);
    ASSERT_EQ(metadata.Rows(), 14);
    ASSERT_EQ(metadata.Cols(), 2);
    ASSERT_EQ(Cell::ToString(metadata(8, 1)), "DupireCalibration");
    ASSERT_EQ(Cell::ToString(metadata(9, 1)), "CompleteFrozenCalibrationContent");
    ASSERT_EQ(Cell::ToString(metadata(10, 1)), frozen.Algorithm());
    ASSERT_EQ(Cell::ToDouble(metadata(11, 1)), frozen.Spot());
    ASSERT_EQ(Cell::ToDouble(metadata(12, 1)), frozen.Rate());
    ASSERT_EQ(Cell::ToDouble(metadata(13, 1)), frozen.DividendYield());
}

TEST(ExcelCalibrationRiskTest, TestSignedJointPortfoliosMatchLegacyAcrossModesAndRepresentations) {
    using namespace Dal;
    for (const auto mode : {CurveJacobianMode_::Value_::ANALYTIC, CurveJacobianMode_::Value_::BUMPED})
        for (const auto representation :
             {CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD,
              CurveParameterization_::Value_::ZERO_RATE, CurveParameterization_::Value_::LOG_DISCOUNT})
            for (bool layered : {false, true}) {
                const auto spec = JointQuoteRiskFixtures::Spec(5, 2, representation, layered);
                const auto options = JointXccyQuoteRiskFixtures::Options(mode);
                const auto calibrated = CalibrateJointMultiCurve(spec, options);
                const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
                ASSERT_NO_FATAL_FAILURE(AssertPricedPortfolio({JointQuoteRiskFixtures::Irs(spec), JointQuoteRiskFixtures::Irs(spec, 1, -250000.0)},
                                                              market, CapturedCurve(spec, calibrated, options, market)));
            }
}

TEST(ExcelCalibrationRiskTest, TestMixedPvCurrenciesUseSeparateRawSeeds) {
    using namespace Dal;
    const auto spec = JointQuoteRiskFixtures::Spec(5, 2, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
    const auto options = JointXccyQuoteRiskFixtures::Options();
    const auto calibrated = CalibrateJointMultiCurve(spec, options);
    const auto market = JointXccyQuoteRiskFixtures::Market(spec, calibrated, true);
    const auto provenance = CapturedCurve(spec, calibrated, options, market);
    const Vector_<RateTradeDefinition_> usd{JointQuoteRiskFixtures::Irs(spec, 1)}, eur{JointXccyQuoteRiskFixtures::Trade(spec)};
    ASSERT_NO_FATAL_FAILURE(AssertPricedPortfolio(usd, market, provenance));
    ASSERT_NO_FATAL_FAILURE(AssertPricedPortfolio(eur, market, provenance));
    const auto legacy = AggregateRatePortfolioQuoteRisk({usd.front(), eur.front()}, market, {*provenance->val_});
    ASSERT_EQ(legacy.pvByActualPvCcy_.size(), 2);
    ASSERT_EQ(legacy.buckets_.size(), 2 * provenance->val_->Axis().quotes_.size());
}

TEST(ExcelCalibrationRiskTest, TestSeedShapeAndFiniteChecksPreserveOutputs) {
    using namespace Dal;
    const auto boundary = DupireBoundary();
    Handle_<StorableCalibrationParameterAdjoints_> parameters;
    Handle_<StorableCalibrationDirectQuoteAdjoints_> direct;
    CalibrationParameterAdjoints_New("good", boundary, Matrix_<>(9, 2, -0.25), &parameters);
    CalibrationDirectQuoteAdjoints_New("good", boundary, Matrix_<>(3, 2, 0.125), &direct);
    const auto savedParameters = parameters;
    const auto savedDirect = direct;
    for (const auto& wrong : {Matrix_<>(), Matrix_<>(0, std::numeric_limits<int>::max()), Matrix_<>(9, 1)}) {
        ASSERT_THROW(CalibrationParameterAdjoints_New("bad", boundary, wrong, &parameters), Exception_);
        ASSERT_EQ(parameters, savedParameters);
        ASSERT_THROW(CalibrationDirectQuoteAdjoints_New("bad", boundary, wrong, &direct), Exception_);
        ASSERT_EQ(direct, savedDirect);
    }
    for (double value :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        Matrix_<> nodes(9, 2, 0.0), quotes(3, 2, 0.0);
        nodes(1, 1) = quotes(1, 1) = value;
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { CalibrationParameterAdjoints_New("bad", boundary, nodes, &parameters); }, "row=1"));
        ASSERT_EQ(parameters, savedParameters);
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { CalibrationDirectQuoteAdjoints_New("bad", boundary, quotes, &direct); }, "column=1"));
        ASSERT_EQ(direct, savedDirect);
    }
}

TEST(ExcelCalibrationRiskTest, TestWrongSourceAndUncapturedProvenanceRetainBoundary) {
    using namespace Dal;
    auto boundary = DupireBoundary();
    const auto saved = boundary;
    ASSERT_THROW(CalibrationPullback_New("null", {}, &boundary), Exception_);
    ASSERT_EQ(boundary, saved);
    ASSERT_THROW(CalibrationPullback_New("wrong", Handle_<Storable_>(new StorableRatePricingMarket_({})), &boundary), Exception_);
    ASSERT_EQ(boundary, saved);
    const Handle_<Storable_> legacy(new StorableRateQuoteRiskProvenance_("legacy", "JOINT_MULTI_CURVE", "QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE"));
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { CalibrationPullback_New("legacy", legacy, &boundary); }, "QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE"));
    ASSERT_EQ(boundary, saved);
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto options = JointXccyQuoteRiskFixtures::Options();
    const auto calibrated = CalibrateJointMultiCurve(spec, options);
    const auto uncaptured = BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, JointQuoteRiskFixtures::Market(spec, calibrated),
                                                                    JointQuoteRiskFixtures::Config(2));
    const Handle_<Storable_> input(new StorableRateQuoteRiskProvenance_(uncaptured));
    ASSERT_NO_FATAL_FAILURE(
        AssertError([&] { CalibrationPullback_New("uncaptured", input, &boundary); }, "QUOTE_RISK_CALIBRATION_RECORD_NOT_RETAINED"));
    ASSERT_EQ(boundary, saved);
}

TEST(ExcelCalibrationRiskTest, TestFullCurveIdentityRejectsCaseOnlyIdAndMixedDomains) {
    using namespace Dal;
    const auto curve = CurveBoundary("CaseId"), other = CurveBoundary("caseid"), dupire = DupireBoundary();
    ASSERT_FALSE(curve->val_.Matches(other->val_));
    Handle_<StorableCalibrationParameterAdjoints_> good, wrong, mixed;
    Handle_<StorableCalibrationDirectQuoteAdjoints_> direct;
    CalibrationParameterAdjoints_New("good", curve, Matrix_<>(curve->val_.ParameterRows(), 1, -0.25), &good);
    CalibrationParameterAdjoints_New("wrong", other, Matrix_<>(other->val_.ParameterRows(), 1, -0.25), &wrong);
    CalibrationParameterAdjoints_New("mixed", dupire, Matrix_<>(9, 2, -0.25), &mixed);
    CalibrationDirectQuoteAdjoints_New("wrong", other, Matrix_<>(other->val_.QuoteRows(), 1, 0.125), &direct);
    Handle_<StorableCalibrationQuoteRisk_> result;
    CalibrationQuoteRisk_New("good", curve, good, {}, &result);
    const auto saved = result;
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { CalibrationQuoteRisk_New("bad", curve, wrong, {}, &result); }, "CalibrationSnapshotMismatch"));
    ASSERT_EQ(result, saved);
    ASSERT_THROW(CalibrationQuoteRisk_New("bad", curve, mixed, {}, &result), Exception_);
    ASSERT_EQ(result, saved);
    ASSERT_THROW(CalibrationQuoteRisk_New("bad", curve, good, direct, &result), Exception_);
    ASSERT_EQ(result, saved);
    CalibrationQuoteRisk_New("recovery", curve, good, {}, &result);
    ASSERT_NO_FATAL_FAILURE(AssertMatrix(result->val_.TotalAdjoints(), saved->val_.TotalAdjoints()));
}

TEST(ExcelCalibrationRiskTest, TestDupireDirectSeedsPreserveQuoteIdentityAcrossFixedBases) {
    using namespace Dal;
    const auto boundary = DupireBoundary(), changed = DupireBoundary(0.25);
    Handle_<StorableCalibrationParameterAdjoints_> parameters, wrong;
    Handle_<StorableCalibrationDirectQuoteAdjoints_> direct;
    CalibrationParameterAdjoints_New("good", boundary, Matrix_<>(9, 2, -0.25), &parameters);
    CalibrationParameterAdjoints_New("wrong", changed, Matrix_<>(9, 2, -0.25), &wrong);
    CalibrationDirectQuoteAdjoints_New("direct", changed, Matrix_<>(3, 2, 0.125), &direct);
    Handle_<StorableCalibrationQuoteRisk_> result;
    CalibrationQuoteRisk_New("result", boundary, parameters, direct, &result);
    const auto& snapshot = std::get<DupireCalibrationSnapshot_>(boundary->val_.Source());
    const auto& other = std::get<DupireCalibrationSnapshot_>(changed->val_.Source());
    const auto expected =
        PullbackDupireCalibration(snapshot, {snapshot, Matrix_<>(9, 2, -0.25)}, DupireDirectQuoteAdjoints_{other, Matrix_<>(3, 2, 0.125)});
    ASSERT_NO_FATAL_FAILURE(AssertMatrix(result->val_.TotalAdjoints(), expected.TotalAdjoints()));
    const auto saved = result;
    ASSERT_THROW(CalibrationQuoteRisk_New("bad", boundary, wrong, direct, &result), Exception_);
    ASSERT_EQ(result, saved);
}

TEST(ExcelCalibrationRiskTest, TestDirectOnlyAndOverflowFailureRecoverWithoutPartialResults) {
    using namespace Dal;
    const auto boundary = CurveBoundary();
    Handle_<StorableCalibrationParameterAdjoints_> zero, extreme;
    Handle_<StorableCalibrationDirectQuoteAdjoints_> direct;
    CalibrationParameterAdjoints_New("zero", boundary, Matrix_<>(boundary->val_.ParameterRows(), 1, 0.0), &zero);
    const Matrix_<> quotes(boundary->val_.QuoteRows(), 1, -0.125);
    CalibrationDirectQuoteAdjoints_New("direct", boundary, quotes, &direct);
    Handle_<StorableCalibrationQuoteRisk_> result;
    CalibrationQuoteRisk_New("direct-only", boundary, zero, direct, &result);
    ASSERT_NO_FATAL_FAILURE(AssertMatrix(result->val_.TotalAdjoints(), quotes));
    const auto saved = result;
    CalibrationParameterAdjoints_New("extreme", boundary, Matrix_<>(boundary->val_.ParameterRows(), 1, 1e300), &extreme);
    const auto finite = PullbackCalibration(boundary->val_, extreme->val_);
    Matrix_<> overflow(boundary->val_.QuoteRows(), 1);
    for (int row = 0; row < overflow.Rows(); ++row)
        overflow(row, 0) = std::copysign(std::numeric_limits<double>::max(), finite.TotalAdjoints()(row, 0));
    CalibrationDirectQuoteAdjoints_New("finite-direct", boundary, overflow, &direct);
    ASSERT_THROW(CalibrationQuoteRisk_New("overflow", boundary, extreme, direct, &result), Exception_);
    ASSERT_EQ(result, saved);
    CalibrationQuoteRisk_New("recovery", boundary, zero, {}, &result);
    ASSERT_NO_FATAL_FAILURE(AssertMatrix(result->val_.TotalAdjoints(), Matrix_<>(quotes.Rows(), 1, 0.0)));
}

TEST(ExcelCalibrationRiskTest, TestCommonGettersAreDetachedAndPassiveDuringUnrelatedRecording) {
    using namespace Dal;
    const auto boundary = CurveBoundary();
    Handle_<StorableCalibrationParameterAdjoints_> parameters;
    Handle_<StorableCalibrationDirectQuoteAdjoints_> direct;
    CalibrationParameterAdjoints_New("parameters", boundary, Matrix_<>(boundary->val_.ParameterRows(), 1, -0.25), &parameters);
    CalibrationDirectQuoteAdjoints_New("direct", boundary, Matrix_<>(boundary->val_.QuoteRows(), 1, 0.125), &direct);
    const auto expected = PullbackCalibration(boundary->val_, parameters->val_, direct->val_);
    const RejectGetterWork_ reject;
    double adjoint = 0.0;
    Excel::ScriptTestWithRecording(
        [&](size_t nodeCount) {
            Handle_<StorableCalibrationQuoteRisk_> result;
            CalibrationQuoteRisk_New("risk", boundary, parameters, direct, &result);
            ASSERT_NO_FATAL_FAILURE(AssertMatrix(result->val_.TotalAdjoints(), expected.TotalAdjoints()));
            Matrix_<> matrix;
            CalibrationParameterAdjoints_Get_Adjoints(parameters, &matrix);
            matrix(0, 0) = -999.0;
            ASSERT_EQ(parameters->val_.Adjoints()(0, 0), -0.25);
            CalibrationDirectQuoteAdjoints_Get_Adjoints(direct, &matrix);
            matrix(0, 0) = -999.0;
            ASSERT_EQ(direct->val_.Adjoints()(0, 0), 0.125);
            CalibrationQuoteRisk_Get_Adjoints(result, "TOTAL", &matrix);
            ASSERT_NO_FATAL_FAILURE(AssertMatrix(matrix, expected.TotalAdjoints()));
            Matrix_<Cell_> metadata;
            CalibrationPullback_Get_Provenance(boundary, &metadata);
            ASSERT_EQ(metadata.Rows(), 17);
            ASSERT_EQ(Cell::ToString(metadata(1, 1)), "RetainedCurveEffectiveInverse");
            ASSERT_EQ(Cell::ToString(metadata(2, 1)), "DECIMAL_QUOTE");
            ASSERT_EQ(Cell::ToString(metadata(3, 1)), "FrozenCalibrationEffectiveInverse");
            CalibrationQuoteRisk_Get_Provenance(result, &metadata);
            Handle_<StorableCalibrationPullback_> projection;
            CalibrationParameterAdjoints_Get_Calibration(parameters, &projection);
            ASSERT_TRUE(projection->val_.Matches(boundary->val_));
            CalibrationDirectQuoteAdjoints_Get_Calibration(direct, &projection);
            ASSERT_TRUE(projection->val_.Matches(boundary->val_));
            CalibrationQuoteRisk_Get_Calibration(result, &projection);
            ASSERT_TRUE(projection->val_.Matches(boundary->val_));
            Handle_<Storable_> source;
            CalibrationPullback_Get_Source(projection, &source);
            const auto native = handle_cast<StorableRateQuoteRiskProvenance_>(source);
            ASSERT_TRUE(native && native->Native());
            ASSERT_EQ(native->val_->CalibrationRecord(), std::get<RateQuoteRiskProvenance_>(boundary->val_.Source()).CalibrationRecord());
            ASSERT_EQ(reject.history_.historyCalls_, 0);
            ASSERT_EQ(reject.workers_.calls_, 0);
            ASSERT_EQ(Excel::ScriptTestTapeNodeCount(), nodeCount);
        },
        false, &adjoint);
    ASSERT_EQ(adjoint, 4.0);
}

TEST(ExcelCalibrationRiskTest, TestNullFactoriesAndGettersRetainPreviousOutputs) {
    using namespace Dal;
    auto boundary = DupireBoundary();
    Handle_<StorableCalibrationParameterAdjoints_> parameters;
    Handle_<StorableCalibrationDirectQuoteAdjoints_> direct;
    CalibrationParameterAdjoints_New("parameters", boundary, Matrix_<>(9, 2, 0.0), &parameters);
    CalibrationDirectQuoteAdjoints_New("direct", boundary, Matrix_<>(3, 2, 0.0), &direct);
    Handle_<StorableCalibrationQuoteRisk_> result;
    CalibrationQuoteRisk_New("risk", boundary, parameters, direct, &result);
    const auto savedParameters = parameters;
    const auto savedDirect = direct;
    const auto savedResult = result;
    const auto savedBoundary = boundary;
    ASSERT_THROW(CalibrationParameterAdjoints_New("null", {}, Matrix_<>(9, 2), &parameters), Exception_);
    ASSERT_EQ(parameters, savedParameters);
    ASSERT_THROW(CalibrationDirectQuoteAdjoints_New("null", {}, Matrix_<>(3, 2), &direct), Exception_);
    ASSERT_EQ(direct, savedDirect);
    ASSERT_THROW(CalibrationQuoteRisk_New("null", {}, parameters, direct, &result), Exception_);
    ASSERT_THROW(CalibrationQuoteRisk_New("null", boundary, {}, direct, &result), Exception_);
    ASSERT_EQ(result, savedResult);
    Matrix_<> matrix(1, 1, 7.0);
    ASSERT_THROW(CalibrationParameterAdjoints_Get_Adjoints({}, &matrix), Exception_);
    ASSERT_THROW(CalibrationDirectQuoteAdjoints_Get_Adjoints({}, &matrix), Exception_);
    ASSERT_THROW(CalibrationQuoteRisk_Get_Adjoints({}, {}, &matrix), Exception_);
    ASSERT_THROW(CalibrationQuoteRisk_Get_Adjoints(result, "unknown", &matrix), Exception_);
    ASSERT_EQ(matrix(0, 0), 7.0);
    ASSERT_THROW(CalibrationParameterAdjoints_Get_Calibration({}, &boundary), Exception_);
    ASSERT_THROW(CalibrationDirectQuoteAdjoints_Get_Calibration({}, &boundary), Exception_);
    ASSERT_THROW(CalibrationQuoteRisk_Get_Calibration({}, &boundary), Exception_);
    ASSERT_EQ(boundary, savedBoundary);
    Matrix_<Cell_> metadata(1, 1, Cell_("saved"));
    ASSERT_THROW(CalibrationPullback_Get_Provenance({}, &metadata), Exception_);
    ASSERT_THROW(CalibrationQuoteRisk_Get_Provenance({}, &metadata), Exception_);
    ASSERT_EQ(Cell::ToString(metadata(0, 0)), "saved");
    Handle_<Storable_> source(boundary);
    ASSERT_THROW(CalibrationPullback_Get_Source({}, &source), Exception_);
    ASSERT_EQ(source.get(), boundary.get());
}

TEST(ExcelCalibrationRiskTest, TestCommonHandlesRejectArchivesExplicitly) {
    using namespace Dal;
    const auto boundary = DupireBoundary();
    Handle_<StorableCalibrationParameterAdjoints_> parameters;
    Handle_<StorableCalibrationDirectQuoteAdjoints_> direct;
    CalibrationParameterAdjoints_New("parameters", boundary, Matrix_<>(9, 2, 0.0), &parameters);
    CalibrationDirectQuoteAdjoints_New("direct", boundary, Matrix_<>(3, 2, 0.0), &direct);
    Handle_<StorableCalibrationQuoteRisk_> result;
    CalibrationQuoteRisk_New("risk", boundary, parameters, direct, &result);
    for (const Handle_<Storable_>& value : Vector_<Handle_<Storable_>>{Handle_<Storable_>(boundary), Handle_<Storable_>(parameters),
                                                                       Handle_<Storable_>(direct), Handle_<Storable_>(result)})
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { JSON::WriteString(*value); }, "CalibrationArchiveUnsupported"));
}

TEST(ExcelCalibrationRiskTest, TestEmbeddedNulNamesAndSelectorsRetainOutputs) {
    using namespace Dal;
    auto boundary = DupireBoundary();
    const auto savedBoundary = boundary;
    const String_ invalid(std::string("a\0b", 3));
    ASSERT_THROW(CalibrationPullback_New(invalid, Handle_<Storable_>(new StorableDupireCalibration_("source", FrozenCalibration())), &boundary),
                 Exception_);
    ASSERT_EQ(boundary, savedBoundary);
    Handle_<StorableCalibrationParameterAdjoints_> parameters;
    Handle_<StorableCalibrationDirectQuoteAdjoints_> direct;
    CalibrationParameterAdjoints_New("parameters", boundary, Matrix_<>(9, 2, 0.0), &parameters);
    CalibrationDirectQuoteAdjoints_New("direct", boundary, Matrix_<>(3, 2, 0.0), &direct);
    const auto savedParameters = parameters;
    const auto savedDirect = direct;
    ASSERT_THROW(CalibrationParameterAdjoints_New(invalid, boundary, Matrix_<>(9, 2), &parameters), Exception_);
    ASSERT_EQ(parameters, savedParameters);
    ASSERT_THROW(CalibrationDirectQuoteAdjoints_New(invalid, boundary, Matrix_<>(3, 2), &direct), Exception_);
    ASSERT_EQ(direct, savedDirect);
    Handle_<StorableCalibrationQuoteRisk_> result;
    CalibrationQuoteRisk_New("risk", boundary, parameters, direct, &result);
    const auto saved = result;
    ASSERT_THROW(CalibrationQuoteRisk_New(invalid, boundary, parameters, direct, &result), Exception_);
    ASSERT_EQ(result, saved);
    Matrix_<> matrix(1, 1, 7.0);
    ASSERT_THROW(CalibrationQuoteRisk_Get_Adjoints(result, invalid, &matrix), Exception_);
    ASSERT_EQ(matrix(0, 0), 7.0);
}

TEST(ExcelCalibrationRiskTest, TestDupireNestedFailurePreservesRecordingAndRecovers) {
    using namespace Dal;
    const auto boundary = DupireBoundary();
    Handle_<StorableCalibrationParameterAdjoints_> parameters;
    CalibrationParameterAdjoints_New("parameters", boundary, Matrix_<>(9, 2, -0.25), &parameters);
    Handle_<StorableCalibrationQuoteRisk_> result;
    CalibrationQuoteRisk_New("risk", boundary, parameters, {}, &result);
    const auto saved = result;
    double adjoint = 0.0;
    Excel::ScriptTestWithRecording(
        [&](size_t nodeCount) {
            ASSERT_THROW(CalibrationQuoteRisk_New("nested", boundary, parameters, {}, &result), Exception_);
            ASSERT_EQ(result, saved);
            ASSERT_EQ(Excel::ScriptTestTapeNodeCount(), nodeCount);
        },
        true, &adjoint);
    ASSERT_EQ(adjoint, 4.0);
    CalibrationQuoteRisk_New("recovery", boundary, parameters, {}, &result);
    ASSERT_NO_FATAL_FAILURE(AssertMatrix(result->val_.TotalAdjoints(), saved->val_.TotalAdjoints()));
}

#ifdef _WIN32
TEST(ExcelCalibrationRiskTest, TestWorksheetCellsRetainLocationsAndRejectShapeBeforeDereference) {
    using namespace Dal;
    OPER_ cells[4]{};
    for (auto& cell : cells) {
        cell.xltype = xltypeNum;
        cell.val.num = 0.125;
    }
    OPER_ range{};
    range.xltype = xltypeMulti;
    range.val.array = {cells, 2, 2};
    for (const auto type : {xltypeBool, xltypeStr, xltypeErr, xltypeNil, xltypeMissing}) {
        cells[3].xltype = type;
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { Excel::ValidateCalibrationAdjointsInput(&range, "seed; adjoints", 2, 2); }, "row=2; column=2"));
    }
    cells[3].xltype = xltypeNum;
    for (double value :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        cells[3].val.num = value;
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { Excel::ValidateCalibrationAdjointsInput(&range, "seed; adjoints", 2, 2); }, "row=2; column=2"));
    }
    range.val.array = {nullptr, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { Excel::ValidateCalibrationAdjointsInput(&range, "seed; adjoints", 2, 2); }, "dimensions"));
}

TEST(ExcelCalibrationRiskTest, TestWorksheetNamesAndCaptureKeepStrictScalarValidation) {
    using namespace Dal;
    wchar_t text[]{4, L'a', L'\0', L'b', L'c'};
    OPER_ cell{};
    cell.xltype = xltypeStr;
    cell.val.str = text;
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { Excel::ValidateCalibrationTextInput(&cell, "factory; name"); }, "InvalidCalibrationPullback"));
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { Excel::ValidateDupireTextInput(&cell, "factory; name"); }, "InvalidDupireInput"));
    OPER_ range{};
    range.xltype = xltypeMulti;
    range.val.array = {&cell, 1, 1};
    ASSERT_THROW(Excel::ValidateCalibrationTextInput(&range, "getter; contribution"), Exception_);
    for (const auto type : {xltypeInt, xltypeNum, xltypeStr, xltypeErr}) {
        cell.xltype = type;
        ASSERT_THROW(Excel::ValidateCalibrationRecordCaptureInput(&range), Exception_);
    }
    for (const auto type : {xltypeBool, xltypeNil, xltypeMissing}) {
        cell.xltype = type;
        ASSERT_NO_THROW(Excel::ValidateCalibrationRecordCaptureInput(&range));
    }
    text[0] = 0;
    cell.xltype = xltypeStr;
    cell.val.str = text;
    ASSERT_NO_THROW(Excel::ValidateCalibrationRecordCaptureInput(&range));
}

TEST(ExcelCalibrationRiskTest, TestWorksheetSeedsAndCaptureRejectCoercionBeforeConversion) {
    using namespace Dal;
    OPER_ cells[2]{};
    cells[0].xltype = xltypeInt;
    cells[0].val.w = -2;
    cells[1].xltype = xltypeNum;
    cells[1].val.num = 0.125;
    OPER_ range{};
    range.xltype = xltypeMulti;
    range.val.array = {cells, 1, 2};
    ASSERT_NO_THROW(Excel::ValidateCalibrationAdjointsInput(&range, "seed; adjoints", 1, 2));
    const Excel::ScriptSettingsInput_ input(&range);
    ASSERT_EQ(input.Get()->val.array.lparray[0].xltype, xltypeNum);
    ASSERT_EQ(input.Get()->val.array.lparray[0].val.num, -2.0);
    ASSERT_EQ(cells[0].xltype, xltypeInt);
    cells[1].xltype = xltypeBool;
    ASSERT_THROW(Excel::ValidateCalibrationAdjointsInput(&range, "seed; adjoints", 1, 2), Exception_);
    cells[1].xltype = xltypeNum;
    cells[1].val.num = std::numeric_limits<double>::infinity();
    ASSERT_THROW(Excel::ValidateCalibrationAdjointsInput(&range, "seed; adjoints", 1, 2), Exception_);
    cells[1].val.num = 0.125;
    ASSERT_THROW(Excel::ValidateCalibrationAdjointsInput(&range, "seed; adjoints", 2, 1), Exception_);
    ASSERT_THROW(Excel::ValidateCalibrationRecordCaptureInput(cells), Exception_);
    cells[0].xltype = xltypeBool;
    cells[0].val.xbool = 1;
    ASSERT_NO_THROW(Excel::ValidateCalibrationRecordCaptureInput(cells));
    cells[0].xltype = xltypeMissing;
    ASSERT_NO_THROW(Excel::ValidateCalibrationRecordCaptureInput(cells));
}
#endif
