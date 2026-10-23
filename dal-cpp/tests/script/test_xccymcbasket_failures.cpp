#include <gtest/gtest.h>

#include "xccymcbasketfixture.hpp"
#include <dal/math/aad/recording.hpp>

using namespace Dal;
using namespace Dal::TestXccyBasket;
using namespace Dal::XccyMcBasketInternal;

namespace {
    void Refuses(const std::function<void()>& operation, const String_& code, const String_& field) {
        try {
            operation();
            FAIL() << "invalid basket accepted";
        } catch (const XccyMcBasketError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, code);
            ASSERT_EQ(error.Failure().diagnostic_.field_, field);
            ASSERT_TRUE(error.Failure().report_);
            ASSERT_FALSE(error.Failure().candidate_);
            ASSERT_FALSE(error.Partial());
        }
    }
    void Controlled(const String_& field = "/execution/payment") {
        CalibrationInternal::Fail("CalibrationNumericalFailure", field, "controlled private test fault");
    }
} // namespace

TEST(XccyMcBasketFailureTest, TestStructuralInputsOwnEarlyErrors) {
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    auto context = Context(spec, Empty());
    const auto settings = Settings(2, 2);
    auto members = Members();
    Refuses([&] { static_cast<void>(PreparedXccyMcBasket_({}, context, model, settings)); }, "InvalidCalibrationPlan", "/members");
    Refuses([&] { static_cast<void>(PreparedXccyMcBasket_(members, context, {}, settings)); }, "InvalidCalibrationPlan", "/model");
    members[2].memberId_ = members[0].memberId_;
    Refuses([&] { static_cast<void>(PreparedXccyMcBasket_(members, context, model, settings)); }, "InvalidCalibrationPlan", "/members/2/member_id");
    members = Members();
    members[2].instrument_.reset();
    Refuses([&] { static_cast<void>(PreparedXccyMcBasket_(members, context, model, settings)); }, "InvalidCalibrationPlan", "/members/2/instrument");
    context = Context(spec, {});
    Refuses([&] { static_cast<void>(PreparedXccyMcBasket_(Members(), context, model, settings)); }, "InvalidCalibrationPlan", "/context/fixings");
}

TEST(XccyMcBasketFailureTest, TestSamplingAndNativeDomainRefuseBeforeDraws) {
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    const auto context = Context(spec, Empty());
    const auto prepare = [&](const XccyMcSettings_& s) { static_cast<void>(PreparedXccyMcBasket_(Members(), context, model, s)); };
    auto settings = Settings(2, 2);
    settings.phase_ = XccyMonteCarloPhase_();
    Refuses([&] { prepare(settings); }, "InvalidCalibrationPlan", "/phase");
    settings = Settings(-1, 2);
    Refuses([&] { prepare(settings); }, "InvalidCalibrationPlan", "/sampling");
    settings = Settings(2, 2);
    settings.streamVersion_ = "DAL.MRG32-NORMAL-PATHS/1";
    Refuses([&] { prepare(settings); }, "InvalidCalibrationPlan", "/sampling");
    settings = Settings(16, 65537);
    Refuses([&] { prepare(settings); }, "CalibrationBudgetExceeded", "/sampling");
    settings = Settings(2, 2);
    settings.aad_ = true;
    settings.parameterLabels_ = {"unknown"};
    Refuses([&] { prepare(settings); }, "DerivativeUnavailable", "/mc/parameter_labels");
}

TEST(XccyMcBasketFailureTest, TestLastMemberPreflightAndRejectedCandidateHaveZeroDrawPartial) {
    const auto spec = NonflatSpec();
    const auto model = Data(spec);
    const auto context = Context(spec, Empty());
    const PreparedXccyMcBasket_ plan(Members(), context, model, AadSettings());
    size_t draws = 0;
    Handle_<XccyMcBasketPartial_> partial;
    try {
        static_cast<void>(TestAccess_::Evaluate(plan, {}, [&](const auto& stage, size_t i, size_t, size_t) {
            if (stage == "AfterDraw")
                ++draws;
            if (stage == "Preflight" && i == 2)
                CalibrationInternal::Fail("CalibrationContextMismatch", "/sampling/mapping", "controlled last member geometry");
        }));
        FAIL();
    } catch (const XccyMcBasketError_& error) {
        partial = error.Partial();
        ASSERT_TRUE(partial);
        ASSERT_FALSE(error.Failure().candidate_);
        ASSERT_EQ(partial->Snapshot().failedMember_, 2U);
        ASSERT_EQ(partial->Report().get(), error.Failure().report_.get());
    }
    ASSERT_EQ(draws, 0U);
    ASSERT_EQ(partial->Snapshot().consumedCoordinates_, 0U);
    ASSERT_FALSE(partial->Snapshot().header_.candidateGeometryValidated_);
    auto changed = spec;
    changed.dynamics_.bucketEndDates_.back() = changed.dynamics_.bucketEndDates_.back().AddDays(1);
    try {
        static_cast<void>(plan.Evaluate(Data(changed)));
        FAIL();
    } catch (const XccyMcBasketError_& error) {
        ASSERT_FALSE(error.Failure().candidate_);
        ASSERT_TRUE(error.Partial());
        ASSERT_EQ(error.Partial()->Snapshot().consumedCoordinates_, 0U);
        ASSERT_NE(error.Partial()->Snapshot().header_.attemptedModelIdentity_, model->Identity());
    }
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0U);
}

