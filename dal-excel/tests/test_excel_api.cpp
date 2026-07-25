//
// Created by wegam on 2026/5/30.
//

#if defined(_WIN32) || defined(DAL_EXCEL_API_TESTS_PORTABLE)

#include <gtest/gtest.h>

#include <dal-excel/src/__curve_storable.hpp>
#include <dal-excel/src/__xccy_test_api.hpp>
#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curveprotocol.hpp>

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

    Handle_<StorableJointXccyCalibrationResult_> JointResult(const Matrix_<Cell_>& settings) {
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

        Handle_<StorableJointXccyCalibrationResult_> result;
        Calibrate_JointXccy(Cell_(DateTime_(Today(), 0, 0)), Pair(), "USD", 1.10, domestic, {maturity}, foreign, {maturity}, basis, {maturity},
                            fixings, settings, &result);
        return result;
    }

    Handle_<StorableJointXccyCalibrationResult_> JointResult() {
        Matrix_<Cell_> settings(3, 2);
        settings(0, 0) = Cell_("initialGuess");
        settings(0, 1) = Cell_(0.01);
        settings(1, 0) = Cell_("tolerance");
        settings(1, 1) = Cell_(1.0e-9);
        settings(2, 0) = Cell_("maxEvaluations");
        settings(2, 1) = Cell_(400.0);
        return JointResult(settings);
    }

    Matrix_<Cell_> StagedSettings(const Vector_<std::pair<String_, Cell_>>& additions = {}) {
        Matrix_<Cell_> settings(3 + additions.size(), 2);
        settings(0, 0) = Cell_("fxSpot");
        settings(0, 1) = Cell_(1.10);
        settings(1, 0) = Cell_("initialGuess");
        settings(1, 1) = Cell_(0.01);
        settings(2, 0) = Cell_("tolerance");
        settings(2, 1) = Cell_(1.0e-8);
        for (int i = 0; i < additions.size(); ++i) {
            settings(3 + i, 0) = Cell_(additions[i].first);
            settings(3 + i, 1) = additions[i].second;
        }
        return settings;
    }

    Handle_<StorableCrossCurrencyCalibrationResult_> StagedResult(const Matrix_<Cell_>& settings) {
        const Date_ maturity = Today().AddDays(2 * 365);
        const Vector_<Date_> curveKnots{Today(), maturity};
        const Vector_<> flatRates{0.04, 0.04};
        const auto domesticCurve = DiscountPWLFNew("usd_ois", "USD", curveKnots, flatRates);
        const auto foreignCurve = DiscountPWLFNew("eur_ois", "EUR", curveKnots, flatRates);
        const Handle_<StorableCurveBlock_> domestic(new StorableCurveBlock_(CurveBlockNew(domesticCurve, DayBasis_New("ACT_365F"))));
        const Handle_<StorableCurveBlock_> foreign(new StorableCurveBlock_(CurveBlockNew(foreignCurve, DayBasis_New("ACT_360"))));

        const auto instrument = CrossCurrencySwapNew(Today(), Today(), maturity, 0.01, Pair()->val_, 100.0, 100.0 / 1.10,
                                                     RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_365F")),
                                                     RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360"), CollateralType_OIS()),
                                                     RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_360")),
                                                     RateIndexConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_360"), CollateralType_OIS()));
        Vector_<Handle_<Storable_>> instruments{
            Handle_<Storable_>(new StorableCrossCurrencySwap_(instrument)),
        };

        Handle_<StorableCrossCurrencyCalibrationResult_> result;
        Calibrate_XccyMarket(Today(), "USD", "EUR", domestic, foreign, instruments, {maturity}, settings, &result);
        return result;
    }

    Handle_<StorableCrossCurrencyCalibrationResult_> StagedResult() { return StagedResult(StagedSettings()); }
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

    const Dal::Cell_ fixingTime(Dal::DateTime_(Today(), 11, 0));
    ASSERT_THROW(MarketFixingSnapshot_New({"USD-SOFR-3M", "USD-SOFR-3M"}, {fixingTime, fixingTime}, {0.04, 0.041}, &snapshot), Dal::Exception_);
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

    for (const char* attribute :
         {"fxForwards", "marketRates", "modelRates", "residuals", "jacobian", "effJacobianInverse", "parameterRanges", "residualRanges"}) {
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
                                      "jacobian", "effJacobianInverse", "parameterRanges", "residualRanges"})
            ASSERT_NE(message.find(attribute), std::string::npos) << attribute << ": " << message;
    }
}

