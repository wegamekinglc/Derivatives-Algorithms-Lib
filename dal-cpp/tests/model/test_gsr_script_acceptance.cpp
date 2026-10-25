#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/model/gsrscriptcalibration.hpp>
#include <dal/time/dateutils.hpp>

using Dal::Date_;
using Dal::Vector_;

namespace {
    const Date_ V(2026, 10, 21), F1(2027, 10, 21), F2(2028, 4, 21), PAY(2028, 10, 21), END(2029, 10, 21);
    struct BasisScope_ {
        Dal::DayBasis_ saved_ = Dal::Ccy::Conventions::LiborDayBasis()(Dal::Ccy_("USD"));
        BasisScope_() { Dal::Ccy::Conventions::LiborDayBasis().XWrite()(Dal::Ccy_("USD"), Dal::DayBasis_("ACT_360")); }
        ~BasisScope_() { Dal::Ccy::Conventions::LiborDayBasis().XWrite()(Dal::Ccy_("USD"), saved_); }
    };
    double Time(const Date_& d) { return (d - V) / 365.; }
    Dal::CalibrationPreparationContext_ Context(double g) {
        Dal::CalibrationPreparationContext_ c;
        c.currency_ = Dal::Ccy_("USD");
        c.discountRoute_ = "USD.OIS";
        c.valuation_.evaluationDate_ = V;
        c.valuation_.fixings_.reset(new Dal::MarketFixingSnapshot_());
        c.curve_.reset(new Dal::GSRCurveData_("audit-curve", V, "USD", {V, END}, {0., -.12}, {}, {}));
        Dal::Handle_<Dal::MultiFactorGSRVolData_> vol(new Dal::MultiFactorGSRVolData_("audit-vol", {"level"}, {V}, Dal::Matrix_<>(1, 1, g), {V},
                                                                                      Dal::Matrix_<>(1, 1, 1.), Dal::Matrix_<>(1, 1, 1.)));
        c.model_.reset(new Dal::MultiFactorGSRModelData_("audit-model", c.curve_, vol));
        return c;
    }
    Dal::Handle_<Dal::CalibrationInstrument_> Instrument() {
        const auto price = Dal::CalibrationValueUnit_::Parse("Price:USD");
        Dal::ScriptInstrumentDefinitionSpec_ s;
        s.header_.payCurrency_ = s.header_.resultCurrency_ = s.header_.collateralCurrency_ = Dal::Ccy_("USD");
        s.header_.discountRoute_ = "USD.OIS";
        s.source_.rows_ = {{Dal::ScriptSourceRowKind_::Value_::Definition, "N", "100"},
                           {Dal::ScriptSourceRowKind_::Value_::Definition, "K", ".04"},
                           {Dal::ScriptSourceRowKind_::Value_::Event, "2027-10-21", "avg = .5*FIX(IR[USD,LIBOR_6M_LCH,2027-10-21])"},
                           {Dal::ScriptSourceRowKind_::Value_::Event, "2028-04-21", "avg = avg+.5*FIX(IR[USD,LIBOR_6M_LCH,2028-04-21])"},
                           {Dal::ScriptSourceRowKind_::Value_::Event, "2028-10-21", "pay PAYS N*MAX(avg-K,0)"}};
        s.parameters_ = {
            {"N", Dal::ScriptParameterType_::Value_::Real, price, Dal::ScriptParameterRole_::Value_::Notional, true, 0., {}, false, true},
            {"K", Dal::ScriptParameterType_::Value_::Real, Dal::CalibrationValueUnit_::Parse("Rate"), Dal::ScriptParameterRole_::Value_::Coupon,
             true}};
        s.measures_ = {{"Price", Dal::CalibrationMeasureKind_::Value_::Price, "PAY", {}, price}};
        return Dal::NewScriptCalibrationInstrument({"independent-coupon-audit", Dal::NewScriptInstrumentDefinition(s)});
    }
    Dal::CalibrationMcSettings_ Sampling(bool full) {
        Dal::CalibrationMcSettings_ s;
        s.allowMonteCarloCalibration_ = s.aad_ = s.retainPaths_ = true;
        s.parameterLabels_ = {"g:level:2026-10-21"};
        s.fitBatches_ = full ? 8 : 2;
        s.fitPathsPerBatch_ = full ? 8192 : 32;
        s.validationBatches_ = s.heldOutBatches_ = full ? 16 : 4;
        s.validationPathsPerBatch_ = s.heldOutPathsPerBatch_ = full ? 16384 : 64;
        s.maxPathsPerView_ = full ? 589824 : 576;
        return s;
    }
    std::pair<double, double> PathOracle(double g, const Vector_<>& z) {
        double w = 0., integralW = 0., integralT2 = 0., previous = 0., average = 0., derivative = 0.;
        const Vector_<Date_> dates{F1, F2, PAY};
        for (int i = 0; i < 3; ++i) {
            const double t = Time(dates[i]), dt = t - previous;
            const double nextW = w + std::sqrt(dt) * z[i], midpoint = .5 * (previous + t);
            integralW += .5 * (w + nextW) * dt;
            integralT2 += midpoint * midpoint * dt;
            if (i < 2) {
                const Date_ end = Dal::Date::AddMonths(dates[i], 6);
                const double tau = (end - dates[i]) / 365., accrual = (end - dates[i]) / 360.;
                const double drift = tau * t * (t + tau);
                const double accumulation = std::exp(.12 * tau / Time(END) + .5 * g * g * drift + tau * g * nextW);
                average += .5 * (accumulation - 1.) / accrual;
                derivative += .5 * accumulation * (g * drift + tau * nextW) / accrual;
            }
            previous = t;
            w = nextW;
        }
        const double discount = std::exp(-.12 * Time(PAY) / Time(END) - g * integralW - .5 * g * g * integralT2);
        const double intrinsic = std::max(average - .04, 0.);
        return {100. * discount * intrinsic, 100. * discount * ((average > .04 ? derivative : 0.) - intrinsic * (integralW + g * integralT2))};
    }
    std::pair<double, double> Moments(const Vector_<>& values) {
        const double mean = std::accumulate(values.begin(), values.end(), 0.) / values.size();
        double squares = 0.;
        for (double v : values)
            squares += (v - mean) * (v - mean);
        return {mean, std::sqrt(squares / (values.size() * (values.size() - 1.)))};
    }
    void Record(const std::string& name, double value) {
        std::ostringstream stream;
        stream << std::setprecision(17) << value;
        ::testing::Test::RecordProperty(name, stream.str());
    }
    void AuditBatches(const Dal::CalibrationMcResult_& result, const std::string& prefix) {
        ASSERT_EQ(result.ranges_.size(), result.batches_.size());
        ASSERT_EQ(result.pathValues_.size(), result.pathsPerView_);
        ASSERT_EQ(result.pathDerivatives_.Rows(), result.pathsPerView_);
        ASSERT_EQ(result.pathDerivatives_.Cols(), 1);
        Vector_<> values, gradients;
        size_t offset = 0;
        for (size_t b = 0; b < result.ranges_.size(); ++b) {
            double value = 0., gradient = 0.;
            const auto& range = result.ranges_[b];
            for (size_t p = 0; p < range.pathCount_; ++p) {
                value += result.pathValues_[offset + p];
                gradient += result.pathDerivatives_(static_cast<int>(offset + p), 0);
            }
            values.push_back(value / range.pathCount_);
            gradients.push_back(gradient / range.pathCount_);
            ASSERT_NEAR(values.back(), result.batches_[b].value_, 1e-10);
            ASSERT_NEAR(gradients.back(), result.batches_[b].derivatives_[0], 1e-8);
            Record(prefix + "_batch_" + std::to_string(b) + "_value", values.back());
            Record(prefix + "_batch_" + std::to_string(b) + "_gradient", gradients.back());
            offset += range.pathCount_;
        }
        ASSERT_EQ(offset, result.pathsPerView_);
        const auto price = Moments(values), risk = Moments(gradients);
        ASSERT_NEAR(price.first, result.mean_, 1e-10);
        ASSERT_NEAR(price.second, result.standardError_, 1e-10);
        ASSERT_NEAR(risk.first, result.gradient_[0], 1e-8);
        ASSERT_NEAR(risk.second, result.gradientStandardError_[0], 1e-8);
        ASSERT_DOUBLE_EQ(result.gradient_[0], result.quoteJacobian_(0, 0));
        Record(prefix + "_mean", price.first);
        Record(prefix + "_se", price.second);
        Record(prefix + "_gradient", risk.first);
        Record(prefix + "_gradient_se", risk.second);
    }
} // namespace

