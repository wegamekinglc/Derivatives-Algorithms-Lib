#include <gtest/gtest.h>

#include <array>

#include <dal/math/random/pseudorandom.hpp>
#include <dal/platform/platform.hpp>
#include <dal/time/dateutils.hpp>

#include "xccyacceptanceextendedfixture.hpp"
#include "xccyacceptanceextendedreference.hpp"

using Dal::Handle_;
using Dal::String_;
using Dal::Vector_;
using Dal::XccyNotionalMode_;

namespace {
    Handle_<Dal::MarketFixingSnapshot_> History(const String_& scenario) {
        Dal::MarketFixingSnapshot_::values_t values;
        if (scenario == "history") {
            values["IR:USD,LIBOR_3M_LCH"][Dal::DateTime_(Dal::Date_(2027, 4, 30))] = .031;
            values["IR:USD,LIBOR_3M_LCH"][Dal::DateTime_(Dal::Date_(2027, 8, 2))] = .037;
            values["IR:EUR,LIBOR_6M_LCH"][Dal::DateTime_(Dal::Date_(2027, 4, 30))] = .029;
            values["FX[EUR/USD]"][Dal::DateTime_(Dal::Date_(2027, 8, 4))] = 1.17;
        }
        return Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(values));
    }
    Dal::TestXccyAcceptance::Fixture_ Fixture(const Dal::TestXccyAcceptance::ExtendedValueReference_& reference) {
        const auto mode = XccyNotionalMode_(reference.mode_);
        if (String_(reference.scenario_) == "history")
            return {Dal::TestXccyMoments::ModelSpec(Dal::Date::FromString(reference.valuation_)),
                    Dal::TestXccyMoments::Instrument(mode, reference.foreign_, reference.receive_)};
        return Dal::TestXccyAcceptance::BroadFixture(reference.scenario_, mode, reference.foreign_, reference.receive_);
    }
    Vector_<> NativeGradient(Dal::AAD::Number_ output, const Vector_<Dal::AAD::Number_>& parameters, const Dal::Matrix_<>& chain, size_t size) {
        Vector_<> gradient(size, 0.);
        if (output.IsPassive())
            return gradient;
        Dal::AAD::ZeroAdjoints(*Dal::AAD::Tape());
        Dal::AAD::Adjoint(output) = 1.;
        Dal::AAD::PropagateToMark(*Dal::AAD::Tape());
        for (size_t i = 0; i < parameters.size(); ++i)
            for (size_t j = 0; j < size; ++j)
                gradient[j] += Dal::AAD::Adjoint(parameters[i]) * chain(static_cast<int>(i), static_cast<int>(j));
        return gradient;
    }
    Vector_<Vector_<>>
    Gradients(const Dal::PreparedCalibrationInstrument_& prepared, const Dal::XccyGaussianModelData_& model, Vector_<String_>* labels) {
        auto* tape = Dal::AAD::Tape();
        Dal::AAD::Clear(*tape);
        const auto inputs = Dal::XccyGaussianMomentInputs(model);
        const auto chain = Dal::XccyGaussianMomentNativeMap(model, labels);
        Vector_<Vector_<>> result;
        {
            Vector_<Dal::AAD::Number_> parameters;
            for (double value : inputs.values_)
                parameters.push_back(Dal::AAD::Number_(value));
            for (auto& parameter : parameters)
                Dal::AAD::PutOnTape(parameter);
            Dal::AAD::Mark(*tape);
            const auto active = Dal::EvaluateXccyGaussianMoments<Dal::AAD::Number_>(prepared, model, parameters);
            Vector_<Dal::AAD::Number_> outputs{active.a_, active.b_, active.price_};
            if (std::abs(Dal::Value(active.a_)) >= 1e-12)
                outputs.push_back(-active.b_ / active.a_);
            for (auto output : outputs)
                result.push_back(NativeGradient(output, parameters, chain, labels->size()));
        }
        Dal::AAD::Clear(*tape);
        return result;
    }
    void CheckGradients(const String_& id, const Dal::PreparedCalibrationInstrument_& prepared, const Dal::XccyGaussianModelData_& model) {
        Vector_<String_> labels;
        const auto actual = Gradients(prepared, model, &labels);
        size_t checked = 0;
        for (const auto& reference : Dal::TestXccyAcceptance::EXTENDED_GRADIENTS) {
            if (id != reference.id_)
                continue;
            SCOPED_TRACE(reference.label_);
            const auto found = std::find(labels.begin(), labels.end(), reference.label_);
            ASSERT_NE(found, labels.end());
            ASSERT_EQ(actual.size(), reference.size_);
            const size_t column = found - labels.begin();
            for (size_t i = 0; i < actual.size(); ++i)
                ASSERT_NEAR(actual[i][column], reference.values_[i], (i == 3 ? 1e-12 : 1e-10) / reference.scale_);
            ++checked;
        }
        ASSERT_EQ(checked, labels.size());
        ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), 0U);
    }
    Vector_<String_> Coordinates(bool fixed, bool nonflat) {
        Vector_<Dal::Date_> dates{Dal::TestXccy::TODAY,   Dal::Date_(2027, 4, 30), Dal::Date_(2027, 5, 4),
                                  Dal::Date_(2027, 8, 2), Dal::Date_(2027, 8, 4),  Dal::Date_(2027, 11, 4)};
        if (!fixed)
            dates.push_back(Dal::Date_(2027, 11, 8));
        if (nonflat)
            dates.Append(Vector_<Dal::Date_>{Dal::Date_(2027, 4, 19), Dal::Date_(2027, 10, 21)});
        std::sort(dates.begin(), dates.end());
        Vector_<String_> coordinates;
        for (size_t i = 1; i < dates.size(); ++i)
            for (const String_ slot : {"xd", "xf", "Id", "If", "Z"})
                coordinates.push_back("interval:" + Dal::Date::ToString(dates[i - 1]) + ":" + Dal::Date::ToString(dates[i]) + ":innovation:" + slot);
        return coordinates;
    }
} // namespace