TEST(ExcelApiTest, TestUnknownJointSettingsKeyListsEveryAcceptedKey) {
    Dal::Matrix_<Dal::Cell_> settings(1, 2);
    settings(0, 0) = Dal::Cell_("bogusKey");
    settings(0, 1) = Dal::Cell_(1.0);
    try {
        JointResult(settings);
        FAIL() << "Expected an unknown joint settings key to fail";
    } catch (const Dal::Exception_& exception) {
        const std::string message(exception.what());
        // the settings dictionary stores condensed (uppercase) keys, as in Dictionary_::At errors
        ASSERT_NE(message.find("BOGUSKEY"), std::string::npos) << message;
        for (const char* key : {"domesticCurveName", "basisSmoothingWeight", "solveMode", "jacobianMode", "computeForwardJacobian"})
            ASSERT_NE(message.find(key), std::string::npos) << key << ": " << message;
    }
}

TEST(ExcelApiTest, TestStagedCalibrationExposesFrozenSensitivityViews) {
    const auto result = StagedResult();
    ASSERT_TRUE(result);

    Matrix_<Cell_> instrumentNames;
    Matrix_<Cell_> knotDates;
    Matrix_<Cell_> jacobian;
    Matrix_<Cell_> inverse;
    XccyCalibrationResult_Get(result, "instrumentNames", &instrumentNames);
    XccyCalibrationResult_Get(result, "parameterKnotDates", &knotDates);
    XccyCalibrationResult_Get(result, "jacobian", &jacobian);
    XccyCalibrationResult_Get(result, "effJacobianInverse", &inverse);
    ASSERT_EQ(instrumentNames.Rows(), 1);
    ASSERT_EQ(instrumentNames.Cols(), 1);
    ASSERT_TRUE(Cell::IsString(instrumentNames(0, 0)));
    ASSERT_EQ(knotDates.Rows(), 1);
    ASSERT_EQ(Cell::ToDate(knotDates(0, 0)), Today().AddDays(2 * 365));
    ASSERT_EQ(jacobian.Rows(), instrumentNames.Rows());
    ASSERT_EQ(jacobian.Cols(), knotDates.Rows());
    ASSERT_EQ(inverse.Rows(), knotDates.Rows());
    ASSERT_EQ(inverse.Cols(), instrumentNames.Rows());

    const std::pair<const char*, const char*> textViews[] = {
        {"jacobianAvailability", "available"},
        {"effJacobianInverseAvailability", "available"},
        {"jacobianScaling", "unscaled"},
        {"effJacobianInverseScaling", "solver_scaled"},
    };
    for (const auto& view : textViews) {
        Matrix_<Cell_> value;
        XccyCalibrationResult_Get(result, view.first, &value);
        ASSERT_EQ(value.Rows(), 1);
        ASSERT_EQ(value.Cols(), 1);
        ASSERT_EQ(Cell::ToString(value(0, 0)), String_(view.second));
    }

    Matrix_<Cell_> tolerance;
    XccyCalibrationResult_Get(result, "residualTolerance", &tolerance);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(tolerance(0, 0)), 1.0e-8);
}

