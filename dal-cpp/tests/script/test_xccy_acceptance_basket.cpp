#include <gtest/gtest.h>

#include <sstream>

#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/script/xccymcbasket.hpp>

#include "xccyacceptanceextendedfixture.hpp"

using Dal::String_;
using Dal::Vector_;

namespace {
    void CheckProjectedCovariance(const Dal::XccyMcBasketSnapshot_& s) {
        for (int i = 0; i < s.outputJacobian_.Rows(); ++i)
            for (int j = 0; j < s.outputJacobian_.Rows(); ++j) {
                long double projected = 0.;
                for (int row = 0; row < s.meanCovariance_.Rows(); ++row)
                    for (int column = 0; column < s.meanCovariance_.Cols(); ++column)
                        projected +=
                            static_cast<long double>(s.outputJacobian_(i, row)) * s.meanCovariance_(row, column) * s.outputJacobian_(j, column);
                ASSERT_NEAR(s.outputCovariance_(i, j), static_cast<double>(projected), 1e-10);
            }
    }
} // namespace

TEST(XccyAcceptanceBasketTest, TestOriginalFittedSourceUsesJoinAbsoluteFiveSlotCoordinates) {
    std::ostringstream trace;
    size_t uses = 0;
    for (const Dal::CurveSolveMode_ mode : {Dal::CurveSolveMode_::Value_::EXACT, Dal::CurveSolveMode_::Value_::APPROXIMATE}) {
        auto spec = Dal::TestXccyAcceptance::StagedSpec({-.06705259701832679, -.054512806033648581, -.049912191545990019});
        spec.solver_.solveMode_ = mode;
        const auto fitted = Dal::CalibrateXccyInstruments(spec, {});
        ASSERT_TRUE(fitted.fitWithinTolerance_);
        Dal::CalibrationPreparationContext_ context;
        context.xccy_ = std::make_shared<const Dal::XccyScriptContext_>(spec.context_);
        ASSERT_TRUE(context.xccy_->fixings_);
        Vector_<Dal::XccyMcBasketMember_> members;
        for (const auto& o : spec.observations_)
            members.push_back({o->Id(), o->Instrument(), "Price"});
        auto settings = Dal::TestXccyMc::Settings(16, 16384);
        settings.phase_ = Dal::XccyMonteCarloPhase_::Value_::HeldOut;
        const Dal::PreparedXccyMcBasket_ prepared(members, context, fitted.gaussianModel_, settings);
        for (size_t i = 0; i < members.size(); ++i) {
            const auto& allocation = prepared.Allocation().members_[i];
            const auto source = members[i].instrument_->Product();
            ASSERT_EQ(allocation.bindingIdentity_, members[i].instrument_->BindingIdentity());
            ASSERT_EQ(allocation.sourceIdentity_, members[i].instrument_->Definition()->SourceIdentity());
            ASSERT_EQ(allocation.requests_.size(), allocation.observations_.Requests().size());
            for (const auto& request : allocation.observations_.Requests()) {
                ASSERT_FALSE(request.historical_);
                ASSERT_TRUE(request.modelSlot_);
                const size_t sample = request.modelSlot_->sampleId_;
                ASSERT_LT(sample, allocation.grid_.size());
                ASSERT_GT(sample, 0U);
                ASSERT_EQ(allocation.grid_[sample], request.key_.fixingTime_.Date());
                ASSERT_FALSE(request.uses_.empty());
                for (const auto& use : request.uses_) {
                    ASSERT_TRUE(use.source_.eventDate_);
                    ASSERT_LT(use.eventId_, source->ParsedEventDates().size());
                    ASSERT_EQ(*use.source_.eventDate_, source->ParsedEventDates()[use.eventId_]);
                    ASSERT_LE(request.key_.fixingTime_.Date(), *use.source_.eventDate_);
                    ASSERT_FALSE(use.indexOriginal_.empty());
                    ASSERT_EQ(use.fixingDate_.has_value(), !use.legacySpot_);
                    ASSERT_EQ(use.fixingDate_.value_or(*use.source_.eventDate_), request.key_.fixingTime_.Date());
                    for (size_t slot = 0; slot < 5; ++slot) {
                        const auto& range = allocation.ranges_.front();
                        const size_t coordinate =
                            allocation.firstCoordinate_ + range.firstPath_ * allocation.mapping_.dimension_ + 5 * (sample - 1) + slot;
                        ASSERT_GE(coordinate, range.firstCoordinate_);
                        ASSERT_LT(coordinate, range.coordinateEnd_);
                        const size_t local = coordinate - allocation.firstCoordinate_;
                        ASSERT_EQ(local / allocation.mapping_.dimension_, range.firstPath_);
                        ASSERT_EQ((local % allocation.mapping_.dimension_) / 5, sample - 1);
                        ASSERT_EQ(local % 5, slot);
                        trace << mode.String() << ',' << allocation.memberId_ << ',' << use.eventId_ << ',' << use.statementId_ << ',' << use.nodeId_
                              << ',' << use.source_.row_ << ',' << use.indexOriginal_ << ',' << request.key_.canonicalIndex_ << ','
                              << Dal::DateTime::ToString(request.key_.fixingTime_) << ',' << sample << ',' << request.modelSlot_->outputId_ << ','
                              << range.firstPath_ << ',' << slot << ',' << coordinate << '\n';
                    }
                    ++uses;
                }
            }
        }
    }
    ASSERT_GT(uses, 0U);
    ::testing::Test::RecordProperty("source_use_scalar_trace", trace.str());
    ::testing::Test::RecordProperty("source_uses", static_cast<int>(uses));
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), 0U);
}

