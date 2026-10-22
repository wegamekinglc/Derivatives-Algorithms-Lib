#include <gtest/gtest.h>

#include <cmath>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/curveinstrumentcalibration.hpp>
#include <dal/curve/curvescript.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/script/event.hpp>
#include <dal/time/dateutils.hpp>

using namespace Dal;

namespace {
    const Date_ FAMILY_TODAY(2026, 10, 21), FAMILY_START(2027, 10, 21), FAMILY_END(2028, 10, 21);
    ScriptInstrumentDefinitionSpec_ ExplicitCouponDefinition() {
        ScriptInstrumentDefinitionSpec_ spec;
        const auto price = CalibrationValueUnit_::Parse("Price:USD"), rate = CalibrationValueUnit_::Parse("Rate");
        spec.header_.payCurrency_ = spec.header_.resultCurrency_ = spec.header_.collateralCurrency_ = Ccy_("USD");
        spec.header_.discountRoute_ = "USD.OIS";
        spec.parameters_ = {{"N", ScriptParameterType_::Value_::Real, price, ScriptParameterRole_::Value_::Notional, true},
                            {"K", ScriptParameterType_::Value_::Real, rate, ScriptParameterRole_::Value_::Coupon, true}};
        spec.source_.rows_ = {
            {ScriptSourceRowKind_::Value_::Definition, "N", "100"},
            {ScriptSourceRowKind_::Value_::Definition, "K", "0.025"},
            {ScriptSourceRowKind_::Value_::Event, "2028-10-21", "PAY PAYS N*(K-(1/FIX(IR[USD,DF,2028-10-21,2027-10-21],2027-10-21)-1))"}};
        spec.measures_ = {{"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, price},
                          {"ParRate", CalibrationMeasureKind_::Value_::ParAffineRate, "PAY", String_("K"), rate, 1, 1e-12}};
        return spec;
    }
    CurveInstrumentCalibrationSpec_ FamilySpec(const Handle_<CalibrationInstrument_>& instrument) {
        CurveInstrumentCalibrationSpec_ result;
        result.geometry_.today_ = FAMILY_TODAY;
        result.geometry_.ccy_ = "USD";
        result.geometry_.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        result.geometry_.knotDates_ = {FAMILY_TODAY, FAMILY_START, FAMILY_END};
        result.valuation_.evaluationDate_ = FAMILY_TODAY;
        result.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        result.market_.routes_ = {{"USD.OIS"}};
        const auto& measure = instrument->Definition()->Spec().measures_[1];
        result.observations_ = {
            NewCalibrationObservation("coupon-rate", instrument, measure.id_, NewCalibrationMarketQuote("rate", .04, measure.unit_), 1e-8)};
        return result;
    }
    RateIndexConvention_ FamilyIndex() {
        RateIndexConvention_ index;
        index.dayBasis_ = DayBasis_("ACT_360");
        index.businessDayConvention_ = BizDayConvention_("Unadjusted");
        return index;
    }
    RateLegConvention_ FamilyLeg(int months, const String_& basis = "ACT_360") {
        RateLegConvention_ leg;
        leg.paymentFrequency_ = PeriodLength_(String::FromInt(months) + "M");
        leg.dayBasis_ = DayBasis_(basis);
        leg.businessDayConvention_ = leg.paymentConvention_ = BizDayConvention_("Unadjusted");
        return leg;
    }
    double OracleDiscount(const Date_& date, const Vector_<>& parameters) {
        const double t = date - FAMILY_TODAY, first = FAMILY_START - FAMILY_TODAY, last = FAMILY_END - FAMILY_TODAY;
        const double logDf = t <= first ? parameters[0] * t / first : parameters[0] + (parameters[1] - parameters[0]) * (t - first) / (last - first);
        return std::exp(logDf);
    }
    double OracleLeg(int months, bool floating, double couponBasis, double indexBasis, const Vector_<>& parameters) {
        double result = 0.;
        for (Date_ start = FAMILY_START; start < FAMILY_END;) {
            const Date_ end = std::min(Date::AddMonths(start, months), FAMILY_END);
            const double alpha = (end - start) / couponBasis;
            const double forward =
                floating ? (OracleDiscount(start, parameters) / OracleDiscount(end, parameters) - 1.) / ((end - start) / indexBasis) : 1.;
            result += alpha * forward * OracleDiscount(end, parameters);
            start = end;
        }
        return result;
    }
    template <class D_>
    double
    TreeFixing(const Script::NodeFix_& fixing, const Vector_<>& parameters, const D_& discount, const std::map<int, double>& backgroundForwards) {
        if (const auto* df = dynamic_cast<const Index::DF_*>(fixing.index_.get())) {
            REQUIRE(Cell::IsDate(df->start_) && Cell::IsDate(df->maturity_), "independent tree probe requires explicit DF dates");
            return discount(Cell::ToDate(df->maturity_)) / discount(Cell::ToDate(df->start_));
        }
        const auto* rate = dynamic_cast<const Index::Libor_*>(fixing.index_.get());
        REQUIRE(rate && Cell::IsDate(rate->start_), "independent tree probe requires an explicit Libor start");
        const Date_ start = Cell::ToDate(rate->start_), end = Date::AddMonths(start, rate->tenor_.Period().Months());
        const auto background = backgroundForwards.find(rate->tenor_.Period().Months());
        const double ratio = background == backgroundForwards.end() ? OracleDiscount(start, parameters) / OracleDiscount(end, parameters)
                                                                    : std::exp(background->second * (end - start) / 365.);
        return (ratio - 1.) / ((end - start) / 360.);
    }
    template <class D_>
    void BindTreeFixings(Script::Node_* node,
                         Vector_<Script::ObservationRequest_>* requests,
                         Vector_<>* known,
                         const Vector_<>& parameters,
                         const D_& discount,
                         const std::map<int, double>& backgroundForwards) {
        if (auto* fixing = dynamic_cast<Script::NodeFix_*>(node)) {
            fixing->observationId_ = requests->size();
            requests->push_back(
                {fixing->index_, {fixing->index_->Name(), DateTime_(fixing->fixingDate_.value_or(FAMILY_TODAY), 0.)}, {}, known->size(), {}, true});
            known->push_back(TreeFixing(*fixing, parameters, discount, backgroundForwards));
        }
        for (const auto& child : node->arguments_)
            BindTreeFixings(child.get(), requests, known, parameters, discount, backgroundForwards);
    }
    double OriginalTreePrice(const Handle_<CalibrationInstrument_>& instrument,
                             const Vector_<>& parameters,
                             const std::optional<double>& discountRate = {},
                             const std::map<int, double>& backgroundForwards = {}) {
        const auto discount = [&](const Date_& date) {
            return discountRate ? std::exp(-*discountRate * (date - FAMILY_TODAY) / 365.) : OracleDiscount(date, parameters);
        };
        auto product = instrument->Product();
        product->IndexVariables();
        Vector_<Script::ObservationRequest_> requests;
        Vector_<> known;
        for (const auto& event : product->Events())
            for (const auto& node : event)
                BindTreeFixings(node.get(), &requests, &known, parameters, discount, backgroundForwards);
        const Script::ObservationPlan_ observations(std::move(requests), std::move(known));
        Script::Evaluator_<double> evaluator(Vector_<>(product->VarNames().size(), 0.), product->ConstVarValues(), product->VectorCapacities());
        evaluator.SetObservations(&observations);
        AAD::Scenario_<> path(product->Events().size());
        for (size_t i = 0; i < path.size(); ++i)
            path[i].numeraire_ = 1. / discount(product->EventDates()[i]);
        evaluator.SetScenario(&path);
        evaluator.Init();
        for (size_t i = 0; i < product->Events().size(); ++i) {
            evaluator.SetCurEvt(i);
            for (const auto& node : product->Events()[i])
                node->Accept(evaluator);
        }
        return evaluator.VarVals()[product->PayOffIdx()];
    }
    double NativeRate(const YCInstrument_& native, const Vector_<>& parameters) {
        const Handle_<DiscountCurve_> discount(NewDiscountLogDF("native-oracle", "USD", {FAMILY_TODAY, FAMILY_START, FAMILY_END},
                                                                {0., parameters[0], parameters[1]}, DayBasis::Act365F(), LogDfScheme_("LOG_LINEAR")));
        return (*native.Precompute({}))(CurveBlock_(discount));
    }
    template <class F_>
    void AssertNativeFamily(const YCInstrument_& native, const Handle_<CalibrationInstrument_>& instrument, const F_& oracle, bool spread = false) {
        auto spec = FamilySpec(instrument);
        const auto unit = CalibrationValueUnit_::Parse(spread ? "BasisSpread" : "Rate");
        spec.observations_[0] = NewCalibrationObservation("native-rate", instrument, spread ? "BasisSpread" : "ParRate",
                                                          NewCalibrationMarketQuote("native-q", 0., unit), 1e-8);
        spec.observations_.push_back(NewCalibrationObservation(
            "native-price", instrument, "Price", NewCalibrationMarketQuote("native-p", 0., CalibrationValueUnit_::Parse("Price:USD")), 1.));
        const Vector_<> parameters = {-.03 * (FAMILY_START - FAMILY_TODAY) / 365., -.03 * (FAMILY_END - FAMILY_TODAY) / 365.};
        for (const auto mode : {CalibrationDerivative_::Value_::FiniteDifference, CalibrationDerivative_::Value_::AAD}) {
            CurveInstrumentCalibrationOptions_ options;
            options.jacobianMode_ = mode;
            options.differenceStep_ = 1e-5;
            const auto value = PreparedCurveInstruments_(spec, options).Evaluate(parameters);
            const auto expected = oracle(parameters);
            ASSERT_NEAR(value.modelMeasures_[0], NativeRate(native, parameters), 1e-12);
            ASSERT_NEAR(value.modelMeasures_[0], expected.first, 1e-12);
            ASSERT_NEAR(value.modelMeasures_[1], expected.second, 1e-12);
            ASSERT_NEAR(value.modelMeasures_[1], OriginalTreePrice(instrument, parameters), 1e-12);
            for (size_t j = 0; j < parameters.size(); ++j) {
                auto up = parameters, down = parameters;
                up[j] += 5e-6;
                down[j] -= 5e-6;
                const auto plus = oracle(up), minus = oracle(down);
                ASSERT_NEAR(value.quoteJacobian_(0, static_cast<int>(j)), (plus.first - minus.first) / (up[j] - down[j]), 1e-8);
                ASSERT_NEAR(value.quoteJacobian_(0, static_cast<int>(j)), (NativeRate(native, up) - NativeRate(native, down)) / (up[j] - down[j]),
                            1e-8);
                ASSERT_NEAR(value.quoteJacobian_(1, static_cast<int>(j)), (plus.second - minus.second) / (up[j] - down[j]), 1e-8);
            }
            ::testing::Test::RecordProperty("instrument_identity",
                                            std::string(instrument->InstrumentIdentity().begin(), instrument->InstrumentIdentity().end()));
            ::testing::Test::RecordProperty("plan_" + std::to_string(static_cast<int>(mode)),
                                            std::string(value.planIdentity_.begin(), value.planIdentity_.end()));
        }
        auto fitSpec = FamilySpec(instrument);
        fitSpec.geometry_.knotDates_ = {FAMILY_TODAY, FAMILY_END};
        fitSpec.observations_[0] = NewCalibrationObservation("fit", instrument, spread ? "BasisSpread" : "ParRate",
                                                             NewCalibrationMarketQuote("fit-q", oracle(parameters).first, unit), 1e-10);
        CurveInstrumentCalibrationOptions_ fitOptions;
        fitOptions.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        const auto fit = CalibrateCurveInstruments(fitSpec, fitOptions);
        ASSERT_TRUE(fit.fitWithinTolerance_);
        ASSERT_NEAR(fit.parameters_[0], parameters[1], 1e-8);
    }
} // namespace

TEST(CurveScriptFamilyTest, TestExplicitDfCouponHasRawNativeJacobian) {
    const auto instrument =
        NewScriptCalibrationInstrument({"explicit-coupon", NewScriptInstrumentDefinition(ExplicitCouponDefinition()), {}, FAMILY_END});
    auto spec = FamilySpec(instrument);
    spec.observations_.push_back(NewCalibrationObservation("coupon-price", instrument, "Price",
                                                           NewCalibrationMarketQuote("price", 0., CalibrationValueUnit_::Parse("Price:USD")), 1.));
    for (const auto method : {CalibrationEvaluation_::Value_::AutoFast, CalibrationEvaluation_::Value_::GenericDeterministic}) {
        CurveInstrumentCalibrationOptions_ options;
        options.evaluation_ = method;
        const auto value = PreparedCurveInstruments_(spec, options).Evaluate({-.03, -.07});
        const double fwd = std::expm1(.04), discount = std::exp(-.07);
        ASSERT_NEAR(value.modelMeasures_[0], fwd, 1e-12);
        ASSERT_NEAR(value.modelMeasures_[1], 100. * (.025 - fwd) * discount, 1e-12);
        ASSERT_NEAR(value.quoteJacobian_(0, 0), std::exp(.04), 1e-8);
        ASSERT_NEAR(value.quoteJacobian_(0, 1), -std::exp(.04), 1e-8);
        ASSERT_NEAR(value.quoteJacobian_(1, 0), -100. * std::exp(-.03), 1e-8);
        ASSERT_NEAR(value.quoteJacobian_(1, 1), 102.5 * discount, 1e-8);
    }
}

TEST(CurveScriptFamilyTest, TestAmountMustRetainDeclaredNotionalDimension) {
    std::optional<CalibrationError_> saved;
    {
        auto definition = ExplicitCouponDefinition();
        definition.source_.rows_[2].text_ = "PAY PAYS K";
        const auto instrument = NewScriptCalibrationInstrument({"wrong-dimension", NewScriptInstrumentDefinition(definition), {}, FAMILY_END});
        try {
            static_cast<void>(PreparedCurveInstruments_(FamilySpec(instrument)));
        } catch (const CalibrationError_& error) {
            saved = error;
        }
    }
    ASSERT_TRUE(saved);
    ASSERT_EQ(saved->Failure().diagnostic_.code_, "UnsupportedFastEvaluator");
    ASSERT_EQ(saved->Failure().diagnostic_.sourceRow_, std::optional<int>(3));
    ASSERT_EQ(saved->Failure().diagnostic_.eventDate_, std::optional<Date_>(FAMILY_END));
    ASSERT_EQ(saved->Failure().diagnostic_.statement_, std::optional<int>(0));
    ASSERT_EQ(saved->Failure().report_->Axes().observations_[0]->Id(), "coupon-rate");
}

TEST(CurveScriptFamilyTest, TestNativeFraArrearsExportAgainstCashflowOracle) {
    const auto index = FamilyIndex();
    const FRA_ native(FAMILY_TODAY, FAMILY_START, FAMILY_END, .025, index);
    const auto instrument = NewFraScriptInstrument(native, FAMILY_END, {"fra", Ccy_("USD"), "USD.OIS", 100.});
    AssertNativeFamily(native, instrument, [](const auto& parameters) {
        const double alpha = (FAMILY_END - FAMILY_START) / 360.;
        const double forward = (OracleDiscount(FAMILY_START, parameters) / OracleDiscount(FAMILY_END, parameters) - 1.) / alpha;
        return std::make_pair(forward, 100. * alpha * (.025 - forward) * OracleDiscount(FAMILY_END, parameters));
    });
}

TEST(CurveScriptFamilyTest, TestNativeIrsExportAgainstCashflowOracle) {
    const Swap_ native(FAMILY_TODAY, FAMILY_START, FAMILY_END, .025, FamilyLeg(12, "ACT_365F"), FamilyIndex(), FamilyLeg(3));
    const auto instrument = NewSwapScriptInstrument(native, {"irs", Ccy_("USD"), "USD.OIS", 100.});
    AssertNativeFamily(native, instrument, [](const auto& parameters) {
        const double annuity = OracleLeg(12, false, 365., 360., parameters), floating = OracleLeg(3, true, 360., 360., parameters);
        return std::make_pair(floating / annuity, 100. * (.025 * annuity - floating));
    });
}

TEST(CurveScriptFamilyTest, TestExplicitPeriodForwardOisExportAgainstCashflowOracle) {
    const OISSwap_ native(FAMILY_TODAY, FAMILY_START, FAMILY_END, .025, FamilyLeg(12, "ACT_365F"), FamilyIndex(), FamilyLeg(3));
    const auto instrument = NewPeriodForwardOisScriptInstrument(native, {"period-ois", Ccy_("USD"), "USD.OIS", 100.});
    AssertNativeFamily(native, instrument, [](const auto& parameters) {
        const double annuity = OracleLeg(12, false, 365., 360., parameters), floating = OracleLeg(3, true, 360., 360., parameters);
        return std::make_pair(floating / annuity, 100. * (.025 * annuity - floating));
    });
}

TEST(CurveScriptFamilyTest, TestNativeBasisExportUsesSpreadUnitAndDeclaredLegSign) {
    const BasisSwap_ native(FAMILY_TODAY, FAMILY_START, FAMILY_END, -.001, FamilyIndex(), FamilyLeg(3), FamilyIndex(), FamilyLeg(6, "ACT_365F"));
    const auto instrument = NewBasisSwapScriptInstrument(native, {"basis", Ccy_("USD"), "USD.OIS", 100.});
    AssertNativeFamily(
        native, instrument,
        [](const auto& parameters) {
            const double annuity = OracleLeg(3, false, 360., 360., parameters), spreadPv = OracleLeg(3, true, 360., 360., parameters);
            const double referencePv = OracleLeg(6, true, 365., 360., parameters);
            return std::make_pair((referencePv - spreadPv) / annuity, 100. * (spreadPv - .001 * annuity - referencePv));
        },
        true);
}

TEST(CurveScriptFamilyTest, TestNativeFraAdvanceSettlementKeepsDiscountedPayoff) {
    const FRA_ native(FAMILY_TODAY, FAMILY_START, FAMILY_END, .025, FamilyIndex());
    const auto instrument = NewFraScriptInstrument(native, FAMILY_START, {"advance-fra", Ccy_("USD"), "USD.OIS", 100.});
    AssertNativeFamily(native, instrument, [](const auto& parameters) {
        const double alpha = (FAMILY_END - FAMILY_START) / 360.;
        const double forward = (OracleDiscount(FAMILY_START, parameters) / OracleDiscount(FAMILY_END, parameters) - 1.) / alpha;
        return std::make_pair(forward, 100. * alpha * (.025 - forward) / (1. + alpha * forward) * OracleDiscount(FAMILY_START, parameters));
    });
}

TEST(CurveScriptFamilyTest, TestNativeStubFixingAndPaymentLagUseActualDates) {
    auto index = FamilyIndex();
    index.fixingLag_ = 2;
    auto fixed = FamilyLeg(12, "ACT_365F"), floating = FamilyLeg(3);
    fixed.paymentLag_ = floating.paymentLag_ = 2;
    fixed.paymentConvention_ = floating.paymentConvention_ = BizDayConvention_("Following");
    const Swap_ native(FAMILY_TODAY, FAMILY_START, Date_(2028, 10, 14), .025, fixed, index, floating);
    const auto instrument = NewSwapScriptInstrument(native, {"stub-lag", Ccy_("USD"), "USD.OIS", 100.});
    ASSERT_EQ(instrument->Pillar(), std::optional<Date_>(Date_(2028, 10, 17)));
    ASSERT_NE(instrument->Definition()->Spec().source_.rows_[3].text_.find("2027-10-19"), String_::npos);
    AssertNativeFamily(native, instrument, [](const auto& parameters) {
        const Vector_<Date_> ends = {Date_(2028, 1, 21), Date_(2028, 4, 21), Date_(2028, 7, 21), Date_(2028, 10, 14)};
        const Vector_<Date_> payments = {Date_(2028, 1, 25), Date_(2028, 4, 25), Date_(2028, 7, 25), Date_(2028, 10, 17)};
        double floatingPv = 0.;
        Date_ start = FAMILY_START;
        for (size_t i = 0; i < ends.size(); ++i) {
            floatingPv += (OracleDiscount(start, parameters) / OracleDiscount(ends[i], parameters) - 1.) * OracleDiscount(payments[i], parameters);
            start = ends[i];
        }
        const double annuity = (ends.back() - FAMILY_START) / 365. * OracleDiscount(payments.back(), parameters);
        return std::make_pair(floatingPv / annuity, 100. * (.025 * annuity - floatingPv));
    });
}

namespace {
    ScriptInstrumentDefinitionSpec_ WeightedDefinition() {
        auto spec = ExplicitCouponDefinition();
        spec.measures_[1].annuitySign_ = -1;
        spec.source_.rows_.pop_back();
        spec.source_.rows_.push_back({ScriptSourceRowKind_::Value_::Event, "2027-04-26",
                                      "PAY PAYS N*(1.3*0.21*((1/FIX(IR[USD,DF,2027-04-21,2027-01-21],2027-01-19)-1)/(90/360))-0.24*K)"});
        spec.source_.rows_.push_back({ScriptSourceRowKind_::Value_::Event, "2027-10-25",
                                      "PAY PAYS N*(0.8*(0.7*0.47*((1/FIX(IR[USD,DF,2027-10-21,2027-04-21],2027-04-19)-1)/(183/360))-0.5*K))"});
        spec.source_.rows_.push_back({ScriptSourceRowKind_::Value_::Event, "2028-07-25",
                                      "PAY PAYS N*(0.5*(1.1*0.69*((1/FIX(IR[USD,DF,2028-07-21,2027-10-21],2027-10-19)-1)/(274/360))-0.75*K))"});
        return spec;
    }
    pair<double, double> WeightedOracle(const Vector_<>& parameters, double notional = 100., double coupon = .025) {
        const Vector_<Date_> starts = {Date_(2027, 1, 21), Date_(2027, 4, 21), Date_(2027, 10, 21)};
        const Vector_<Date_> ends = {Date_(2027, 4, 21), Date_(2027, 10, 21), Date_(2028, 7, 21)};
        const Vector_<Date_> pays = {Date_(2027, 4, 26), Date_(2027, 10, 25), Date_(2028, 7, 25)};
        const Vector_<> amounts = {1., .8, .5}, weights = {1.3, .7, 1.1}, alpha = {.21, .47, .69}, beta = {.24, .5, .75};
        double floating = 0., annuity = 0.;
        for (size_t i = 0; i < starts.size(); ++i) {
            const double forward =
                (OracleDiscount(starts[i], parameters) / OracleDiscount(ends[i], parameters) - 1.) / ((ends[i] - starts[i]) / 360.);
            const double df = OracleDiscount(pays[i], parameters);
            floating += amounts[i] * weights[i] * alpha[i] * forward * df;
            annuity += amounts[i] * beta[i] * df;
        }
        return {floating / annuity, notional * (floating - coupon * annuity)};
    }
} // namespace

TEST(CurveScriptFamilyTest, TestWeightedAmortizingSourceDrivesParPriceAndRawPartials) {
    const auto instrument = NewScriptCalibrationInstrument({"weighted", NewScriptInstrumentDefinition(WeightedDefinition()), {}, FAMILY_END});
    auto spec = FamilySpec(instrument);
    spec.observations_.push_back(NewCalibrationObservation(
        "weighted-pv", instrument, "Price", NewCalibrationMarketQuote("weighted-price", 0., CalibrationValueUnit_::Parse("Price:USD")), 1.));
    const Vector_<> parameters = {-.025, -.067};
    for (const auto mode : {CalibrationDerivative_::Value_::FiniteDifference, CalibrationDerivative_::Value_::AAD}) {
        for (const auto method : {CalibrationEvaluation_::Value_::AutoFast, CalibrationEvaluation_::Value_::GenericDeterministic}) {
            CurveInstrumentCalibrationOptions_ options;
            options.evaluation_ = method;
            options.jacobianMode_ = mode;
            options.differenceStep_ = 1e-5;
            const auto result = PreparedCurveInstruments_(spec, options).Evaluate(parameters);
            const auto expected = WeightedOracle(parameters);
            ASSERT_NEAR(result.modelMeasures_[0], expected.first, 1e-12);
            ASSERT_NEAR(result.modelMeasures_[1], expected.second, 1e-12);
            ASSERT_NEAR(result.modelMeasures_[1], OriginalTreePrice(instrument, parameters), 1e-12);
            for (int j = 0; j < 2; ++j) {
                auto up = parameters, down = parameters;
                up[j] += 5e-6;
                down[j] -= 5e-6;
                const auto plus = WeightedOracle(up), minus = WeightedOracle(down);
                ASSERT_NEAR(result.quoteJacobian_(0, j), (plus.first - minus.first) / (up[j] - down[j]), 1e-8);
                ASSERT_NEAR(result.quoteJacobian_(1, j), (plus.second - minus.second) / (up[j] - down[j]), 1e-8);
            }
        }
    }
    const auto rebound = BindCalibrationInstrumentParameters(
        instrument, {{{"N", 200., CalibrationValueUnit_::Parse("Price:USD")}, {"K", .027, CalibrationValueUnit_::Parse("Rate")}}, {}});
    ASSERT_NEAR(OriginalTreePrice(rebound, parameters), WeightedOracle(parameters, 200., .027).second, 1e-12);
    ASSERT_EQ(LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(rebound))->InstrumentIdentity(), rebound->InstrumentIdentity());
    ASSERT_DOUBLE_EQ(instrument->Binding()[0].value_, 100.);
}

