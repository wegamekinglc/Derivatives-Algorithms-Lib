#if defined(_WIN32) || defined(DAL_EXCEL_API_TESTS_PORTABLE)

#include <gtest/gtest.h>

#include <dal-excel/src/__jointcalibration_test_api.hpp>
#include <dal-public/tests/jointquoteriskfixture.hpp>

namespace {
    using namespace Dal;

    Matrix_<Cell_> Settings(std::initializer_list<std::pair<String_, Cell_>> entries) {
        Matrix_<Cell_> result(static_cast<int>(entries.size()), 2);
        int row = 0;
        for (const auto& [key, value] : entries) {
            result(row, 0) = Cell_(key);
            result(row++, 1) = value;
        }
        return result;
    }

    Handle_<StorableJointMultiCurveCalibrationSpec_> ConstructSpec(bool layered) {
        const auto input = JointQuoteRiskPublicFixture::Spec(layered);
        Vector_<Handle_<Storable_>> declarations;
        for (const auto& value : input.curves_) {
            Vector_<Handle_<Storable_>> instruments;
            for (const auto& instrument : value.instruments_)
                instruments.push_back(Handle_<Storable_>(new StorableYCInstrument_(instrument)));
            const auto settings = Settings({{"parameterization", Cell_(String_(value.parameterization_.String()))},
                                            {"baseLayeredOverDiscount", Cell_(value.baseLayeredOverDiscount_)}});
            Handle_<StorableJointCurveDeclaration_> declaration;
            JointCurveDeclaration_New(value.curveName_, instruments, value.knotDates_, value.calibrateDiscountCurve_, "OIS",
                                      value.calibrateDiscountCurve_ ? String_() : String_("3M"), settings, {}, &declaration);
            declarations.push_back(Handle_<Storable_>(declaration));
        }
        Handle_<StorableJointMultiCurveCalibrationSpec_> spec;
        JointMultiCurveCalibrationSpec_New(input.today_, input.ccy_, declarations,
                                           Settings({{"tolerance", Cell_(input.tolerance_)},
                                                     {"initialGuess", Cell_(input.initialGuess_)},
                                                     {"maxEvaluations", Cell_(1000.0)},
                                                     {"maxRestarts", Cell_(100.0)}}),
                                           &spec);
        return spec;
    }
} // namespace

