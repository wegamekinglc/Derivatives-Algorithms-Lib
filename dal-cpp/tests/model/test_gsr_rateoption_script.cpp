#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <limits>

#include <dal/math/aad/recording.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/model/gsr.hpp>
#include <dal/model/gsrrateoption.hpp>
#include <dal/model/gsrslvdata.hpp>
#include <dal/script/calibrationmc.hpp>
#include <dal/script/preparation.hpp>
#include <dal/storage/canonicaljson_internal.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

namespace {
    struct LiborBasisScope_ {
        DayBasis_ saved_ = Ccy::Conventions::LiborDayBasis()(Ccy_("USD"));
        LiborBasisScope_() { Ccy::Conventions::LiborDayBasis().XWrite()(Ccy_("USD"), DayBasis_("ACT_360")); }
        ~LiborBasisScope_() { Ccy::Conventions::LiborDayBasis().XWrite()(Ccy_("USD"), saved_); }
    };
    const Date_ V(2026, 10, 21), E(2027, 10, 21), End(2029, 10, 21);

    CalibrationPreparationContext_ Context(double g = .02) {
        CalibrationPreparationContext_ c;
        c.currency_ = Ccy_("USD");
        c.discountRoute_ = "USD.OIS";
        c.valuation_.evaluationDate_ = V;
        c.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        c.curve_.reset(new GSRCurveData_("curve", V, "USD", {V, End}, {0., -.03 * (End - V) / 365.}, {}, {}));
        Handle_<MultiFactorGSRVolData_> vol(
            new MultiFactorGSRVolData_("vol", {"level"}, {V}, Matrix_<>(1, 1, g), {V}, Matrix_<>(1, 1, 1.), Matrix_<>(1, 1, 1.)));
        c.model_.reset(new MultiFactorGSRModelData_("model", c.curve_, vol));
        return c;
    }
    Handle_<CalibrationInstrument_> SourceCaplet() {
        auto spec = NewBondOptionScriptInstrument({E, End, .97, OptionType_("CALL")}, {"base", Ccy_("USD"), "USD.OIS", 100.})->Definition()->Spec();
        spec.source_.rows_[1].text_ = ".03";
        spec.parameters_[1].role_ = ScriptParameterRole_::Value_::RateStrike;
        spec.parameters_[1].unit_ = CalibrationValueUnit_::Parse("Rate");
        spec.parameters_[1].lower_.reset();
        spec.parameters_[1].lowerInclusive_ = true;
        spec.source_.rows_[2].key_ = "2028-02-04";
        spec.source_.rows_[2].text_ = "PAY PAYS N*(0.25555555555555554*MAX(FIX(IR[USD,LIBOR_3M_LCH,2027-10-21],2027-10-21)-K,0))";
        return NewScriptCalibrationInstrument({"caplet", NewScriptInstrumentDefinition(spec)});
    }
    Caplet_ Caplet(bool call = true, double strike = .03) {
        return {E, E, Date_(2028, 1, 21), Date_(2028, 2, 4), 92. / 360., 92. / 360., "3M", strike, OptionType_(call ? "CALL" : "PUT")};
    }
    Swaption_ Swaption(bool call = true, double strike = .03) {
        Swaption_ option{E, {{Date_(2028, 10, 21), 366. / 365.}, {End, 1.}}, {}, strike, OptionType_(call ? "CALL" : "PUT")};
        Date_ start = E;
        for (const Date_ end : {Date_(2028, 4, 21), Date_(2028, 10, 21), Date_(2029, 4, 21), End}) {
            option.floating_.push_back({start, start, end, end, (end - start) / 360., (end - start) / 360., "6M"});
            start = end;
        }
        return option;
    }
    Handle_<CalibrationInstrument_> Instrument(bool caplet, bool call = true, double strike = .03, double notional = 100.) {
        const RateOptionScriptImportSettings_ settings{caplet ? "caplet" : "swaption", Ccy_("USD"), "USD.OIS", notional};
        return caplet ? NewCapletScriptInstrument(Caplet(call, strike), settings) : NewSwaptionScriptInstrument(Swaption(call, strike), settings);
    }
    EuropeanRateOption_ AccrualOption(int location, double accrual, bool call = true, double strike = .03) {
        const Date_ payment(2028, 1, 21);
        const double indexAccrual = Ccy::Conventions::LiborDayBasis()(Ccy_("USD"))(E, payment, nullptr);
        const OptionType_ type(call ? "CALL" : "PUT");
        if (location == 0)
            return Caplet_{E, E, payment, payment, indexAccrual, accrual, "3M", strike, type};
        return Swaption_{E,
                         {{payment, location == 1 ? accrual : .25}},
                         {{E, E, payment, payment, indexAccrual, location == 2 ? accrual : .25, "3M"}},
                         strike,
                         type};
    }
    Handle_<CalibrationInstrument_> AccrualInstrument(int location, double accrual, bool call = true, double strike = .03, double notional = 100.) {
        const RateOptionScriptImportSettings_ settings{"accrual", Ccy_("USD"), "USD.OIS", notional};
        const auto option = AccrualOption(location, accrual, call, strike);
        return location == 0 ? NewCapletScriptInstrument(std::get<Caplet_>(option), settings)
                             : NewSwaptionScriptInstrument(std::get<Swaption_>(option), settings);
    }
    Handle_<CalibrationInstrument_> DecimalAccrualInstrument(int location, const String_& decimal, bool call = true) {
        auto spec = AccrualInstrument(location, .25, call)->Definition()->Spec();
        const String_ libor = "FIX(IR[USD,LIBOR_3M_LCH,2027-10-21],2027-10-21)";
        const String_ df = "FIX(IR[USD,DF,2028-01-21],2027-10-21)";
        if (location == 0)
            spec.source_.rows_[2].text_ = "PAY PAYS N*(" + decimal + "*MAX(" + (call ? libor + "-K" : "K-" + libor) + ",0))";
        else {
            const auto floating = "(" + (location == 2 ? decimal : "0.25") + "*" + libor + "*" + df + ")";
            const auto fixed = "K*(" + (location == 1 ? decimal : "0.25") + "*" + df + ")";
            spec.source_.rows_[2].text_ = "PAY PAYS N*(MAX(" + (call ? floating + "-" + fixed : fixed + "-" + floating) + ",0))";
        }
        return NewScriptCalibrationInstrument({"accrual", NewScriptInstrumentDefinition(spec)});
    }
    void AssertAccrualConstant(const Handle_<CalibrationInstrument_>& instrument, int location, double expected, bool call = true) {
        const auto product = instrument->Product();
        ASSERT_EQ(product->Events().size(), 1);
        ASSERT_EQ(product->Events()[0].size(), 1);
        const Script::Node_* node = product->Events()[0][0].get();
        ASSERT_NE(dynamic_cast<const Script::NodePays_*>(node), nullptr);
        const Vector_<size_t> path = location == 0   ? Vector_<size_t>{1, 1, 0}
                                     : location == 1 ? Vector_<size_t>{1, 1, 0, call ? 1u : 0u, 1, 0}
                                                     : Vector_<size_t>{1, 1, 0, call ? 0u : 1u, 0, 0};
        for (const size_t child : path) {
            ASSERT_GT(node->arguments_.size(), child);
            node = node->arguments_[child].get();
        }
        const auto* constant = dynamic_cast<const Script::NodeConst_*>(node);
        ASSERT_NE(constant, nullptr);
        ASSERT_TRUE(constant->arguments_.empty());
        ASSERT_EQ(constant->constVal_, expected);
    }
    void AssertTinyAccrual(int location) {
        const LiborBasisScope_ basis;
        const auto c = Context();
        for (bool call : {true, false}) {
            const double native = std::visit(
                [&](const auto& option) { return PriceGSREuropeanOption(static_cast<const MultiFactorGSRModelData_&>(*c.model_), option).price_; },
                AccrualOption(location, 1e-7, call));
            ASSERT_TRUE(std::isfinite(native));
            const auto decimal = DecimalAccrualInstrument(location, "0.0000001", call);
            const auto instrument = AccrualInstrument(location, 1e-7, call);
            AssertAccrualConstant(instrument, location, 1e-7, call);
            ASSERT_EQ(instrument->Definition()->Spec().source_.rows_[2].text_, decimal->Definition()->Spec().source_.rows_[2].text_);
            const auto plan = PrepareCalibrationInstrument(instrument, "Price", c);
            ASSERT_EQ(plan.Capability().ruleId_, location == 0 ? "dal.gaussian-caplet/1" : "dal.gaussian-swaption/1");
            ASSERT_EQ(plan.Capability().backend_, CalibrationBackend_::Value_::FastGaussian);
            const auto& descriptor = std::get<CalibrationRateOptionDescriptor_>(plan.Descriptor());
            const double accrual = location == 0   ? std::get<Caplet_>(descriptor.option_).couponAccrual_
                                   : location == 1 ? std::get<Swaption_>(descriptor.option_).fixed_[0].accrual_
                                                   : std::get<Swaption_>(descriptor.option_).floating_[0].couponAccrual_;
            ASSERT_EQ(accrual, 1e-7);
            const double actual = PriceGaussianRateOption(plan, instrument, c).price_;
            ASSERT_EQ(actual, PriceGaussianRateOption(PrepareCalibrationInstrument(decimal, "Price", c), decimal, c).price_);
            if (native == 0.)
                ASSERT_EQ(actual, 0.);
            else
                ASSERT_LE(std::abs(actual / (100. * native) - 1.), 1e-12);
        }
    }
    double Fast(const Handle_<CalibrationInstrument_>& instrument, const CalibrationPreparationContext_& context) {
        return PriceGaussianRateOption(PrepareCalibrationInstrument(instrument, "Price", context), instrument, context).price_;
    }
    double Cdf(double x) { return .5 * std::erfc(-x / std::sqrt(2.)); }
    double Density(double x) { return std::exp(-.5 * x * x) / std::sqrt(2. * std::acos(-1.)); }
    std::pair<double, double> CapletOracle(double g, bool call, double strike = .03, double notional = 100.) {
        const auto option = Caplet(call, strike);
        const double tau = (option.end_ - option.start_) / 365., lag = (option.payment_ - option.end_) / 365.;
        const double sigma = g * tau, forward = std::exp(.03 * tau - g * g * tau * lag);
        const double bondStrike = 1. + option.indexAccrual_ * strike, direction = call ? 1. : -1.;
        const double discount = std::exp(-.03 * (option.payment_ - V) / 365.);
        const double d1 = std::log(forward / bondStrike) / sigma + .5 * sigma, d2 = d1 - sigma;
        const double price = direction * (forward * Cdf(direction * d1) - bondStrike * Cdf(direction * d2));
        const double derivative = direction * Cdf(direction * d1) * (-2. * g * tau * lag * forward) + forward * Density(d1) * tau;
        return {notional * discount * price, notional * discount * derivative};
    }
    std::pair<double, double> SwaptionOracle(double g, bool call) {
        const auto option = Swaption(call);
        const double tau1 = (option.fixed_[0].payment_ - E) / 365., tau2 = (End - E) / 365.;
        const double a = .03 * option.fixed_[0].accrual_ * std::exp(-.03 * tau1);
        const double b = (1. + .03 * option.fixed_[1].accrual_) * std::exp(-.03 * tau2);
        const double beta1 = g * tau1, beta2 = g * tau2;
        const auto underlying = [&](double z) {
            return 1. - a * std::exp(-beta1 * z - .5 * beta1 * beta1) - b * std::exp(-beta2 * z - .5 * beta2 * beta2);
        };
        double lo = -14., hi = 14.;
        for (int i = 0; i < 100; ++i) {
            const double mid = .5 * (lo + hi);
            if (underlying(mid) < 0.)
                lo = mid;
            else
                hi = mid;
        }
        const double root = .5 * (lo + hi), direction = call ? 1. : -1., discount = 100. * std::exp(-.03);
        const double price = direction * (Cdf(-direction * root) - a * Cdf(-direction * (root + beta1)) - b * Cdf(-direction * (root + beta2)));
        return {discount * price, discount * (a * Density(root + beta1) * tau1 + b * Density(root + beta2) * tau2)};
    }
    std::pair<double, double> Oracle(bool caplet, double g, bool call) { return caplet ? CapletOracle(g, call) : SwaptionOracle(g, call); }
    CalibrationMcSettings_ McSettings(bool aad = true) {
        CalibrationMcSettings_ s;
        s.allowMonteCarloCalibration_ = true;
        s.aad_ = aad;
        s.seed_ = 1024;
        s.fitBatches_ = 8;
        s.fitPathsPerBatch_ = 8192;
        s.validationBatches_ = 16;
        s.validationPathsPerBatch_ = 16384;
        s.maxPathsPerView_ = 327680;
        if (aad)
            s.parameterLabels_ = {"g:level:2026-10-21"};
        return s;
    }
    CalibrationMcPrecision_ Precision() {
        CalibrationMcPrecision_ p;
        p.errorScale_ = .1;
        p.parameterScales_ = {.01};
        p.epsilonGradient_ = {1e-8};
        return p;
    }
    CalibrationMcReference_ Reference(const Handle_<CalibrationInstrument_>& instrument, double g) {
        const auto fd = [&](double h) { return (Fast(instrument, Context(g + h)) - Fast(instrument, Context(g - h))) / (2. * h); };
        const double coarse = fd(1e-4), fine = fd(5e-5);
        return {Fast(instrument, Context(g)), 0., 0., {fine}, {0.}, {std::abs(fine - coarse)}};
    }
    void Record(const std::string& key, const CalibrationMcResult_& result, const CalibrationMcReference_& reference) {
        const auto field = [&](const std::string& name, const auto& value) { ::testing::Test::RecordProperty(key + "_" + name, value); };
        field("mean", result.mean_);
        field("se", result.standardError_);
        field("gradient", result.gradient_[0]);
        field("gradient_se", result.gradientStandardError_[0]);
        field("fast", reference.value_);
        field("fd_fine", reference.gradient_[0]);
        field("fd_refinement", reference.finiteDifferenceError_[0]);
        field("instrument", std::string(result.instrumentIdentity_.c_str()));
        field("context", std::string(result.contextIdentity_.c_str()));
        field("plan", std::string(result.planIdentity_.c_str()));
        field("mapping", std::string(result.mappingIdentity_.c_str()));
        for (size_t i = 0; i < result.batches_.size(); ++i) {
            const auto batch = "batch_" + std::to_string(i) + "_";
            field(batch + "first", static_cast<int>(result.ranges_[i].firstPath_));
            field(batch + "paths", static_cast<int>(result.ranges_[i].pathCount_));
            field(batch + "value", result.batches_[i].value_);
            field(batch + "gradient", result.batches_[i].derivatives_[0]);
        }
    }
    CalibrationPreparationContext_ Factors(int count, bool independent) {
        auto c = Context();
        MultiFactorGSRVolSettings_ s;
        s.factorNames_ = {"one", "two", "three", "four"};
        s.gKnotDates_ = {V};
        s.hKnotDates_ = {V, Date_(2028, 4, 21), Date_(2028, 10, 21), Date_(2029, 4, 21)};
        s.gValues_ = Matrix_<>(count, 1, .01);
        s.hValues_ = Matrix_<>(count, 4, independent ? 0. : 1.);
        s.correlations_ = Matrix_<>(count, count, 0.);
        for (int i = 0; i < count; ++i) {
            s.correlations_(i, i) = 1.;
            if (independent)
                s.hValues_(i, i) = 1.;
        }
        Handle_<MultiFactorGSRVolData_> vol(new MultiFactorGSRVolData_("vol", s));
        c.model_.reset(new MultiFactorGSRModelData_("factors", c.curve_, vol));
        return c;
    }
} // namespace