namespace {
    Handle_<DiscountCurve_> FlatFamilyCurve(const String_& name, double rate) {
        return Handle_<DiscountCurve_>(NewDiscountLogDF(name, "USD", {FAMILY_TODAY, FAMILY_END}, {0., -rate * (FAMILY_END - FAMILY_TODAY) / 365.},
                                                        DayBasis::Act365F(), LogDfScheme_("LOG_LINEAR")));
    }
    pair<double, double> ProjectionOracle(bool basis, const Vector_<>& parameters) {
        const auto discount = [](const Date_& date) { return std::exp(-.02 * (date - FAMILY_TODAY) / 365.); };
        double firstPv = 0., firstAnnuity = 0., referencePv = 0.;
        for (Date_ start = FAMILY_START; start < FAMILY_END;) {
            const auto end = std::min(Date::AddMonths(start, 3), FAMILY_END);
            firstPv += (OracleDiscount(start, parameters) / OracleDiscount(end, parameters) - 1.) * discount(end);
            firstAnnuity += (end - start) / 360. * discount(end);
            start = end;
        }
        if (!basis) {
            const double fixed = (FAMILY_END - FAMILY_START) / 365. * discount(FAMILY_END);
            return {firstPv / fixed, 100. * (.025 * fixed - firstPv)};
        }
        for (Date_ start = FAMILY_START; start < FAMILY_END;) {
            const auto end = std::min(Date::AddMonths(start, 6), FAMILY_END);
            referencePv += std::expm1(.045 * (end - start) / 365.) * discount(end);
            start = end;
        }
        return {(referencePv - firstPv) / firstAnnuity, 100. * (firstPv - .001 * firstAnnuity - referencePv)};
    }
    void AssertProjectionFamily(bool basis) {
        auto first = FamilyIndex(), second = FamilyIndex();
        first.useProjectionCurve_ = second.useProjectionCurve_ = true;
        first.forecastTenor_ = PeriodLength_("3M");
        second.forecastTenor_ = PeriodLength_("6M");
        std::unique_ptr<YCInstrument_> native;
        Handle_<CalibrationInstrument_> instrument;
        if (basis) {
            native = std::make_unique<BasisSwap_>(FAMILY_TODAY, FAMILY_START, FAMILY_END, -.001, first, FamilyLeg(3), second, FamilyLeg(6));
            instrument = NewBasisSwapScriptInstrument(static_cast<const BasisSwap_&>(*native), {"projected-basis", Ccy_("USD"), "USD.OIS", 100.});
        } else {
            native = std::make_unique<Swap_>(FAMILY_TODAY, FAMILY_START, FAMILY_END, .025, FamilyLeg(12, "ACT_365F"), first, FamilyLeg(3));
            instrument = NewSwapScriptInstrument(static_cast<const Swap_&>(*native), {"projected-irs", Ccy_("USD"), "USD.OIS", 100.});
        }
        auto spec = FamilySpec(instrument);
        spec.geometry_.calibrateDiscountCurve_ = false;
        spec.geometry_.targetTenor_ = PeriodLength_("3M");
        spec.market_.discountCurves_ = {{CollateralType_("OIS"), FlatFamilyCurve("discount", .02)}};
        spec.market_.forwardCurves_ = {{PeriodLength_("6M"), FlatFamilyCurve("six-month", .045)}};
        const auto route3 = CurveScriptProjectionRoute(Ccy_("USD"), PeriodLength_("3M"));
        const auto route6 = CurveScriptProjectionRoute(Ccy_("USD"), PeriodLength_("6M"));
        spec.market_.routes_.push_back({route3, true, CollateralType_("OIS"), PeriodLength_("3M")});
        spec.market_.routes_.push_back({route6, true, CollateralType_("OIS"), PeriodLength_("6M")});
        spec.observations_.push_back(NewCalibrationObservation(
            "projected-pv", instrument, "Price", NewCalibrationMarketQuote("projected-price", 0., CalibrationValueUnit_::Parse("Price:USD")), 1.));
        const Vector_<> parameters = {-.025, -.067};
        for (const auto mode : {CalibrationDerivative_::Value_::FiniteDifference, CalibrationDerivative_::Value_::AAD}) {
            CurveInstrumentCalibrationOptions_ options;
            options.jacobianMode_ = mode;
            options.differenceStep_ = 1e-5;
            const auto result = PreparedCurveInstruments_(spec, options).Evaluate(parameters);
            const auto expected = ProjectionOracle(basis, parameters);
            auto forwards = spec.market_.forwardCurves_;
            forwards[PeriodLength_("3M")] = result.curve_;
            const CurveBlock_ nativeMarket("native", "USD", spec.market_.discountCurves_, forwards);
            ASSERT_NEAR(result.modelMeasures_[0], (*native->Precompute({}))(nativeMarket), 1e-12);
            ASSERT_NEAR(result.modelMeasures_[0], expected.first, 1e-12);
            ASSERT_NEAR(result.modelMeasures_[1], expected.second, 1e-12);
            ASSERT_NEAR(result.modelMeasures_[1], OriginalTreePrice(instrument, parameters, .02, {{6, .045}}), 1e-12);
            for (int j = 0; j < 2; ++j) {
                auto up = parameters, down = parameters;
                up[j] += 5e-6;
                down[j] -= 5e-6;
                const auto plus = ProjectionOracle(basis, up), minus = ProjectionOracle(basis, down);
                ASSERT_NEAR(result.quoteJacobian_(0, j), (plus.first - minus.first) / (up[j] - down[j]), 1e-8);
                ASSERT_NEAR(result.quoteJacobian_(1, j), (plus.second - minus.second) / (up[j] - down[j]), 1e-8);
            }
        }
        auto wrong = spec;
        wrong.market_.routes_[1].tenor_ = PeriodLength_("6M");
        ASSERT_THROW(static_cast<void>(PreparedCurveInstruments_(wrong)), CalibrationError_);
        wrong = spec;
        wrong.market_.routes_.erase(wrong.market_.routes_.begin() + 1);
        ASSERT_THROW(static_cast<void>(PreparedCurveInstruments_(wrong)), CalibrationError_);
        const FRA_ early(FAMILY_TODAY, FAMILY_TODAY, Date_(2027, 1, 21), .025, first);
        const auto earlyInstrument = NewFraScriptInstrument(early, Date_(2027, 1, 21), {"early", Ccy_("USD"), "USD.OIS", 100.});
        const double earlyRate = (1. / OracleDiscount(Date_(2027, 1, 21), parameters) - 1.) / (92. / 360.);
        const auto lateUnit = CalibrationValueUnit_::Parse(basis ? "BasisSpread" : "Rate");
        spec.observations_ = {NewCalibrationObservation("early-fit", earlyInstrument, "ParRate",
                                                        NewCalibrationMarketQuote("early-q", earlyRate, CalibrationValueUnit_::Parse("Rate")), 1e-10),
                              NewCalibrationObservation("late-fit", instrument, basis ? "BasisSpread" : "ParRate",
                                                        NewCalibrationMarketQuote("late-q", ProjectionOracle(basis, parameters).first, lateUnit),
                                                        1e-10)};
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        const auto fit = CalibrateCurveInstruments(spec, options);
        ASSERT_TRUE(fit.fitWithinTolerance_);
        ASSERT_NEAR(fit.parameters_[0], parameters[0], 1e-8);
        ASSERT_NEAR(fit.parameters_[1], parameters[1], 1e-8);
        ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
    }
} // namespace

