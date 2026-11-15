#include <gtest/gtest.h>

#include <functional>
#include <type_traits>

#include <dal/curve/curveinstrumentcontext_internal.hpp>
#include <dal/curve/curveinstrumentevaluation_internal.hpp>
#include <dal/curve/scriptcurvequoterisk.hpp>
#include <dal/math/matrix/linearsolveaccuracy.hpp>

#include "compoundedoiscalibrationfixture.hpp"

using namespace Dal;
using namespace Dal::CompoundedOisCalibrationTest;

namespace {
    template <class F_> void AssertFailure(const F_& action, const String_& code) {
        try {
            action();
            FAIL() << "Invalid frozen OIS context accepted";
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, code);
            ASSERT_TRUE(error.Failure().report_);
        }
    }
    void AssertJacobian(const Matrix_<>& jacobian, const CurveInstrumentCalibrationSpec_& spec, const String_& role, bool zeroRate) {
        const auto theta = Parameters(zeroRate);
        for (int j = 0; j < 2; ++j) {
            Vector_<> previous;
            for (const double h : {1e-4, 5e-5}) {
                auto up = theta, down = theta;
                up[j] += h;
                down[j] -= h;
                const auto plus = Measures(spec, role, up, zeroRate), minus = Measures(spec, role, down, zeroRate);
                Vector_<> fd(2);
                for (int i = 0; i < 2; ++i) {
                    fd[i] = (plus[i] - minus[i]) / (2. * h);
                    const double tolerance = 1e-7 + 1e-5 * std::abs(fd[i]);
                    ASSERT_NEAR(jacobian(i, j), fd[i], tolerance);
                    if (!previous.empty())
                        ASSERT_NEAR(fd[i], previous[i], tolerance);
                }
                previous = fd;
            }
        }
    }
    void AssertFit(const CurveInstrumentCalibrationSpec_& spec, const String_& role, bool zeroRate = false) {
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = CalibrationDerivative_("AAD");
        const auto fit = CalibrateCurveInstruments(spec, options);
        ASSERT_TRUE(fit.fitWithinTolerance_);
        ASSERT_EQ(fit.solveMode_, CurveSolveMode_("EXACT"));
        ASSERT_TRUE(fit.evidence_);
        ASSERT_EQ(fit.axes_.quotes_.size(), 2u);
        ASSERT_EQ(fit.freeParameters_.size(), 2u);
        ASSERT_EQ(fit.evidence_->CandidateIdentity(), fit.candidateIdentity_);
        ASSERT_EQ(fit.evidence_->ObjectiveIdentity(), fit.objectiveIdentity_);
        ASSERT_EQ(fit.evidence_->ConfigurationIdentity(), CurveInstrumentInternal::ConfigurationIdentity(*fit.resolvedSpec_, *fit.resolvedOptions_));
        for (int i = 0; i < 2; ++i) {
            ASSERT_NEAR(fit.parameters_[i], Parameters(zeroRate)[i], 1e-8);
            ASSERT_LE(std::abs(fit.rawResiduals_[i]), 1e-12);
        }
        const PreparedCurveInstruments_ prepared(spec, options);
        const auto evaluated = prepared.Evaluate(Parameters(zeroRate));
        ASSERT_NO_FATAL_FAILURE(AssertJacobian(evaluated.quoteJacobian_, spec, role, zeroRate));
        SquareMatrix_<> matrix(2, 0.);
        Matrix_<> rhs(2, 2, 0.);
        for (int i = 0; i < 2; ++i) {
            rhs(i, i) = 1.;
            for (int j = 0; j < 2; ++j)
                matrix(i, j) = evaluated.quoteJacobian_(i, j);
        }
        const CheckedLinearSolve_ checked(matrix, rhs, {1e-12, 1e-12});
        ASSERT_GE(checked.Diagnostics().reciprocalConditionInfinity_, 1e-10);
        const auto context = CurveInstrumentInternal::PrepareCurvePriceContext(prepared);
        const auto pv = NewCompoundedOisScriptInstrument(spec.compoundedOis_.back());
        const auto price = CurveInstrumentInternal::PrepareCurvePrice(context, pv, "Price");
        const CandidateCurveProvider_<double> provider(price.geometry_, fit.parameters_);
        const double value = CurveScriptInternal::Measure(price.plan_, provider, spec.geometry_.today_);
        ASSERT_NEAR(value, Oracle(spec.compoundedOis_.back()->Spec(), role, fit.parameters_, zeroRate, spec.valuation_)[0], 1e-10);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, {{1e-12, 1e-12}, 1e-10, 1e-12});
        ASSERT_FALSE(snapshot.RiskAvailable());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedCapability");
    }
} // namespace