TEST(GSRScriptAcceptanceTest, TestEveryPhaseUsesItsDeclaredNormalCoordinatesAndRawDerivative) {
    const BasisScope_ basis;
    const auto instrument = Instrument();
    const auto context = Context(.016);
    for (bool compiled : {false, true}) {
        auto s = Sampling(false);
        s.compiled_ = compiled;
        const auto plan = Dal::PrepareCalibrationMc(instrument, "Price", context, s);
        ASSERT_EQ(plan.Capability().ruleId_, "dal.gaussian-script-price/1");
        ASSERT_EQ(plan.Mapping().dimension_, 3);
        ASSERT_EQ(plan.Mapping().timeline_, (Vector_<>{Time(F1), Time(F2), Time(PAY)}));
        ASSERT_EQ(plan.Mapping().normalCoordinates_,
                  (Vector_<Dal::String_>{"sample:0:factor:level", "sample:1:factor:level", "sample:2:factor:level"}));
        ASSERT_EQ(plan.Mapping().normalPrecision_, "Precise");
        ASSERT_FALSE(plan.Mapping().brownianBridge_);
        const Vector_<Dal::CalibrationMcResult_> phases{plan.Evaluate(instrument, context, false), plan.Evaluate(instrument, context, true),
                                                        plan.EvaluateHeldOut(instrument, context)};
        auto random = Dal::New(Dal::RNGType_("MRG32"), 1024, 3, true);
        Vector_<> z(3);
        size_t path = 0;
        for (const auto& result : phases) {
            ASSERT_EQ(result.ranges_.front().firstPath_, path);
            for (size_t i = 0; i < result.pathValues_.size(); ++i, ++path) {
                random->FillNormal(&z);
                const auto expected = PathOracle(.016, z);
                ASSERT_NEAR(result.pathValues_[i], expected.first, 1e-10);
                ASSERT_NEAR(result.pathDerivatives_(static_cast<int>(i), 0), expected.second, 1e-8);
            }
        }
        ASSERT_EQ(path * plan.Mapping().dimension_, 1728);
        for (double h : {1e-6, 5e-7}) {
            const auto up = Context(.016 + h), down = Context(.016 - h);
            const auto upper = plan.RefreshG(up), lower = plan.RefreshG(down);
            ASSERT_EQ(upper.Mapping().identity_, plan.Mapping().identity_);
            const double fd = (upper.Evaluate(instrument, up, false).mean_ - lower.Evaluate(instrument, down, false).mean_) / (2. * h);
            ASSERT_NEAR(fd, phases[0].gradient_[0], 2e-6);
        }
    }
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), 0);
}

