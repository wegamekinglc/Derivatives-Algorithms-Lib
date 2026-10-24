#pragma once

#include <dal-public/src/xccymontecarlobasket.hpp>
#include <dal/calibration/validation_internal.hpp>

#include "xccy_acceptance_fixture.hpp"

namespace Dal::TestXccyBasketAcceptance {
    inline Vector_<String_> Ids() { return {"A", "a", "tail\"\n"}; }
    inline Vector_<XccyMcBasketMember_> Members() {
        Vector_<XccyMcBasketMember_> rows;
        const auto ids = Ids();
        for (int i = 0; i < 3; ++i)
            rows.push_back({ids[i], LoadCalibrationInstrumentJson(TestXccyAcceptance::ReadFixture("basket/" + std::to_string(i) + ".json")),
                            i == 1 ? String_("BasisSpread") : String_("Price")});
        return rows;
    }
    inline XccyMonteCarloBasketRequest_ Request(XccyMonteCarloPhase_ phase) {
        XccyMonteCarloBasketRequest_ request;
        request.phase_ = phase;
        request.sampling_ = {"PseudoRandom", 3, 4, 1024, "dal.mrg32-normal-paths/1"};
        request.fixings_.reset(new MarketFixingSnapshot_({}));
        request.simulation_.compiled_ = true;
        request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
        request.parameterLabels_ = {"sigma:EUR/USD:2029-10-20", "fxspot:EUR/USD"};
        return request;
    }
    inline XccyMcBasketResult_
    Core(const Vector_<XccyMcBasketMember_>& rows, const Handle_<XccyGaussianModelData_>& model, const XccyMonteCarloBasketRequest_& request) {
        XccyMcSettings_ settings;
        settings.phase_ = request.phase_;
        settings.pathsPerBatch_ = 3;
        settings.batches_ = 4;
        settings.seed_ = 1024;
        settings.streamVersion_ = "dal.mrg32-normal-paths/1";
        settings.compiled_ = settings.aad_ = true;
        settings.parameterLabels_ = request.parameterLabels_;
        return PreparedXccyMcBasket_(rows, TestXccyAcceptance::Context(*model, request.fixings_), model, settings).Evaluate();
    }
    inline void CheckComplete(const XccyMcBasketSnapshot_& snapshot, XccyMonteCarloPhase_ phase) {
        ASSERT_EQ(snapshot.members_.size(), 3U);
        ASSERT_EQ(snapshot.coefficientRows_.size(), 4U);
        ASSERT_EQ(snapshot.coefficientAxes_.size(), 18U);
        ASSERT_EQ(snapshot.outputAxes_.size(), 9U);
        ASSERT_EQ(snapshot.coefficientCovariance_.Rows(), 18);
        ASSERT_EQ(snapshot.coefficientCovariance_.Cols(), 18);
        ASSERT_EQ(snapshot.meanCovariance_.Rows(), 18);
        ASSERT_EQ(snapshot.meanCovariance_.Cols(), 18);
        ASSERT_EQ(snapshot.outputJacobian_.Rows(), 9);
        ASSERT_EQ(snapshot.outputJacobian_.Cols(), 18);
        ASSERT_EQ(snapshot.outputCovariance_.Rows(), 9);
        ASSERT_EQ(snapshot.outputCovariance_.Cols(), 9);
        for (const auto& row : snapshot.coefficientRows_)
            ASSERT_EQ(row.size(), 18U);
        const auto ids = Ids();
        size_t consumption = 0;
        for (size_t i = 0; i < 3; ++i) {
            const auto& member = snapshot.members_[i];
            const auto& allocated = snapshot.allocation_.members_[i];
            ASSERT_EQ(CalibrationInternal::Bytes(member.memberId_), CalibrationInternal::Bytes(ids[i]));
            ASSERT_EQ(member.state_, "Complete");
            ASSERT_EQ(member.completedBatches_, 4U);
            ASSERT_EQ(member.completedPaths_, 12U);
            ASSERT_TRUE(member.completedEstimate_);
            const auto& estimate = member.completedEstimate_->estimate_;
            ASSERT_EQ(estimate.parameterLabels_, Request(phase).parameterLabels_);
            ASSERT_EQ(estimate.gradient_.size(), 2U);
            ASSERT_EQ(estimate.gradientStandardError_.size(), 2U);
            ASSERT_EQ(estimate.validation_, phase == XccyMonteCarloPhase_::Value_::Validation);
            ASSERT_EQ(estimate.heldOut_, phase == XccyMonteCarloPhase_::Value_::HeldOut);
            ASSERT_EQ(member.consumedCoordinates_, 12 * allocated.mapping_.dimension_);
            consumption += member.consumedCoordinates_;
            if (i)
                ASSERT_NE(allocated.mapping_.dimension_, snapshot.allocation_.members_[i - 1].mapping_.dimension_);
        }
        ASSERT_EQ(snapshot.consumedCoordinates_, consumption);
    }
} // namespace Dal::TestXccyBasketAcceptance
