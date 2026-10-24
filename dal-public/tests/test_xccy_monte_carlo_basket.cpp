#include <gtest/gtest.h>

#include <dal-public/src/global.hpp>
#include <dal-public/src/xccymontecarlobasket.hpp>
#include <dal/calibration/validation_internal.hpp>

#include "../../dal-cpp/tests/script/xccymcbasketfixture.hpp"

using namespace Dal;
using namespace Dal::TestXccyBasket;

namespace {
    void CheckMatrixJson(const CalibrationInternal::Json_& json, const char* field, const Matrix_<>& expected) {
        const auto& matrix = CalibrationInternal::Field(json, field).array_;
        ASSERT_EQ(matrix.size(), static_cast<size_t>(expected.Rows()));
        for (int row = 0; row < expected.Rows(); ++row) {
            ASSERT_EQ(matrix[row].array_.size(), static_cast<size_t>(expected.Cols()));
            for (int col = 0; col < expected.Cols(); ++col)
                ASSERT_DOUBLE_EQ(matrix[row].array_[col].number_, expected(row, col));
        }
    }
} // namespace

TEST(PublicXccyMonteCarloBasketTest, TestCoreParityAndCompleteOwnedJson) {
    InitGlobalData();
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    XccyMonteCarloBasketRequest_ request;
    request.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
    request.sampling_ = {"PseudoRandom", 3, 4, 1024, "dal.mrg32-normal-paths/1"};
    request.fixings_ = Empty();
    request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
    request.parameterLabels_ = {"fxspot:EUR/USD"};
    auto settings = Settings(4, 3);
    settings.phase_ = request.phase_;
    settings.aad_ = true;
    settings.parameterLabels_ = request.parameterLabels_;
    const auto core = PreparedXccyMcBasket_(Members(), Context(spec, request.fixings_), model, settings).Evaluate();
    const auto result = PrepareXccyBasketMonteCarlo(Members(), model, request).Evaluate();
    const auto& actual = result.Snapshot();
    const auto& expected = core.Snapshot();
    ASSERT_EQ(actual.allocation_.identity_, expected.allocation_.identity_);
    ASSERT_EQ(actual.coefficientRows_, expected.coefficientRows_);
    ASSERT_EQ(actual.coefficientMeans_, expected.coefficientMeans_);
    ASSERT_EQ(actual.coefficientAxes_.size(), 12U);
    ASSERT_EQ(actual.members_.size(), 3U);
    for (int i = 0; i < expected.coefficientCovariance_.Rows(); ++i)
        for (int j = 0; j < expected.coefficientCovariance_.Cols(); ++j)
            ASSERT_DOUBLE_EQ(actual.coefficientCovariance_(i, j), expected.coefficientCovariance_(i, j));
    const auto json = CalibrationInternal::Parse(DumpXccyMonteCarloBasketResultJson(result));
    ASSERT_EQ(CalibrationInternal::Field(json, "schema").string_, "dal.xccy-gaussian-public-basket-result/1");
    ASSERT_EQ(CalibrationInternal::Field(json, "consumed_coordinates").string_, "1980");
    ASSERT_EQ(CalibrationInternal::Field(json, "members").array_.size(), 3U);
    ASSERT_EQ(CalibrationInternal::Field(json, "output_jacobian").array_.size(), 6U);
    ASSERT_NO_FATAL_FAILURE(CheckMatrixJson(json, "coefficient_covariance", expected.coefficientCovariance_));
    ASSERT_NO_FATAL_FAILURE(CheckMatrixJson(json, "mean_covariance", expected.meanCovariance_));
    ASSERT_NO_FATAL_FAILURE(CheckMatrixJson(json, "output_jacobian", expected.outputJacobian_));
    ASSERT_NO_FATAL_FAILURE(CheckMatrixJson(json, "output_covariance", expected.outputCovariance_));
    const auto& axes = CalibrationInternal::Field(json, "coefficient_axes").array_;
    ASSERT_EQ(axes.size(), expected.coefficientAxes_.size());
    for (size_t i = 0; i < axes.size(); ++i) {
        ASSERT_EQ(CalibrationInternal::Field(axes[i], "member_id").string_, CalibrationInternal::Bytes(expected.coefficientAxes_[i].memberId_));
        ASSERT_EQ(CalibrationInternal::Field(axes[i], "label").string_, CalibrationInternal::Bytes(expected.coefficientAxes_[i].label_));
        ASSERT_EQ(CalibrationInternal::Field(axes[i], "unit").string_, CalibrationInternal::Bytes(expected.coefficientAxes_[i].unit_));
    }
    ASSERT_EQ(CalibrationInternal::Field(json, "numerical_validation_passed").kind_, CalibrationInternal::JsonKind_::NIL);
    ASSERT_FALSE(result.Report()->QuoteRiskAvailable());
}

