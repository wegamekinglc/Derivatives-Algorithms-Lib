#include <gtest/gtest.h>

#include <array>

#include <dal/math/random/pseudorandom.hpp>
#include <dal/platform/platform.hpp>

#include "xccyacceptancefixture.hpp"
#include "xccyacceptancereference.hpp"

using Dal::Handle_;
using Dal::String_;
using Dal::Vector_;
using Dal::XccyNotionalMode_;

namespace {
    std::array<double, 4> Values(const Dal::XccyGaussianMomentValues_<double>& value) {
        return {value.a_, value.b_, value.price_, -value.b_ / value.a_};
    }
    Vector_<Vector_<>>
    Gradients(const Dal::PreparedCalibrationInstrument_& prepared, const Dal::XccyGaussianModelData_& model, Vector_<String_>* labels) {
        auto* tape = Dal::AAD::Tape();
        Dal::AAD::Clear(*tape);
        const auto inputs = Dal::XccyGaussianMomentInputs(model);
        const auto map = Dal::XccyGaussianMomentNativeMap(model, labels);
        Vector_<Vector_<>> result;
        {
            Vector_<Dal::AAD::Number_> parameters;
            for (double value : inputs.values_)
                parameters.push_back(Dal::AAD::Number_(value));
            for (auto& parameter : parameters)
                Dal::AAD::PutOnTape(parameter);
            Dal::AAD::Mark(*tape);
            const auto active = Dal::EvaluateXccyGaussianMoments<Dal::AAD::Number_>(prepared, model, parameters);
            const std::array<Dal::AAD::Number_, 4> outputs{active.a_, active.b_, active.price_, -active.b_ / active.a_};
            for (auto output : outputs) {
                Dal::AAD::ZeroAdjoints(*tape);
                Dal::AAD::Adjoint(output) = 1.;
                Dal::AAD::PropagateToMark(*tape);
                Vector_<> gradient(labels->size(), 0.);
                for (size_t row = 0; row < parameters.size(); ++row)
                    for (size_t column = 0; column < labels->size(); ++column)
                        gradient[column] += Dal::AAD::Adjoint(parameters[row]) * map(static_cast<int>(row), static_cast<int>(column));
                result.push_back(std::move(gradient));
            }
        }
        Dal::AAD::Clear(*tape);
        return result;
    }
} // namespace

TEST(XccyAcceptanceTest, TestIndependentCalendarAndSixtyCashflowReferences) {
    for (const auto& reference : Dal::TestXccyAcceptance::VALUES) {
        SCOPED_TRACE(String_(reference.scenario_) + "/" + reference.mode_);
        const auto fixture =
            Dal::TestXccyAcceptance::Fixture(reference.scenario_, XccyNotionalMode_(reference.mode_), reference.foreign_, reference.receive_);
        const auto model = Dal::TestXccyMc::Data(fixture.spec_);
        const auto prepared = Dal::TestXccyMoments::Prepare(fixture.instrument_, fixture.spec_);
        const auto& descriptor = **std::get_if<Dal::CalibrationXccy_>(&prepared.Descriptor());
        ASSERT_TRUE(descriptor.plan_.config_.convention_.domesticIndex_.fixingHolidays_.String().empty());
        ASSERT_TRUE(descriptor.plan_.config_.convention_.foreignIndex_.fixingHolidays_.String().empty());
        ASSERT_TRUE(descriptor.plan_.config_.convention_.foreignLeg_.paymentHolidays_.String().empty());
        ASSERT_EQ(descriptor.plan_.domesticPeriods_.size(), 2U);
        ASSERT_EQ(descriptor.plan_.foreignPeriods_.size(), 1U);
        ASSERT_EQ(descriptor.plan_.domesticPeriods_[0].rateFixingTime_.Date(), Dal::Date_(2027, 4, 30));
        ASSERT_EQ(descriptor.plan_.domesticPeriods_[1].rateFixingTime_.Date(), Dal::Date_(2027, 8, 2));
        ASSERT_EQ(descriptor.plan_.foreignPeriods_[0].rateFixingTime_.Date(), Dal::Date_(2027, 4, 30));
        ASSERT_EQ(descriptor.plan_.foreignPeriods_[0].schedule_.paymentDate_,
                  String_(reference.scenario_) == "no-lag" ? Dal::TestXccy::END : Dal::Date_(2027, 11, 8));
        const auto actual = Values(Dal::EvaluateXccyGaussianMoments<double>(prepared, *model));
        for (size_t i = 0; i < actual.size(); ++i)
            ASSERT_NEAR(actual[i], reference.values_[i], i == 3 ? 1e-12 : 1e-10);
        ASSERT_NEAR(actual[2], actual[1] + .001 * actual[0], 1e-10);
    }
}