TEST(CurveScriptFamilyTest, TestProjectedIrsUsesExplicitBackgroundDiscountAndFitsNativeCoordinates) { AssertProjectionFamily(false); }

TEST(CurveScriptFamilyTest, TestProjectedBasisPreservesIndependentThreeAndSixMonthRoutes) { AssertProjectionFamily(true); }

namespace {
    CalibrationPreparationContext_ FamilyContext() {
        CalibrationPreparationContext_ context;
        context.valuation_.evaluationDate_ = FAMILY_TODAY;
        context.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        context.currency_ = Ccy_("USD");
        context.discountRoute_ = "USD.OIS";
        context.curve_.reset(
            new GSRCurveData_("family-context", FAMILY_TODAY, "USD", {FAMILY_TODAY, FAMILY_START, FAMILY_END}, {0., -.025, -.067}, {}, {}));
        return context;
    }
    void AssertSourceRefusal(const String_& expression, const String_& code) {
        std::optional<CalibrationError_> saved;
        {
            auto definition = ExplicitCouponDefinition();
            definition.source_.rows_[2].text_ = "PAY PAYS " + expression;
            const auto instrument = NewScriptCalibrationInstrument({"refused", NewScriptInstrumentDefinition(definition), {}, FAMILY_END});
            try {
                static_cast<void>(PreparedCurveInstruments_(FamilySpec(instrument)));
            } catch (const CalibrationError_& error) {
                saved = error;
            }
        }
        ASSERT_TRUE(saved) << expression;
        const auto& failure = saved->Failure();
        ASSERT_EQ(failure.diagnostic_.code_, code) << saved->what();
        ASSERT_EQ(failure.diagnostic_.sourceRow_, std::optional<int>(3));
        ASSERT_EQ(failure.diagnostic_.eventDate_, std::optional<Date_>(FAMILY_END));
        ASSERT_EQ(failure.diagnostic_.statement_, std::optional<int>(0));
        ASSERT_EQ(failure.report_->Axes().observations_[0]->Id(), "coupon-rate");
        ASSERT_EQ(failure.report_->Axes().quotes_[0]->Id(), "rate");
        ASSERT_EQ(failure.report_->Errors().size(), 1);
    }
} // namespace