TEST(ExcelApiTest, TestStagedAvailabilitySeparatesRequestFlagsAndMode) {
    const auto disabled = StagedResult(StagedSettings({
        {"computeForwardJacobian", Cell_(false)},
        {"computeEffJacobianInverse", Cell_(false)},
    }));
    for (const char* attribute : {"jacobianAvailability", "effJacobianInverseAvailability"}) {
        Matrix_<Cell_> value;
        XccyCalibrationResult_Get(disabled, attribute, &value);
        ASSERT_EQ(Cell::ToString(value(0, 0)), String_("not_requested"));
    }

    Matrix_<Cell_> matrix;
    XccyCalibrationResult_Get(disabled, "jacobian", &matrix);
    ASSERT_TRUE(matrix.Empty());
    XccyCalibrationResult_Get(disabled, "effJacobianInverse", &matrix);
    ASSERT_TRUE(matrix.Empty());

    const auto bumped = StagedResult(StagedSettings({{"jacobianMode", Cell_("BUMPED")}}));
    Matrix_<Cell_> forwardAvailability;
    Matrix_<Cell_> inverseAvailability;
    XccyCalibrationResult_Get(bumped, "jacobianAvailability", &forwardAvailability);
    XccyCalibrationResult_Get(bumped, "effJacobianInverseAvailability", &inverseAvailability);
    ASSERT_EQ(Cell::ToString(forwardAvailability(0, 0)), String_("not_available_for_mode"));
    ASSERT_EQ(Cell::ToString(inverseAvailability(0, 0)), String_("available"));
}

TEST(ExcelApiTest, TestNewStagedSettingsRejectWrongTypesWithoutChangingLegacyLooseKeys) {
    ASSERT_NO_THROW(StagedResult(StagedSettings({{"maxEvaluations", Cell_("ignored-as-before")}})));

    for (const auto& setting : Vector_<std::pair<String_, Cell_>>{
             {"jacobianMode", Cell_(1.0)},
             {"computeForwardJacobian", Cell_("false")},
             {"computeEffJacobianInverse", Cell_("false")},
         }) {
        try {
            StagedResult(StagedSettings({setting}));
            FAIL() << "Expected wrong-typed staged setting to fail: " << setting.first;
        } catch (const Exception_& exception) {
            const std::string message(exception.what());
            const char* normalizedKey = setting.first == "jacobianMode"
                                            ? "JACOBIANMODE"
                                            : (setting.first == "computeForwardJacobian" ? "COMPUTEFORWARDJACOBIAN" : "COMPUTEEFFJACOBIANINVERSE");
            ASSERT_NE(message.find(normalizedKey), std::string::npos) << message;
            ASSERT_NE(message.find("received"), std::string::npos) << message;
            ASSERT_NE(message.find(setting.first == "jacobianMode" ? "number" : "string"), std::string::npos) << message;
            ASSERT_NE(message.find(setting.first == "jacobianMode" ? "string" : "boolean"), std::string::npos) << message;
        }
    }
}

TEST(ExcelApiTest, TestUnknownStagedSettingsAndViewsListNewContractSurface) {
    try {
        StagedResult(StagedSettings({{"bogusKey", Cell_(1.0)}}));
        FAIL() << "Expected an unknown staged setting to fail";
    } catch (const Exception_& exception) {
        const std::string message(exception.what());
        for (const char* key : {"jacobianMode", "computeForwardJacobian", "computeEffJacobianInverse"})
            ASSERT_NE(message.find(key), std::string::npos) << key << ": " << message;
    }

    try {
        Matrix_<Cell_> value;
        XccyCalibrationResult_Get(StagedResult(), "unknown", &value);
        FAIL() << "Expected an unknown staged result view to fail";
    } catch (const Exception_& exception) {
        const std::string message(exception.what());
        for (const char* attribute : {"instrumentNames", "parameterKnotDates", "jacobian", "effJacobianInverse", "jacobianAvailability",
                                      "effJacobianInverseAvailability", "residualTolerance", "jacobianScaling", "effJacobianInverseScaling"})
            ASSERT_NE(message.find(attribute), std::string::npos) << attribute << ": " << message;
    }
}

#endif // defined(_WIN32) || defined(DAL_EXCEL_API_TESTS_PORTABLE)
