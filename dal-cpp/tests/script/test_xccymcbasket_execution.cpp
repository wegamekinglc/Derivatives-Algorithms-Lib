#include <gtest/gtest.h>

#include "xccymcbasketfixture.hpp"
#include <dal/curve/xccygaussianstaged.hpp>
#include <dal/math/specialfunctions.hpp>

using namespace Dal;
using namespace Dal::TestXccyBasket;
using namespace Dal::XccyMcBasketInternal;

TEST(XccyMcBasketTest, TestCanonicalThreeMaturitiesAndFullCoordinateTrace) {
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    const auto members = Members();
    const auto context = Context(spec, Empty());
    auto settings = Settings(2, 3);
    settings.retainPaths_ = true;
    const PreparedXccyMcBasket_ plan(members, context, model, settings);
    const auto value = plan.Evaluate();
    const auto& result = value.Snapshot();
    const Vector_<size_t> dims{30, 55, 80}, starts{0, 70778880, 200540160}, ends{70778880, 200540160, 389283840};
    const Vector_<String_> hashes{"sha256:47d8cbb515b7160dac446e6810a52cad3a0a64c0e464b21759604b8088628ece",
                                  "sha256:d005a08bc0a3adcea595bf0add2ac49b10b021c025fb9d8bcefb34ee023c42de",
                                  "sha256:dd9895f992c4abfc5047d51a65f90ddf0b1871027c2a1c167b3612d9805122ae"};
    for (size_t i = 0; i < members.size(); ++i) {
        const auto& allocation = plan.Allocation().members_[i];
        const auto& progress = result.members_[i];
        ASSERT_EQ(allocation.mapping_.dimension_, dims[i]);
        ASSERT_EQ(allocation.mapping_.identity_, hashes[i]);
        ASSERT_EQ(allocation.firstCoordinate_, starts[i]);
        ASSERT_EQ(allocation.coordinateEnd_, ends[i]);
        ASSERT_EQ(allocation.grid_.back(), Vector_<Date_>({Date_(2027, 11, 8), Date_(2028, 5, 8), Date_(2028, 11, 7)})[i]);
        ASSERT_EQ(progress.completedPaths_, 6U);
        ASSERT_EQ(progress.consumedCoordinates_, 6 * dims[i]);
        ASSERT_EQ(progress.state_, "Complete");
        auto reference = New(RNGType_("MRG32"), settings.seed_, 1, true);
        for (size_t b = 0; b < progress.batches_.size(); ++b) {
            const auto& batch = progress.batches_[b];
            const auto& range = allocation.ranges_[b];
            ASSERT_EQ(range.firstCoordinate_, starts[i] + range.firstPath_ * dims[i]);
            reference->SkipNormalTo(range.firstCoordinate_);
            Vector_<> normals(dims[i]);
            reference->FillNormal(&normals);
            ASSERT_EQ(batch.firstNormals_, normals);
            ASSERT_EQ(progress.actualRanges_[b].coordinateEnd_, range.coordinateEnd_);
        }
        const Script::PreparedXccyScript_ source(members[i].instrument_, context, model);
        const Script::XccyScriptPathProvider_<> provider(source);
        ASSERT_EQ(provider.Model().Grid(), allocation.grid_);
        ASSERT_EQ(allocation.mapping_.normalCoordinates_.size(), 5 * (allocation.grid_.size() - 1));
        for (size_t j = 0; j + 1 < allocation.grid_.size(); ++j)
            for (size_t s = 0; s < 5; ++s)
                ASSERT_EQ(allocation.mapping_.normalCoordinates_[5 * j + s], "interval:" + Date::ToString(allocation.grid_[j]) + ":" +
                                                                                 Date::ToString(allocation.grid_[j + 1]) +
                                                                                 ":innovation:" + provider.Model().InnovationSlots()[s]);
    }
    ASSERT_EQ(result.consumedCoordinates_, 6U * 165);
    ASSERT_EQ(result.coefficientRows_.size(), 2U);
    ASSERT_THROW(RequireXccyMcIndependent(PreparedXccyMc_(members[0].instrument_, "Price", context, model, settings).Reservation(),
                                          PreparedXccyMc_(members[1].instrument_, "Price", context, model, settings).Reservation()),
                 CalibrationError_);
    ASSERT_THROW(
        static_cast<void>(PreparedXccyMcBasket_(
            {{"bad", Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true, Date_(2028, 11, 6)), "Price"}}, context, model, settings)),
        CalibrationError_);
}