TEST(GSRRateOptionScriptTest, TestCompleteCapletSourceReachesFastGaussian) {
    const LiborBasisScope_ basis;
    const auto plan = PrepareCalibrationInstrument(SourceCaplet(), "Price", Context());
    ASSERT_EQ(plan.Capability().ruleId_, "dal.gaussian-caplet/1");
    ASSERT_EQ(plan.Capability().backend_, CalibrationBackend_::Value_::FastGaussian);
}

TEST(GSRRateOptionScriptTest, TestTinyCapletAccrual) { AssertTinyAccrual(0); }

TEST(GSRRateOptionScriptTest, TestTinySwaptionFixedAccrual) { AssertTinyAccrual(1); }

TEST(GSRRateOptionScriptTest, TestTinySwaptionFloatingAccrual) { AssertTinyAccrual(2); }

TEST(GSRRateOptionScriptTest, TestAccrualLiteralNormalBoundariesAreExactConstants) {
    const LiborBasisScope_ basis;
    for (int location : {0, 1, 2})
        for (double value :
             {.25, 1e-6, std::nextafter(1e-6, 0.), std::nextafter(1e-6, 1.), 1e-7, 1.2345678901234567e-7, 1.2345678901234568e22, 1e20, 1e21,
              std::nextafter(1e21, 0.), std::nextafter(1e21, 1e22), std::numeric_limits<double>::min(), std::numeric_limits<double>::max()}) {
            SCOPED_TRACE(::testing::Message() << location << " " << value);
            const auto instrument = AccrualInstrument(location, value);
            AssertAccrualConstant(instrument, location, value);
            const std::string source(instrument->Definition()->Spec().source_.rows_[2].text_.c_str());
            const std::string marker = location == 0 ? "N*(" : location == 1 ? "K*(" : "MAX((";
            const auto begin = source.find(marker);
            ASSERT_NE(begin, std::string::npos);
            const auto literal = source.substr(begin + marker.size(), source.find('*', begin + marker.size()) - begin - marker.size());
            ASSERT_EQ(literal.find_first_not_of("0123456789."), std::string::npos);
            ASSERT_LE(literal.size(), 512);
        }
}

