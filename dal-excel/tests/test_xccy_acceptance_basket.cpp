#include <gtest/gtest.h>

#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__xccymontecarlobasket.hpp>
#include <dal-public/src/global.hpp>
#include <dal/platform/platform.hpp>

#include "../../dal-cpp/tests/script/xccymcbasketfixture.hpp"
#include "../../dal-public/tests/xccy_acceptance_basket_fixture.hpp"

using namespace Dal;
namespace Basket = Dal::TestXccyBasketAcceptance;

namespace {
    String_ Json(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome) {
        Vector_<String_> chunks;
        XccyMonteCarloBasketOutcome_Get_Json(outcome, &chunks);
        String_ result;
        for (const auto& chunk : chunks)
            result += chunk;
        return result;
    }
    void Unavailable(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome, const String_& status) {
        Handle_<StorableXccyMonteCarloBasketResult_> result;
        Handle_<StorableXccyMonteCarloBasketPartial_> partial;
        Handle_<StorablePreparedXccyMonteCarloBasket_> prepared;
        if (status != "Estimate")
            ASSERT_THROW(XccyMonteCarloBasketOutcome_Get_Result(outcome, &result), CalibrationError_);
        if (status != "Prepared")
            ASSERT_THROW(XccyMonteCarloBasketOutcome_Get_Prepared(outcome, &prepared), CalibrationError_);
        if (status != "Failed")
            ASSERT_THROW(XccyMonteCarloBasketOutcome_Get_Partial(outcome, &partial), CalibrationError_);
    }
} // namespace

TEST(ExcelXccyAcceptanceBasketTest, TestCompleteDatedMappingsAcrossPhases) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    AAD::Clear(*AAD::Tape());
    const Vector_<XccyMonteCarloPhase_> phases{XccyMonteCarloPhase_::Value_::Fit, XccyMonteCarloPhase_::Value_::Validation,
                                               XccyMonteCarloPhase_::Value_::HeldOut};
    for (const auto phase : phases) {
        Handle_<StorableXccyMonteCarloBasketOutcome_> evaluated;
        String_ expected;
        {
            const auto model = TestXccyAcceptance::Model();
            const auto request = Basket::Request(phase);
            const auto rows = Basket::Members();
            expected = DumpXccyMonteCarloBasketResultJson(Basket::Core(rows, model, request));
            Matrix_<Cell_> table(4, 3);
            table(0, 0) = "measure_id";
            table(0, 1) = "member_id";
            table(0, 2) = "instrument";
            for (int i = 0; i < 3; ++i) {
                table(i + 1, 0) = rows[i].measureId_;
                table(i + 1, 1) = rows[i].memberId_;
                table(i + 1, 2) = Excel::ScriptTestStoreRepository(handle_cast<Storable_>(rows[i].instrument_));
            }
            Handle_<StorableXccyMonteCarloBasketMembers_> members;
            XccyMonteCarloBasketMembers_New(table, &members);
            const Handle_<StorableXccyMonteCarloBasketRequest_> input(new StorableXccyMonteCarloBasketRequest_(request));
            Handle_<StorableXccyMonteCarloBasketOutcome_> preparedOutcome;
            XccyBasket_PrepareMonteCarlo(members, model, input, &preparedOutcome);
            ASSERT_EQ(preparedOutcome->val_.status_, "Prepared");
            ASSERT_NO_FATAL_FAILURE(Unavailable(preparedOutcome, "Prepared"));
            const auto envelope = CalibrationInternal::Parse(Json(preparedOutcome));
            ASSERT_EQ(CalibrationInternal::Field(envelope, "result").kind_, CalibrationInternal::JsonKind_::NIL);
            ASSERT_EQ(CalibrationInternal::Field(envelope, "partial").kind_, CalibrationInternal::JsonKind_::NIL);
            Handle_<StorablePreparedXccyMonteCarloBasket_> prepared;
            XccyMonteCarloBasketOutcome_Get_Prepared(preparedOutcome, &prepared);
            XccyBasket_EvaluateMonteCarlo(prepared, {}, &evaluated);
        }
        ASSERT_EQ(evaluated->val_.status_, "Estimate");
        ASSERT_NO_FATAL_FAILURE(Unavailable(evaluated, "Estimate"));
        Handle_<StorableXccyMonteCarloBasketResult_> result;
        XccyMonteCarloBasketOutcome_Get_Result(evaluated, &result);
        const auto envelope = CalibrationInternal::Parse(Json(evaluated));
        ASSERT_EQ(CalibrationInternal::Field(envelope, "schema").string_, "dal.xccy-gaussian-public-basket-outcome/1");
        ASSERT_EQ(CalibrationInternal::Jcs(CalibrationInternal::Field(envelope, "result")), CalibrationInternal::Bytes(expected));
        ASSERT_EQ(CalibrationInternal::Field(envelope, "prepared_plan_identity").kind_, CalibrationInternal::JsonKind_::NIL);
        evaluated.reset();
        const auto actual = DumpXccyMonteCarloBasketResultJson(result->val_);
        ASSERT_EQ(actual, expected);
        ASSERT_NO_FATAL_FAILURE(Basket::CheckComplete(result->val_.Snapshot(), phase));
        RecordProperty(phase.String(), std::string(actual.c_str()));
    }
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0U);
    AAD::Clear(*AAD::Tape());
}