TEST(XccyAcceptanceExtendedTest, TestIndependentSignedAndHistoricalValuesAndDerivatives) {
    for (const auto& reference : Dal::TestXccyAcceptance::EXTENDED_VALUES) {
        SCOPED_TRACE(reference.id_);
        const auto fixture = Fixture(reference);
        const auto model = Dal::TestXccyMc::Data(fixture.spec_);
        const auto prepared = Dal::TestXccyMoments::Prepare(fixture.instrument_, fixture.spec_, "Price", History(reference.scenario_));
        const auto value = Dal::EvaluateXccyGaussianMoments<double>(prepared, *model);
        Vector_<> actual{value.a_, value.b_, value.price_};
        if (reference.size_ == 4)
            actual.push_back(-value.b_ / value.a_);
        for (size_t i = 0; i < actual.size(); ++i)
            ASSERT_NEAR(actual[i], reference.values_[i], i == 3 ? 1e-12 : 1e-10);
        CheckGradients(reference.id_, prepared, *model);
    }
}

TEST(XccyAcceptanceExtendedTest, TestThreeMaturityIndependentQuotesAndNativeBasisAxes) {
    for (const Dal::CurveSolveMode_ mode : {Dal::CurveSolveMode_::Value_::EXACT, Dal::CurveSolveMode_::Value_::APPROXIMATE}) {
        const Vector_<> quotes{-.06705259701832679, -.054512806033648581, -.049912191545990019};
        auto spec = Dal::TestXccyAcceptance::StagedSpec(quotes);
        spec.solver_.solveMode_ = mode;
        Dal::CurveInstrumentCalibrationOptions_ options;
        options.computeQuoteJacobian_ = true;
        options.jacobianMode_ = Dal::CalibrationDerivative_::Value_::AAD;
        const auto result = Dal::CalibrateXccyInstruments(spec, options);
        ASSERT_TRUE(result.fitWithinTolerance_);
        ASSERT_EQ(result.freeParameters_.size(), 3U);
        ASSERT_EQ(result.quoteJacobian_.Rows(), 3);
        ASSERT_EQ(result.quoteJacobian_.Cols(), 3);
        for (size_t i = 0; i < 3; ++i) {
            ASSERT_EQ(result.freeParameters_[i].date_, Dal::TestXccyAcceptance::StagedKnots()[i]);
            ASSERT_EQ(result.freeParameters_[i].component_.String(), "LOG_DISCOUNT_FACTOR");
            ASSERT_EQ(result.axes_.observations_[i]->Id(), spec.observations_[i]->Id());
            ASSERT_DOUBLE_EQ(result.axes_.observations_[i]->ErrorScale(), 1e-4);
            ASSERT_NEAR(result.modelMeasures_[i], quotes[i], 1e-4);
        }
        ASSERT_LE(result.maxAbsScaledResidual_, 1.);
        ASSERT_LE(result.scaledNorm_, .5);
    }
}