TEST(CurveScriptFamilyTest, TestWholeSourceRejectsNonlinearUnsupportedAndLookAheadObservations) {
    for (const String_ source : {"N*(K*K)", "N*(K/(1+K))", "N*((K-K)*K)"})
        AssertSourceRefusal(source, "UnsupportedCalibrationMeasure");
    for (const String_ source :
         {"N*MAX(K,0)", "N*EXP(K)", "N*(N*K)", "N*(K-FIX(IR[USD,SWAP,5Y,2027-10-21],2027-10-21))", "N*(K-FIX(IR[USD,LIBOR_3M_LCH],2027-10-21))",
          "N*(K-FIX(IR[USD,DF,1Y],2027-10-21))", "N*(K-FIX(IR[USD,DF,2028-10-21,2027-10-21],2029-10-21))"})
        AssertSourceRefusal(source, "UnsupportedFastEvaluator");
}

TEST(CurveScriptFamilyTest, TestHistoricalAndTodayFixingPoliciesUseOwnedSnapshot) {
    for (bool today : {false, true}) {
        auto definition = ExplicitCouponDefinition();
        const String_ fixing = today ? "2026-10-21" : "2026-10-20";
        definition.source_.rows_[2].text_ = "PAY PAYS N*(K-(1/FIX(IR[USD,DF,2028-10-21,2027-10-21]," + fixing + ")-1))";
        const auto instrument = NewScriptCalibrationInstrument({"historical", NewScriptInstrumentDefinition(definition), {}, FAMILY_END});
        auto spec = FamilySpec(instrument);
        spec.valuation_.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
        std::optional<CalibrationError_> missing;
        try {
            static_cast<void>(PreparedCurveInstruments_(spec));
        } catch (const CalibrationError_& error) {
            missing = error;
        }
        ASSERT_TRUE(missing);
        ASSERT_EQ(missing->Failure().diagnostic_.code_, "MissingFixing");
        ASSERT_NE(missing->Failure().diagnostic_.actual_->find(fixing), String_::npos);
        const MarketFixingSnapshot_::values_t values = {{"IR[DF]:USD,2027-10-21,2028-10-21", {{DateTime_(Date::FromString(fixing), 0.), .96}}}};
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_(values));
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        const auto result = PreparedCurveInstruments_(spec, options).Evaluate({-.025, -.067});
        ASSERT_NEAR(result.modelMeasures_[0], 1. / .96 - 1., 1e-12);
        ASSERT_NEAR(result.quoteJacobian_(0, 0), 0., 1e-12);
        ASSERT_NEAR(result.quoteJacobian_(0, 1), 0., 1e-12);
        if (today) {
            spec.valuation_.todayFixingPolicy_ = TodayFixingPolicy_::Value_::MODEL;
            const auto model = PreparedCurveInstruments_(spec).Evaluate({-.025, -.067});
            ASSERT_NEAR(model.modelMeasures_[0], std::expm1(.042), 1e-12);
            ASSERT_NE(model.planIdentity_, result.planIdentity_);
        }
    }
}