TEST(GSRScriptAcceptanceTest, TestFrozenBatchStatisticsAndSeparateAcceptanceIndicators) {
    const BasisScope_ basis;
    const auto instrument = Instrument();
    const auto context = Context(.02);
    const auto plan = Dal::PrepareCalibrationMc(instrument, "Price", context, Sampling(true));
    const auto fit = plan.Evaluate(instrument, context, false), validation = plan.Evaluate(instrument, context, true);
    const auto held = plan.EvaluateHeldOut(instrument, context);
    ASSERT_NO_FATAL_FAILURE(AuditBatches(fit, "fit"));
    ASSERT_NO_FATAL_FAILURE(AuditBatches(validation, "validation"));
    ASSERT_NO_FATAL_FAILURE(AuditBatches(held, "held"));
    ASSERT_EQ(fit.ranges_.front().firstPath_, 0);
    ASSERT_EQ(validation.ranges_.front().firstPath_ * 3, 196608);
    ASSERT_EQ(held.ranges_.front().firstPath_ * 3, 983040);
    ASSERT_EQ((held.ranges_.back().firstPath_ + held.ranges_.back().pathCount_) * 3, 1769472);
    Dal::CalibrationMcPrecision_ p;
    p.errorScale_ = .1;
    p.parameterScales_ = {.01};
    p.epsilonGradient_ = {1e-8};
    const auto report = Dal::ValidateCalibrationMc(fit, validation, p);
    const double uncertainty = 3. * std::hypot(fit.standardError_, validation.standardError_);
    const double difference = std::abs(fit.mean_ - validation.mean_);
    const double gradientUncertainty = 3. * std::hypot(fit.gradientStandardError_[0], validation.gradientStandardError_[0]);
    ASSERT_NEAR(report.precisionIndicator_, uncertainty, 1e-12);
    ASSERT_NEAR(report.errorIndicator_, difference + uncertainty, 1e-12);
    ASSERT_EQ(report.parityPassed_, difference <= uncertainty + 1e-10);
    ASSERT_EQ(report.precisionPassed_, uncertainty <= .025);
    ASSERT_EQ(report.valueErrorPassed_, difference + uncertainty <= .025);
    ASSERT_EQ(report.gradientParityPassed_, std::abs(fit.gradient_[0] - validation.gradient_[0]) <= gradientUncertainty + 1e-8);
    ASSERT_NEAR(report.gradientPrecisionIndicators_[0], gradientUncertainty, 1e-12);
    ASSERT_EQ(report.gradientPrecisionPassed_, gradientUncertainty <= 2.5);
    ASSERT_TRUE(report.numericalValidationPassed_);
    Record("value_error_indicator", report.errorIndicator_);
    Record("gradient_precision_indicator", gradientUncertainty);
    const auto doubledInstrument =
        Dal::BindCalibrationInstrumentParameters(instrument, {{{"N", 200., Dal::CalibrationValueUnit_::Parse("Price:USD")}}, {}});
    auto sampling = Sampling(true);
    sampling.retainPaths_ = false;
    const auto doubledPlan = Dal::PrepareCalibrationMc(doubledInstrument, "Price", context, sampling);
    const auto doubledFit = doubledPlan.Evaluate(doubledInstrument, context, false);
    const auto doubledValidation = doubledPlan.Evaluate(doubledInstrument, context, true);
    ASSERT_NEAR(doubledFit.mean_, 2. * fit.mean_, 1e-10);
    ASSERT_NEAR(doubledFit.gradient_[0], 2. * fit.gradient_[0], 1e-8);
    ASSERT_NEAR(doubledValidation.standardError_, 2. * validation.standardError_, 1e-10);
    const auto doubled = Dal::ValidateCalibrationMc(doubledFit, doubledValidation, p);
    ASSERT_TRUE(doubled.parityPassed_);
    ASSERT_FALSE(doubled.numericalValidationPassed_);
    ASSERT_TRUE(doubled.budgetExhausted_);
    ASSERT_GT(doubled.errorIndicator_, .025);
    Record("doubled_value_error_indicator", doubled.errorIndicator_);
    ASSERT_TRUE(held.heldOut_);
    ASSERT_THROW(Dal::ValidateCalibrationMc(fit, held, p), Dal::CalibrationError_);
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), 0);
}