TEST(GSRRateOptionScriptTest, TestSubnormalAccrualUsesIndependentDecimalParserBoundary) {
    const LiborBasisScope_ basis;
    const Vector_<std::pair<double, String_>> cases{
        {std::numeric_limits<double>::denorm_min(), String_("0." + std::string(323, '0') + "5")},
        {std::nextafter(std::numeric_limits<double>::min(), 0.), String_("0." + std::string(307, '0') + "2225073858507201")}};
    for (int location : {0, 1, 2})
        for (const auto& [value, decimal] : cases) {
            Handle_<CalibrationInstrument_> imported, independent;
            std::optional<CalibrationError_> actualError, expectedError;
            try {
                independent = DecimalAccrualInstrument(location, decimal);
            } catch (const CalibrationError_& error) {
                expectedError = error;
            }
            try {
                imported = AccrualInstrument(location, value);
            } catch (const CalibrationError_& error) {
                actualError = error;
            }
            ASSERT_EQ(actualError.has_value(), expectedError.has_value());
            const auto key = "subnormal_" + std::to_string(location) + (value == std::numeric_limits<double>::denorm_min() ? "_min" : "_max");
            RecordProperty(key, expectedError ? std::string(expectedError->Failure().diagnostic_.actual_.value_or("").c_str()) : "parsed");
            if (expectedError) {
                const auto& actual = actualError->Failure().diagnostic_;
                const auto& expected = expectedError->Failure().diagnostic_;
                ASSERT_EQ(expected.code_, "InvalidScriptInstrumentDefinition");
                ASSERT_EQ(expected.stage_, CalibrationStage_::Value_::Input);
                ASSERT_EQ(expected.field_, "/source/rows/2/text");
                ASSERT_EQ(actual.code_, expected.code_);
                ASSERT_EQ(actual.stage_, expected.stage_);
                ASSERT_EQ(actual.field_, expected.field_);
                ASSERT_EQ(actual.actual_, expected.actual_);
                ASSERT_EQ(actual.instrumentId_, expected.instrumentId_);
                ASSERT_EQ(actual.sourceRow_, expected.sourceRow_);
                ASSERT_EQ(actual.eventDate_, expected.eventDate_);
                ASSERT_EQ(actual.statement_, expected.statement_);
                ASSERT_TRUE(actualError->Failure().report_);
                ASSERT_EQ(actualError->Failure().report_->Errors().size(), 1);
            } else {
                ASSERT_TRUE(imported);
                AssertAccrualConstant(independent, location, value);
                AssertAccrualConstant(imported, location, value);
                ASSERT_EQ(imported->Definition()->SourceIdentity(), independent->Definition()->SourceIdentity());
            }
        }
}