namespace {
    XccyMonteCarloBasketRequest_ Request(XccyMonteCarloPhase_ phase = XccyMonteCarloPhase_::Value_::Validation) {
        XccyMonteCarloBasketRequest_ r;
        r.phase_ = phase;
        r.sampling_ = {"PseudoRandom", 3, 4, 1024, "dal.mrg32-normal-paths/1"};
        r.fixings_ = Empty();
        return r;
    }
    void Refuses(const std::function<void()>& operation, const String_& code, const String_& field) {
        try {
            operation();
            FAIL() << "invalid public basket input accepted";
        } catch (const CalibrationError_& e) {
            ASSERT_EQ(e.Failure().diagnostic_.code_, code);
            ASSERT_EQ(e.Failure().diagnostic_.field_, field);
            ASSERT_TRUE(e.Failure().report_);
        }
    }
} // namespace

TEST(PublicXccyMonteCarloBasketTest, TestRequiredSnapshotPhaseAndStrictSettings) {
    InitGlobalData();
    const auto model = Data(ModelSpec());
    auto r = Request();
    r.fixings_.reset();
    Refuses([&] { static_cast<void>(PrepareXccyBasketMonteCarlo(Members(), model, r)); }, "InvalidCalibrationPlan", "/context/fixings");
    r = Request();
    r.phase_ = {};
    Refuses([&] { static_cast<void>(PrepareXccyBasketMonteCarlo(Members(), model, r)); }, "InvalidCalibrationPlan", "/phase");
    r = Request();
    r.simulation_.useBb_ = true;
    Refuses([&] { static_cast<void>(PrepareXccyBasketMonteCarlo(Members(), model, r)); }, "InvalidCalibrationPlan", "/simulation");
    r = Request();
    r.sampling_.mode_ = "pseudorandom";
    Refuses([&] { static_cast<void>(PrepareXccyBasketMonteCarlo(Members(), model, r)); }, "InvalidCalibrationPlan", "/sampling/mode");
    r = Request();
    r.derivativeMethod_ = CalibrationDerivative_::Value_::FiniteDifference;
    Refuses([&] { static_cast<void>(PrepareXccyBasketMonteCarlo(Members(), model, r)); }, "DerivativeUnavailable", "/derivative_method");
    Refuses([] { static_cast<void>(DumpXccyMonteCarloBasketPartialJson({})); }, "CalibrationCandidateUnavailable", "/outcome/partial");
}

TEST(PublicXccyMonteCarloBasketTest, TestPhaseOrderDuplicateEconomicsAndNumericRefresh) {
    InitGlobalData();
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    auto rows = Members();
    rows[1].instrument_ = rows[0].instrument_;
    rows[0].memberId_ = "A";
    rows[1].memberId_ = "a";
    const auto plan = PrepareXccyBasketMonteCarlo(rows, model, Request());
    const auto validation = plan.Evaluate();
    const auto fit = PrepareXccyBasketMonteCarlo(rows, model, Request(XccyMonteCarloPhase_::Value_::Fit)).Evaluate();
    ASSERT_EQ(validation.Snapshot().allocation_.identity_, fit.Snapshot().allocation_.identity_);
    ASSERT_NE(validation.Snapshot().allocation_.executionIdentity_, fit.Snapshot().allocation_.executionIdentity_);
    ASSERT_EQ(validation.Snapshot().members_[0].memberId_, "A");
    ASSERT_EQ(validation.Snapshot().members_[1].memberId_, "a");
    ASSERT_NE(plan.Allocation().members_[0].firstCoordinate_, plan.Allocation().members_[1].firstCoordinate_);
    auto changed = spec;
    changed.market_.fxSpot_ = 1.2;
    const auto refreshed = plan.Evaluate(Data(changed));
    ASSERT_NE(refreshed.Snapshot().model_->Identity(), model->Identity());
    ASSERT_EQ(refreshed.Snapshot().allocation_.identity_, validation.Snapshot().allocation_.identity_);
    ASSERT_NE(refreshed.Snapshot().coefficientRows_, validation.Snapshot().coefficientRows_);
    ASSERT_EQ(plan.Evaluate().Snapshot().coefficientRows_, validation.Snapshot().coefficientRows_);
    ASSERT_EQ(CalibrationInternal::Field(CalibrationInternal::Parse(DumpXccyMonteCarloBasketResultJson(validation)), "members")
                  .array_[0]
                  .object_.at("completed_estimate")
                  .object_.at("gradient")
                  .kind_,
              CalibrationInternal::JsonKind_::NIL);
}