TEST(XccyAcceptanceExtendedTest, TestNoncanonicalNamedMaturityAndMixedMappingBasketRefusals) {
    try {
        static_cast<void>(Dal::TestXccyMoments::Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true, Dal::Date_(2028, 11, 6)));
        FAIL();
    } catch (const Dal::CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "UnsupportedCalibrationProduct");
        ASSERT_EQ(error.Failure().diagnostic_.field_, "/import/rate");
    }
    const auto spec = Dal::TestXccyAcceptance::StagedSpec({-.06705259701832679, -.054512806033648581, -.049912191545990019});
    Dal::CalibrationPreparationContext_ context;
    context.xccy_ = std::make_shared<const Dal::XccyScriptContext_>(spec.context_);
    const auto model = Dal::TestXccyMc::Data(Dal::TestXccyAcceptance::StagedModel());
    auto settings = Dal::TestXccyMc::Settings(16, 16384);
    settings.phase_ = Dal::XccyMonteCarloPhase_::Value_::HeldOut;
    Vector_<Dal::XccyMcReservation_> reservations;
    const std::array<size_t, 3> dimensions{30, 55, 80};
    for (size_t i = 0; i < spec.observations_.size(); ++i) {
        const Dal::PreparedXccyMc_ prepared(spec.observations_[i]->Instrument(), "Price", context, model, settings);
        ASSERT_EQ(prepared.Mapping().dimension_, dimensions[i]);
        reservations.push_back(prepared.Reservation());
    }
    try {
        Dal::RequireXccyMcIndependent(reservations[0], reservations[1]);
        FAIL();
    } catch (const Dal::CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "CalibrationContextMismatch");
        ASSERT_EQ(error.Failure().diagnostic_.field_, "/sampling/mapping");
    }
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), 0U);
}

TEST(XccyAcceptanceExtendedTest, TestExtendedFiveSlotScalarCoordinatesAndDirectRngTrace) {
    const std::array<Dal::TestXccyAcceptance::Fixture_, 3> fixtures{
        Dal::TestXccyAcceptance::BroadFixture("rho-df", XccyNotionalMode_::Value_::FIXED, true, false),
        Dal::TestXccyAcceptance::BroadFixture("negative-basis", XccyNotionalMode_::Value_::RESETTABLE, true, true),
        Dal::TestXccyAcceptance::BroadFixture("nonflat", XccyNotionalMode_::Value_::MARK_TO_MARKET, true, false)};
    const std::array<int, 3> dimensions{25, 30, 40};
    for (size_t i = 0; i < fixtures.size(); ++i) {
        const auto& fixture = fixtures[i];
        const auto model = Dal::TestXccyMc::Data(fixture.spec_);
        const auto context = Dal::TestXccyExecution::Context(fixture.spec_, Dal::TestXccyExecution::Empty());
        const auto result = Dal::PreparedXccyMc_(fixture.instrument_, "Price", context, model, Dal::TestXccyMc::Settings(2, 3)).Evaluate();
        ASSERT_EQ(result.mapping_.normalCoordinates_, Coordinates(i == 0, i == 2));
        ASSERT_EQ(result.mapping_.dimension_, static_cast<size_t>(dimensions[i]));
        ASSERT_EQ(result.consumedCoordinates_, static_cast<size_t>(6 * dimensions[i]));
        auto rng = Dal::New(Dal::RNGType_("MRG32"), 1024, dimensions[i], true);
        for (size_t batch = 0; batch < 2; ++batch) {
            const auto& range = result.reservation_.ranges_[batch];
            ASSERT_EQ(range.firstPath_, 262144 + batch * 3);
            ASSERT_EQ(range.firstCoordinate_, range.firstPath_ * dimensions[i]);
            rng->SkipNormalTo(range.firstPath_);
            Vector_<> normals(dimensions[i]);
            rng->FillNormal(&normals);
            ASSERT_EQ(normals, result.firstNormals_[batch]);
            rng->FillNormal(&normals);
            ASSERT_NE(normals, result.firstNormals_[batch]);
        }
    }
}

TEST(XccyAcceptanceExtendedTest, TestOmittedRateFixingStateFailsBeforeCashflowExecution) {
    const auto fixture = Dal::TestXccyAcceptance::BroadFixture("rho-df", XccyNotionalMode_::Value_::FIXED, true, false);
    const auto model = Dal::TestXccyMc::Data(fixture.spec_);
    const auto context = Dal::TestXccyExecution::Context(fixture.spec_, Dal::TestXccyExecution::Empty());
    const Dal::Script::PreparedXccyScript_ prepared(fixture.instrument_, context, model);
    const Dal::Script::XccyScriptPathProvider_<> provider(prepared);
    auto states = provider.Model().GenerateStates(Vector_<>(provider.Model().SimDim(), .17));
    const auto fixing = std::find_if(states.begin(), states.end(), [](const auto& state) { return state.date_ == Dal::Date_(2027, 4, 30); });
    ASSERT_NE(fixing, states.end());
    static_cast<void>(provider.Observe(states));
    states.erase(fixing);
    ASSERT_THROW(static_cast<void>(provider.Observe(states)), Dal::CalibrationError_);
}