TEST(CurveScriptFamilyTest, TestActualAnnuitySignAndFloorAreCheckedOnEachCandidate) {
    for (bool wrongSign : {false, true}) {
        std::optional<CalibrationError_> saved;
        {
            auto definition = WeightedDefinition();
            if (wrongSign)
                definition.measures_[1].annuitySign_ = 1;
            else
                definition.measures_[1].minAbsAnnuity_ = 1e6;
            const auto instrument = NewScriptCalibrationInstrument({"annuity", NewScriptInstrumentDefinition(definition), {}, FAMILY_END});
            try {
                static_cast<void>(PreparedCurveInstruments_(FamilySpec(instrument)).Evaluate({-.025, -.067}));
            } catch (const CalibrationError_& error) {
                saved = error;
            }
        }
        ASSERT_TRUE(saved);
        ASSERT_EQ(saved->Failure().diagnostic_.code_, "InvalidCalibrationAnnuity");
        ASSERT_EQ(saved->Failure().diagnostic_.stage_, CalibrationStage_::Value_::Evaluate);
        ASSERT_EQ(saved->Failure().report_->Axes().observations_[0]->Id(), "coupon-rate");
        ASSERT_TRUE(saved->Failure().candidate_);
    }
}

TEST(CurveScriptFamilyTest, TestRebindContextChangeAndUnqualifiedMcCannotReusePlan) {
    const auto instrument = NewScriptCalibrationInstrument({"family", NewScriptInstrumentDefinition(ExplicitCouponDefinition()), {}, FAMILY_END});
    auto context = FamilyContext();
    const auto plan = PrepareCalibrationInstrument(instrument, "Price", context);
    const auto rebound = BindCalibrationInstrumentParameters(instrument, {{{"N", 200., CalibrationValueUnit_::Parse("Price:USD")}}, {}});
    ASSERT_THROW(plan.RequireCompatible(rebound, context), CalibrationError_);
    context.discountRoute_ = "different";
    ASSERT_THROW(plan.RequireCompatible(instrument, context), CalibrationError_);
    context = FamilyContext();
    ASSERT_THROW(static_cast<void>(plan.Price({.93})), CalibrationError_);
    ASSERT_THROW(static_cast<void>(plan.Coefficients({.93})), CalibrationError_);
    CalibrationEvaluationOptions_ mc;
    mc.evaluation_ = CalibrationEvaluation_::Value_::ScriptMC;
    mc.allowMonteCarloCalibration_ = true;
    mc.sampling_ = CalibrationSamplingSpec_{100, 42, "mrg32"};
    std::optional<CalibrationError_> saved;
    try {
        static_cast<void>(PrepareCalibrationInstrument(instrument, "Price", context, mc));
    } catch (const CalibrationError_& error) {
        saved = error;
    }
    ASSERT_TRUE(saved);
    ASSERT_EQ(saved->Failure().diagnostic_.code_, "UnsupportedFastEvaluator");
    ASSERT_EQ(saved->Failure().diagnostic_.field_, std::optional<String_>("/options/evaluation"));
    ASSERT_TRUE(saved->Failure().report_);
}

