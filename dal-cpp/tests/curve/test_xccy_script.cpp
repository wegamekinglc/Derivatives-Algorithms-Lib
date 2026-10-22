#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/curve/aadjacobian.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/curve/xccyscript.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/script/calibrationmc.hpp>
#include <dal/script/event.hpp>
#include <dal/storage/_repository.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/dateutils.hpp>

using namespace Dal;

namespace {
    const Date_ XCCY_TODAY(2026, 10, 21), XCCY_END(2027, 10, 21);
    template <class T_> class ScriptFlatCurve_ : public Tape::DiscountCurve_<T_> {
        T_ rate_;

    public:
        ScriptFlatCurve_(const String_& currency, const T_& rate) : Tape::DiscountCurve_<T_>(currency, currency), rate_(rate) {}
        T_ operator()(const Date_& from, const Date_& to) const override {
            using std::exp;
            return exp(-rate_ * ((to - from) / 365.));
        }
        void Poll(Vector_<const YCComponent_*>* all) const override { all->push_back(this); }
        void Poll(std::map<const YCComponent_*, Handle_<YCComponent_>>*) const override {}
        std::unique_ptr<YCComponent_> Clone(const String_&, const YCComponent_::substitutions_t&) const override {
            return std::make_unique<ScriptFlatCurve_>(this->ccy_.String(), rate_);
        }
        void Write(Archive::Store_&) const override {}
    };
    template <class T_> struct ScriptXccyMarket_ {
        ScriptFlatCurve_<T_> domestic_, foreign_, basis_;
        Tape::JointCurveBlock_<T_> domesticBlock_, foreignBlock_;
        ScriptXccyMarket_(T_ domestic = .04, T_ foreign = .02, T_ basis = .003)
            : domestic_("USD", domestic), foreign_("EUR", foreign), basis_("USD", basis) {
            domesticBlock_.discountCurves[CollateralType_("OIS")] = &domestic_;
            foreignBlock_.discountCurves[CollateralType_("OIS")] = &foreign_;
            domesticBlock_.forwardCurves[PeriodLength_("3M")] = &domestic_;
            foreignBlock_.forwardCurves[PeriodLength_("6M")] = &foreign_;
        }
        XccyMarketView_<T_> View(const DateTime_& valuation = DateTime_(XCCY_TODAY)) const {
            return {valuation, CurrencyPair_(Ccy_("USD"), Ccy_("EUR")), Ccy_("USD"), T_(1.1), &domesticBlock_, &foreignBlock_, &basis_};
        }
    };
    CrossCurrencySwapConfig_ ScriptConfig(XccyNotionalMode_ mode = XccyNotionalMode_::Value_::FIXED) {
        CrossCurrencySwapConfig_ c;
        c.domesticNotional_ = 110.;
        c.foreignNotional_ = 100.;
        c.notionalMode_ = mode;
        c.convention_.initialNotionalExchange_ = c.convention_.finalNotionalExchange_ = true;
        c.convention_.domesticLeg_.paymentFrequency_ = PeriodLength_("3M");
        c.convention_.foreignLeg_.paymentFrequency_ = PeriodLength_("6M");
        c.convention_.foreignLeg_.paymentLag_ = 2;
        for (auto* leg : {&c.convention_.domesticLeg_, &c.convention_.foreignLeg_}) {
            leg->dayBasis_ = DayBasis_("ACT_360");
            leg->businessDayConvention_ = leg->paymentConvention_ = BizDayConvention_("Unadjusted");
        }
        for (auto* index : {&c.convention_.domesticIndex_, &c.convention_.foreignIndex_}) {
            index->useProjectionCurve_ = true;
            index->fixingLag_ = 2;
            index->dayBasis_ = DayBasis_("ACT_360");
        }
        c.convention_.domesticIndex_.forecastTenor_ = PeriodLength_("3M");
        c.convention_.foreignIndex_.forecastTenor_ = PeriodLength_("6M");
        c.convention_.domesticIndex_.fixingHolidays_ = Ccy::Conventions::LiborFixHolidays()(Ccy_("USD"));
        c.convention_.foreignIndex_.fixingHolidays_ = Ccy::Conventions::LiborFixHolidays()(Ccy_("EUR"));
        c.domesticRateFixing_ = {"IR:USD,LIBOR_3M_LCH", 0, 0};
        c.foreignRateFixing_ = {"IR:EUR,LIBOR_6M_LCH", 0, 0};
        c.fxReset_.fixingLag_ = 0;
        c.fxReset_.fixingHour_ = c.fxReset_.fixingMinute_ = 0;
        return c;
    }
    const Date_ XCCY_START(2027, 5, 4), XCCY_MATURITY(2027, 11, 4);
    CalibrationPreparationContext_ ScriptContext(const DateTime_& valuation = DateTime_(XCCY_TODAY),
                                                 const Handle_<MarketFixingSnapshot_>& fixings = {}) {
        CalibrationPreparationContext_ c;
        auto xccy = std::make_shared<XccyScriptContext_>();
        xccy->valuationTime_ = valuation;
        xccy->pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        xccy->collateralCurrency_ = Ccy_("USD");
        xccy->discountRoute_ = "USD.OIS";
        xccy->fixings_ = fixings;
        c.xccy_ = xccy;
        return c;
    }
} // namespace

