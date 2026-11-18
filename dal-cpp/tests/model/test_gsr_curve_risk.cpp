#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include <dal/curve/calibration.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/model/gsrcurverisk.hpp>
#include <dal/model/gsrmarketcalibration.hpp>
#include <dal/model/gsrslvcalibration.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;

namespace {
    struct CurveInput_ {
        CurveCalibrationSpec_ spec_;
        CurveCalibrationOptions_ options_;
        CurveCalibrationResult_ result_;
        RatePricingMarket_ market_;
        RateQuoteRiskProvenanceConfig_ config_;

        explicit CurveInput_(double rate = 0.03, int bumpedQuote = -1, double bump = 0.0, bool inverse = true) {
            spec_.today_ = Date_(2026, 10, 2);
            spec_.ccy_ = "USD";
            spec_.curveName_ = "rates";
            spec_.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
            spec_.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
            spec_.solveMode_ = CurveSolveMode_::Value_::EXACT;
            spec_.targetCollateral_ = CollateralType_("OIS");
            spec_.liborBasis_ = DayBasis_("ACT_365F");
            spec_.tolerance_ = 1e-10;
            spec_.initialGuess_ = 0.025;
            spec_.knotDates_ = {spec_.today_};
            RateIndexConvention_ index;
            index.forecastTenor_ = PeriodLength_("12M");
            index.dayBasis_ = spec_.liborBasis_;
            index.businessDayConvention_ = BizDayConvention_("Unadjusted");
            index.fixingHolidays_ = index.accrualHolidays_ = Holidays::None();
            index.collateral_ = spec_.targetCollateral_;
            for (int year = 1; year <= 2; ++year) {
                const auto maturity = spec_.today_.AddDays(year * 365);
                spec_.knotDates_.push_back(maturity);
                spec_.instruments_.push_back(Handle_<YCInstrument_>(new Deposit_(
                    spec_.today_, spec_.today_, maturity, std::expm1(rate * year) / year + (year - 1 == bumpedQuote ? bump : 0.0), index)));
            }
            options_.computeEffJacobianInverse_ = inverse;
            result_ = CalibrateYieldCurve(spec_, options_);
            market_.valuationTime_ = DateTime_(spec_.today_, 9, 0);
            market_.resultCurrency_ = Ccy_("USD");
            const std::shared_ptr<const DiscountCurve_> alias(std::shared_ptr<void>(), result_.curve_.get());
            market_.curveComponents_["discount"] = Handle_<DiscountCurve_>(alias);
            config_.calibrationId_ = "curve-fit";
            config_.componentKeyByParameterBlock_[spec_.curveName_] = "discount";
        }

        GSRCurveData_ Snapshot() const {
            Vector_<Date_> nodes{spec_.today_, spec_.today_.AddDays(365), spec_.today_.AddDays(730)};
            Vector_<> logs;
            for (const auto& node : nodes)
                logs.push_back(std::log((*result_.curve_)(spec_.today_, node)));
            return GSRCurveData_("curve", spec_.today_, "USD", nodes, logs, {}, Matrix_<>(0, 0));
        }

        RateQuoteRiskProvenance_ Provenance() const { return BuildSingleCurveQuoteRiskProvenance(spec_, result_, options_, market_, config_); }
    };

    template <class F_> void AssertUnsupportedSource(F_ operation, const char* expected, const String_& actual) {
        try {
            operation();
            FAIL() << "Unsupported quote-risk source accepted";
        } catch (const Exception_& error) {
            const std::string message(error.what());
            ASSERT_NE(message.find("QUOTE_RISK_STATE_SCHEME_NOT_SUPPORTED: field=gsr.curve.source"), std::string::npos);
            ASSERT_NE(message.find(std::string("expected=") + expected), std::string::npos);
            ASSERT_NE(message.find("actual=" + std::string(actual.data(), actual.size())), std::string::npos);
        }
    }

    void AssertSameJacobian(const GSRCurveQuoteRisk_& actual, const GSRCurveQuoteRisk_& expected) {
        const auto& lhs = actual.LogDFQuoteJacobian();
        const auto& rhs = expected.LogDFQuoteJacobian();
        ASSERT_EQ(lhs.Rows(), rhs.Rows());
        ASSERT_EQ(lhs.Cols(), rhs.Cols());
        for (int row = 0; row < lhs.Rows(); ++row)
            for (int col = 0; col < lhs.Cols(); ++col)
                ASSERT_EQ(lhs(row, col), rhs(row, col));
    }