TEST(CurveScriptFamilyTest, TestNumericalDenominatorFailureKeepsSourceAndAxes) {
    auto definition = ExplicitCouponDefinition();
    definition.source_.rows_[2].text_ =
        "PAY PAYS N*(K/(FIX(IR[USD,DF,2028-10-21,2027-10-21],2027-10-21)-FIX(IR[USD,DF,2028-10-21,2027-10-21],2027-10-21)))";
    std::optional<CalibrationError_> saved;
    {
        const auto instrument = NewScriptCalibrationInstrument({"denominator", NewScriptInstrumentDefinition(definition), {}, FAMILY_END});
        try {
            static_cast<void>(PreparedCurveInstruments_(FamilySpec(instrument)).Evaluate({-.025, -.067}));
        } catch (const CalibrationError_& error) {
            saved = error;
        }
    }
    ASSERT_TRUE(saved);
    const auto& failure = saved->Failure();
    ASSERT_EQ(failure.diagnostic_.code_, "CalibrationNumericalFailure");
    ASSERT_EQ(failure.diagnostic_.stage_, CalibrationStage_::Value_::Evaluate);
    ASSERT_EQ(failure.diagnostic_.sourceRow_, std::optional<int>(3));
    ASSERT_EQ(failure.diagnostic_.eventDate_, std::optional<Date_>(FAMILY_END));
    ASSERT_EQ(failure.diagnostic_.statement_, std::optional<int>(0));
    ASSERT_EQ(failure.report_->Axes().quotes_[0]->Id(), "rate");
    ASSERT_NE(std::string(saved->what()).find("nonzero coupon denominator"), std::string::npos);
    ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
}