namespace {
    double OracleDf(double rate, const Date_& date, const Date_& valuation = XCCY_TODAY) { return std::exp(-rate * (date - valuation) / 365.); }
    double OracleFx(const Vector_<>& p, const Date_& date, const Date_& valuation = XCCY_TODAY) {
        return p[3] * OracleDf(p[1], date, valuation) / (OracleDf(p[0], date, valuation) * OracleDf(p[2], date, valuation));
    }
    double OracleRate(double rate, const Date_& start, const Date_& end) {
        return (std::exp(rate * (end - start) / 365.) - 1.) / ((end - start) / 360.);
    }
    std::pair<double, double> CashflowOracle(const Vector_<>& p, XccyNotionalMode_ mode, bool foreignSpread, bool receive) {
        const Date_ reset(2027, 8, 4), foreignPayment(2027, 11, 8);
        const double laterNotional = mode == XccyNotionalMode_::Value_::FIXED ? 110. : 100. * OracleFx(p, reset);
        const double firstAnnuity = 110. * ((reset - XCCY_START) / 360.) * OracleDf(p[0], reset);
        const double secondAnnuity = laterNotional * ((XCCY_MATURITY - reset) / 360.) * OracleDf(p[0], XCCY_MATURITY);
        double domestic = firstAnnuity * OracleRate(p[0], XCCY_START, reset) + secondAnnuity * OracleRate(p[0], reset, XCCY_MATURITY);
        domestic += -110. * OracleDf(p[0], XCCY_START) + laterNotional * OracleDf(p[0], XCCY_MATURITY);
        if (mode == XccyNotionalMode_::Value_::MARK_TO_MARKET)
            domestic += (laterNotional - 110.) * OracleDf(p[0], reset);
        const auto conversion = [&](const Date_& date) { return p[3] * OracleDf(p[1], date) / OracleDf(p[2], date); };
        const double foreignAnnuity = 100. * ((XCCY_MATURITY - XCCY_START) / 360.) * conversion(foreignPayment);
        const double foreign =
            foreignAnnuity * OracleRate(p[1], XCCY_START, XCCY_MATURITY) - 100. * conversion(XCCY_START) + 100. * conversion(XCCY_MATURITY);
        const double b = foreignSpread ? domestic - foreign : foreign - domestic;
        const double a = -(foreignSpread ? foreignAnnuity : firstAnnuity + secondAnnuity);
        return {(receive ? 1. : -1.) * (b + .001 * a), -b / a};
    }
    double TreeFixing(const Script::NodeFix_& node, const Vector_<>& p) {
        const auto date = *node.fixingDate_;
        const auto* libor = dynamic_cast<const Index::Libor_*>(node.index_.get());
        if (!libor)
            return OracleFx(p, date);
        const auto start = libor->StartDate(DateTime_(date));
        const auto end = Date::NominalMaturity(start, libor->tenor_.Period(), libor->ccy_);
        return OracleRate(libor->ccy_ == Ccy_("USD") ? p[0] : p[1], start, end);
    }
    void BindTree(Script::Node_* node, Vector_<Script::ObservationRequest_>* requests, Vector_<>* known, const Vector_<>& p) {
        if (auto* fix = dynamic_cast<Script::NodeFix_*>(node)) {
            fix->observationId_ = requests->size();
            requests->push_back({fix->index_, {fix->index_->Name(), DateTime_(*fix->fixingDate_)}, {}, known->size(), {}, true});
            known->push_back(TreeFixing(*fix, p));
        }
        for (const auto& child : node->arguments_)
            BindTree(child.get(), requests, known, p);
    }
    double OriginalTreePrice(const Handle_<CalibrationInstrument_>& instrument, const Vector_<>& p) {
        auto product = instrument->Product();
        product->IndexVariables();
        Vector_<Script::ObservationRequest_> requests;
        Vector_<> known;
        for (const auto& event : product->Events())
            for (const auto& node : event)
                BindTree(node.get(), &requests, &known, p);
        const Script::ObservationPlan_ observations(std::move(requests), std::move(known));
        Script::Evaluator_<double> evaluator(Vector_<>(product->VarNames().size(), 0.), product->ConstVarValues(), product->VectorCapacities());
        AAD::Scenario_<> path(product->Events().size());
        for (size_t i = 0; i < path.size(); ++i) {
            path[i].numeraire_ = 1. / OracleDf(p[0], product->EventDates()[i]);
            path[i].spot_ = OracleFx(p, product->EventDates()[i]);
        }
        evaluator.SetObservations(&observations);
        evaluator.SetScenario(&path);
        evaluator.Init();
        for (size_t i = 0; i < product->Events().size(); ++i) {
            evaluator.SetCurEvt(i);
            for (const auto& node : product->Events()[i])
                node->Accept(evaluator);
        }
        return evaluator.VarVals()[product->PayOffIdx()];
    }
    void AssertOracleCase(XccyNotionalMode_ mode, bool foreignSpread, bool receive) {
        auto config = ScriptConfig(mode);
        config.convention_.spreadOnForeignLeg_ = foreignSpread;
        const CrossCurrencySwap_ native(XCCY_TODAY, XCCY_START, XCCY_MATURITY, .001, config);
        const auto source = NewXccyScriptInstrument(native, {"oracle", "USD.OIS", receive});
        const auto price = PrepareCalibrationInstrument(source, "Price", ScriptContext());
        const auto spread = PrepareCalibrationInstrument(source, "BasisSpread", ScriptContext());
        const ScriptXccyMarket_<double> market;
        const auto view = market.View();
        const auto oracle = CashflowOracle({.04, .02, .003, 1.1}, mode, foreignSpread, receive);
        const auto nativePlan = BuildXccyCashflowPlan(XCCY_START, XCCY_MATURITY, config);
        const MarketFixingSnapshot_ empty;
        ASSERT_NEAR(EvaluateXccyScript(price, view), oracle.first, 1e-10);
        ASSERT_NEAR(EvaluateXccyScript(spread, view), oracle.second, 1e-12);
        ASSERT_NEAR(PriceXccyContract(nativePlan, view, empty, .001, foreignSpread, receive), oracle.first, 1e-10);
        ASSERT_NEAR(PriceXccyParSpread(nativePlan, view, empty), oracle.second, 1e-12);
        ASSERT_NEAR(OriginalTreePrice(source, {.04, .02, .003, 1.1}), oracle.first, 1e-10);
        ::testing::Test::RecordProperty("price", std::to_string(oracle.first));
        ::testing::Test::RecordProperty("spread", std::to_string(oracle.second));
    }
} // namespace

