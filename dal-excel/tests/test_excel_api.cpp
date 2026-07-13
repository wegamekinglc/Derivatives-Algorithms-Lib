//
// Created by wegam on 2026/5/30.
//

#if defined(_WIN32) || defined(DAL_EXCEL_API_TESTS_PORTABLE)

#include <gtest/gtest.h>

#include <dal-excel/src/__curve_storable.hpp>
#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curveprotocol.hpp>

namespace Dal {
    void XccyResetConvention_New(int fixingLag,
                                 const String_& fixingHolidays,
                                 const String_& fixingConvention,
                                 int fixingHour,
                                 int fixingMinute,
                                 Handle_<StorableFxResetConvention_>* resetConvention);
    void MarketFixingSnapshot_New(const Vector_<String_>& indexNames,
                                  const Vector_<Cell_>& fixingTimes,
                                  const Vector_<>& values,
                                  Handle_<StorableMarketFixingSnapshot_>* snapshot);
    void CrossCurrencySwapConfig_New(const Handle_<StorableCurrencyPair_>& currencies,
                                     const Handle_<StorableRateLegConvention_>& domesticLeg,
                                     const Handle_<StorableRateIndexConvention_>& domesticIndex,
                                     const Handle_<StorableRateLegConvention_>& foreignLeg,
                                     const Handle_<StorableRateIndexConvention_>& foreignIndex,
                                     const Handle_<StorableFxResetConvention_>& resetConvention,
                                     const String_& notionalMode,
                                     const String_& domesticRateIndex,
                                     int domesticRateFixingHour,
                                     int domesticRateFixingMinute,
                                     const String_& foreignRateIndex,
                                     int foreignRateFixingHour,
                                     int foreignRateFixingMinute,
                                     double domesticNotional,
                                     double foreignNotional,
                                     Handle_<StorableCrossCurrencySwapConfig_>* config);
    void CrossCurrencySwap_Config_New(const Date_& tradeDate,
                                      const Date_& start,
                                      const Date_& maturity,
                                      double marketRate,
                                      const Handle_<StorableCrossCurrencySwapConfig_>& config,
                                      Handle_<StorableCrossCurrencySwap_>* instrument);
    void Calibrate_JointXccy(const Cell_& valuationTime,
                             const Handle_<StorableCurrencyPair_>& currencies,
                             const String_& collateralCurrency,
                             double fxSpot,
                             const Vector_<Handle_<Storable_>>& domesticInstruments,
                             const Vector_<Date_>& domesticKnotDates,
                             const Vector_<Handle_<Storable_>>& foreignInstruments,
                             const Vector_<Date_>& foreignKnotDates,
                             const Vector_<Handle_<Storable_>>& basisInstruments,
                             const Vector_<Date_>& basisKnotDates,
                             const Handle_<StorableMarketFixingSnapshot_>& fixings,
                             const Matrix_<Cell_>& settings,
                             Handle_<StorableJointXccyCalibrationResult_>* result);
    void JointXccyCalibrationResult_Get_DomesticBlock(const Handle_<StorableJointXccyCalibrationResult_>& result,
                                                      Handle_<StorableCurveBlock_>* block);
    void JointXccyCalibrationResult_Get_ForeignBlock(const Handle_<StorableJointXccyCalibrationResult_>& result, Handle_<StorableCurveBlock_>* block);
    void JointXccyCalibrationResult_Get_BasisCurve(const Handle_<StorableJointXccyCalibrationResult_>& result,
                                                   Handle_<StorableDiscountCurve_>* curve);
    void JointXccyCalibrationResult_Get(const Handle_<StorableJointXccyCalibrationResult_>& result, const String_& attribute, Matrix_<Cell_>* value);
} // namespace Dal

namespace {
    using namespace Dal;

    Date_ Today() { return Date_(2025, 1, 16); }

    Handle_<StorableCurrencyPair_> Pair() { return Handle_<StorableCurrencyPair_>(new StorableCurrencyPair_(CurrencyPair_New("USD", "EUR"))); }

