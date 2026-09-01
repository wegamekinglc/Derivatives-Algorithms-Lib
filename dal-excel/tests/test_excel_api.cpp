//
// Created by wegam on 2026/5/30.
//

#if defined(_WIN32) || defined(DAL_EXCEL_API_TESTS_PORTABLE)

#include <gtest/gtest.h>

#include <cmath>

#include <dal-excel/src/__curve_storable.hpp>
#include <dal-excel/src/__curvepricing_test_api.hpp>
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

    Handle_<StorableRatePricingMarket_>
    JointPricingMarket(const Handle_<StorableJointXccyCalibrationResult_>& result, Vector_<String_>* blockKeys, Vector_<String_>* componentKeys) {
        const CollateralType_ ois(CollateralType_::Value_::OIS);
        const Vector_<Handle_<DiscountCurve_>> curves = {
            result->domesticBlock_->DiscountCurves().at(ois),
            result->foreignBlock_->DiscountCurves().at(ois),
            result->basisCurve_,
        };
        REQUIRE(curves.size() == result->val_.parameterRanges_.size(), "Unexpected joint XCCY parameter range count");
        RatePricingMarket_ market;
        market.valuationTime_ = result->spec_.valuationTime_;
        market.resultCurrency_ = result->spec_.pair_.domestic_;
        market.fixings_ = result->val_.fixings_;
        for (int index = 0; index < static_cast<int>(curves.size()); ++index) {
            blockKeys->push_back(result->val_.parameterRanges_[index].name_);
            componentKeys->push_back("joint-component-" + String::FromInt(index));
            market.curveComponents_[componentKeys->back()] = curves[index];
        }
        auto xccy = std::make_shared<CrossCurrencyMarket_>(result->domesticBlock_, result->foreignBlock_, result->spec_.fxSpot_,
                                                          result->spec_.valuationTime_, result->spec_.collateralCurrency_, result->val_.fixings_);
        xccy->SetBasisCurve(result->basisCurve_);
        market.xccyMarket_ = xccy;
        return Handle_<StorableRatePricingMarket_>(new StorableRatePricingMarket_(market));
    }

    Handle_<StorableRatePricingMarket_>
    StagedPricingMarket(const Handle_<StorableCrossCurrencyCalibrationResult_>& result, Vector_<String_>* blockKeys, Vector_<String_>* componentKeys) {
        blockKeys->push_back(String_("basis:xccy_basis_") + result->spec_.basisPair_.domestic_.String());
        componentKeys->push_back("staged-basis-component");
        RatePricingMarket_ market;
        market.valuationTime_ = result->spec_.valuationTime_.IsValid() ? result->spec_.valuationTime_ : DateTime_(result->spec_.today_);
        market.resultCurrency_ = result->spec_.basisPair_.domestic_;
        market.curveComponents_[componentKeys->front()] = result->basisCurve_;
        market.fixings_ = result->val_.market_.Fixings();
        market.xccyMarket_ = std::make_shared<CrossCurrencyMarket_>(result->val_.market_);
        return Handle_<StorableRatePricingMarket_>(new StorableRatePricingMarket_(market));
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
    ASSERT_EQ(domestic->val_, result->domesticBlock_);
    ASSERT_EQ(foreign->val_, result->foreignBlock_);
    ASSERT_EQ(basis->val_, result->basisCurve_);

    const auto& calibration = result->val_;
    const char* const attributes[] = {"FxFoRwArDs", "mArKeTrAtEs", "MoDeLrAtEs", "rEsIdUaLs", "JaCoBiAn", "eFfJaCoBiAnInVeRsE",
                                      "PaRaMeTeRrAnGeS", "rEsIdUaLrAnGeS"};
    const int rows[] = {
        static_cast<int>(JointXccyResultFxForwards(calibration).dates_.size()),
        static_cast<int>(JointXccyResultMarketRates(calibration).size()),
        static_cast<int>(JointXccyResultModelRates(calibration).size()),
        static_cast<int>(JointXccyResultResiduals(calibration).size()),
        JointXccyResultJacobian(calibration).Rows(),
        JointXccyResultEffJacobianInverse(calibration).Rows(),
        static_cast<int>(JointXccyResultParameterRanges(calibration).size()),
        static_cast<int>(JointXccyResultResidualRanges(calibration).size()),
    };
    const int cols[] = {
        2,
        1,
        1,
        1,
        JointXccyResultJacobian(calibration).Cols(),
        JointXccyResultEffJacobianInverse(calibration).Cols(),
        3,
        3,
    };
    Matrix_<Cell_> values[8];
    for (int i = 0; i < 8; ++i) {
        const char* const attribute = attributes[i];
        auto& value = values[i];
        JointXccyCalibrationResult_Get(result, attribute, &value);
        ASSERT_EQ(value.Rows(), rows[i]) << attribute;
        ASSERT_EQ(value.Cols(), cols[i]) << attribute;
    }

    const auto& forwards = JointXccyResultFxForwards(calibration);
    for (int i = 0; i < forwards.dates_.size(); ++i) {
        ASSERT_EQ(Cell::ToDate(values[0](i, 0)), forwards.dates_[i]);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(values[0](i, 1)), forwards.forwards_[i]);
    }
    const Vector_<>* const vectors[] = {
        &JointXccyResultMarketRates(calibration),
        &JointXccyResultModelRates(calibration),
        &JointXccyResultResiduals(calibration),
    };
    for (int view = 0; view < 3; ++view)
        for (int row = 0; row < vectors[view]->size(); ++row)
            ASSERT_DOUBLE_EQ(Cell::ToDouble(values[view + 1](row, 0)), (*vectors[view])[row]);
    const Matrix_<>* const matrices[] = {
        &JointXccyResultJacobian(calibration),
        &JointXccyResultEffJacobianInverse(calibration),
    };
    for (int view = 0; view < 2; ++view)
        for (int row = 0; row < matrices[view]->Rows(); ++row)
            for (int col = 0; col < matrices[view]->Cols(); ++col)
                ASSERT_DOUBLE_EQ(Cell::ToDouble(values[view + 4](row, col)), (*matrices[view])(row, col));
    const Vector_<CalibrationBlockRange_>* const ranges[] = {
        &JointXccyResultParameterRanges(calibration),
        &JointXccyResultResidualRanges(calibration),
    };
    for (int view = 0; view < 2; ++view)
        for (int row = 0; row < ranges[view]->size(); ++row) {
            ASSERT_EQ(Cell::ToString(values[view + 6](row, 0)), (*ranges[view])[row].name_);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(values[view + 6](row, 1)), (*ranges[view])[row].offset_);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(values[view + 6](row, 2)), (*ranges[view])[row].size_);
        }
}