TEST(XccyScriptTest, TestIndependentCashflowOracleAllModesAndDirections) {
    for (const auto mode : {XccyNotionalMode_::Value_::FIXED, XccyNotionalMode_::Value_::RESETTABLE, XccyNotionalMode_::Value_::MARK_TO_MARKET})
        for (bool foreign : {false, true})
            for (bool receive : {false, true})
                AssertOracleCase(mode, foreign, receive);
}

TEST(XccyScriptTest, TestRawAadAndRefinedFdAtNonroot) {
    const auto mode = XccyNotionalMode_::Value_::MARK_TO_MARKET;
    const CrossCurrencySwap_ native(XCCY_TODAY, XCCY_START, XCCY_MATURITY, .001, ScriptConfig(mode));
    const auto instrument = NewXccyScriptInstrument(native, {"raw-aad", "USD.OIS"});
    const auto price = PrepareCalibrationInstrument(instrument, "Price", ScriptContext());
    const auto spread = PrepareCalibrationInstrument(instrument, "BasisSpread", ScriptContext());
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);
    {
        TapeGuard_ scope(tape);
        auto p = RegisterCurveParameters({.045, .018, .004, 1.12});
        AAD::NewRecording(*tape);
        const ScriptXccyMarket_<AAD::Number_> market(p[0], p[1], p[2]);
        auto view = market.View();
        view.fxSpot_ = p[3];
        Vector_<AAD::Number_> values{EvaluateXccyScript(price, view), EvaluateXccyScript(spread, view)};
        const auto jacobian = HarvestCurveJacobian(*tape, p, values);
        for (size_t j = 0; j < p.size(); ++j) {
            for (double h : {1e-5, 5e-6}) {
                Vector_<> up{.045, .018, .004, 1.12}, down = up;
                up[j] += h;
                down[j] -= h;
                const auto plus = CashflowOracle(up, mode, true, true), minus = CashflowOracle(down, mode, true, true);
                ASSERT_NEAR(jacobian(0, static_cast<int>(j)), (plus.first - minus.first) / (2. * h), 1e-8);
                ASSERT_NEAR(jacobian(1, static_cast<int>(j)), (plus.second - minus.second) / (2. * h), 1e-8);
            }
        }
    }
    AAD::Clear(*tape);
}

TEST(XccyScriptTest, TestFutureFxDirectionBasisAndReciprocal) {
    const ScriptXccyMarket_<double> market;
    const auto view = market.View();
    const double expected = 1.1 * std::exp(-.02) / (std::exp(-.04) * std::exp(-.003));
    ASSERT_NEAR(XccyFxForward(view, XCCY_END), expected, 1e-12);
    ASSERT_NEAR(XccyFxForward(view, XCCY_END, true), 1. / expected, 1e-12);
}

TEST(XccyScriptTest, TestNativeExportAndSharedPreparation) {
    const CrossCurrencySwap_ native(XCCY_TODAY, XCCY_START, XCCY_MATURITY, .001, ScriptConfig());
    const auto instrument = NewXccyScriptInstrument(native, {"xccy", "USD.OIS"});
    const auto prepared = PrepareCalibrationInstrument(instrument, "BasisSpread", ScriptContext());
    ASSERT_EQ(prepared.Capability().ruleId_, "dal.xccy-cashflows/1");
    const auto& descriptor = *std::get<CalibrationXccy_>(prepared.Descriptor());
    ASSERT_EQ(descriptor.plan_.domesticPeriods_.size(), 2U);
    ASSERT_EQ(descriptor.plan_.foreignPeriods_.size(), 1U);
    ASSERT_EQ(descriptor.plan_.config_.domesticNotional_, 110.);
    ASSERT_EQ(descriptor.plan_.config_.foreignNotional_, 100.);
}

