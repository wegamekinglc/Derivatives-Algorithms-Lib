#include <gtest/gtest.h>

#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__xccymontecarlobasket.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/storage.hpp>
#include <dal/calibration/validation_internal.hpp>

#include "../../dal-cpp/tests/script/xccymcbasketfixture.hpp"

using namespace Dal;
using namespace Dal::TestXccyBasket;

TEST(ExcelXccyMonteCarloBasketTest, TestPrepareEvaluateAndOwnedAlternatives) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    XccyMonteCarloBasketRequest_ r;
    r.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
    r.sampling_ = {"PseudoRandom", 3, 4, 1024, "dal.mrg32-normal-paths/1"};
    r.fixings_ = Empty();
    Handle_<StorableXccyMonteCarloBasketMembers_> members(new StorableXccyMonteCarloBasketMembers_(Members()));
    Handle_<StorableXccyMonteCarloBasketRequest_> request(new StorableXccyMonteCarloBasketRequest_(r));
    Handle_<StorableXccyMonteCarloBasketOutcome_> preparedOutcome, evaluated;
    Handle_<StorablePreparedXccyMonteCarloBasket_> prepared;
    const auto model = Data(ModelSpec());
    XccyBasket_PrepareMonteCarlo(members, model, request, &preparedOutcome);
    ASSERT_EQ(preparedOutcome->val_.status_, "Prepared");
    XccyMonteCarloBasketOutcome_Get_Prepared(preparedOutcome, &prepared);
    XccyBasket_EvaluateMonteCarlo(prepared, {}, &evaluated);
    ASSERT_EQ(evaluated->val_.status_, "Estimate");
    Handle_<StorableXccyMonteCarloBasketResult_> result;
    XccyMonteCarloBasketOutcome_Get_Result(evaluated, &result);
    const auto direct = PrepareXccyBasketMonteCarlo(Members(), model, r).Evaluate();
    ASSERT_EQ(DumpXccyMonteCarloBasketResultJson(result->val_), DumpXccyMonteCarloBasketResultJson(direct));
    members.reset();
    request.reset();
    preparedOutcome.reset();
    prepared.reset();
    Vector_<String_> json;
    XccyMonteCarloBasketOutcome_Get_Json(evaluated, &json);
    ASSERT_FALSE(json.empty());
    Handle_<StorableXccyMonteCarloBasketPartial_> partial;
    ASSERT_THROW(XccyMonteCarloBasketOutcome_Get_Partial(evaluated, &partial), CalibrationError_);
    ASSERT_THROW(XccyMonteCarloBasketOutcome_Get_Prepared(evaluated, &prepared), CalibrationError_);
}

namespace {
    String_ Join(const Vector_<String_>& chunks) {
        String_ result;
        for (const auto& chunk : chunks)
            result += chunk;
        return result;
    }
    Matrix_<Cell_> Fields(std::initializer_list<std::pair<const char*, Cell_>> fields) {
        Matrix_<Cell_> result(static_cast<int>(fields.size()), 2);
        int row = 0;
        for (const auto& [key, value] : fields) {
            result(row, 0) = key;
            result(row++, 1) = value;
        }
        return result;
    }
    XccyMonteCarloBasketRequest_ Request() {
        XccyMonteCarloBasketRequest_ r;
        r.phase_ = XccyMonteCarloPhase_::Value_::Validation;
        r.sampling_ = {"PseudoRandom", 3, 4, 1024, "dal.mrg32-normal-paths/1"};
        r.fixings_ = Empty();
        return r;
    }
    void InputRefuses(const std::function<void()>& operation, const String_& field) {
        try {
            operation();
            FAIL() << "unavailable basket object returned";
        } catch (const CalibrationError_& e) {
            ASSERT_EQ(e.Failure().diagnostic_.code_, "CalibrationCandidateUnavailable");
            ASSERT_EQ(e.Failure().diagnostic_.field_, field);
            ASSERT_EQ(e.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Input);
            ASSERT_TRUE(e.Failure().report_);
        }
    }
} // namespace