TEST(CompoundedOisCalibrationTest, TestFourModesThreeTargetsFitIndependentSquareQuotes) {
    for (const auto& mode : CompoundedOisObservationModeListAll())
        for (const auto* role : {"Overnight", "Discount", "Shared"}) {
            SCOPED_TRACE(mode.String());
            SCOPED_TRACE(role);
            const auto spec = Spec(mode.String(), role);
            ASSERT_NO_FATAL_FAILURE(AssertFit(spec, role));
        }
}

TEST(CompoundedOisCalibrationTest, TestZeroRateAndMixedLegacyVersionTwoShareNativeJacobian) {
    for (const auto& mode : CompoundedOisObservationModeListAll()) {
        for (const auto* role : {"Overnight", "Discount", "Shared"})
            ASSERT_NO_FATAL_FAILURE(AssertFit(Spec(mode.String(), role, false, true), role, true));
        for (const auto* role : {"Overnight", "Shared"})
            ASSERT_NO_FATAL_FAILURE(AssertFit(Spec(mode.String(), role, true), role));
    }
}

TEST(CompoundedOisCalibrationTest, TestAllUnusedPvRegistrationsAreValidatedBeforeCalibration) {
    auto spec = Spec("None", "Shared");
    spec.compoundedOis_.push_back(spec.compoundedOis_.back());
    ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { PreparedCurveInstruments_ prepared(spec); }, "InvalidCompoundedOis"));
    spec = Spec("None", "Shared");
    auto valuation = spec.valuation_;
    valuation.todayFixingPolicy_ = TodayFixingPolicy_("RequireHistorical");
    spec.compoundedOis_.back() = PrepareCompoundedOis(spec.compoundedOis_.back()->Spec(), valuation);
    ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { PreparedCurveInstruments_ prepared(spec); }, "CalibrationContextMismatch"));
    spec = Spec("None", "Shared");
    auto contract = spec.compoundedOis_.back()->Spec();
    contract.projectionRoute_ = "unregistered-route";
    spec.compoundedOis_.back() = PrepareCompoundedOis(contract, spec.valuation_);
    ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { PreparedCurveInstruments_ prepared(spec); }, "CalibrationContextMismatch"));
}

TEST(CompoundedOisCalibrationTest, TestFrozenContextRejectsFinanciallyEqualButChangedPvSource) {
    const auto spec = Spec("ObservationShift", "Shared");
    const PreparedCurveInstruments_ prepared(spec);
    const auto context = CurveInstrumentInternal::PrepareCurvePriceContext(prepared);
    const auto original = NewCompoundedOisScriptInstrument(spec.compoundedOis_.back());
    auto definition = original->Definition()->Spec();
    definition.source_.rows_.back().text_ += " ";
    const auto changed = NewScriptCalibrationInstrument({original->Id(), NewScriptInstrumentDefinition(definition), {}, original->Pillar()});
    ASSERT_NE(changed->InstrumentIdentity(), original->InstrumentIdentity());
    ASSERT_NO_FATAL_FAILURE(
        AssertFailure([&] { CurveInstrumentInternal::PrepareCurvePrice(context, changed, "Price"); }, "CompoundedOisSourceMismatch"));
}

TEST(CompoundedOisCalibrationTest, TestFrozenContextRejectsChangedCalendarValuationRoutesAndQuoteIdentity) {
    const auto spec = Spec("Lookback", "Shared");
    const PreparedCurveInstruments_ prepared(spec);
    const auto context = CurveInstrumentInternal::PrepareCurvePriceContext(prepared);
    ASSERT_NO_THROW(CurveInstrumentInternal::RequireCurvePriceContext(context, spec));
    const Vector_<std::function<void(CurveInstrumentCalibrationSpec_*)>> changes{
        [](auto* s) {
            auto c = s->compoundedOis_.back()->Spec();
            c.calendar_ = HolidayCenterData_("different-calendar", {Date_(2024, 3, 4)}, {Date_(2024, 3, 9)});
            s->compoundedOis_.back() = PrepareCompoundedOis(c, s->valuation_);
        },
        [](auto* s) { s->market_.routes_[0].useTargetCurve_ = false; },
        [](auto* s) {
            auto values = s->valuation_.fixings_->Values();
            values["unused-empty-series"] = {};
            s->valuation_.fixings_.reset(new MarketFixingSnapshot_(values));
        },
        [](auto* s) {
            const auto row = s->observations_[0];
            s->observations_[0] = NewCalibrationObservation(
                row->Id(), row->Instrument(), row->MeasureId(),
                NewCalibrationMarketQuote("different-quote-id", row->Quote()->Value(), row->Quote()->Unit()), row->ErrorScale());
        },
        [](auto* s) { s->solver_.initialGuessPerNode_[0] *= 2.; }};
    for (const auto& change : changes) {
        auto different = spec;
        change(&different);
        ASSERT_NO_FATAL_FAILURE(
            AssertFailure([&] { CurveInstrumentInternal::RequireCurvePriceContext(context, different); }, "CalibrationContextMismatch"));
    }
}