TEST(XccyScriptTest, TestMovedMtmExchangeRejected) {
    const CrossCurrencySwap_ native(XCCY_TODAY, XCCY_START, XCCY_MATURITY, .001, ScriptConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));
    auto spec = NewXccyScriptInstrument(native, {"mtm", "USD.OIS"})->Definition()->Spec();
    spec.source_.rows_.back().key_ = "2027-08-05";
    const auto changed = NewScriptCalibrationInstrument({"moved-principal", NewScriptInstrumentDefinition(spec)});
    ASSERT_THROW(PrepareCalibrationInstrument(changed, "Price", ScriptContext()), CalibrationError_);
}

TEST(XccyScriptTest, TestPriceCannotSmuggleBondPriceSpread) {
    const CrossCurrencySwap_ native(XCCY_TODAY, XCCY_START, XCCY_MATURITY, .001, ScriptConfig());
    auto spec = NewXccyScriptInstrument(native, {"units", "USD.OIS"})->Definition()->Spec();
    spec.parameters_[1].unit_ = CalibrationValueUnit_::Parse("BondPrice");
    spec.parameters_[1].role_ = ScriptParameterRole_::Value_::Strike;
    spec.measures_ = {spec.measures_[0]};
    const auto changed = NewScriptCalibrationInstrument({"wrong-unit", NewScriptInstrumentDefinition(spec)});
    ASSERT_THROW(PrepareCalibrationInstrument(changed, "Price", ScriptContext()), CalibrationError_);
}

TEST(XccyScriptTest, TestPriceRejectsOverflow) {
    const CrossCurrencySwap_ native(XCCY_TODAY, XCCY_START, XCCY_MATURITY, .001, ScriptConfig());
    const auto price = PrepareCalibrationInstrument(NewXccyScriptInstrument(native, {"overflow", "USD.OIS"}), "Price", ScriptContext());
    const ScriptXccyMarket_<double> market;
    auto view = market.View();
    view.fxSpot_ = std::numeric_limits<double>::max();
    ASSERT_THROW(EvaluateXccyScript(price, view), CalibrationError_);
}

TEST(XccyScriptTest, TestDeclaredAnnuitySignCheckedDuringPreparation) {
    const CrossCurrencySwap_ native(XCCY_TODAY, XCCY_START, XCCY_MATURITY, .001, ScriptConfig());
    auto spec = NewXccyScriptInstrument(native, {"sign", "USD.OIS"})->Definition()->Spec();
    spec.measures_[1].annuitySign_ = 1;
    const auto changed = NewScriptCalibrationInstrument({"wrong-annuity-sign", NewScriptInstrumentDefinition(spec)});
    ASSERT_THROW(PrepareCalibrationInstrument(changed, "BasisSpread", ScriptContext()), CalibrationError_);
}

namespace {
    template <class F_> void AssertCode(const F_& action, const String_& code) {
        try {
            action();
            FAIL() << "expected " << code;
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, code) << error.what();
        }
    }
    Handle_<CalibrationInstrument_> TimestampInstrument(XccyNotionalMode_ mode = XccyNotionalMode_::Value_::MARK_TO_MARKET) {
        const CrossCurrencySwap_ native(XCCY_TODAY, XCCY_START, XCCY_MATURITY, .001, ScriptConfig(mode));
        return NewXccyScriptInstrument(native, {"timestamp", "USD.OIS"});
    }
    MarketFixingSnapshot_::values_t HistoricalValues() {
        MarketFixingSnapshot_::values_t values;
        values["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2027, 4, 30))] = .04;
        values["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2027, 8, 2))] = .045;
        values["IR:EUR,LIBOR_6M_LCH"][DateTime_(Date_(2027, 4, 30))] = .02;
        return values;
    }
    double TimestampOracle(double notional, const Date_& valuation) {
        const Vector_<> p{.04, .02, .003, 1.1};
        const Date_ reset(2027, 8, 4), foreignPayment(2027, 11, 8);
        const double firstAlpha = (reset - XCCY_START) / 360., secondAlpha = (XCCY_MATURITY - reset) / 360.;
        const auto discount = [&](const Date_& date) { return OracleDf(p[0], date, valuation); };
        double domestic = 110. * firstAlpha * .04 * discount(reset) + notional * secondAlpha * .045 * discount(XCCY_MATURITY);
        domestic += (notional - 110.) * discount(reset) + notional * discount(XCCY_MATURITY);
        const auto conversion = [&](const Date_& date) { return p[3] * OracleDf(p[1], date, valuation) / OracleDf(p[2], date, valuation); };
        const double annuity = 100. * ((XCCY_MATURITY - XCCY_START) / 360.) * conversion(foreignPayment);
        const double foreign = annuity * .02 + 100. * conversion(XCCY_MATURITY);
        return (domestic - foreign) / annuity;
    }
    struct HistoriesScope_ {
        std::map<String_, FixHistory_> originals_;
        explicit HistoriesScope_(const MarketFixingSnapshot_::values_t& values) {
            for (const auto& index : values) {
                originals_[index.first] = Global::Fixings_().History(index.first);
                FixHistory_ history;
                for (const auto& value : index.second)
                    history.vals_.push_back(value);
                XGLOBAL::StoreFixings(index.first, history, false);
            }
        }
        ~HistoriesScope_() {
            for (const auto& original : originals_) {
                if (!original.second.vals_.empty())
                    XGLOBAL::StoreFixings(original.first, original.second, false);
                else
                    for (const auto& object : ObjectAccess_::Find(".*"))
                        if (object->Name() == "##GLOBAL##FixingsFor:" + original.first)
                            static_cast<void>(ObjectAccess_::Erase(*object));
            }
        }
    };
} // namespace

TEST(XccyScriptTest, TestSameTimestampFxSuppliedEmptyAndReverseSnapshot) {
    const auto instrument = TimestampInstrument();
    const Date_ today(2027, 8, 4);
    const ScriptXccyMarket_<double> market;
    for (bool supplied : {false, true}) {
        auto values = HistoricalValues();
        if (supplied)
            values["FX[USD/EUR]"][DateTime_(today)] = 1. / 1.25;
        const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_(values));
        const auto prepared = PrepareCalibrationInstrument(instrument, "BasisSpread", ScriptContext(DateTime_(today), snapshot));
        ASSERT_NEAR(EvaluateXccyScript(prepared, market.View(DateTime_(today))), TimestampOracle(supplied ? 125. : 110., today), 1e-12);
    }
}

