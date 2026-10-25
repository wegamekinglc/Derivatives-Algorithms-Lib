#include <gtest/gtest.h>

#include <dal-public/src/global.hpp>
#include <dal/platform/platform.hpp>

#include "xccy_acceptance_basket_fixture.hpp"

using namespace Dal;
namespace Basket = Dal::TestXccyBasketAcceptance;

TEST(PublicXccyAcceptanceBasketTest, TestCompleteDatedMappingsAcrossPhases) {
    InitGlobalData();
    AAD::Clear(*AAD::Tape());
    const Vector_<XccyMonteCarloPhase_> phases{XccyMonteCarloPhase_::Value_::Fit, XccyMonteCarloPhase_::Value_::Validation,
                                               XccyMonteCarloPhase_::Value_::HeldOut};
    for (const auto phase : phases) {
        const auto model = TestXccyAcceptance::Model();
        const auto members = Basket::Members();
        const auto request = Basket::Request(phase);
        const auto expected = Basket::Core(members, model, request);
        const auto prepared = PrepareXccyBasketMonteCarlo(members, model, request);
        ASSERT_TRUE(prepared.Report());
        const auto actual = prepared.Evaluate();
        ASSERT_EQ(DumpXccyMonteCarloBasketResultJson(actual), DumpXccyMonteCarloBasketResultJson(expected));
        ASSERT_NO_FATAL_FAILURE(Basket::CheckComplete(actual.Snapshot(), phase));
        RecordProperty(phase.String(), std::string(DumpXccyMonteCarloBasketResultJson(actual).c_str()));
        ASSERT_FALSE(actual.Report()->QuoteRiskAvailable());
    }
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0U);
    AAD::Clear(*AAD::Tape());
}

TEST(PublicXccyAcceptanceBasketTest, TestIndependentPhasesAndPrefixAreNotNewScenarios) {
    InitGlobalData();
    AAD::Clear(*AAD::Tape());
    const auto model = TestXccyAcceptance::Model();
    const auto members = Basket::Members();
    const auto fit = PrepareXccyBasketMonteCarlo(members, model, Basket::Request(XccyMonteCarloPhase_::Value_::Fit)).Evaluate();
    const auto held = PrepareXccyBasketMonteCarlo(members, model, Basket::Request(XccyMonteCarloPhase_::Value_::HeldOut)).Evaluate();
    ASSERT_NO_THROW(RequireXccyMcBasketIndependent(fit, held));
    auto longerRequest = Basket::Request(XccyMonteCarloPhase_::Value_::HeldOut);
    longerRequest.sampling_.batches_ = 5;
    const auto prefix = PrepareXccyBasketMonteCarlo(members, model, longerRequest).Evaluate();
    ASSERT_THROW(RequireXccyMcBasketIndependent(held, prefix), CalibrationError_);
    for (size_t m = 0; m < 3; ++m)
        for (size_t b = 0; b < 4; ++b)
            ASSERT_EQ(held.Snapshot().members_[m].batches_[b].row_, prefix.Snapshot().members_[m].batches_[b].row_);
    ASSERT_NE(held.Snapshot().allocation_.members_[0].ranges_[0].firstCoordinate_,
              held.Snapshot().allocation_.members_[1].ranges_[0].firstCoordinate_);
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0U);
    AAD::Clear(*AAD::Tape());
}