TEST(ExcelXccyMonteCarloBasketTest, TestStrictCompleteMemberTableAndRequiredSnapshot) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    const auto instrument = Members()[0].instrument_;
    const auto key = Excel::ScriptTestStoreRepository(handle_cast<Storable_>(instrument));
    Matrix_<Cell_> table(3, 3);
    table(0, 0) = "member_id";
    table(0, 1) = "instrument";
    table(0, 2) = "measure_id";
    for (int row = 1; row < 3; ++row) {
        table(row, 0) = row == 1 ? "A" : "a";
        table(row, 1) = key;
        table(row, 2) = "Price";
    }
    Handle_<StorableXccyMonteCarloBasketMembers_> members;
    XccyMonteCarloBasketMembers_New(table, &members);
    ASSERT_EQ(members->val_.size(), 2U);
    ASSERT_EQ(CalibrationInternal::Bytes(members->val_[0].memberId_), "A");
    ASSERT_EQ(CalibrationInternal::Bytes(members->val_[1].memberId_), "a");
    table(2, 2) = true;
    try {
        XccyMonteCarloBasketMembers_New(table, &members);
        FAIL();
    } catch (const Exception_& e) {
        ASSERT_NE(std::string(e.what()).find("members row=3 column=3"), std::string::npos);
    }
    table(2, 2) = "Price";
    table(0, 2) = "MEMBER_ID";
    ASSERT_THROW(XccyMonteCarloBasketMembers_New(table, &members), Exception_);
    const Handle_<StorableCalibrationMonteCarloSampling_> sampling(new StorableCalibrationMonteCarloSampling_(Request().sampling_));
    Handle_<StorableXccyMonteCarloBasketRequest_> request;
    try {
        XccyMonteCarloBasketRequest_New(Cell_("HeldOut"), sampling, {}, {}, &request);
        FAIL();
    } catch (const CalibrationError_& e) {
        ASSERT_EQ(e.Failure().diagnostic_.field_, "/context/fixings");
    }
    const Handle_<StorableMarketFixingSnapshot_> fixings(new StorableMarketFixingSnapshot_(Empty()));
    XccyMonteCarloBasketRequest_New(Cell_("HeldOut"), sampling, fixings, {}, &request);
    ASSERT_TRUE(request->val_.fixings_);
    for (const auto& fields : {Fields({{"phase", Cell_("Fit")}}), Fields({{"simulation", Cell_(true)}}),
                               Fields({{"derivative_method", Cell_("None")}, {"DERIVATIVE_METHOD", Cell_("None")}})}) {
        ASSERT_THROW(XccyMonteCarloBasketRequest_New(Cell_("HeldOut"), sampling, fixings, fields, &request), Exception_);
    }
}

TEST(ExcelXccyMonteCarloBasketTest, TestEarlyFailureEnvelopeAndAbsentGetters) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    const Handle_<StorableXccyMonteCarloBasketMembers_> members(new StorableXccyMonteCarloBasketMembers_(Members()));
    auto r = Request();
    r.fixings_.reset();
    const Handle_<StorableXccyMonteCarloBasketRequest_> request(new StorableXccyMonteCarloBasketRequest_(r));
    Handle_<StorableXccyMonteCarloBasketOutcome_> outcome;
    XccyBasket_PrepareMonteCarlo(members, Data(ModelSpec()), request, &outcome);
    ASSERT_EQ(outcome->val_.status_, "Failed");
    ASSERT_FALSE(outcome->val_.partial_);
    ASSERT_TRUE(outcome->val_.report_);
    Handle_<StorablePreparedXccyMonteCarloBasket_> prepared;
    Handle_<StorableXccyMonteCarloBasketResult_> result;
    Handle_<StorableXccyMonteCarloBasketPartial_> partial;
    Handle_<XccyGaussianModelData_> model;
    InputRefuses([&] { XccyMonteCarloBasketOutcome_Get_Prepared(outcome, &prepared); }, "/outcome/prepared");
    InputRefuses([&] { XccyMonteCarloBasketOutcome_Get_Result(outcome, &result); }, "/outcome/result");
    InputRefuses([&] { XccyMonteCarloBasketOutcome_Get_Partial(outcome, &partial); }, "/outcome/partial");
    InputRefuses([&] { XccyMonteCarloBasketOutcome_Get_Model(outcome, &model); }, "/outcome/model");
    Vector_<String_> json;
    XccyMonteCarloBasketOutcome_Get_Json(outcome, &json);
    const auto envelope = CalibrationInternal::Parse(Join(json));
    ASSERT_EQ(CalibrationInternal::Field(envelope, "schema").string_, "dal.xccy-gaussian-public-basket-outcome/1");
    ASSERT_EQ(CalibrationInternal::Field(envelope, "partial").kind_, CalibrationInternal::JsonKind_::NIL);
    ASSERT_EQ(CalibrationInternal::Field(envelope, "result").kind_, CalibrationInternal::JsonKind_::NIL);
    ASSERT_EQ(CalibrationInternal::Field(envelope, "validated_model_identity").kind_, CalibrationInternal::JsonKind_::NIL);
}