TEST(GSRRateOptionScriptTest, TestInvalidAccrualRetainsOwningInputErrors) {
    const LiborBasisScope_ basis;
    const double inf = std::numeric_limits<double>::infinity();
    for (int location : {0, 1, 2})
        for (double value : {0., -0., -1e-7, std::numeric_limits<double>::quiet_NaN(), inf, -inf}) {
            std::optional<CalibrationError_> saved;
            try {
                static_cast<void>(AccrualInstrument(location, value));
            } catch (const CalibrationError_& error) {
                saved = error;
            }
            ASSERT_TRUE(saved);
            const auto& d = saved->Failure().diagnostic_;
            ASSERT_EQ(d.code_, "UnsupportedCalibrationProduct");
            ASSERT_EQ(d.stage_, CalibrationStage_::Value_::Input);
            ASSERT_EQ(d.field_, location == 1 ? "/option/fixed" : "/option/coupon");
            ASSERT_EQ(d.instrumentId_, "accrual");
            ASSERT_TRUE(saved->Failure().report_);
            ASSERT_EQ(saved->Failure().report_->Stage(), CalibrationStage_::Value_::Input);
            ASSERT_EQ(saved->Failure().report_->Errors().size(), 1);
        }
}

TEST(GSRRateOptionScriptTest, TestAccrualArchivesRebindingAndStalePlans) {
    const LiborBasisScope_ basis;
    const auto c = Context();
    for (int location : {0, 1, 2})
        for (double value : {.25, 1e-7}) {
            const auto instrument = AccrualInstrument(location, value);
            const auto plan = PrepareCalibrationInstrument(instrument, "Price", c);
            const auto json = DumpCalibrationInstrumentJson(instrument);
            const auto portable = LoadCalibrationInstrumentJson(json);
            const auto archive = handle_cast<CalibrationInstrument_>(JSON::ReadString(JSON::WriteString(*instrument), false));
            for (const auto& restored : Vector_<Handle_<CalibrationInstrument_>>{portable, archive}) {
                ASSERT_TRUE(restored);
                ASSERT_EQ(DumpCalibrationInstrumentJson(restored), json);
                ASSERT_EQ(restored->Definition()->SourceIdentity(), instrument->Definition()->SourceIdentity());
                ASSERT_EQ(restored->Definition()->DefinitionIdentity(), instrument->Definition()->DefinitionIdentity());
                ASSERT_EQ(restored->BindingIdentity(), instrument->BindingIdentity());
                ASSERT_EQ(restored->InstrumentIdentity(), instrument->InstrumentIdentity());
                ASSERT_EQ(Fast(restored, c), Fast(instrument, c));
            }
            const auto rebound = BindCalibrationInstrumentParameters(
                instrument, {{{"N", 200., CalibrationValueUnit_::Parse("Price:USD")}, {"K", -.01, CalibrationValueUnit_::Parse("Rate")}}});
            ASSERT_EQ(DumpCalibrationInstrumentJson(instrument), json);
            ASSERT_THROW(PriceGaussianRateOption(plan, rebound, c), CalibrationError_);
            ASSERT_THROW(PriceGaussianRateOption(plan, AccrualInstrument(location, value == .25 ? 1e-7 : .25), c), CalibrationError_);
            ASSERT_THROW(PriceGaussianRateOption(plan, instrument, Context(.03)), CalibrationError_);
            const double native = std::visit(
                [&](const auto& option) { return PriceGSREuropeanOption(static_cast<const MultiFactorGSRModelData_&>(*c.model_), option).price_; },
                AccrualOption(location, value, true, -.01));
            ASSERT_GT(native, 0.);
            ASSERT_LE(std::abs(Fast(rebound, c) / (200. * native) - 1.), 1e-12);
        }
}

TEST(GSRRateOptionScriptTest, TestTinyAccrualTreeCompiledAmountsAndNumeraire) {
    const LiborBasisScope_ basis;
    const auto c = Context();
    for (int location : {0, 1, 2})
        for (bool call : {true, false})
            for (double libor : {0., .04, 100000.}) {
                const double floating = (location == 2 ? 1e-7 : .25) * libor * .91;
                const double fixed = .03 * (location == 1 ? 1e-7 : .25) * .91;
                const double difference = location == 0 ? 1e-7 * (libor - .03) : floating - fixed;
                const double expected = 100. * std::max(call ? difference : -difference, 0.) / 1.7;
                for (const auto& instrument : Vector_<Handle_<CalibrationInstrument_>>{AccrualInstrument(location, 1e-7, call),
                                                                                       DecimalAccrualInstrument(location, "0.0000001", call)}) {
                    AAD::GSR_<double> model(static_cast<const MultiFactorGSRModelData_&>(*c.model_));
                    Script::MonteCarloSettings_ simulation;
                    simulation.compiled_ = true;
                    const auto prepared = Script::Detail::PrepareBoundScript(instrument->Product(), &model, c.valuation_, simulation,
                                                                             instrument->Definition()->Spec().source_.settings_);
                    AAD::Scenario_<> path;
                    AAD::AllocatePath(prepared.DefLine(), path);
                    for (size_t sample = 0; sample < path.size(); ++sample) {
                        path[sample].numeraire_ = 1.7;
                        for (size_t i = 0; i < path[sample].observations_.size(); ++i)
                            path[sample].observations_[i] =
                                std::string(prepared.DefLine()[sample].indexNames_[i].c_str()).find("LIBOR") != std::string::npos ? libor : .91;
                    }
                    auto tree = prepared.BuildEvaluator<double>();
                    prepared.Evaluate(path, tree);
                    auto compiled = prepared.BuildEvalState<double>();
                    prepared.CompiledProgram().Evaluate(path, compiled);
                    for (double actual : {tree.VarVals()[prepared.PayOffIdx()], compiled.VarVals()[prepared.PayOffIdx()]}) {
                        if (expected == 0.)
                            ASSERT_EQ(actual, 0.);
                        else
                            ASSERT_LE(std::abs(actual / expected - 1.), 1e-12);
                    }
                }
            }
}

TEST(GSRRateOptionScriptTest, TestAccrualCompatibilitySnapshots) {
    const LiborBasisScope_ basis;
    const auto c = Context();
    int ordinal = 0;
    std::string snapshot;
    for (int location : {0, 1, 2})
        for (bool call : {true, false})
            for (const auto& values :
                 Vector_<Vector_<>>{{.25, 100., .03}, {1e-6, 100., .03}, {.25, 1e-7, 1e-7}, {.25, 1e22, 1e-7}, {.25, 100., 0.}, {.25, 100., -.01}}) {
                const auto instrument = AccrualInstrument(location, values[0], call, values[2], values[1]);
                const auto plan = PrepareCalibrationInstrument(instrument, "Price", c);
                const auto prefix = "case_" + std::to_string(ordinal++) + "_";
                const auto record = [&](const char* key, const String_& value) {
                    RecordProperty(prefix + key, std::string(value.c_str()));
                    snapshot += prefix + key + "\n" + std::string(value.c_str()) + "\n";
                };
                record("source", instrument->Definition()->SourceIdentity());
                record("definition", instrument->Definition()->DefinitionIdentity());
                record("binding", instrument->BindingIdentity());
                record("instrument", instrument->InstrumentIdentity());
                record("compiler", plan.CompilerIdentity());
                record("context", plan.ContextIdentity());
                record("plan", plan.PlanIdentity());
                record("json", DumpCalibrationInstrumentJson(instrument));
                for (size_t row = 0; row < 3; ++row) {
                    const auto& source = instrument->Definition()->Spec().source_.rows_[row];
                    record(("row_" + std::to_string(row) + "_key").c_str(), source.key_);
                    record(("row_" + std::to_string(row) + "_text").c_str(), source.text_);
                }
            }
    ASSERT_EQ(CanonicalJson::FingerprintCanonical(snapshot), "sha256:e6449292ee247ec0e27d808d028b63133a23a39d62b761d6126e41b5526ef8b0");
}