TEST(CurveScriptFamilyTest, TestImporterRefusesUnrepresentedProjectionSettlementAndLegacyStir) {
    auto projected = FamilyIndex();
    projected.useProjectionCurve_ = true;
    projected.forecastTenor_ = PeriodLength_("3M");
    const FRA_ stub(FAMILY_TODAY, FAMILY_START, Date_(2028, 1, 20), .025, projected);
    const FRA_ fra(FAMILY_TODAY, FAMILY_START, FAMILY_END, .025, FamilyIndex());
    const OISSwap_ ois(FAMILY_TODAY, FAMILY_START, FAMILY_END, .025, FamilyLeg(12), projected, FamilyLeg(3));
    const STIR_ stir(FAMILY_TODAY, FAMILY_START, FAMILY_END, .025, DayBasis_("ACT_360"));
    const CurveScriptImportSettings_ settings{"refused-import", Ccy_("USD"), "USD.OIS", 100.};
    ASSERT_THROW(static_cast<void>(NewFraScriptInstrument(stub, Date_(2028, 1, 20), settings)), CalibrationError_);
    ASSERT_THROW(static_cast<void>(NewFraScriptInstrument(fra, Date_(2028, 1, 21), settings)), CalibrationError_);
    ASSERT_THROW(static_cast<void>(NewPeriodForwardOisScriptInstrument(ois, settings)), CalibrationError_);
    ASSERT_THROW(static_cast<void>(NewSwapScriptInstrument(ois, settings)), CalibrationError_);
    ASSERT_THROW(static_cast<void>(NewFraScriptInstrument(stir, FAMILY_END, settings)), CalibrationError_);
}