TEST(XccyScriptTest, TestOmittedSnapshotCapturesOnlyStrictHistoryAndFreezes) {
    auto values = HistoricalValues();
    const DateTime_ valuation(Date_(2027, 8, 4));
    values["FX[EUR/USD]"][valuation] = 1.99;
    const HistoriesScope_ histories(values);
    const auto instrument = TimestampInstrument();
    const auto context = ScriptContext(valuation);
    const auto prepared = PrepareCalibrationInstrument(instrument, "BasisSpread", context);
    const auto& captured = *std::get<CalibrationXccy_>(prepared.Descriptor())->fixings_;
    ASSERT_FALSE(captured.Find("FX[EUR/USD]", valuation));
    const ScriptXccyMarket_<double> market;
    const double before = EvaluateXccyScript(prepared, market.View(valuation));
    FixHistory_ changed;
    changed.vals_ = {{DateTime_(Date_(2027, 4, 30)), .9}, {DateTime_(Date_(2027, 8, 2)), .8}};
    XGLOBAL::StoreFixings("IR:USD,LIBOR_3M_LCH", changed, false);
    prepared.RequireCompatible(instrument, context);
    ASSERT_DOUBLE_EQ(EvaluateXccyScript(prepared, market.View(valuation)), before);
    ASSERT_NEAR(before, TimestampOracle(110., valuation.Date()), 1e-12);
    const Handle_<MarketFixingSnapshot_> empty(new MarketFixingSnapshot_());
    AssertCode([&] { PrepareCalibrationInstrument(instrument, "BasisSpread", ScriptContext(valuation, empty)); }, "MissingFixing");
}

TEST(XccyScriptTest, TestFutureSnapshotEntriesIgnored) {
    const Date_ today(2027, 8, 3), reset(2027, 8, 4);
    auto values = HistoricalValues();
    values["FX[EUR/USD]"][DateTime_(reset)] = 9.;
    const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_(values));
    const auto prepared = PrepareCalibrationInstrument(TimestampInstrument(), "BasisSpread", ScriptContext(DateTime_(today), snapshot));
    const ScriptXccyMarket_<double> market;
    ASSERT_NEAR(EvaluateXccyScript(prepared, market.View(DateTime_(today))),
                TimestampOracle(100. * OracleFx({.04, .02, .003, 1.1}, reset, today), today), 1e-12);
}

TEST(XccyScriptTest, TestSameTimestampRatesSuppliedOrActiveAndFutureIgnored) {
    const DateTime_ valuation(Date_(2027, 4, 30));
    const auto instrument = TimestampInstrument(XccyNotionalMode_::Value_::FIXED);
    const ScriptXccyMarket_<double> market;
    for (bool supplied : {false, true}) {
        auto values = MarketFixingSnapshot_::values_t{};
        if (supplied) {
            values["IR:USD,LIBOR_3M_LCH"][valuation] = .07;
            values["IR:EUR,LIBOR_6M_LCH"][valuation] = -.01;
        }
        values["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2027, 8, 2))] = 9.;
        const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_(values));
        const auto prepared = PrepareCalibrationInstrument(instrument, "BasisSpread", ScriptContext(valuation, snapshot));
        const auto native = BuildXccyCashflowPlan(XCCY_START, XCCY_MATURITY, ScriptConfig());
        ASSERT_NEAR(EvaluateXccyScript(prepared, market.View(valuation)), PriceXccyParSpread(native, market.View(valuation), *snapshot), 1e-12);
    }
    const Handle_<MarketFixingSnapshot_> empty(new MarketFixingSnapshot_());
    const auto omitted = PrepareCalibrationInstrument(instrument, "Price", ScriptContext());
    const auto explicitEmpty = PrepareCalibrationInstrument(instrument, "Price", ScriptContext(DateTime_(XCCY_TODAY), empty));
    ASSERT_NE(omitted.PlanIdentity(), explicitEmpty.PlanIdentity());
}

TEST(XccyScriptTest, TestExactHistoryDoesNotCopyIntradayValueToMidnight) {
    auto values = HistoricalValues();
    values["IR:USD,LIBOR_3M_LCH"].erase(DateTime_(Date_(2027, 4, 30)));
    values["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2027, 4, 30), 11, 0)] = .9;
    const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_(values));
    AssertCode([&] { PrepareCalibrationInstrument(TimestampInstrument(), "Price", ScriptContext(DateTime_(Date_(2027, 8, 4)), snapshot)); },
               "MissingFixing");
}