TEST(CompoundedOisCalibrationTest, TestEveryPvUsesItsOwnRegisteredSourceBindingAndTable) {
    const auto spec = Spec("Lockout", "Shared");
    const PreparedCurveInstruments_ prepared(spec);
    const auto context = CurveInstrumentInternal::PrepareCurvePriceContext(prepared);
    const auto original = NewCompoundedOisScriptInstrument(spec.compoundedOis_.back());
    const auto rebound = BindCalibrationInstrumentParameters(original, {{{"K", .044, CalibrationValueUnit_::Parse("Rate")}}, {}});
    ASSERT_NO_FATAL_FAILURE(
        AssertFailure([&] { CurveInstrumentInternal::PrepareCurvePrice(context, rebound, "Price"); }, "CompoundedOisSourceMismatch"));
    auto contract = spec.compoundedOis_.back()->Spec();
    contract.instrumentId_ = "not-registered";
    const auto unknown = NewCompoundedOisScriptInstrument(PrepareCompoundedOis(contract, spec.valuation_));
    ASSERT_NO_FATAL_FAILURE(
        AssertFailure([&] { CurveInstrumentInternal::PrepareCurvePrice(context, unknown, "Price"); }, "CompoundedOisUnregisteredInstrument"));
    contract = Contract(false, "ObservationShift");
    contract.instrumentId_ = original->Id();
    const auto wrongTable = NewCompoundedOisScriptInstrument(PrepareCompoundedOis(contract, spec.valuation_));
    ASSERT_NO_FATAL_FAILURE(
        AssertFailure([&] { CurveInstrumentInternal::PrepareCurvePrice(context, wrongTable, "Price"); }, "CompoundedOisSourceMismatch"));
    const auto clone = Handle_<CalibrationInstrument_>(original->Clone());
    const auto accepted = CurveInstrumentInternal::PrepareCurvePrice(context, clone, "Price");
    ASSERT_EQ(accepted.plan_.Instrument()->InstrumentIdentity(), original->InstrumentIdentity());
    ASSERT_NO_FATAL_FAILURE(
        AssertFailure([&] { CurveInstrumentInternal::PrepareCurvePrice(CurveInstrumentInternal::PreparedCurvePriceContext_{}, original, "Price"); },
                      "CalibrationContextMismatch"));
}