TEST(GSRRateOptionScriptTest, TestCompleteSwaptionSourceReachesFastGaussian) {
    const LiborBasisScope_ basis;
    const auto plan = PrepareCalibrationInstrument(Instrument(false), "Price", Context());
    ASSERT_EQ(plan.Capability().ruleId_, "dal.gaussian-swaption/1");
    ASSERT_EQ(plan.Capability().backend_, CalibrationBackend_::Value_::FastGaussian);
}

TEST(GSRRateOptionScriptTest, TestIdenticallyZeroRatePayoffRefusesHardAad) {
    const LiborBasisScope_ basis;
    const Date_ start(2028, 4, 21), end(2028, 10, 21);
    const Swaption_ option{E, {{end, .5}}, {{start, start, end, end, 183. / 360., 183. / 360., "6M"}}, 0., OptionType_("CALL")};
    const auto instrument = NewSwaptionScriptInstrument(option, {"zero", Ccy_("USD"), "USD.OIS", 100.});
    auto c = Context();
    c.curve_.reset(new GSRCurveData_("zero", V, "USD", {V, End}, {0., 0.}, {}, {}));
    Matrix_<> h(1, 2, 0.);
    h(0, 0) = 1.;
    Handle_<MultiFactorGSRVolData_> vol(
        new MultiFactorGSRVolData_("stopped", {"level"}, {V}, Matrix_<>(1, 1, .02), {V, start}, h, Matrix_<>(1, 1, 1.)));
    c.model_.reset(new MultiFactorGSRModelData_("stopped", c.curve_, vol));
    ASSERT_DOUBLE_EQ(Fast(instrument, c), 0.);
    auto settings = McSettings();
    ASSERT_THROW(PrepareCalibrationMc(instrument, "Price", c, settings), CalibrationError_);
}

TEST(GSRRateOptionScriptTest, TestExportersAndFastPriceUseCompleteBoundSource) {
    const LiborBasisScope_ basis;
    for (bool caplet : {true, false})
        for (bool call : {true, false}) {
            const auto instrument = Instrument(caplet, call);
            const auto restored = LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(instrument));
            ASSERT_EQ(instrument->InstrumentIdentity(), restored->InstrumentIdentity());
            for (double g : {.012, .016, .02, .0201, .0199, .02005, .01995}) {
                SCOPED_TRACE(::testing::Message() << caplet << " " << call << " " << g);
                const auto c = Context(g);
                const auto& model = static_cast<const MultiFactorGSRModelData_&>(*c.model_);
                const double native =
                    caplet ? PriceGSREuropeanOption(model, Caplet(call)).price_ : PriceGSREuropeanOption(model, Swaption(call)).price_;
                ASSERT_NEAR(Fast(restored, c), 100. * native, 1e-10);
                ASSERT_NEAR(Fast(restored, c), Oracle(caplet, g, call).first, 1e-10);
                ASSERT_NEAR(Reference(restored, g).gradient_[0], Oracle(caplet, g, call).second, 2e-4);
            }
            const auto rebound = BindCalibrationInstrumentParameters(
                instrument, {{{"K", -.01, CalibrationValueUnit_::Parse("Rate")}, {"N", 200., CalibrationValueUnit_::Parse("Price:USD")}}});
            const auto c = Context();
            const auto& model = static_cast<const MultiFactorGSRModelData_&>(*c.model_);
            const double native =
                caplet ? PriceGSREuropeanOption(model, Caplet(call, -.01)).price_ : PriceGSREuropeanOption(model, Swaption(call, -.01)).price_;
            ASSERT_NEAR(Fast(rebound, c), 200. * native, 1e-10);
            ASSERT_NEAR(Fast(instrument, c), Oracle(caplet, .02, call).first, 1e-10);
        }
}

TEST(GSRRateOptionScriptTest, TestOwnedSourceAndConventionRefusals) {
    const LiborBasisScope_ basis;
    for (bool caplet : {true, false}) {
        const auto instrument = Instrument(caplet);
        const auto original = instrument->Definition()->Spec();
        for (const String_ text :
             {original.source_.rows_.back().text_ + "+1", original.source_.rows_.back().text_ + " PAY PAYS 1",
              String_("IF K>0 THEN ") + original.source_.rows_.back().text_ + " END", original.source_.rows_.back().text_ + " ON 2029-10-23"}) {
            auto spec = original;
            spec.source_.rows_.back().text_ = text;
            const auto changed = NewScriptCalibrationInstrument({"changed", NewScriptInstrumentDefinition(spec)});
            std::optional<CalibrationError_> saved;
            try {
                static_cast<void>(Fast(changed, Context()));
            } catch (const CalibrationError_& error) {
                saved = error;
            }
            ASSERT_TRUE(saved);
            ASSERT_TRUE(saved->Failure().report_);
            ASSERT_EQ(saved->Failure().diagnostic_.stage_, CalibrationStage_::Value_::Prepare);
            ASSERT_EQ(saved->Failure().diagnostic_.sourceRow_, 3);
            ASSERT_EQ(saved->Failure().diagnostic_.statement_, 0);
        }
    }
    const RateOptionScriptImportSettings_ settings{"bad", Ccy_("USD"), "USD.OIS", 100.};
    auto caplet = Caplet();
    caplet.indexAccrual_ = .25;
    ASSERT_THROW(NewCapletScriptInstrument(caplet, settings), CalibrationError_);
    caplet = Caplet();
    caplet.end_ = caplet.end_.AddDays(1);
    ASSERT_THROW(NewCapletScriptInstrument(caplet, settings), CalibrationError_);
    caplet = Caplet();
    caplet.tenor_ = "1Y";
    ASSERT_THROW(NewCapletScriptInstrument(caplet, settings), CalibrationError_);
    auto swaption = Swaption();
    swaption.floating_[0].payment_ = swaption.floating_[0].payment_.AddDays(1);
    ASSERT_THROW(NewSwaptionScriptInstrument(swaption, settings), CalibrationError_);
}