TEST(XccyScriptTest, TestIntradayAndSameDayEarlierValuationRejected) {
    const auto instrument = TimestampInstrument();
    const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_(HistoricalValues()));
    for (const auto date : {XCCY_TODAY, Date_(2027, 4, 30)})
        AssertCode([&] { PrepareCalibrationInstrument(instrument, "Price", ScriptContext(DateTime_(date, 9, 0), snapshot)); },
                   "UnsupportedCalibrationFixingTime");
}

TEST(XccyScriptTest, TestGenericTodayOverridesRejected) {
    const auto instrument = TimestampInstrument();
    for (const auto policy : {TodayFixingPolicy_::Value_::MODEL, TodayFixingPolicy_::Value_::REQUIREHISTORICAL}) {
        auto context = ScriptContext();
        auto xccy = std::make_shared<XccyScriptContext_>(*context.xccy_);
        xccy->todayFixingOverride_ = TodayFixingPolicy_(policy);
        context.xccy_ = xccy;
        AssertCode([&] { PrepareCalibrationInstrument(instrument, "Price", context); }, "InvalidCalibrationFixingPolicy");
    }
    auto context = ScriptContext();
    context.valuation_.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
    AssertCode([&] { PrepareCalibrationInstrument(instrument, "Price", context); }, "InvalidCalibrationFixingPolicy");
}

TEST(XccyScriptTest, TestCustomNativeIdentityRejectedWithOwnedDiagnostic) {
    auto config = ScriptConfig();
    config.domesticRateFixing_.indexName_ = "CUSTOM-USD";
    const CrossCurrencySwap_ native(XCCY_TODAY, XCCY_START, XCCY_MATURITY, .001, config);
    AssertCode([&] { NewXccyScriptInstrument(native, {"custom", "USD.OIS"}); }, "UnsupportedCalibrationFixingTime");
}

TEST(XccyScriptTest, TestIntradayNativeFixingAndResetRejected) {
    auto config = ScriptConfig();
    config.domesticRateFixing_.fixingHour_ = 11;
    const CrossCurrencySwap_ rate(XCCY_TODAY, XCCY_START, XCCY_MATURITY, .001, config);
    AssertCode([&] { NewXccyScriptInstrument(rate, {"intraday-rate", "USD.OIS"}); }, "UnsupportedCalibrationFixingTime");
    config = ScriptConfig(XccyNotionalMode_::Value_::RESETTABLE);
    config.fxReset_.fixingHour_ = 10;
    const CrossCurrencySwap_ reset(XCCY_TODAY, XCCY_START, XCCY_MATURITY, .001, config);
    AssertCode([&] { NewXccyScriptInstrument(reset, {"intraday-reset", "USD.OIS"}); }, "UnsupportedCalibrationFixingTime");
}

TEST(XccyScriptTest, TestMissingOmittedHistoryHasCalibrationDiagnostic) {
    auto values = HistoricalValues();
    values["IR:USD,LIBOR_3M_LCH"] = {{DateTime_(Date_(2025, 1, 1)), .03}};
    values["IR:EUR,LIBOR_6M_LCH"] = {{DateTime_(Date_(2025, 1, 1)), .02}};
    const HistoriesScope_ histories(values);
    AssertCode([&] { PrepareCalibrationInstrument(TimestampInstrument(), "Price", ScriptContext(DateTime_(Date_(2027, 8, 4)))); }, "MissingFixing");
}

TEST(XccyScriptTest, TestHistoricalCouponsDoNotRequireUnusedProjectionRoutes) {
    const DateTime_ valuation(Date_(2027, 8, 4));
    const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_(HistoricalValues()));
    const auto prepared = PrepareCalibrationInstrument(TimestampInstrument(), "BasisSpread", ScriptContext(valuation, snapshot));
    ScriptXccyMarket_<double> market;
    market.domesticBlock_.forwardCurves.clear();
    market.foreignBlock_.forwardCurves.clear();
    ASSERT_NEAR(EvaluateXccyScript(prepared, market.View(valuation)), TimestampOracle(110., valuation.Date()), 1e-12);
}

TEST(XccyScriptTest, TestCanonicalConventionChangeInvalidatesPlan) {
    const auto instrument = TimestampInstrument();
    const auto context = ScriptContext();
    const auto prepared = PrepareCalibrationInstrument(instrument, "Price", context);
    const Ccy_ usd("USD");
    struct Restore_ {
        int old_ = Ccy::Conventions::LiborFixDays()(Ccy_("USD"));
        ~Restore_() { Ccy::Conventions::LiborFixDays().XWrite()(Ccy_("USD"), old_); }
    } restore;
    Ccy::Conventions::LiborFixDays().XWrite()(usd, restore.old_ + 1);
    AssertCode([&] { prepared.RequireCompatible(instrument, context); }, "CalibrationContextMismatch");
}