TEST(ExcelXccyMonteCarloBasketTest, TestLateMemberAndBatchKeepIndependentPartialHandle) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    for (size_t batch : {0U, 1U}) {
        Handle_<StorableXccyMonteCarloBasketPartial_> partial;
        String_ expected;
        {
            const auto spec = NonflatSpec();
            const PreparedXccyMcBasket_ plan(Members(), Context(spec, Empty()), Data(spec), AadSettings());
            Handle_<StorableXccyMonteCarloBasketOutcome_> outcome;
            try {
                static_cast<void>(XccyMcBasketInternal::TestAccess_::Evaluate(plan, {}, [batch](const auto& stage, size_t i, size_t b, size_t) {
                    if (stage == "AfterDraw" && i == 1 && b == batch)
                        CalibrationInternal::Fail("CalibrationNumericalFailure", "/execution/payment", "quotes\" and controls\n retained");
                }));
                FAIL();
            } catch (const XccyMcBasketError_& e) {
                XccyBasketFailureForTesting(e, &outcome);
                ASSERT_EQ(outcome->val_.report_.get(), e.Partial()->Report().get());
                expected = DumpXccyMonteCarloBasketPartialJson(e.Partial());
            }
            ASSERT_EQ(outcome->val_.status_, "Failed");
            ASSERT_TRUE(outcome->val_.model_);
            ASSERT_FALSE(outcome->val_.prepared_);
            ASSERT_FALSE(outcome->val_.result_);
            XccyMonteCarloBasketOutcome_Get_Partial(outcome, &partial);
            Handle_<StorableXccyMonteCarloBasketResult_> result;
            InputRefuses([&] { XccyMonteCarloBasketOutcome_Get_Result(outcome, &result); }, "/outcome/result");
            Vector_<String_> chunks;
            XccyMonteCarloBasketOutcome_Get_Json(outcome, &chunks);
            const auto envelope = CalibrationInternal::Parse(Join(chunks));
            ASSERT_EQ(CalibrationInternal::Jcs(CalibrationInternal::Field(envelope, "partial")), CalibrationInternal::Bytes(expected));
            ASSERT_EQ(CalibrationInternal::Field(envelope, "diagnostic").object_.at("message").string_,
                      CalibrationInternal::Bytes(outcome->val_.diagnostic_->message_));
        }
        Vector_<String_> chunks;
        XccyMonteCarloBasketPartial_Get_Json(partial, &chunks);
        ASSERT_EQ(CalibrationInternal::Jcs(CalibrationInternal::Parse(Join(chunks))), CalibrationInternal::Bytes(expected));
        ASSERT_EQ(partial->val_->Snapshot().members_[1].batches_.size(), batch);
        ASSERT_FALSE(partial->val_->Snapshot().members_[1].completedEstimate_);
        ASSERT_EQ(partial->val_->Snapshot().members_[0].state_, "Complete");
    }
}

TEST(ExcelXccyMonteCarloBasketTest, TestRejectedGeometryCandidateAndSessionPersistence) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    const auto spec = ModelSpec();
    const Handle_<StorablePreparedXccyMonteCarloBasket_> prepared(
        new StorablePreparedXccyMonteCarloBasket_(PrepareXccyBasketMonteCarlo(Members(), Data(spec), Request())));
    auto changed = spec;
    changed.dynamics_.bucketEndDates_[0] = changed.dynamics_.bucketEndDates_[0].AddDays(1);
    Handle_<StorableXccyMonteCarloBasketOutcome_> outcome;
    XccyBasket_EvaluateMonteCarlo(prepared, Data(changed), &outcome);
    ASSERT_EQ(outcome->val_.status_, "Failed");
    ASSERT_TRUE(outcome->val_.partial_);
    ASSERT_EQ(outcome->val_.partial_->Snapshot().consumedCoordinates_, 0U);
    ASSERT_FALSE(outcome->val_.model_);
    Handle_<StorableXccyMonteCarloBasketPartial_> partial;
    XccyMonteCarloBasketOutcome_Get_Partial(outcome, &partial);
    Vector_<String_> unavailable;
    InputRefuses([&] { XccyMonteCarloBasketPartial_Get_Json({}, &unavailable); }, "/outcome/partial");
    ASSERT_THROW(static_cast<void>(WriteObjectJson(*partial)), CalibrationError_);
}

TEST(ExcelXccyMonteCarloBasketTest, TestLastMemberPreflightHasNoDrawsOrFallbackModel) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    Handle_<StorableXccyMonteCarloBasketOutcome_> outcome;
    size_t draws = 0;
    {
        const auto spec = NonflatSpec();
        const PreparedXccyMcBasket_ plan(Members(), Context(spec, Empty()), Data(spec), AadSettings());
        try {
            static_cast<void>(XccyMcBasketInternal::TestAccess_::Evaluate(plan, {}, [&](const auto& stage, size_t i, size_t, size_t) {
                if (stage == "AfterDraw")
                    ++draws;
                if (stage == "Preflight" && i == 2)
                    CalibrationInputFailure("CalibrationContextMismatch", "/sampling/mapping", "controlled last-member geometry");
            }));
            FAIL();
        } catch (const XccyMcBasketError_& error) {
            XccyBasketFailureForTesting(error, &outcome);
        }
    }
    ASSERT_EQ(draws, 0U);
    ASSERT_EQ(outcome->val_.status_, "Failed");
    ASSERT_FALSE(outcome->val_.model_);
    ASSERT_EQ(outcome->val_.partial_->Snapshot().failedMember_, 2U);
    ASSERT_EQ(outcome->val_.partial_->Snapshot().consumedCoordinates_, 0U);
    ASSERT_FALSE(outcome->val_.partial_->Snapshot().header_.candidateGeometryValidated_);
    Handle_<XccyGaussianModelData_> model;
    InputRefuses([&] { XccyMonteCarloBasketOutcome_Get_Model(outcome, &model); }, "/outcome/model");
}