TEST(CurveScriptFamilyTest, TestTypedDeclarationOrderKeepsNamedSlotSemantics) {
    auto definition = ExplicitCouponDefinition();
    std::swap(definition.parameters_[0], definition.parameters_[1]);
    std::swap(definition.source_.rows_[0], definition.source_.rows_[1]);
    const auto instrument = NewScriptCalibrationInstrument({"reordered", NewScriptInstrumentDefinition(definition), {}, FAMILY_END});
    const auto result = PreparedCurveInstruments_(FamilySpec(instrument)).Evaluate({-.025, -.067});
    ASSERT_NEAR(result.modelMeasures_[0], std::expm1(.042), 1e-12);
}

TEST(CurveScriptFamilyTest, TestExtremeAadCannotPublishNonfiniteNativePartials) {
    const auto instrument = NewScriptCalibrationInstrument({"extreme", NewScriptInstrumentDefinition(ExplicitCouponDefinition()), {}, FAMILY_END});
    auto spec = FamilySpec(instrument);
    spec.observations_ = {NewCalibrationObservation("extreme-pv", instrument, "Price",
                                                    NewCalibrationMarketQuote("extreme-price", 0., CalibrationValueUnit_::Parse("Price:USD")), 1.)};
    CurveInstrumentCalibrationOptions_ options;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    const PreparedCurveInstruments_ plan(spec, options);
    const auto finite = plan.Evaluate({-.03, -600.}, false);
    ASSERT_TRUE(std::isfinite(finite.modelMeasures_[0]));
    for (bool direct : {false, true}) {
        std::optional<CalibrationError_> saved;
        try {
            if (direct)
                static_cast<void>(plan.Jacobian({-.03, -600.}));
            else
                static_cast<void>(plan.Evaluate({-.03, -600.}));
        } catch (const CalibrationError_& error) {
            saved = error;
        }
        ASSERT_TRUE(saved) << "direct=" << direct;
        ASSERT_EQ(saved->Failure().diagnostic_.code_, "CalibrationNumericalFailure");
        ASSERT_EQ(saved->Failure().diagnostic_.stage_, CalibrationStage_::Value_::Evaluate);
        ASSERT_EQ(saved->Failure().report_->Axes().observations_[0]->Id(), "extreme-pv");
        ASSERT_EQ(saved->Failure().report_->Axes().quotes_[0]->Id(), "extreme-price");
        ASSERT_TRUE(saved->Failure().candidate_);
        ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0);
        ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
    }
    ASSERT_TRUE(std::isfinite(plan.Evaluate({-.025, -.067}).quoteJacobian_(0, 0)));
}

TEST(CurveScriptFamilyTest, TestProjectionRouteCanonicalTokensAndOwnedAdmission) {
    ASSERT_EQ(CurveScriptProjectionRoute(Ccy_("USD"), PeriodLength_("Quarterly")), "IR:USD,LIBOR_3M_LCH");
    ASSERT_EQ(CurveScriptProjectionRoute(Ccy_("USD"), PeriodLength_("Semiannual")), "IR:USD,LIBOR_6M_LCH");
    ASSERT_THROW(static_cast<void>(CurveScriptProjectionRoute(Ccy_(), PeriodLength_("3M"))), CalibrationError_);
    ASSERT_THROW(static_cast<void>(CurveScriptProjectionRoute(Ccy_("USD"), PeriodLength_())), CalibrationError_);
    ASSERT_THROW(static_cast<void>(CurveScriptProjectionRoute(Ccy_("USD"), PeriodLength_("1M"))), CalibrationError_);
}