    void AssertRejectedKind(const char* kind, const char* expected) {
        const CurveInput_ input;
        const auto snapshot = input.Snapshot();
        const auto source = input.Provenance();
        const auto reference = BuildGSRCurveQuoteRisk(snapshot, input.market_, source, "discount");
        const auto original = source.Kind();
        const_cast<String_&>(source.Kind()) = kind;
        ASSERT_NO_FATAL_FAILURE(
            AssertUnsupportedSource([&] { BuildGSRCurveQuoteRisk(snapshot, input.market_, source, "discount"); }, expected, source.State().scheme_));
        auto missing = input.market_;
        missing.curveComponents_.clear();
        ASSERT_NO_FATAL_FAILURE(
            AssertUnsupportedSource([&] { BuildGSRCurveQuoteRisk(snapshot, missing, source, "discount"); }, expected, source.State().scheme_));
        const_cast<String_&>(source.Kind()) = original;
        ASSERT_NO_FATAL_FAILURE(AssertSameJacobian(BuildGSRCurveQuoteRisk(snapshot, input.market_, source, "discount"), reference));
    }

    Handle_<GSRSLVModelData_> Smile(const GSRCurveData_& curve) {
        const Handle_<GSRCurveData_> snapshot(new GSRCurveData_(curve.name_, curve.evaluationDate_, curve.currency_, curve.nodeDates_,
                                                                curve.discountLogDF_, curve.projectionTenors_, curve.projectionLogDF_));
        MultiFactorGSRVolSettings_ vol;
        vol.factorNames_ = {"level"};
        vol.gKnotDates_ = vol.hKnotDates_ = {curve.evaluationDate_};
        vol.gValues_ = Matrix_<>(1, 1, 0.02);
        vol.hValues_ = vol.correlations_ = Matrix_<>(1, 1, 1.0);
        const Handle_<MultiFactorGSRModelData_> gaussian(
            new MultiFactorGSRModelData_("rates", snapshot, Handle_<MultiFactorGSRVolData_>(new MultiFactorGSRVolData_("vol", vol))));
        GSRSLVSettings_ settings;
        settings.maxStep_ = 0.25;
        return Handle_<GSRSLVModelData_>(new GSRSLVModelData_(
            "smile", gaussian, Handle_<GSRLeverageData_>(new GSRLeverageData_("leverage", {0.0}, {0.0}, Matrix_<>(1, 1, 1.0))), settings));
    }
} // namespace

TEST(GSRCurveRiskTest, TestRejectsCaseMismatchedKindBeforeComponentWork) {
    AssertRejectedKind("single_curve", "UNSUPPORTED_KIND");
}

TEST(GSRCurveRiskTest, TestRejectsJointKindWithSingleCurveState) {
    AssertRejectedKind("JOINT_MULTI_CURVE", "dal.quote-risk-state/4+jcs+sha256");
}

TEST(GSRCurveRiskTest, TestRejectsUnknownKindBeforeComponentWork) {
    AssertRejectedKind("UNKNOWN", "UNSUPPORTED_KIND");
}

TEST(GSRCurveRiskTest, TestRejectsHistoricalOrMismatchedStateAndRecovers) {
    const CurveInput_ input;
    const auto snapshot = input.Snapshot();
    const auto source = input.Provenance();
    const auto reference = BuildGSRCurveQuoteRisk(snapshot, input.market_, source, "discount");
    auto& state = const_cast<RateQuoteRiskState_&>(source.State());
    const auto original = state.scheme_;
    for (const String_& scheme : Vector_<String_>{"dal.quote-risk-state/1+jcs+sha256", "dal.quote-risk-state/2+jcs+sha256",
                                                  "dal.quote-risk-state/4+jcs+sha256", "unknown/99"}) {
        state.scheme_ = scheme;
        ASSERT_NO_FATAL_FAILURE(AssertUnsupportedSource([&] { BuildGSRCurveQuoteRisk(snapshot, input.market_, source, "discount"); },
                                                        "dal.quote-risk-state/3+jcs+sha256", scheme));
        state.scheme_ = original;
        ASSERT_NO_FATAL_FAILURE(AssertSameJacobian(BuildGSRCurveQuoteRisk(snapshot, input.market_, source, "discount"), reference));
    }
}

TEST(GSRCurveRiskTest, TestSubsecondValuationAndFixingChangesRejectStaleSourceAndRecover) {
    CurveInput_ input;
    const auto snapshot = input.Snapshot();
    input.market_.valuationTime_ = DateTime_(input.spec_.today_, .1 / 86400.);
    input.market_.fixings_.reset(new MarketFixingSnapshot_({{"EQ[UNUSED]", {{DateTime_(input.spec_.today_.AddDays(-1), .1 / 86400.), 99.}}}}));
    const auto source = input.Provenance();
    const auto reference = BuildGSRCurveQuoteRisk(snapshot, input.market_, source, "discount");
    for (bool fixing : {false, true}) {
        auto after = input.market_;
        if (fixing)
            after.fixings_.reset(new MarketFixingSnapshot_({{"EQ[UNUSED]", {{DateTime_(input.spec_.today_.AddDays(-1), .2 / 86400.), 99.}}}}));
        else
            after.valuationTime_ = DateTime_(input.spec_.today_, .2 / 86400.);
        ASSERT_THROW(BuildGSRCurveQuoteRisk(snapshot, after, source, "discount"), Exception_);
        ASSERT_NO_FATAL_FAILURE(AssertSameJacobian(BuildGSRCurveQuoteRisk(snapshot, input.market_, source, "discount"), reference));
    }
}