TEST(XccyScriptTest, TestSourceFxPrincipalAndCurrencyMutationsRejected) {
    const auto original = TimestampInstrument();
    const auto base = original->Definition()->Spec();
    for (const auto& replacement : {String_("(1/SPOT())"), String_("(SPOT()+1)")}) {
        auto spec = base;
        for (auto& row : spec.source_.rows_) {
            const auto offset = row.text_.find("SPOT()");
            if (offset != String_::npos) {
                row.text_.replace(offset, 6, replacement);
                break;
            }
        }
        const auto changed = NewScriptCalibrationInstrument({"bad-conversion", NewScriptInstrumentDefinition(spec)});
        ASSERT_THROW(PrepareCalibrationInstrument(changed, "Price", ScriptContext()), CalibrationError_);
    }
    {
        auto spec = base;
        spec.source_.rows_.back().text_ = "PAY PAYS N*(-1*(1-1)*1)";
        const auto changed = NewScriptCalibrationInstrument({"bad-principal", NewScriptInstrumentDefinition(spec)});
        ASSERT_THROW(PrepareCalibrationInstrument(changed, "Price", ScriptContext()), CalibrationError_);
    }
    {
        auto context = ScriptContext();
        auto xccy = std::make_shared<XccyScriptContext_>(*context.xccy_);
        xccy->pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("GBP"));
        context.xccy_ = xccy;
        ASSERT_THROW(PrepareCalibrationInstrument(original, "Price", context), CalibrationError_);
    }
}

TEST(XccyScriptTest, TestRebindingPersistenceAndChangedPaymentDriveNewPlans) {
    const auto original = TimestampInstrument();
    const auto rebound = BindCalibrationInstrumentParameters(original, {{{"N", 220., CalibrationValueUnit_::Parse("Price:USD")}}, {}});
    const auto restored = LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(rebound));
    ASSERT_EQ(restored->InstrumentIdentity(), rebound->InstrumentIdentity());
    const auto first = PrepareCalibrationInstrument(original, "Price", ScriptContext());
    const auto second = PrepareCalibrationInstrument(restored, "Price", ScriptContext());
    const ScriptXccyMarket_<double> market;
    ASSERT_NEAR(EvaluateXccyScript(second, market.View()), 2. * EvaluateXccyScript(first, market.View()), 1e-10);
    ASSERT_THROW(first.RequireCompatible(restored, ScriptContext()), CalibrationError_);
    auto spec = original->Definition()->Spec();
    for (auto& row : spec.source_.rows_)
        if (row.key_ == "2027-11-08")
            row.key_ = "2027-11-09";
    const auto changed = NewScriptCalibrationInstrument({"new-payment", NewScriptInstrumentDefinition(spec)});
    const auto moved = PrepareCalibrationInstrument(changed, "Price", ScriptContext());
    ASSERT_NE(moved.PlanIdentity(), first.PlanIdentity());
    ASSERT_NE(EvaluateXccyScript(moved, market.View()), EvaluateXccyScript(first, market.View()));
    ASSERT_NEAR(EvaluateXccyScript(moved, market.View()), OriginalTreePrice(changed, {.04, .02, .003, 1.1}), 1e-10);
}

TEST(XccyScriptTest, TestMarketCurrencyCollateralBasisAndProjectionRoutesRejected) {
    const auto prepared = PrepareCalibrationInstrument(TimestampInstrument(), "Price", ScriptContext());
    ScriptXccyMarket_<double> market;
    {
        auto view = market.View();
        view.pair_ = view.pair_.Reversed();
        AssertCode([&] { EvaluateXccyScript(prepared, view); }, "CalibrationContextMismatch");
        view = market.View();
        view.collateralCurrency_ = Ccy_("EUR");
        AssertCode([&] { EvaluateXccyScript(prepared, view); }, "CalibrationContextMismatch");
        view = market.View();
        view.basis_ = &market.foreign_;
        AssertCode([&] { EvaluateXccyScript(prepared, view); }, "CalibrationNumericalFailure");
    }
    market.domesticBlock_.forwardCurves[PeriodLength_("3M")] = &market.foreign_;
    AssertCode([&] { EvaluateXccyScript(prepared, market.View()); }, "CalibrationContextMismatch");
    market.domesticBlock_.forwardCurves.clear();
    AssertCode([&] { EvaluateXccyScript(prepared, market.View()); }, "CalibrationContextMismatch");
}

TEST(XccyScriptTest, TestNoMatchingStochasticModelAndUnavailableMethodsFailExplicitly) {
    const auto instrument = TimestampInstrument();
    CalibrationEvaluationOptions_ options;
    options.evaluation_ = CalibrationEvaluation_::Value_::ScriptMC;
    options.allowMonteCarloCalibration_ = true;
    options.sampling_ = CalibrationSamplingSpec_{1024, 17, "irn"};
    AssertCode([&] { PrepareCalibrationInstrument(instrument, "Price", ScriptContext(), options); }, "MCCrossCurrencyUnsupported");
    options = {};
    options.evaluation_ = CalibrationEvaluation_::Value_::GenericDeterministic;
    AssertCode([&] { PrepareCalibrationInstrument(instrument, "Price", ScriptContext(), options); }, "UnsupportedFastEvaluator");
    options = {};
    options.derivativeMethod_ = CalibrationDerivative_::Value_::Analytic;
    AssertCode([&] { PrepareCalibrationInstrument(instrument, "Price", ScriptContext(), options); }, "DerivativeUnavailable");
}