TEST(JointCalibrationExcelTest, TestReachableConstructionMatchesPublicRiskAndRetainsLegacyExclusion) {
    for (bool layered : {false, true})
        for (const String_& mode : {String_("ANALYTIC"), String_("BUMPED")}) {
            const auto spec = ConstructSpec(layered);
            Handle_<StorableJointMultiCurveCalibrationResult_> result;
            Calibrate_JointMultiCurve(spec, Settings({{"jacobianMode", Cell_(mode)}, {"computeEffJacobianInverse", Cell_(true)}}), &result);
            ASSERT_TRUE(result->val_.converged_);
            Handle_<StorableDiscountCurve_> discount, forward;
            JointMultiCurveCalibrationResult_Get_Curve(result, 0, &discount);
            JointMultiCurveCalibrationResult_Get_Curve(result, 1, &forward);
            Handle_<StorableRatePricingMarket_> market;
            const auto fixings = Handle_<StorableMarketFixingSnapshot_>(
                new StorableMarketFixingSnapshot_(Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_())));
            RatePricingMarket_New(Cell_(DateTime_(spec->val_.today_)), "USD", {"discount", "forward"},
                                  {Handle_<Storable_>(discount), Handle_<Storable_>(forward)}, fixings, {}, {}, 0.0, String_(), {}, &market);
            Handle_<StorableRateQuoteRiskProvenance_> provenance;
            JointMultiCurveQuoteRiskProvenance_New(result, "python-generic-joint", {"curve:0", "curve:1"}, {"discount", "forward"}, market,
                                                   &provenance);
            ASSERT_TRUE(provenance->Native());
            ASSERT_TRUE(provenance->val_->Available()) << provenance->reason_;
            const auto native = BuildJointMultiCurveQuoteRiskProvenance(JointQuoteRiskPublicFixture::Spec(layered), result->val_, result->options_,
                                                                        market->val_, JointQuoteRiskPublicFixture::Config());
            ASSERT_EQ(native.Axis().fingerprint_, provenance->val_->Axis().fingerprint_);
            ASSERT_EQ(native.State().fingerprint_, provenance->val_->State().fingerprint_);
            const auto trade = JointQuoteRiskPublicFixture::Trade();
            const auto expected = AggregateRatePortfolioQuoteRisk({trade}, market->val_, {native});
            if (!layered && mode == "ANALYTIC") {
                const auto reference = JointQuoteRiskPublicFixture::ParityRows();
                ASSERT_EQ(expected.buckets_.size(), reference.size());
                for (int row = 0; row < static_cast<int>(reference.size()); ++row) {
                    ASSERT_EQ(expected.buckets_[row].axisFingerprint_, String_(reference[row][0]));
                    ASSERT_NEAR(expected.buckets_[row].dPvDDecimalQuote_, std::stod(reference[row][3]), 1.0e-5);
                    ASSERT_NEAR(expected.buckets_[row].dv01_, std::stod(reference[row][4]), 1.0e-9);
                }
            }
            const auto tradeHandle = Handle_<Storable_>(new StorableRateTradeDefinition_(trade));
            Matrix_<Cell_> spill;
            RatePortfolioQuoteRisk_Spill({tradeHandle}, market, {Handle_<Storable_>(provenance)}, &spill);
            ASSERT_EQ(spill.Cols(), 10);
            ASSERT_EQ(spill.Rows(), 5);
            for (int row = 0; row < 5; ++row) {
                ASSERT_EQ(Cell::ToString(spill(row, 2)), expected.buckets_[row].quoteKey_);
                ASSERT_EQ(Cell::ToString(spill(row, 5)), "USD");
                ASSERT_DOUBLE_EQ(Cell::ToDouble(spill(row, 6)), expected.buckets_[row].dPvDDecimalQuote_);
                ASSERT_DOUBLE_EQ(Cell::ToDouble(spill(row, 7)), expected.buckets_[row].dv01_);
            }
            Matrix_<Cell_> ranges;
            JointMultiCurveCalibrationResult_Get(result, "residualRanges", &ranges);
            ASSERT_EQ(ranges.Rows(), 2);
            ASSERT_EQ(ranges.Cols(), 3);
            ASSERT_EQ(Cell::ToDouble(ranges(1, 1)), 3.0);
            JointMultiCurveCalibrationResult_Get(result, "effJacobianInverse", &ranges);
            ASSERT_EQ(ranges.Rows(), 5);
            ASSERT_EQ(ranges.Cols(), 5);
            Handle_<StorableRateQuoteRiskProvenance_> legacy;
            RateQuoteRiskProvenance_New(Handle_<Storable_>(result), "legacy", {"curve:0", "curve:1"}, {"discount", "forward"}, market, &legacy);
            ASSERT_FALSE(legacy->Native());
            ASSERT_EQ(legacy->reason_, "QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE");
        }
}

TEST(JointCalibrationExcelTest, TestStrictSettingsAndDefaultUnavailability) {
    const auto spec = ConstructSpec(false);
    Handle_<StorableJointMultiCurveCalibrationResult_> result;
    ASSERT_THROW(Calibrate_JointMultiCurve(spec, Settings({{"computeEffJacobianInverse", Cell_(1.0)}}), &result), Exception_);
    ASSERT_THROW(Calibrate_JointMultiCurve(spec, Settings({{"unknown", Cell_(true)}}), &result), Exception_);
    ASSERT_THROW(Calibrate_JointMultiCurve(spec, Settings({{"jacobianMode", Cell_(String_("invalid"))}}), &result), Exception_);
    ASSERT_THROW(
        Calibrate_JointMultiCurve(spec, Settings({{"computeEffJacobianInverse", Cell_(true)}, {"computeEffJacobianInverse", Cell_(false)}}), &result),
        Exception_);
    Calibrate_JointMultiCurve(spec, {}, &result);
    ASSERT_TRUE(result->val_.effJacobianInverse_.Empty());
    ASSERT_EQ(result->val_.effJacobianInverseAvailability_, "not_requested");
    Handle_<StorableDiscountCurve_> curve;
    ASSERT_THROW(JointMultiCurveCalibrationResult_Get_Curve(result, -1, &curve), Exception_);
    ASSERT_THROW(JointMultiCurveCalibrationResult_Get_Curve(result, 2, &curve), Exception_);
    Matrix_<Cell_> value;
    ASSERT_THROW(JointMultiCurveCalibrationResult_Get(result, "unknown", &value), Exception_);
}

#endif