TEST(PublicXccyMonteCarloBasketTest, TestRejectedRefreshPreservesDerivedErrorAndNullCandidate) {
    InitGlobalData();
    Handle_<XccyMcBasketPartial_> partial;
    Handle_<CalibrationReport_> report;
    {
        const auto spec = ModelSpec();
        const auto plan = PrepareXccyBasketMonteCarlo(Members(), Data(spec), Request());
        auto changed = spec;
        changed.dynamics_.bucketEndDates_[0] = changed.dynamics_.bucketEndDates_[0].AddDays(1);
        try {
            static_cast<void>(plan.Evaluate(Data(changed)));
            FAIL();
        } catch (const XccyMcBasketError_& e) {
            partial = e.Partial();
            report = e.Failure().report_;
            ASSERT_FALSE(e.Failure().candidate_);
            ASSERT_EQ(partial->Report().get(), report.get());
        }
    }
    ASSERT_TRUE(partial);
    ASSERT_EQ(partial->Snapshot().consumedCoordinates_, 0U);
    ASSERT_FALSE(partial->Snapshot().header_.candidateGeometryValidated_);
    ASSERT_FALSE(partial->Snapshot().header_.nativeMapIdentity_);
    const auto json = CalibrationInternal::Parse(DumpXccyMonteCarloBasketPartialJson(partial));
    ASSERT_EQ(CalibrationInternal::Field(json, "status").string_, "Failed");
    ASSERT_EQ(CalibrationInternal::Field(json, "mean_covariance").kind_, CalibrationInternal::JsonKind_::NIL);
    ASSERT_EQ(CalibrationInternal::Field(json, "completed_members").string_, "0");
}

TEST(PublicXccyMonteCarloBasketTest, TestLateMemberAndBatchPartialJsonOutliveUnwind) {
    InitGlobalData();
    for (size_t batch : {0U, 1U}) {
        Handle_<XccyMcBasketPartial_> partial;
        Handle_<Storable_> candidate;
        {
            const auto spec = NonflatSpec();
            const PreparedXccyMcBasket_ plan(Members(), Context(spec, Empty()), Data(spec), AadSettings());
            try {
                static_cast<void>(XccyMcBasketInternal::TestAccess_::Evaluate(plan, {}, [batch](const auto& stage, size_t i, size_t b, size_t) {
                    if (stage == "AfterDraw" && i == 1 && b == batch)
                        CalibrationInternal::Fail("CalibrationNumericalFailure", "/execution/payment", "owned late fault");
                }));
                FAIL();
            } catch (const XccyMcBasketError_& e) {
                partial = e.Partial();
                candidate = e.Failure().candidate_;
                ASSERT_EQ(partial->Report().get(), e.Failure().report_.get());
            }
        }
        ASSERT_TRUE(candidate);
        const auto& p = partial->Snapshot();
        ASSERT_EQ(p.members_[0].state_, "Complete");
        ASSERT_EQ(p.members_[1].batches_.size(), batch);
        ASSERT_FALSE(p.members_[1].completedEstimate_);
        ASSERT_EQ(p.members_[2].state_, "NotStarted");
        ASSERT_EQ(p.consumedCoordinates_, 12U * 40 + (batch * 3 + 1) * 65);
        const auto json = CalibrationInternal::Parse(DumpXccyMonteCarloBasketPartialJson(partial));
        ASSERT_EQ(CalibrationInternal::Field(json, "schema").string_, "dal.xccy-gaussian-mc-basket-partial/1");
        ASSERT_EQ(CalibrationInternal::Field(json, "coefficient_covariance").kind_, CalibrationInternal::JsonKind_::NIL);
        const auto& member = CalibrationInternal::Field(json, "members").array_[1];
        ASSERT_EQ(CalibrationInternal::Field(member, "batches").array_.size(), batch);
        ASSERT_EQ(CalibrationInternal::Field(member, "completed_estimate").kind_, CalibrationInternal::JsonKind_::NIL);
    }
}

TEST(PublicXccyMonteCarloBasketTest, TestActualFacadeSummaryFailureRetainsPartialWithoutEstimate) {
    InitGlobalData();
    Handle_<XccyMcBasketPartial_> partial;
    Handle_<Storable_> candidate;
    {
        auto members = Members("BasisSpread");
        const CrossCurrencySwap_ swap(TestXccy::TODAY, TestXccy::START, Date_(2028, 5, 4), .001,
                                      TestXccy::Config(XccyNotionalMode_::Value_::MARK_TO_MARKET));
        members[1].instrument_ = NewXccyScriptInstrument(swap, {"large-annuity-floor", "USD.OIS", true, 1e6});
        try {
            static_cast<void>(PrepareXccyBasketMonteCarlo(members, Data(ModelSpec()), Request()).Evaluate());
            FAIL();
        } catch (const XccyMcBasketError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, "InvalidCalibrationAnnuity");
            partial = error.Partial();
            candidate = error.Failure().candidate_;
            ASSERT_EQ(partial->Report().get(), error.Failure().report_.get());
        }
    }
    ASSERT_TRUE(candidate);
    ASSERT_EQ(partial->Snapshot().members_[1].state_, "CoefficientsComplete");
    ASSERT_EQ(partial->Snapshot().members_[1].batches_.size(), 4U);
    ASSERT_FALSE(partial->Snapshot().members_[1].completedEstimate_);
    const auto json = CalibrationInternal::Parse(DumpXccyMonteCarloBasketPartialJson(partial));
    ASSERT_EQ(CalibrationInternal::Field(json, "members").array_[1].object_.at("completed_estimate").kind_, CalibrationInternal::JsonKind_::NIL);
    ASSERT_EQ(CalibrationInternal::Field(json, "standard_error").kind_, CalibrationInternal::JsonKind_::NIL);
}