TEST(GSRCurveRiskTest, TestSnapshotQuoteJacobianMatchesIndependentDepositFormula) {
    const CurveInput_ input;
    const auto snapshot = input.Snapshot();
    const auto bridge = BuildGSRCurveQuoteRisk(snapshot, input.market_, input.Provenance(), "discount");
    ASSERT_EQ(bridge.QuoteNames().size(), 2U);
    ASSERT_EQ(bridge.LogDFQuoteJacobian().Rows(), 3);
    ASSERT_EQ(bridge.LogDFQuoteJacobian().Cols(), 2);
    for (int col = 0; col < 2; ++col) {
        const double quote = input.result_.diagnostics_.marketRates_[col];
        const int quoteYear = std::abs(quote - std::expm1(0.03)) < 1e-9 ? 1 : 2;
        ASSERT_NEAR(quote, std::expm1(0.03 * quoteYear) / quoteYear, 1e-9);
        for (int row = 0; row < 3; ++row) {
            const double expected = row == quoteYear ? -row * std::exp(-0.03 * row) : 0.0;
            ASSERT_NEAR(bridge.LogDFQuoteJacobian()(row, col), expected, 1e-7) << "row=" << row << " col=" << col;
        }
        const auto shifted = bridge.Shifted(col, 1e-5);
        ASSERT_NEAR(shifted->discountLogDF_[quoteYear] - snapshot.discountLogDF_[quoteYear], 1e-5 * bridge.LogDFQuoteJacobian()(quoteYear, col),
                    1e-14);
    }
}

TEST(GSRCurveRiskTest, TestRejectsStaleProvenanceAndMismatchedSnapshot) {
    CurveInput_ input;
    const auto provenance = input.Provenance();
    const auto snapshot = input.Snapshot();
    auto logs = snapshot.discountLogDF_;
    logs[1] += 1e-4;
    const GSRCurveData_ bad("bad", snapshot.evaluationDate_, snapshot.currency_, snapshot.nodeDates_, logs, {}, Matrix_<>(0, 0));
    ASSERT_THROW(BuildGSRCurveQuoteRisk(bad, input.market_, provenance, "discount"), Exception_);
    ASSERT_THROW(BuildGSRCurveQuoteRisk(snapshot, input.market_, provenance, "missing"), Exception_);
    ASSERT_THROW(BuildGSRCurveQuoteRisk(snapshot, input.market_, provenance, "discount", {"discount"}), Exception_);
    input.market_.valuationTime_ = DateTime_(input.spec_.today_, 10, 0);
    ASSERT_THROW(BuildGSRCurveQuoteRisk(snapshot, input.market_, provenance, "discount"), Exception_);
}

TEST(GSRCurveRiskTest, TestProjectionRowsAndUnavailableInverse) {
    const CurveInput_ input;
    const auto snapshot = input.Snapshot();
    Matrix_<> projections(2, 3);
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 3; ++col)
            projections(row, col) = snapshot.discountLogDF_[col];
    const GSRCurveData_ projected("curve", snapshot.evaluationDate_, "USD", snapshot.nodeDates_, snapshot.discountLogDF_, {"6M", "12M"}, projections);
    const auto bridge = BuildGSRCurveQuoteRisk(projected, input.market_, input.Provenance(), "discount", {"discount", "discount"});
    ASSERT_EQ(bridge.LogDFQuoteJacobian().Rows(), 9);
    for (int row = 0; row < 9; ++row)
        for (int col = 0; col < 2; ++col)
            ASSERT_NEAR(bridge.LogDFQuoteJacobian()(row, col), bridge.LogDFQuoteJacobian()(row % 3, col), 1e-12);
    const CurveInput_ unavailable(0.03, -1, 0.0, false);
    ASSERT_FALSE(unavailable.Provenance().Available());
    ASSERT_THROW(BuildGSRCurveQuoteRisk(unavailable.Snapshot(), unavailable.market_, unavailable.Provenance(), "discount"), Exception_);
}