TEST(ExcelApiTest, TestUnknownJointResultViewListsEveryAcceptedAttribute) {
    const auto result = JointResult();
    for (const char* attribute : {"unknown", "DOMESTICBLOCK", "FOREIGNBLOCK", "BASISCURVE"}) {
        try {
            Dal::Matrix_<Dal::Cell_> value;
            JointXccyCalibrationResult_Get(result, attribute, &value);
            FAIL() << "Expected an unknown joint result attribute to fail";
        } catch (const Dal::Exception_& exception) {
            const std::string message(exception.what());
            const std::string expected =
                std::string("Unknown joint XCCY calibration attribute: ") + attribute +
                " (accepted views: domesticBlock, foreignBlock, basisCurve, fxForwards, marketRates, modelRates, residuals, jacobian, "
                "effJacobianInverse, parameterRanges, residualRanges; use the dedicated DOMESTICBLOCK, FOREIGNBLOCK, and BASISCURVE getter "
                "functions for handle views)";
            ASSERT_GE(message.size(), expected.size());
            ASSERT_EQ(message.substr(message.size() - expected.size()), expected);
        }
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

    Matrix_<Cell_> marketRates;
    Matrix_<Cell_> modelRates;
    Matrix_<Cell_> residuals;
    Matrix_<Cell_> maxAbsResidual;
    Matrix_<Cell_> rmsResidual;
    Matrix_<Cell_> instrumentNames;
    Matrix_<Cell_> knotDates;
    Matrix_<Cell_> jacobian;
    Matrix_<Cell_> inverse;
    XccyCalibrationResult_Get(result, "marketRates", &marketRates);
    XccyCalibrationResult_Get(result, "modelRates", &modelRates);
    XccyCalibrationResult_Get(result, "residuals", &residuals);
    XccyCalibrationResult_Get(result, "maxAbsResidual", &maxAbsResidual);
    XccyCalibrationResult_Get(result, "rmsResidual", &rmsResidual);
    XccyCalibrationResult_Get(result, "instrumentNames", &instrumentNames);
    XccyCalibrationResult_Get(result, "parameterKnotDates", &knotDates);
    XccyCalibrationResult_Get(result, "jacobian", &jacobian);
    XccyCalibrationResult_Get(result, "effJacobianInverse", &inverse);
    ASSERT_EQ(marketRates.Rows(), 1);
    ASSERT_EQ(marketRates.Cols(), 1);
    ASSERT_EQ(modelRates.Rows(), 1);
    ASSERT_EQ(modelRates.Cols(), 1);
    ASSERT_EQ(residuals.Rows(), 1);
    ASSERT_EQ(residuals.Cols(), 1);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(marketRates(0, 0)), 0.01);
    const double modelRate = Cell::ToDouble(modelRates(0, 0));
    const double residual = Cell::ToDouble(residuals(0, 0));
    ASSERT_TRUE(std::isfinite(modelRate));
    ASSERT_TRUE(std::isfinite(residual));
    ASSERT_NEAR(modelRate - Cell::ToDouble(marketRates(0, 0)), residual, 1.0e-15);
    ASSERT_EQ(maxAbsResidual.Rows(), 1);
    ASSERT_EQ(maxAbsResidual.Cols(), 1);
    ASSERT_EQ(rmsResidual.Rows(), 1);
    ASSERT_EQ(rmsResidual.Cols(), 1);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(maxAbsResidual(0, 0)), std::abs(residual));
    ASSERT_DOUBLE_EQ(Cell::ToDouble(rmsResidual(0, 0)), std::abs(residual));
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