TEST(XccyMcBasketTest, TestTraversalReplayPhasesAndPrefixStayDeterministic) {
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    const auto context = Context(spec, Empty());
    auto settings = Settings(2, 3);
    settings.retainPaths_ = true;
    const PreparedXccyMcBasket_ plan(Members(), context, model, settings);
    const auto first = plan.Evaluate(), reversed = TestAccess_::Evaluate(plan, {}, {}, true), repeat = plan.Evaluate();
    ASSERT_EQ(first.Snapshot().coefficientRows_, reversed.Snapshot().coefficientRows_);
    ASSERT_EQ(first.Snapshot().coefficientRows_, repeat.Snapshot().coefficientRows_);
    ASSERT_THROW(RequireXccyMcBasketIndependent(first, repeat), CalibrationError_);
    ASSERT_NO_THROW(RequireXccyMcBasketIndependent(first, repeat, true));
    settings.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
    const PreparedXccyMcBasket_ held(Members(), context, model, settings);
    const auto heldValue = held.Evaluate();
    ASSERT_EQ(plan.Allocation().identity_, held.Allocation().identity_);
    ASSERT_NO_THROW(RequireXccyMcBasketIndependent(first, heldValue));
    ASSERT_THROW(RequireXccyMcBasketIndependent(first, heldValue, true), CalibrationError_);
    settings.phase_ = XccyMonteCarloPhase_::Value_::Validation;
    settings.pathsPerBatch_ = 6;
    const PreparedXccyMcBasket_ twice(Members(), context, model, settings);
    ASSERT_EQ(plan.Allocation().identity_, twice.Allocation().identity_);
    const auto twiceValue = twice.Evaluate();
    ASSERT_THROW(RequireXccyMcBasketIndependent(first, twiceValue), CalibrationError_);
    ASSERT_THROW(RequireXccyMcBasketIndependent(first, twiceValue, true), CalibrationError_);
    for (size_t i = 0; i < 3; ++i) {
        ASSERT_EQ(first.Snapshot().members_[i].batches_[0].firstNormals_, twiceValue.Snapshot().members_[i].batches_[0].firstNormals_);
        for (size_t path = 0; path < 6; ++path) {
            const auto& x = first.Snapshot().members_[i].retainedPaths_[path];
            const auto& y = twiceValue.Snapshot().members_[i].retainedPaths_[path];
            ASSERT_EQ(x.path_, y.path_);
            ASSERT_EQ(x.firstCoordinate_, y.firstCoordinate_);
            ASSERT_EQ(x.coefficients_.a_, y.coefficients_.a_);
            ASSERT_EQ(x.coefficients_.b_, y.coefficients_.b_);
        }
    }
    settings.phase_ = XccyMonteCarloPhase_::Value_::Fit;
    const auto fit = PreparedXccyMcBasket_(Members(), context, model, settings).Evaluate();
    ASSERT_NO_THROW(RequireXccyMcBasketIndependent(first, fit));
    ASSERT_NO_THROW(RequireXccyMcBasketIndependent(heldValue, fit));
    ASSERT_EQ(held.Evaluate().Snapshot().coefficientRows_, heldValue.Snapshot().coefficientRows_);
}

TEST(XccyMcBasketTest, TestAadTreeCompiledRefreshAndCrnNativeMap) {
    const auto spec = NonflatSpec();
    const auto model = Data(spec);
    const auto context = Context(spec, Empty());
    auto settings = AadSettings();
    const PreparedXccyMcBasket_ plan(Members(), context, model, settings);
    const auto aad = plan.Evaluate();
    settings.compiled_ = !settings.compiled_;
    const auto other = PreparedXccyMcBasket_(Members(), context, model, settings).Evaluate();
    ASSERT_EQ(aad.Snapshot().coefficientRows_, other.Snapshot().coefficientRows_);
    settings.aad_ = false;
    settings.parameterLabels_.clear();
    const PreparedXccyMcBasket_ plain(Members(), context, model, settings);
    for (size_t j = 0; j < plan.Allocation().members_.size(); ++j) {
        const auto& label = aad.Snapshot().header_.settings_.parameterLabels_[j];
        for (const double h : {1e-4, 5e-5, 2.5e-5}) {
            const auto up = plain.Evaluate(Bump(*model, label, h)), down = plain.Evaluate(Bump(*model, label, -h));
            ASSERT_NO_THROW(RequireXccyMcBasketIndependent(aad, up, true));
            ASSERT_NE(up.Snapshot().header_.attemptedModelIdentity_, down.Snapshot().header_.attemptedModelIdentity_);
            for (size_t i = 0; i < 3; ++i) {
                const auto& a = aad.Snapshot().members_[i].completedEstimate_->estimate_;
                const auto& u = up.Snapshot().members_[i].completedEstimate_->estimate_;
                const auto& d = down.Snapshot().members_[i].completedEstimate_->estimate_;
                ASSERT_NEAR(a.gradient_[j], (u.mean_ - d.mean_) / (2 * h), 2e-5);
            }
        }
    }
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0U);
}

TEST(XccyMcBasketTest, TestBothFittedModelsRunCanonicalBasketWithSelectedBasisPartials) {
    auto spec = SyntheticSpec();
    auto settings = Settings(2, 2);
    settings.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
    settings.aad_ = true;
    const auto base = ModelSpec();
    const auto context = Context(base, Empty());
    for (const auto mode : {CurveSolveMode_::Value_::EXACT, CurveSolveMode_::Value_::APPROXIMATE}) {
        spec.solver_.solveMode_ = mode;
        spec.solver_.fitWeight_ = 1e8;
        spec.solver_.fitNormTolerance_ = .5;
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        options.computeQuoteJacobian_ = true;
        const auto fit = CalibrateXccyInstruments(spec, options);
        ASSERT_TRUE(fit.gaussianModel_);
        settings.parameterLabels_ = {fit.gaussianModel_->market_->basis_->nativeLabels_.back()};
        const auto result = PreparedXccyMcBasket_(Members("BasisSpread"), context, fit.gaussianModel_, settings).Evaluate();
        ASSERT_EQ(result.Snapshot().members_.size(), 3U);
        ASSERT_EQ(result.Snapshot().members_[2].completedEstimate_->estimate_.gradient_.size(), 1U);
        ASSERT_TRUE(std::isfinite(result.Snapshot().members_[2].completedEstimate_->estimate_.gradient_[0]));
        ASSERT_EQ(spec.knotDates_[2], Date_(2028, 11, 8));
    }
}