    Handle_<StorableRateLegConvention_> Leg(const char* basis) {
        return Handle_<StorableRateLegConvention_>(
            new StorableRateLegConvention_(RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New(basis))));
    }

    Handle_<StorableRateIndexConvention_> Index(const char* basis) {
        return Handle_<StorableRateIndexConvention_>(
            new StorableRateIndexConvention_(RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New(basis), CollateralType_OIS())));
    }

    Handle_<StorableCrossCurrencySwapConfig_> ResettableConfig() {
        Handle_<StorableFxResetConvention_> reset;
        XccyResetConvention_New(0, "", "Preceding", 11, 0, &reset);

        Handle_<StorableCrossCurrencySwapConfig_> config;
        CrossCurrencySwapConfig_New(Pair(), Leg("ACT_365F"), Index("ACT_365F"), Leg("ACT_360"), Index("ACT_365F"), reset, "RESETTABLE", "USD-SOFR-3M",
                                    11, 0, "EUR-ESTR-3M", 11, 0, 110.0, 100.0, &config);
        return config;
    }

    Handle_<StorableJointXccyCalibrationResult_> JointResult() {
        const Date_ maturity = Today().AddDays(365);
        Vector_<Handle_<Storable_>> domestic;
        domestic.push_back(Handle_<Storable_>(new StorableYCInstrument_(DepositNew(
            Today(), Today(), maturity, 0.04, RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_365F"), CollateralType_OIS())))));
        Vector_<Handle_<Storable_>> foreign;
        foreign.push_back(Handle_<Storable_>(new StorableYCInstrument_(DepositNew(
            Today(), Today(), maturity, 0.03, RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_365F"), CollateralType_OIS())))));

        Handle_<StorableCrossCurrencySwap_> xccy;
        CrossCurrencySwap_Config_New(Today(), Today(), maturity, 0.001, ResettableConfig(), &xccy);
        Vector_<Handle_<Storable_>> basis{Handle_<Storable_>(xccy)};

        Handle_<StorableMarketFixingSnapshot_> fixings;
        MarketFixingSnapshot_New({}, {}, {}, &fixings);
        Matrix_<Cell_> settings(3, 2);
        settings(0, 0) = Cell_("initialGuess");
        settings(0, 1) = Cell_(0.01);
        settings(1, 0) = Cell_("tolerance");
        settings(1, 1) = Cell_(1.0e-9);
        settings(2, 0) = Cell_("maxEvaluations");
        settings(2, 1) = Cell_(400.0);

        Handle_<StorableJointXccyCalibrationResult_> result;
        Calibrate_JointXccy(Cell_(DateTime_(Today(), 0, 0)), Pair(), "USD", 1.10, domestic, {maturity}, foreign, {maturity}, basis, {maturity},
                            fixings, settings, &result);
        return result;
    }
} // namespace

TEST(ExcelApiTest, TestConfiguredCrossCurrencySwapConstruction) {
    const auto config = ResettableConfig();
    ASSERT_TRUE(config);
    ASSERT_EQ(config->val_.notionalMode_.Switch(), Dal::XccyNotionalMode_::Value_::RESETTABLE);
    ASSERT_EQ(config->val_.fxReset_.fixingHour_, 11);
    ASSERT_EQ(config->val_.domesticRateFixing_.indexName_, Dal::String_("USD-SOFR-3M"));

    Dal::Handle_<Dal::StorableCrossCurrencySwap_> instrument;
    CrossCurrencySwap_Config_New(Today(), Today(), Today().AddDays(365), 0.001, config, &instrument);
    ASSERT_TRUE(instrument);
    ASSERT_EQ(instrument->val_->Config().notionalMode_.Switch(), Dal::XccyNotionalMode_::Value_::RESETTABLE);
}

TEST(ExcelApiTest, TestMarketFixingSnapshotRejectsNonParallelInputs) {
    Dal::Handle_<Dal::StorableMarketFixingSnapshot_> snapshot;
    ASSERT_THROW(MarketFixingSnapshot_New({"USD-SOFR-3M"}, {}, {0.04}, &snapshot), Dal::Exception_);
    ASSERT_THROW(MarketFixingSnapshot_New({"USD-SOFR-3M"}, {Dal::Cell_(Dal::DateTime_(Today(), 11, 0))}, {}, &snapshot), Dal::Exception_);
}

TEST(ExcelApiTest, TestJointCalibrationAndEveryResultView) {
    const auto result = JointResult();
    ASSERT_TRUE(result);

    Dal::Handle_<Dal::StorableCurveBlock_> domestic;
    Dal::Handle_<Dal::StorableCurveBlock_> foreign;
    Dal::Handle_<Dal::StorableDiscountCurve_> basis;
    JointXccyCalibrationResult_Get_DomesticBlock(result, &domestic);
    JointXccyCalibrationResult_Get_ForeignBlock(result, &foreign);
    JointXccyCalibrationResult_Get_BasisCurve(result, &basis);
    ASSERT_TRUE(domestic);
    ASSERT_TRUE(foreign);
    ASSERT_TRUE(basis);

    for (const char* attribute : {"fxForwards", "marketRates", "modelRates", "residuals", "jacobian", "parameterRanges", "residualRanges"}) {
        Dal::Matrix_<Dal::Cell_> value;
        JointXccyCalibrationResult_Get(result, attribute, &value);
        ASSERT_FALSE(value.Empty()) << attribute;
    }
}

TEST(ExcelApiTest, TestUnknownJointResultViewListsEveryAcceptedAttribute) {
    const auto result = JointResult();
    try {
        Dal::Matrix_<Dal::Cell_> value;
        JointXccyCalibrationResult_Get(result, "unknown", &value);
        FAIL() << "Expected an unknown joint result attribute to fail";
    } catch (const Dal::Exception_& exception) {
        const std::string message(exception.what());
        for (const char* attribute : {"domesticBlock", "foreignBlock", "basisCurve", "fxForwards", "marketRates", "modelRates", "residuals",
                                      "jacobian", "parameterRanges", "residualRanges"})
            ASSERT_NE(message.find(attribute), std::string::npos) << attribute << ": " << message;
    }
}

#endif // defined(_WIN32) || defined(DAL_EXCEL_API_TESTS_PORTABLE)