TEST(XccyAcceptanceBasketTest, TestOriginalFittedParAndNativeGradientJacobians) {
    for (const Dal::CurveSolveMode_ mode : {Dal::CurveSolveMode_::Value_::EXACT, Dal::CurveSolveMode_::Value_::APPROXIMATE}) {
        auto spec = Dal::TestXccyAcceptance::StagedSpec({-.06705259701832679, -.054512806033648581, -.049912191545990019});
        spec.solver_.solveMode_ = mode;
        const auto fitted = Dal::CalibrateXccyInstruments(spec, {});
        ASSERT_TRUE(fitted.fitWithinTolerance_);
        Dal::CalibrationPreparationContext_ context;
        context.xccy_ = std::make_shared<const Dal::XccyScriptContext_>(spec.context_);
        Vector_<Dal::XccyMcBasketMember_> members;
        for (const auto& o : spec.observations_)
            members.push_back({o->Id(), o->Instrument(), "BasisSpread"});
        auto settings = Dal::TestXccyMc::Settings(2, 3);
        settings.phase_ = Dal::XccyMonteCarloPhase_::Value_::HeldOut;
        settings.aad_ = true;
        settings.parameterLabels_ = {"fxspot:EUR/USD", "logdf:USD:BASIS:EUR/USD:2028-11-08", "sigma:EUR/USD:2029-10-20"};
        const auto value = Dal::PreparedXccyMcBasket_(members, context, fitted.gaussianModel_, settings).Evaluate();
        const auto& s = value.Snapshot();
        for (size_t m = 0; m < members.size(); ++m) {
            const auto& e = *s.members_[m].completedEstimate_;
            const double a = e.coefficients_.a_, b = e.coefficients_.b_;
            ASSERT_LT(a, 0.);
            ASSERT_NEAR(e.estimate_.mean_, -b / a, 1e-12);
            ASSERT_DOUBLE_EQ(s.outputJacobian_(4 * m, 8 * m), -1. / a);
            ASSERT_DOUBLE_EQ(s.outputJacobian_(4 * m, 8 * m + 1), b / (a * a));
            for (size_t k = 0; k < 3; ++k) {
                const double da = e.coefficients_.da_[k], db = e.coefficients_.db_[k];
                ASSERT_NEAR(e.estimate_.gradient_[k], -db / a + b * da / (a * a), 1e-10);
                const int r = 4 * m + 1 + k, c = 8 * m;
                ASSERT_DOUBLE_EQ(s.outputJacobian_(r, c), da / (a * a));
                ASSERT_DOUBLE_EQ(s.outputJacobian_(r, c + 1), db / (a * a) - 2. * b * da / (a * a * a));
                ASSERT_DOUBLE_EQ(s.outputJacobian_(r, c + 2 + 2 * k), -1. / a);
                ASSERT_DOUBLE_EQ(s.outputJacobian_(r, c + 3 + 2 * k), b / (a * a));
            }
        }
        CheckProjectedCovariance(s);
        ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), 0U);
    }
}

TEST(XccyAcceptanceBasketTest, TestOriginalFittedZeroSigmaBoundaryOwnsUnavailableResult) {
    const auto spec = Dal::TestXccyAcceptance::StagedSpec({-.06705259701832679, -.054512806033648581, -.049912191545990019});
    const auto fitted = Dal::CalibrateXccyInstruments(spec, {});
    ASSERT_TRUE(fitted.fitWithinTolerance_);
    Dal::CalibrationPreparationContext_ context;
    context.xccy_ = std::make_shared<const Dal::XccyScriptContext_>(spec.context_);
    Vector_<Dal::XccyMcBasketMember_> members;
    for (const auto& o : spec.observations_)
        members.push_back({o->Id(), o->Instrument(), "Price"});
    auto settings = Dal::TestXccyMc::Settings(2, 3);
    settings.phase_ = Dal::XccyMonteCarloPhase_::Value_::HeldOut;
    settings.aad_ = true;
    settings.parameterLabels_ = {"sigma:EUR/USD:2029-10-20"};
    const auto& model = fitted.gaussianModel_;
    const auto boundary = Dal::TestXccyMc::Bump(*model, settings.parameterLabels_[0], -model->dynamics_->fxSigma_[0]);
    Dal::Handle_<Dal::XccyMcBasketPartial_> owned;
    {
        const Dal::PreparedXccyMcBasket_ prepared(members, context, model, settings);
        try {
            static_cast<void>(prepared.Evaluate(boundary));
            FAIL();
        } catch (const Dal::XccyMcBasketError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, "DerivativeUnavailable");
            ASSERT_EQ(error.Failure().diagnostic_.field_, "/derivatives/sigma");
            owned = error.Partial();
            ASSERT_TRUE(owned);
        }
    }
    ASSERT_EQ(owned->Snapshot().consumedCoordinates_, 0U);
    ASSERT_EQ(owned->Snapshot().completedMembers_, 0U);
    ASSERT_FALSE(owned->Snapshot().basketComplete_);
    for (const auto& member : owned->Snapshot().members_) {
        ASSERT_TRUE(member.batches_.empty());
        ASSERT_FALSE(member.completedEstimate_);
    }
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), 0U);
}