TEST(GSRRateOptionScriptTest, TestPlanInvalidationAndModelDomain) {
    const LiborBasisScope_ basis;
    for (bool caplet : {true, false}) {
        const auto instrument = Instrument(caplet),
                   rebound = BindCalibrationInstrumentParameters(instrument, {{{"K", 0., CalibrationValueUnit_::Parse("Rate")}}});
        const auto c = Context();
        const auto plan = PrepareCalibrationInstrument(instrument, "Price", c);
        ASSERT_THROW(PriceGaussianRateOption(plan, rebound, c), CalibrationError_);
        ASSERT_THROW(PriceGaussianRateOption(plan, instrument, Context(.03)), CalibrationError_);
        auto changed = c;
        changed.smoothing_ = .001;
        ASSERT_THROW(Fast(instrument, changed), CalibrationError_);
        ASSERT_THROW(PriceGaussianRateOption(plan, instrument, changed), CalibrationError_);
        changed = c;
        Handle_<GSRLeverageData_> leverage(new GSRLeverageData_("flat", {0.}, {0.}, Matrix_<>(1, 1, 1.)));
        changed.model_.reset(new GSRSLVModelData_("slv", handle_cast<MultiFactorGSRModelData_>(c.model_), leverage, {}));
        ASSERT_THROW(Fast(instrument, changed), CalibrationError_);
        changed = c;
        Handle_<GSRVolData_> vol(new GSRVolData_("legacy", {V}, {.02}, {V}, {1.}));
        changed.model_.reset(new GSRModelData_("legacy", c.curve_, vol));
        ASSERT_NEAR(Fast(instrument, changed), Fast(instrument, c), 1e-10);
        ASSERT_THROW(PrepareCalibrationMc(instrument, "Price", Context(0.), McSettings()), CalibrationError_);
        ASSERT_TRUE(std::isfinite(Fast(instrument, Context(0.))));
    }
}

TEST(GSRRateOptionScriptTest, TestImporterSnapshotAndUnsupportedContexts) {
    const LiborBasisScope_ basis;
    auto input = Caplet();
    const auto instrument = NewCapletScriptInstrument(input, {"snapshot", Ccy_("USD"), "USD.OIS", 100.});
    const auto original = instrument->InstrumentIdentity();
    input.strike_ = .5;
    input.payment_ = End;
    ASSERT_EQ(original, instrument->InstrumentIdentity());
    ASSERT_NEAR(Fast(instrument, Context()), CapletOracle(.02, true).first, 1e-10);
    for (bool caplet : {true, false}) {
        const auto rate = Instrument(caplet);
        auto c = Context();
        const auto vol = static_cast<const MultiFactorGSRModelData_&>(*c.model_).vol_;
        c.curve_.reset(new GSRCurveData_("projection", V, "USD", {V, End}, {0., -.09}, {"3M"}, Matrix_<>(1, 2, 0.)));
        c.model_.reset(new MultiFactorGSRModelData_("projection", c.curve_, vol));
        ASSERT_THROW(Fast(rate, c), CalibrationError_);
        c = Context();
        c.curve_.reset(new GSRCurveData_("short", V, "USD", {V, E}, {0., -.03}, {}, {}));
        c.model_.reset(new MultiFactorGSRModelData_("short", c.curve_, vol));
        ASSERT_THROW(Fast(rate, c), CalibrationError_);
        c = Context();
        c.curve_.reset(new GSRCurveData_("different", V, "USD", {V, End}, {0., -.1}, {}, {}));
        ASSERT_THROW(Fast(rate, c), CalibrationError_);
        auto options = CalibrationEvaluationOptions_{};
        options.evaluation_ = CalibrationEvaluation_::Value_::ScriptMC;
        ASSERT_THROW(PrepareCalibrationInstrument(rate, "Price", Context(), options), CalibrationError_);
        options.evaluation_ = CalibrationEvaluation_::Value_::GenericDeterministic;
        ASSERT_THROW(PrepareCalibrationInstrument(rate, "Price", Context(), options), CalibrationError_);
        auto spec = rate->Definition()->Spec();
        spec.header_.settlement_ = ScriptSettlement_();
        ASSERT_THROW(NewScriptInstrumentDefinition(spec), CalibrationError_);
    }
}

TEST(GSRRateOptionScriptTest, TestIdenticalScenarioUsesObservationAxisAndNumeraire) {
    const LiborBasisScope_ basis;
    for (bool caplet : {true, false}) {
        const auto instrument = Instrument(caplet);
        const auto c = Context();
        AAD::GSR_<double> model(static_cast<const MultiFactorGSRModelData_&>(*c.model_));
        Script::MonteCarloSettings_ simulation;
        simulation.compiled_ = true;
        const auto p = Script::Detail::PrepareBoundScript(instrument->Product(), &model, c.valuation_, simulation,
                                                          instrument->Definition()->Spec().source_.settings_);
        AAD::Scenario_<> path;
        AAD::AllocatePath(p.DefLine(), path);
        double value = 0.;
        for (size_t sample = 0; sample < path.size(); ++sample) {
            path[sample].numeraire_ = 1.7 * static_cast<double>(sample + 1);
            for (size_t i = 0; i < path[sample].observations_.size(); ++i) {
                const auto& name = p.DefLine()[sample].indexNames_[i];
                const double observation = std::string(name.c_str()).find("LIBOR") != std::string::npos ? .04 : .91;
                path[sample].observations_[i] = observation;
            }
        }
        if (caplet)
            value = 100. * (92. / 360.) * (.04 - .03) / path.back().numeraire_;
        else {
            double floating = 0., annuity = 0.;
            for (const auto& coupon : Swaption().floating_)
                floating += coupon.couponAccrual_ * .04 * .91;
            for (const auto& coupon : Swaption().fixed_)
                annuity += coupon.accrual_ * .91;
            value = 100. * std::max(floating - .03 * annuity, 0.) / 1.7;
        }
        auto tree = p.BuildEvaluator<double>();
        p.Evaluate(path, tree);
        auto compiled = p.BuildEvalState<double>();
        p.CompiledProgram().Evaluate(path, compiled);
        ASSERT_NEAR(tree.VarVals()[p.PayOffIdx()], value, 1e-12);
        ASSERT_NEAR(compiled.VarVals()[p.PayOffIdx()], value, 1e-12);
    }
}

TEST(GSRRateOptionScriptTest, TestTreeCompiledAadAndCrnDerivativesAtBothSteps) {
    const LiborBasisScope_ basis;
    for (bool caplet : {true, false}) {
        const auto instrument = Instrument(caplet);
        const auto c = Context();
        auto s = McSettings();
        s.fitBatches_ = s.validationBatches_ = 2;
        s.fitPathsPerBatch_ = 8;
        s.validationPathsPerBatch_ = 16;
        s.maxPathsPerView_ = 48;
        s.retainPaths_ = true;
        const auto compiled = PrepareCalibrationMc(instrument, "Price", c, s).Evaluate(instrument, c, false);
        s.compiled_ = false;
        const auto tree = PrepareCalibrationMc(instrument, "Price", c, s).Evaluate(instrument, c, false);
        ASSERT_EQ(compiled.pathValues_, tree.pathValues_);
        ASSERT_NEAR(compiled.gradient_[0], tree.gradient_[0], 1e-10);
        s.aad_ = false;
        s.parameterLabels_.clear();
        for (double h : {1e-5, 5e-6}) {
            const auto u = Context(.02 + h), d = Context(.02 - h);
            const auto up = PrepareCalibrationMc(instrument, "Price", u, s).Evaluate(instrument, u, false);
            const auto down = PrepareCalibrationMc(instrument, "Price", d, s).Evaluate(instrument, d, false);
            ASSERT_EQ(up.mappingIdentity_, down.mappingIdentity_);
            for (int i = 0; i < 16; ++i)
                ASSERT_NEAR(compiled.pathDerivatives_(i, 0), (up.pathValues_[i] - down.pathValues_[i]) / (2. * h), 1e-5);
        }
    }
}