TEST(ExcelXccyAcceptanceBasketTest, TestActualLateSummaryFailureKeepsCandidateAndMissingEstimate) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    AAD::Clear(*AAD::Tape());
    Handle_<StorableXccyMonteCarloBasketPartial_> partial;
    Handle_<XccyGaussianModelData_> candidate;
    Handle_<CalibrationReport_> report;
    {
        auto rows = Basket::Members();
        const CrossCurrencySwap_ swap(TestXccy::TODAY, TestXccy::START, Date_(2028, 11, 4), .001,
                                      TestXccy::Config(XccyNotionalMode_::Value_::MARK_TO_MARKET));
        rows[2].instrument_ = NewXccyScriptInstrument(swap, {"acceptance-late-floor", "USD.OIS", true, 1e6});
        rows[2].measureId_ = "BasisSpread";
        const auto request = Basket::Request(XccyMonteCarloPhase_::Value_::Validation);
        const Handle_<StorableXccyMonteCarloBasketMembers_> members(new StorableXccyMonteCarloBasketMembers_(rows));
        const Handle_<StorableXccyMonteCarloBasketRequest_> input(new StorableXccyMonteCarloBasketRequest_(request));
        Handle_<StorableXccyMonteCarloBasketOutcome_> outcome;
        XccyBasket_PrepareMonteCarlo(members, TestXccyAcceptance::Model(), input, &outcome);
        ASSERT_EQ(outcome->val_.status_, "Prepared");
        Handle_<StorablePreparedXccyMonteCarloBasket_> prepared;
        XccyMonteCarloBasketOutcome_Get_Prepared(outcome, &prepared);
        XccyBasket_EvaluateMonteCarlo(prepared, {}, &outcome);
        ASSERT_EQ(outcome->val_.status_, "Failed");
        ASSERT_EQ(outcome->val_.diagnostic_->code_, "InvalidCalibrationAnnuity");
        ASSERT_NO_FATAL_FAILURE(Unavailable(outcome, "Failed"));
        XccyMonteCarloBasketOutcome_Get_Partial(outcome, &partial);
        XccyMonteCarloBasketOutcome_Get_Model(outcome, &candidate);
        XccyMonteCarloBasketOutcome_Get_Report(outcome, &report);
        const auto envelope = CalibrationInternal::Parse(Json(outcome));
        ASSERT_EQ(CalibrationInternal::Field(envelope, "result").kind_, CalibrationInternal::JsonKind_::NIL);
        ASSERT_EQ(CalibrationInternal::Field(envelope, "partial").object_.at("output_covariance").kind_, CalibrationInternal::JsonKind_::NIL);
    }
    ASSERT_TRUE(candidate);
    ASSERT_EQ(partial->val_->Report().get(), report.get());
    const auto& snapshot = partial->val_->Snapshot();
    ASSERT_EQ(snapshot.completedMembers_, 2U);
    ASSERT_EQ(snapshot.members_[0].state_, "Complete");
    ASSERT_EQ(snapshot.members_[1].state_, "Complete");
    ASSERT_EQ(snapshot.members_[2].state_, "CoefficientsComplete");
    ASSERT_EQ(snapshot.members_[2].batches_.size(), 4U);
    ASSERT_FALSE(snapshot.members_[2].completedEstimate_);
    ASSERT_FALSE(snapshot.basketComplete_);
    ASSERT_EQ(candidate->Identity(), *snapshot.header_.attemptedModelIdentity_);
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0U);
    AAD::Clear(*AAD::Tape());
}

TEST(ExcelXccyAcceptanceBasketTest, TestTrailingSettingsRejectUnknownNormalizedDuplicateAndNestedWrongType) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    const auto request = Basket::Request(XccyMonteCarloPhase_::Value_::HeldOut);
    const Handle_<StorableCalibrationMonteCarloSampling_> sampling(new StorableCalibrationMonteCarloSampling_(request.sampling_));
    const Handle_<StorableMarketFixingSnapshot_> fixings(new StorableMarketFixingSnapshot_(request.fixings_));
    for (const char* key : {"DERIVATIVE_METHOD", "unknown", "simulation"}) {
        Matrix_<Cell_> fields(4, 2);
        fields(0, 0) = "derivative_method";
        fields(0, 1) = "AAD";
        fields(3, 0) = key;
        fields(3, 1) = true;
        Handle_<StorableXccyMonteCarloBasketRequest_> result;
        try {
            XccyMonteCarloBasketRequest_New(Cell_("HeldOut"), sampling, fixings, fields, &result);
            FAIL();
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("settings row=4"), std::string::npos);
        }
    }
}