TEST(XccyMcBasketFailureTest, TestLateBatchPayloadOutlivesEveryCallerAndError) {
    Handle_<XccyMcBasketPartial_> partial;
    std::optional<CalibrationFailure_> failure;
    String_ allocation, modelIdentity;
    {
        const auto spec = NonflatSpec();
        const auto model = Data(spec);
        modelIdentity = model->Identity();
        const PreparedXccyMcBasket_ plan(Members(), Context(spec, Empty()), model, AadSettings());
        allocation = plan.Allocation().identity_;
        try {
            static_cast<void>(TestAccess_::Evaluate(plan, {}, [](const auto& stage, size_t i, size_t b, size_t) {
                if (stage == "AfterDraw" && i == 1 && b == 1)
                    Controlled();
            }));
            FAIL();
        } catch (const XccyMcBasketError_& error) {
            const auto copied = error;
            partial = copied.Partial();
            failure = copied.Failure();
            ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0U);
        }
    }
    ASSERT_TRUE(partial);
    const auto& p = partial->Snapshot();
    ASSERT_EQ(p.allocation_.identity_, allocation);
    ASSERT_EQ(p.header_.attemptedModelIdentity_, modelIdentity);
    ASSERT_TRUE(p.header_.candidateGeometryValidated_);
    ASSERT_EQ(p.completedMembers_, 1U);
    ASSERT_EQ(p.members_[0].state_, "Complete");
    ASSERT_TRUE(p.members_[0].completedEstimate_);
    ASSERT_EQ(p.members_[1].batches_.size(), 1U);
    ASSERT_FALSE(p.members_[1].completedEstimate_);
    ASSERT_EQ(p.members_[1].completedPaths_, 3U);
    ASSERT_EQ(p.allocation_.members_[0].mapping_.dimension_, 40U);
    ASSERT_EQ(p.allocation_.members_[1].mapping_.dimension_, 65U);
    ASSERT_EQ(p.members_[1].consumedCoordinates_, 4U * 65);
    ASSERT_EQ(p.members_[1].retainedPaths_.size(), 3U);
    ASSERT_EQ(p.members_[2].state_, "NotStarted");
    ASSERT_EQ(p.members_[1].batches_[0].row_.size(), 8U);
    ASSERT_EQ(p.consumedCoordinates_, 12U * 40 + 4U * 65);
    ASSERT_EQ(partial->Report().get(), failure->report_.get());
    ASSERT_EQ(handle_cast<XccyGaussianModelData_>(failure->candidate_)->Identity(), modelIdentity);
    ASSERT_FALSE(p.basketComplete_);
}

TEST(XccyMcBasketFailureTest, TestLateMemberAndSummaryFailureDoNotInventEstimates) {
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    const auto context = Context(spec, Empty());
    const PreparedXccyMcBasket_ plan(Members("BasisSpread"), context, model, Settings(4, 3));
    for (const String_ fault : {"AfterDraw", "Summary"}) {
        const std::map<String_, std::tuple<String_, size_t, size_t>> expected{{"Summary", {"CoefficientsComplete", 4, 12 * 55}},
                                                                              {"AfterDraw", {"InProgress", 0, 55}}};
        try {
            static_cast<void>(TestAccess_::Evaluate(plan, {}, [&](const auto& stage, size_t i, size_t, size_t) {
                if (stage == fault && i == 1) {
                    if (fault == "Summary")
                        CalibrationInternal::Fail("InvalidCalibrationAnnuity", "/mc/annuity", "controlled denominator confidence");
                    Controlled();
                }
            }));
            FAIL();
        } catch (const XccyMcBasketError_& error) {
            const auto& p = error.Partial()->Snapshot();
            ASSERT_EQ(p.completedMembers_, 1U);
            ASSERT_TRUE(p.members_[0].completedEstimate_);
            ASSERT_FALSE(p.members_[1].completedEstimate_);
            ASSERT_FALSE(p.members_[2].completedEstimate_);
            ASSERT_EQ(p.members_[1].state_, std::get<0>(expected.at(fault)));
            ASSERT_EQ(p.members_[1].batches_.size(), std::get<1>(expected.at(fault)));
            ASSERT_EQ(p.members_[1].consumedCoordinates_, std::get<2>(expected.at(fault)));
            ASSERT_TRUE(error.Failure().candidate_);
        }
    }
}