TEST(GSRRateOptionScriptTest, TestNativeEffectiveRankLimitIsReused) {
    const LiborBasisScope_ basis;
    const auto instrument = Instrument(false);
    const auto rankOne = Factors(4, false);
    const auto plan = PrepareCalibrationInstrument(instrument, "Price", rankOne);
    ASSERT_NEAR(PriceGaussianRateOption(plan, instrument, rankOne).price_, Fast(instrument, Context(.02)), 1e-10);
    std::optional<CalibrationError_> saved;
    try {
        static_cast<void>(PrepareCalibrationInstrument(instrument, "Price", Factors(4, true)));
    } catch (const CalibrationError_& error) {
        saved = error;
    }
    ASSERT_TRUE(saved);
    ASSERT_EQ(saved->Failure().diagnostic_.code_, "UnsupportedFastEvaluator");
    ASSERT_EQ(saved->Failure().diagnostic_.stage_, CalibrationStage_::Value_::Prepare);
    ASSERT_NE(std::string(saved->what()).find("at most three effective Gaussian directions"), std::string::npos);
    ASSERT_TRUE(saved->Failure().report_);
}

TEST(GSRRateOptionScriptTest, TestEffectiveLiborConventionInvalidatesFastAndMcPlans) {
    const LiborBasisScope_ basis;
    for (bool caplet : {true, false}) {
        const auto instrument = Instrument(caplet);
        const auto c = Context();
        const auto fast = PrepareCalibrationInstrument(instrument, "Price", c);
        const auto mc = PrepareCalibrationMc(instrument, "Price", c, McSettings());
        Ccy::Conventions::LiborDayBasis().XWrite()(Ccy_("USD"), DayBasis_("ACT_365F"));
        ASSERT_THROW(PriceGaussianRateOption(fast, instrument, c), CalibrationError_);
        ASSERT_THROW(static_cast<void>(mc.Evaluate(instrument, c, false)), CalibrationError_);
        ASSERT_NE(fast.ContextIdentity(), PrepareCalibrationInstrument(instrument, "Price", c).ContextIdentity());
        Ccy::Conventions::LiborDayBasis().XWrite()(Ccy_("USD"), DayBasis_("ACT_360"));
        ASSERT_TRUE(std::isfinite(PriceGaussianRateOption(fast, instrument, c).price_));
    }
}

TEST(GSRRateOptionScriptTest, TestRebindingUpdatesBothMcViewsWithoutChangingTheOriginal) {
    const LiborBasisScope_ basis;
    for (bool caplet : {true, false}) {
        const auto original = Instrument(caplet);
        const auto doubled = BindCalibrationInstrumentParameters(original, {{{"N", 200., CalibrationValueUnit_::Parse("Price:USD")}}});
        const auto restruck = BindCalibrationInstrumentParameters(original, {{{"K", -.01, CalibrationValueUnit_::Parse("Rate")}}});
        const auto c = Context();
        auto s = McSettings();
        s.fitBatches_ = s.validationBatches_ = 2;
        s.fitPathsPerBatch_ = 8;
        s.validationPathsPerBatch_ = 16;
        s.maxPathsPerView_ = 48;
        const auto plan = PrepareCalibrationMc(original, "Price", c, s);
        const auto before = plan.Evaluate(original, c, false);
        const auto twice = PrepareCalibrationMc(doubled, "Price", c, s).Evaluate(doubled, c, false);
        ASSERT_DOUBLE_EQ(twice.mean_, 2. * before.mean_);
        ASSERT_DOUBLE_EQ(twice.gradient_[0], 2. * before.gradient_[0]);
        ASSERT_EQ(before.mappingIdentity_, twice.mappingIdentity_);
        ASSERT_NE(before.planIdentity_, twice.planIdentity_);
        ASSERT_NE(PrepareCalibrationMc(restruck, "Price", c, s).Evaluate(restruck, c, false).mean_, before.mean_);
        ASSERT_EQ(plan.Evaluate(original, c, false).gradient_, before.gradient_);
        ASSERT_THROW(static_cast<void>(plan.Evaluate(doubled, c, false)), CalibrationError_);
        ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
    }
}

TEST(GSRRateOptionScriptTest, TestFastPriceOwnsKernelFailureAndPreservesLegacyException) {
    const LiborBasisScope_ basis;
    for (bool caplet : {true, false}) {
        std::optional<CalibrationError_> saved;
        std::string cause;
        {
            const auto c = Context();
            const auto instrument = Instrument(caplet);
            const auto plan = PrepareCalibrationInstrument(instrument, "Price", c);
            try {
                static_cast<void>(PriceGSREuropeanOption(static_cast<const MultiFactorGSRModelData_&>(*c.model_), Swaption(), {0, true}));
            } catch (const CalibrationError_&) {
                FAIL() << "legacy error changed";
            } catch (const Exception_& error) {
                cause = error.what();
            }
            ASSERT_FALSE(cause.empty());
            try {
                static_cast<void>(PriceGaussianRateOption(plan, instrument, c, {0, true}));
            } catch (const CalibrationError_& error) {
                saved = error;
            }
        }
        ASSERT_TRUE(saved);
        ASSERT_EQ(saved->Failure().diagnostic_.code_, "CalibrationNumericalFailure");
        ASSERT_EQ(saved->Failure().diagnostic_.stage_, CalibrationStage_::Value_::Evaluate);
        ASSERT_EQ(saved->Failure().diagnostic_.instrumentId_, caplet ? "caplet" : "swaption");
        ASSERT_EQ(saved->Failure().diagnostic_.measureId_, "Price");
        ASSERT_EQ(saved->Failure().diagnostic_.actual_, String_(cause));
        ASSERT_EQ(saved->Failure().report_->Stage(), CalibrationStage_::Value_::Evaluate);
        ASSERT_EQ(saved->Failure().report_->Errors().size(), 1);
    }
}