TEST(XccyAcceptanceBasketTest, TestHistoricalSnapshotIgnoresGlobalMutationBetweenCrnBumps) {
    const auto spec = Dal::TestXccyMoments::ModelSpec(Dal::Date_(2027, 8, 4));
    const auto model = Dal::TestXccyMc::Data(spec);
    const auto instrument = Dal::TestXccyMoments::Instrument(Dal::XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const Dal::TestXccyExecution::ScopedHistory_ usd(
        "IR:USD,LIBOR_3M_LCH", {{{Dal::DateTime_(Dal::Date_(2027, 4, 30)), .031}, {Dal::DateTime_(Dal::Date_(2027, 8, 2)), .037}}});
    const Dal::TestXccyExecution::ScopedHistory_ eur("IR:EUR,LIBOR_6M_LCH", {{{Dal::DateTime_(Dal::Date_(2027, 4, 30)), .029}}});
    const Dal::TestXccyExecution::ScopedHistory_ fx("FX[EUR/USD]", {{{spec.market_.valuationTime_, 1.17}}});
    auto settings = Dal::TestXccyMc::Settings(2, 3);
    settings.phase_ = Dal::XccyMonteCarloPhase_::Value_::HeldOut;
    ASSERT_THROW(static_cast<void>(Dal::PreparedXccyMcBasket_({{"historical-original", instrument, "Price"}}, Dal::TestXccyExecution::Context(spec),
                                                              model, settings)),
                 Dal::CalibrationError_);
    const Dal::MarketFixingSnapshot_::values_t values{
        {"IR:USD,LIBOR_3M_LCH", {{Dal::DateTime_(Dal::Date_(2027, 4, 30)), .031}, {Dal::DateTime_(Dal::Date_(2027, 8, 2)), .037}}},
        {"IR:EUR,LIBOR_6M_LCH", {{Dal::DateTime_(Dal::Date_(2027, 4, 30)), .029}}},
        {"FX[EUR/USD]", {{spec.market_.valuationTime_, 1.17}}}};
    const auto context = Dal::TestXccyExecution::Context(spec, Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(values)));
    const Dal::PreparedXccyMcBasket_ prepared({{"historical-original", instrument, "Price"}}, context, model, settings);
    for (const String_ label : {"fxspot:EUR/USD", "logdf:USD:BASIS:EUR/USD:2029-10-20", "sigma:EUR/USD:2029-10-20"})
        for (double divisor : {1., 2., 4.}) {
            const double h = Dal::TestXccyMc::Scale(label) * .001 / divisor;
            const auto plus = Dal::TestXccyMc::Bump(*model, label, h), minus = Dal::TestXccyMc::Bump(*model, label, -h);
            const auto expectedPlus = prepared.Evaluate(plus), expectedMinus = prepared.Evaluate(minus);
            usd.Set({{{Dal::DateTime_(Dal::Date_(2027, 4, 30)), .9}, {Dal::DateTime_(Dal::Date_(2027, 8, 2)), .8}}});
            eur.Set({{{Dal::DateTime_(Dal::Date_(2027, 4, 30)), .7}}});
            fx.Set({{{spec.market_.valuationTime_, 2.9}}});
            const auto actualPlus = prepared.Evaluate(plus);
            fx.Set({{{spec.market_.valuationTime_, 3.9}}});
            const auto actualMinus = prepared.Evaluate(minus);
            ASSERT_EQ(expectedPlus.Snapshot().coefficientRows_, actualPlus.Snapshot().coefficientRows_);
            ASSERT_EQ(expectedMinus.Snapshot().coefficientRows_, actualMinus.Snapshot().coefficientRows_);
            ASSERT_EQ(expectedPlus.Snapshot().allocation_.identity_, actualMinus.Snapshot().allocation_.identity_);
            ASSERT_EQ(expectedPlus.Snapshot().members_[0].batches_[0].firstNormals_, actualMinus.Snapshot().members_[0].batches_[0].firstNormals_);
        }
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), 0U);
}