TEST(CompoundedOisCalibrationTest, TestFrozenPriceAndEvidenceSurviveAliasesAndOwnerDestruction) {
    CurveInstrumentInternal::PreparedCurvePriceContext_ context;
    Handle_<CalibrationInstrument_> pv;
    double value = 0.;
    {
        auto spec = Spec("Lookback", "Overnight");
        auto curve = std::make_shared<DiscountLogDF_>("writable", "USD", spec.geometry_.knotDates_, Vector_<>{0., -.0002, -.0014},
                                                      DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR"));
        spec.market_.discountCurves_[CollateralType_("OIS")] = Handle_<DiscountCurve_>(curve);
        auto fixings = std::make_shared<MarketFixingSnapshot_>(spec.valuation_.fixings_->Values());
        spec.valuation_.fixings_ = Handle_<MarketFixingSnapshot_>(fixings);
        const auto first = spec.observations_[0];
        auto quote = std::make_shared<CalibrationMarketQuote_>(*first->Quote());
        spec.observations_[0] = NewCalibrationObservation(first->Id(), first->Instrument(), first->MeasureId(),
                                                          Handle_<CalibrationMarketQuote_>(quote), first->ErrorScale());
        const PreparedCurveInstruments_ prepared(spec);
        const auto before = prepared.Evaluate(Parameters());
        const auto configuration = before.evidence_->ConfigurationIdentity();
        context = CurveInstrumentInternal::PrepareCurvePriceContext(prepared);
        pv = NewCompoundedOisScriptInstrument(spec.compoundedOis_.back());
        const auto price = CurveInstrumentInternal::PrepareCurvePrice(context, pv, "Price");
        value = CurveScriptInternal::Measure(price.plan_, CandidateCurveProvider_<double>(price.geometry_, Parameters()), spec.geometry_.today_);
        const Vector_<> delta{.01, .01};
        curve->ApplyDX(delta.begin(), 1.);
        static_assert(!std::is_copy_assignable_v<MarketFixingSnapshot_>);
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        fixings.reset();
        *quote = *NewCalibrationMarketQuote(quote->Id(), quote->Value() + .01, quote->Unit());
        spec.compoundedOis_.clear();
        spec.market_.routes_.clear();
        const auto after = prepared.Evaluate(Parameters());
        ASSERT_EQ(after.modelMeasures_, before.modelMeasures_);
        ASSERT_EQ(after.evidence_->ConfigurationIdentity(), configuration);
        ASSERT_DOUBLE_EQ(before.resolvedSpec_->observations_[0]->Quote()->Value(), first->Quote()->Value());
    }
    const auto price = CurveInstrumentInternal::PrepareCurvePrice(context, pv, "Price");
    ASSERT_DOUBLE_EQ(CurveScriptInternal::Measure(price.plan_, CandidateCurveProvider_<double>(price.geometry_, Parameters()), Date_(2024, 3, 1)),
                     value);
}

TEST(CompoundedOisCalibrationTest, TestEveryRegisteredPvChangesPlanAndConfigurationEvidence) {
    auto spec = Spec("None", "Shared");
    const auto first = PreparedCurveInstruments_(spec).Describe();
    auto changed = spec.compoundedOis_.back()->Spec();
    changed.calendar_ = HolidayCenterData_("other-calendar-identity", {Date_(2024, 3, 4)}, {Date_(2024, 3, 9)});
    spec.compoundedOis_.back() = PrepareCompoundedOis(changed, spec.valuation_);
    const auto second = PreparedCurveInstruments_(spec).Describe();
    ASSERT_NE(first.planIdentity_, second.planIdentity_);
    ASSERT_NE(first.evidence_->ConfigurationIdentity(), second.evidence_->ConfigurationIdentity());
    ASSERT_EQ(first.resolvedSpec_->observations_[0]->Instrument()->InstrumentIdentity(),
              second.resolvedSpec_->observations_[0]->Instrument()->InstrumentIdentity());
}

TEST(CompoundedOisCalibrationTest, TestObservationBindingsRecheckExactRouteAndRole) {
    const auto spec = Spec("None", "Overnight");
    const CandidateCurveProvider_<double> provider(std::make_shared<const CandidateCurveGeometry_>(spec), Parameters());
    CalibrationCurveFamilyDescriptor_ descriptor;
    CalibrationCurveObservation_ projection, known, discount;
    projection.route_ = "USD.SOFR";
    projection.projection_ = true;
    known.known_ = .02;
    discount.route_ = "USD.OIS";
    descriptor.observations_ = {projection, projection, known, projection, discount, projection};
    Vector_<CurveScriptInternal::ObservationBinding_<double>> bindings;
    CurveScriptInternal::BindObservations(descriptor, provider, &bindings);
    ASSERT_EQ(bindings.size(), 6u);
    ASSERT_EQ(bindings[0].route_, bindings[1].route_);
    ASSERT_EQ(bindings[0].route_, bindings[3].route_);
    ASSERT_EQ(bindings[0].route_, bindings[5].route_);
    ASSERT_FALSE(bindings[2].route_);
    ASSERT_EQ(bindings[0].curve_.target_, provider.Target().get());
    ASSERT_TRUE(bindings[4].curve_.background_);
    descriptor.observations_ = {projection, projection};
    descriptor.observations_.back().route_ = "usd.sofr";
    EXPECT_THROW(CurveScriptInternal::BindObservations(descriptor, provider, &bindings), CalibrationError_);
    descriptor.observations_.back() = projection;
    descriptor.observations_.back().projection_ = false;
    EXPECT_THROW(CurveScriptInternal::BindObservations(descriptor, provider, &bindings), CalibrationError_);
}