TEST(GSRRateOptionNumericalTest, TestTwoFactorQuadratureRefinementAndIndependentIntegration) {
    const LiborBasisScope_ basis;
    auto c = Context();
    Matrix_<> h(2, 2), g(2, 1), correlation(2, 2, .25);
    h(0, 0) = h(0, 1) = 1.;
    h(1, 0) = .3;
    h(1, 1) = -.4;
    g(0, 0) = .02;
    g(1, 0) = .015;
    correlation(0, 0) = correlation(1, 1) = 1.;
    Handle_<MultiFactorGSRVolData_> vol(new MultiFactorGSRVolData_("two", {"level", "slope"}, {V}, g, {V, Date_(2028, 10, 21)}, h, correlation));
    c.model_.reset(new MultiFactorGSRModelData_("two", c.curve_, vol));
    const auto instrument = Instrument(false);
    const auto plan = PrepareCalibrationInstrument(instrument, "Price", c);
    const auto coarse = PriceGaussianRateOption(plan, instrument, c, {16, false});
    const auto fine = PriceGaussianRateOption(plan, instrument, c, {32, false});
    const auto refined = PriceGaussianRateOption(plan, instrument, c, {16, true});
    const auto option = Swaption();
    auto random = NewSobol(2, 1);
    Vector_<> normals(2);
    double total = 0.;
    const double split = (option.fixed_[0].payment_ - E) / 365.;
    for (int path = 0; path < 131072; ++path) {
        random->FillNormal(&normals);
        double underlying = 1.;
        for (size_t i = 0; i < option.fixed_.size(); ++i) {
            const double tau = (option.fixed_[i].payment_ - E) / 365.;
            const double b1 = .3 * std::min(tau, split) - .4 * std::max(tau - split, 0.);
            const double beta0 = -.02 * tau - .25 * .015 * b1, beta1 = -.015 * b1 * std::sqrt(1. - .25 * .25);
            const double bond = std::exp(-.03 * tau - .5 * (beta0 * beta0 + beta1 * beta1) + beta0 * normals[0] + beta1 * normals[1]);
            underlying -= (.03 * option.fixed_[i].accrual_ + (i + 1 == option.fixed_.size() ? 1. : 0.)) * bond;
        }
        total += std::max(underlying, 0.);
    }
    const double oracle = 100. * std::exp(-.03) * total / 131072.;
    ASSERT_NEAR(fine.price_, oracle, 2e-4);
    ASSERT_NEAR(fine.price_, coarse.price_, 1e-6);
    ASSERT_NEAR(refined.numericalError_, std::abs(fine.price_ - coarse.price_), 1e-12);
    RecordProperty("quadrature_16", coarse.price_);
    RecordProperty("quadrature_32", fine.price_);
    RecordProperty("quadrature_refinement", refined.numericalError_);
    RecordProperty("independent_sobol", oracle);
}

TEST(GSRRateOptionNumericalTest, TestNativeFastFitAndFrozenSamplingBudgetOutcomes) {
    const LiborBasisScope_ basis;
    for (bool caplet : {true, false}) {
        const auto instrument = Instrument(caplet);
        const auto initial = Context(.012);
        const EuropeanRateOption_ option = caplet ? EuropeanRateOption_(Caplet()) : EuropeanRateOption_(Swaption());
        const double target = Oracle(caplet, .02, true).first;
        const auto fit = CalibrateGSRVolatility(static_cast<const MultiFactorGSRModelData_&>(*initial.model_),
                                                {{"fixed-contract", option, target / 100., 1e-6}}, {{0, 0, 0., .1}});
        ASSERT_TRUE(fit.fitWithinTolerance_);
        ASSERT_NEAR(fit.parameters_[0], .02, 1e-8);
        auto c = Context();
        c.model_ = Handle_<ModelData_>(fit.model_);
        const auto plan = PrepareCalibrationMc(instrument, "Price", c, McSettings());
        const auto validation = plan.Evaluate(instrument, c, true), training = plan.Evaluate(instrument, c, false);
        ASSERT_LE(training.ranges_.back().firstPath_ + training.ranges_.back().pathCount_, validation.ranges_.front().firstPath_);
        ASSERT_TRUE(ValidateCalibrationMc(validation, Reference(instrument, fit.parameters_[0]), Precision()).numericalValidationPassed_);
        const auto separate = ValidateCalibrationMc(training, validation, Precision());
        // Swaption fit SE exceeds the frozen budget: retain that refusal, independently of fast-fit validation passing.
        ASSERT_EQ(separate.numericalValidationPassed_, caplet)
            << "caplet=" << caplet << " fit=" << training.mean_ << " validation=" << validation.mean_ << " SE=" << training.standardError_ << ","
            << validation.standardError_ << " gradient=" << training.gradient_[0] << "," << validation.gradient_[0]
            << " gradientSE=" << training.gradientStandardError_[0] << "," << validation.gradientStandardError_[0]
            << " parity=" << separate.parityPassed_ << " precision=" << separate.precisionPassed_ << " error=" << separate.valueErrorPassed_
            << " gparity=" << separate.gradientParityPassed_ << " gprecision=" << separate.gradientPrecisionPassed_;
        ASSERT_TRUE(separate.parityPassed_);
        ASSERT_EQ(separate.budgetExhausted_, !caplet);
        Record(caplet ? "caplet_fit" : "swaption_fit", training, Reference(instrument, fit.parameters_[0]));
        Record(caplet ? "caplet_validation" : "swaption_validation", validation, Reference(instrument, fit.parameters_[0]));
        auto small = McSettings();
        small.fitBatches_ = small.validationBatches_ = 2;
        small.fitPathsPerBatch_ = 8;
        small.validationPathsPerBatch_ = 16;
        small.maxPathsPerView_ = 48;
        const auto exhausted = PrepareCalibrationMc(instrument, "Price", c, small).Evaluate(instrument, c, true);
        ASSERT_TRUE(ValidateCalibrationMc(exhausted, Reference(instrument, fit.parameters_[0]), Precision()).budgetExhausted_);
        RecordProperty(caplet ? "caplet_fit_g" : "swaption_fit_g", fit.parameters_[0]);
    }
}

TEST(GSRRateOptionNumericalTest, TestFrozenThreeWayMatrix) {
    const LiborBasisScope_ basis;
    for (bool caplet : {true, false})
        for (bool call : {true, false}) {
            const auto instrument = Instrument(caplet, call);
            const std::string key = std::string(caplet ? "caplet" : "swaption") + (call ? "_call" : "_put");
            RecordProperty(key + "_definition", std::string(instrument->Definition()->DefinitionIdentity().c_str()));
            RecordProperty(key + "_binding", std::string(instrument->BindingIdentity().c_str()));
            int ordinal = 0;
            for (double g : {.012, .016, .02, .0201, .0199, .02005, .01995}) {
                SCOPED_TRACE(::testing::Message() << key << " " << g);
                const auto c = Context(g);
                const auto reference = Reference(instrument, g);
                const auto oracle = Oracle(caplet, g, call);
                ASSERT_NEAR(reference.value_, oracle.first, 1e-10);
                ASSERT_NEAR(reference.gradient_[0], oracle.second, 2e-4);
                const auto plan = PrepareCalibrationMc(instrument, "Price", c, McSettings());
                const auto result = plan.Evaluate(instrument, c, true);
                ASSERT_TRUE(ValidateCalibrationMc(result, reference, Precision()).numericalValidationPassed_)
                    << result.mean_ << " " << reference.value_ << " " << result.standardError_ << " gradient " << result.gradient_[0] << " "
                    << reference.gradient_[0] << " " << result.gradientStandardError_[0];
                const CalibrationMcReference_ independent{oracle.first, 0., 0., {oracle.second}, {0.}, {0.}};
                ASSERT_TRUE(ValidateCalibrationMc(result, independent, Precision()).numericalValidationPassed_);
                ASSERT_EQ(result.pathsPerView_, 262144);
                ASSERT_EQ(result.engineRuns_, 16);
                ASSERT_EQ(result.instrumentIdentity_, instrument->InstrumentIdentity());
                ASSERT_EQ(plan.Mapping().numeraire_, "dal.native-gsr/1");
                ASSERT_EQ(plan.Mapping().streamVersion_, "dal.mrg32-normal-paths/1");
                Record(key + "_" + std::to_string(ordinal++), result, reference);
            }
        }
}