TEST(ExcelApiTest, TestJointAndStagedQuoteRiskProvenanceHandlesUsePublicFactories) {
    {
        const auto result = JointResult();
        Vector_<String_> blockKeys;
        Vector_<String_> componentKeys;
        const auto market = JointPricingMarket(result, &blockKeys, &componentKeys);
        Handle_<StorableRateQuoteRiskProvenance_> provenance;
        JointXccyQuoteRiskProvenance_New(result, "joint", blockKeys, componentKeys, market, &provenance);
        ASSERT_TRUE(provenance->val_->Available());
        ASSERT_EQ(provenance->val_->Kind(), String_("JOINT_XCCY"));
    }
    {
        const auto result = StagedResult();
        Vector_<String_> blockKeys;
        Vector_<String_> componentKeys;
        const auto market = StagedPricingMarket(result, &blockKeys, &componentKeys);
        Handle_<StorableRateQuoteRiskProvenance_> provenance;
        StagedXccyBasisQuoteRiskProvenance_New(result, "staged", blockKeys, componentKeys, market, &provenance);
        ASSERT_TRUE(provenance->val_->Available());
        ASSERT_EQ(provenance->val_->Kind(), String_("STAGED_XCCY_BASIS"));
    }
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
        const std::string expected =
            "Unknown XCCY calibration attribute: unknown (accepted views: marketRates, modelRates, residuals, maxAbsResidual, rmsResidual, "
            "instrumentNames, parameterKnotDates, jacobian, effJacobianInverse, residualTolerance, jacobianScaling, "
            "effJacobianInverseScaling, jacobianAvailability, effJacobianInverseAvailability)";
        ASSERT_GE(message.size(), expected.size());
        ASSERT_EQ(message.substr(message.size() - expected.size()), expected);
        for (const char* attribute : {"instrumentNames", "parameterKnotDates", "jacobian", "effJacobianInverse", "jacobianAvailability",
                                      "effJacobianInverseAvailability", "residualTolerance", "jacobianScaling", "effJacobianInverseScaling"})
            ASSERT_NE(message.find(attribute), std::string::npos) << attribute << ": " << message;
    }
}

#endif // defined(_WIN32) || defined(DAL_EXCEL_API_TESTS_PORTABLE)