TEST(XccyScriptTest, TestTodayFxConversionStaysActiveModelSpot) {
    const auto instrument = TimestampInstrument(XccyNotionalMode_::Value_::FIXED);
    auto values = HistoricalValues();
    values["FX[EUR/USD]"][DateTime_(XCCY_START)] = 9.;
    const Handle_<MarketFixingSnapshot_> supplied(new MarketFixingSnapshot_(values));
    const auto prepared = PrepareCalibrationInstrument(instrument, "Price", ScriptContext(DateTime_(XCCY_START), supplied));
    const ScriptXccyMarket_<double> market;
    const auto native = BuildXccyCashflowPlan(XCCY_START, XCCY_MATURITY, ScriptConfig());
    ASSERT_NEAR(EvaluateXccyScript(prepared, market.View(DateTime_(XCCY_START))),
                PriceXccyContract(native, market.View(DateTime_(XCCY_START)), *supplied, .001, true, true), 1e-10);
}

TEST(XccyScriptTest, TestGlobalSnapshotFailureKeepsOwnedDiagnostic) {
    auto values = HistoricalValues();
    values["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2027, 4, 30))] = std::numeric_limits<double>::quiet_NaN();
    const HistoriesScope_ histories(values);
    AssertCode([&] { PrepareCalibrationInstrument(TimestampInstrument(), "Price", ScriptContext(DateTime_(Date_(2027, 8, 4)))); },
               "CalibrationNumericalFailure");
}

TEST(XccyScriptTest, TestStartedMultiResetUsesOnlyUnsettledDependencyClosure) {
    const Date_ end(2028, 5, 4), valuation(2028, 2, 5);
    const CrossCurrencySwap_ native(XCCY_TODAY, XCCY_START, end, .001, ScriptConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));
    const auto instrument = NewXccyScriptInstrument(native, {"multi-reset", "USD.OIS"});
    MarketFixingSnapshot_::values_t values;
    values["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2028, 2, 2))] = .05;
    values["IR:EUR,LIBOR_6M_LCH"][DateTime_(Date_(2027, 11, 2))] = .025;
    values["FX[EUR/USD]"][DateTime_(Date_(2028, 2, 4))] = 1.2;
    const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_(values));
    const auto plan = PrepareCalibrationInstrument(instrument, "Price", ScriptContext(DateTime_(valuation), snapshot));
    ASSERT_EQ(std::get<CalibrationXccy_>(plan.Descriptor())->plan_.resets_.size(), 3U);
    const auto discount = [&](double rate, const Date_& date) { return OracleDf(rate, date, valuation); };
    const double domestic = 120. * (1. + ((end - Date_(2028, 2, 4)) / 360.) * .05) * discount(.04, end);
    const auto conversion = [&](const Date_& date) { return 1.1 * discount(.02, date) / discount(.003, date); };
    const double annuity = 100. * ((end - Date_(2027, 11, 4)) / 360.) * conversion(Date_(2028, 5, 8));
    const double foreign = .025 * annuity + 100. * conversion(end);
    const ScriptXccyMarket_<double> market;
    ASSERT_NEAR(EvaluateXccyScript(plan, market.View(DateTime_(valuation))), domestic - foreign - .001 * annuity, 1e-10);
}

TEST(XccyScriptTest, TestLegacyAnonymousIdentityRemainsOutsideSharedAdmission) {
    auto config = ScriptConfig();
    config.domesticRateFixing_ = {};
    const CrossCurrencySwap_ native(XCCY_TODAY, XCCY_START, XCCY_MATURITY, .001, config);
    AssertCode([&] { NewXccyScriptInstrument(native, {"legacy-anonymous", "USD.OIS"}); }, "UnsupportedCalibrationFixingTime");
    const ScriptXccyMarket_<double> market;
    ASSERT_TRUE(std::isfinite(PriceXccyParSpread(BuildXccyCashflowPlan(XCCY_START, XCCY_MATURITY, config), market.View(), MarketFixingSnapshot_())));
}

TEST(XccyScriptTest, TestMonteCarloEntryRejectsCrossCurrencyBeforeModelExecution) {
    CalibrationMcSettings_ settings;
    settings.allowMonteCarloCalibration_ = true;
    settings.fitBatches_ = settings.validationBatches_ = 2;
    settings.fitPathsPerBatch_ = 64;
    settings.validationPathsPerBatch_ = 128;
    settings.maxPathsPerView_ = 384;
    AssertCode([&] { PrepareCalibrationMc(TimestampInstrument(), "Price", ScriptContext(), settings); }, "MCCrossCurrencyUnsupported");
}

TEST(XccyScriptTest, TestFutureFxRejectsSameCurrencyPair) {
    const ScriptXccyMarket_<double> market;
    auto view = market.View();
    view.pair_.foreign_ = Ccy_("USD");
    view.foreign_ = view.domestic_;
    ASSERT_THROW(XccyFxForward(view, XCCY_END), Exception_);
}

TEST(XccyScriptTest, TestFutureFxRejectsNonFiniteResult) {
    const ScriptXccyMarket_<double> market;
    auto view = market.View();
    view.fxSpot_ = std::numeric_limits<double>::max();
    ASSERT_THROW(XccyFxForward(view, XCCY_END), Exception_);
}