TEST(XccyAcceptanceTest, TestIndependentNativeCoordinateDerivatives) {
    for (const String_ scenario : {"flat", "nonflat"}) {
        const auto fixture = Dal::TestXccyAcceptance::Fixture(scenario, XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true);
        const auto model = Dal::TestXccyMc::Data(fixture.spec_);
        const auto prepared = Dal::TestXccyMoments::Prepare(fixture.instrument_, fixture.spec_);
        Vector_<String_> labels;
        const auto actual = Gradients(prepared, *model, &labels);
        size_t checked = 0;
        for (const auto& reference : Dal::TestXccyAcceptance::GRADIENTS) {
            if (scenario != reference.scenario_)
                continue;
            SCOPED_TRACE(reference.label_);
            const auto found = std::find(labels.begin(), labels.end(), reference.label_);
            ASSERT_NE(found, labels.end());
            const auto column = static_cast<size_t>(found - labels.begin());
            for (size_t i = 0; i < actual.size(); ++i)
                ASSERT_NEAR(actual[i][column], reference.values_[i], (i == 3 ? 1e-12 : 1e-10) / reference.scale_);
            ++checked;
        }
        ASSERT_EQ(checked, labels.size());
        ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), 0U);
    }
}

TEST(XccyAcceptanceTest, TestFrozenPhaseAndCompleteScalarCoordinateTrace) {
    const auto fixture = Dal::TestXccyAcceptance::Fixture("flat", XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true);
    const auto model = Dal::TestXccyMc::Data(fixture.spec_);
    const auto context = Dal::TestXccyExecution::Context(fixture.spec_, Dal::TestXccyExecution::Empty());
    const std::array<Dal::Date_, 7> dates{Dal::Date_(2026, 10, 21), Dal::Date_(2027, 4, 30), Dal::Date_(2027, 5, 4), Dal::Date_(2027, 8, 2),
                                          Dal::Date_(2027, 8, 4),   Dal::Date_(2027, 11, 4), Dal::Date_(2027, 11, 8)};
    Vector_<String_> expected;
    for (size_t step = 1; step < dates.size(); ++step)
        for (const String_ slot : {"xd", "xf", "Id", "If", "Z"})
            expected.push_back("interval:" + Dal::Date::ToString(dates[step - 1]) + ":" + Dal::Date::ToString(dates[step]) + ":innovation:" + slot);
    ASSERT_EQ(model->market_->FactorNames().size(), 3U);
    const std::array<String_, 5> phases{"HeldOut", "Validation", "Fit", "Validation", "HeldOut"};
    Vector_<Vector_<>> starts;
    for (const auto& phase : phases) {
        auto settings = Dal::TestXccyMc::Settings(2, 3);
        settings.phase_ = Dal::ParseXccyMonteCarloPhase(phase);
        const auto result = Dal::PreparedXccyMc_(fixture.instrument_, "Price", context, model, settings).Evaluate();
        ASSERT_EQ(result.mapping_.dimension_, 30U);
        ASSERT_EQ(result.mapping_.normalCoordinates_, expected);
        ASSERT_EQ(result.consumedCoordinates_, 180U);
        const size_t first = phase == "Fit" ? 0 : (phase == "Validation" ? 262144 : 1310720);
        ASSERT_EQ(result.reservation_.ranges_.front().firstPath_, first);
        ASSERT_EQ(result.reservation_.ranges_.back().pathEnd_, first + 6);
        ASSERT_EQ(result.estimate_.validation_, phase == "Validation");
        ASSERT_EQ(result.estimate_.heldOut_, phase == "HeldOut");
        auto rng = Dal::New(Dal::RNGType_("MRG32"), 1024, 30, true);
        for (size_t batch = 0; batch < 2; ++batch) {
            const auto& range = result.reservation_.ranges_[batch];
            ASSERT_EQ(range.firstCoordinate_, range.firstPath_ * 30);
            ASSERT_EQ(range.coordinateEnd_, range.pathEnd_ * 30);
            rng->SkipNormalTo(first + batch * 3);
            Vector_<> normals(30);
            rng->FillNormal(&normals);
            ASSERT_EQ(normals, result.firstNormals_[batch]);
            rng->FillNormal(&normals);
            ASSERT_NE(normals, result.firstNormals_[batch]);
        }
        starts.push_back(result.firstNormals_[0]);
    }
    ASSERT_EQ(starts[0], starts[4]);
    ASSERT_EQ(starts[1], starts[3]);
    ASSERT_NE(starts[0], starts[1]);
    ASSERT_NE(starts[1], starts[2]);
}