TEST(XccyMcBasketFailureTest, TestReportContainsOriginalSourcesAndMemberPlans) {
    const auto spec = ModelSpec();
    const auto value = PreparedXccyMcBasket_(Members(), Context(spec, Empty()), Data(spec), Settings(2, 2)).Evaluate();
    ASSERT_EQ(value.Report()->Data().plans_.size(), 3U);
    ASSERT_EQ(value.Report()->Data().observations_.size(), 3U);
    ASSERT_TRUE(value.Report()->Data().observations_[2].instrument_);
    ASSERT_EQ(value.Report()->Data().plans_[2].sourceIdentity_, Members()[2].instrument_->Definition()->SourceIdentity());
}

TEST(XccyMcBasketFailureTest, TestStaleBlockStartAndEqualDimensionDateAreRejected) {
    const auto spec = ModelSpec();
    const PreparedXccyMcBasket_ plan(Members(), Context(spec, Empty()), Data(spec), Settings(2, 2));
    auto allocation = plan.Allocation();
    ++allocation.members_[1].firstCoordinate_;
    ASSERT_THROW(RequireAllocation(allocation), CalibrationError_);
    allocation = plan.Allocation();
    allocation.members_[1].grid_[2] = allocation.members_[1].grid_[2].AddDays(1);
    ASSERT_THROW(RequireAllocation(allocation), CalibrationError_);
    allocation = plan.Allocation();
    allocation.members_[1].mapping_.dimension_ += 5;
    ASSERT_THROW(RequireAllocation(allocation), CalibrationError_);
    allocation = plan.Allocation();
    ++allocation.members_[1].ranges_[0].firstCoordinate_;
    ASSERT_THROW(RequireAllocation(allocation), CalibrationError_);
}

TEST(XccyMcBasketFailureTest, TestActualAnnuitySummaryFailureOwnsCompleteRows) {
    Handle_<XccyMcBasketPartial_> partial;
    {
        const auto spec = ModelSpec();
        auto members = Members("BasisSpread");
        const CrossCurrencySwap_ swap(TestXccy::TODAY, TestXccy::START, Date_(2028, 5, 4), .001,
                                      TestXccy::Config(XccyNotionalMode_::Value_::MARK_TO_MARKET));
        members[1].instrument_ = NewXccyScriptInstrument(swap, {"high-annuity-floor", "USD.OIS", true, 1e6});
        const PreparedXccyMcBasket_ plan(members, Context(spec, Empty()), Data(spec), Settings(4, 3));
        try {
            static_cast<void>(plan.Evaluate());
            FAIL();
        } catch (const XccyMcBasketError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, "InvalidCalibrationAnnuity");
            partial = error.Partial();
            ASSERT_TRUE(error.Failure().candidate_);
        }
    }
    ASSERT_TRUE(partial);
    ASSERT_EQ(partial->Snapshot().members_[0].state_, "Complete");
    ASSERT_EQ(partial->Snapshot().members_[1].state_, "CoefficientsComplete");
    ASSERT_EQ(partial->Snapshot().members_[1].batches_.size(), 4U);
    ASSERT_FALSE(partial->Snapshot().members_[1].completedEstimate_);
    ASSERT_EQ(partial->Snapshot().members_[2].state_, "NotStarted");
}

TEST(XccyMcBasketFailureTest, TestActiveCallerRecordingRejectsBeforeDrawAndPreservesGraph) {
    const auto spec = NonflatSpec();
    const auto model = Data(spec);
    const auto context = Context(spec, Empty());
    const PreparedXccyMcBasket_ plan(Members(), context, model, AadSettings());
    {
        AAD::RecordingScope_ caller;
        caller.StartRecording();
        AAD::Number_ x(2.), y = x * x;
        const auto nodes = AAD::Tape()->nodes_.Size();
        try {
            static_cast<void>(plan.Evaluate());
            FAIL();
        } catch (const XccyMcBasketError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.field_, "/derivatives/tape");
            ASSERT_EQ(error.Partial()->Snapshot().consumedCoordinates_, 0U);
            ASSERT_FALSE(error.Failure().candidate_);
        }
        ASSERT_EQ(AAD::Tape()->nodes_.Size(), nodes);
        caller.FinishRecording();
        Adjoint(y) = 1.;
        caller.Reverse();
        ASSERT_EQ(Adjoint(x), 4.);
    }
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0U);
}