TEST(GSRCurveRiskTest, TestCurveAndSmileRecalibrationMatchIndependentQuoteBumps) {
    const CurveInput_ base;
    const auto snapshot = base.Snapshot();
    const auto initial = Smile(snapshot);
    const auto bridge = BuildGSRCurveQuoteRisk(snapshot, base.market_, base.Provenance(), "discount");
    const BondOption_ quoted{snapshot.nodeDates_[1], snapshot.nodeDates_[2], 0.97, OptionType_("CALL")};
    const BondOption_ target{quoted.expiry_, quoted.maturity_, 0.95, OptionType_("CALL")};
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_ = {1024, 1729};
    settings.validation_ = {1024, 81173};
    settings.solver_.priorWeight_ = 2.0;
    settings.solver_.gradientTolerance_ = 1e-8;
    const double price = PriceGSRSLVEuropeanOptions(*initial, {quoted}, settings.pricing_)[0].price_;
    const Vector_<CalibrationQuote_> quotes{{"bond", quoted, 1.1 * price, 0.005}};
    const Vector_<GSRSLVCalibrationParameter_> parameters{{"leverage:0:0", 0.2, 2.0, 1.0}};
    const auto risk = GSRSLVQuoteRisk(*initial, quotes, parameters, {target}, settings, {}, &bridge);
    ASSERT_TRUE(risk.curveRiskIncluded_);
    ASSERT_EQ(risk.sensitivities_.Cols(), 3);
    const CurveInput_ up(0.03, 0, 5e-7), down(0.03, 0, -5e-7);
    const auto high = CalibrateGSRSLV(*Smile(up.Snapshot()), quotes, parameters, settings);
    const auto low = CalibrateGSRSLV(*Smile(down.Snapshot()), quotes, parameters, settings);
    ASSERT_TRUE(high.converged_);
    ASSERT_TRUE(low.converged_);
    const double reference = (PriceGSRSLVEuropeanOptions(*high.model_, {target}, settings.pricing_)[0].price_ -
                              PriceGSRSLVEuropeanOptions(*low.model_, {target}, settings.pricing_)[0].price_) /
                             1e-6;
    ASSERT_GT(std::abs(reference), 1e-3);
    ASSERT_NEAR(risk.sensitivities_(0, 1), reference, 2e-4);
    ASSERT_TRUE(risk.stable_[1]);
}

TEST(GSRCurveRiskTest, TestMarketVolatilityRiskRepricesQuotesOnShiftedCurves) {
    const CurveInput_ base;
    const auto snapshot = base.Snapshot();
    const auto initial = Smile(snapshot);
    const auto bridge = BuildGSRCurveQuoteRisk(snapshot, base.market_, base.Provenance(), "discount");
    const auto expiry = snapshot.nodeDates_[1], end = snapshot.nodeDates_[2];
    const Caplet_ option{expiry, expiry, end, end, 1.0, 1.0, "12M", 0.03, OptionType_("CALL")};
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_ = {512, 1729};
    settings.validation_ = {512, 81173};
    settings.solver_.priorWeight_ = 0.5;
    settings.solver_.gradientTolerance_ = 1e-8;
    const Vector_<VolQuote_> quotes{{"normal", option, 0.02, 0.005}};
    const Vector_<GSRSLVCalibrationParameter_> parameters{{"leverage:0:0", 0.2, 2.0, 1.0}};
    const auto risk = GSRSLVMarketQuoteRisk(*initial, quotes, parameters, {option}, settings, {}, &bridge);
    ASSERT_EQ(risk.quoteUnits_[0], "NORMAL_VOL");
    ASSERT_EQ(risk.sensitivities_.Cols(), 3);
    for (int coordinate = 0; coordinate <= 1; ++coordinate) {
        auto upQuotes = quotes, downQuotes = quotes;
        Handle_<GSRSLVModelData_> upModel = initial, downModel = initial;
        const double step = coordinate == 0 ? 1e-4 : 5e-7;
        if (coordinate == 0) {
            upQuotes[0].volatility_ += step;
            downQuotes[0].volatility_ -= step;
        } else {
            upModel = Smile(CurveInput_(0.03, 0, step).Snapshot());
            downModel = Smile(CurveInput_(0.03, 0, -step).Snapshot());
        }
        const auto high = CalibrateGSRSLVMarket(*upModel, upQuotes, parameters, settings),
                   low = CalibrateGSRSLVMarket(*downModel, downQuotes, parameters, settings);
        ASSERT_TRUE(high.converged_);
        ASSERT_TRUE(low.converged_);
        const double reference = (PriceGSRSLVEuropeanOptions(*high.model_, {option}, settings.pricing_)[0].price_ -
                                  PriceGSRSLVEuropeanOptions(*low.model_, {option}, settings.pricing_)[0].price_) /
                                 (2.0 * step);
        ASSERT_NEAR(risk.sensitivities_(0, coordinate), reference, 2e-4);
        ASSERT_TRUE(risk.stable_[coordinate]);
    }
}
