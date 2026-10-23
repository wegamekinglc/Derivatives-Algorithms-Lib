#include <gtest/gtest.h>

#include <dal/script/xccymcbasket.hpp>

#include "xccymcfixture.hpp"

using namespace Dal;
using namespace Dal::TestXccyMc;
using namespace Dal::TestXccyMoments;
using namespace Dal::TestXccyExecution;

TEST(XccyMcBasketTest, TestSingletonMatchesLegacySingleExactly) {
    const auto spec = NonflatSpec();
    const auto model = Data(spec);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const auto context = Context(spec, Empty());
    auto settings = Settings(4, 3);
    settings.retainPaths_ = true;
    const auto old = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
    const auto basket = PreparedXccyMcBasket_({{"first", instrument, "Price"}}, context, model, settings).Evaluate();
    const auto& result = basket.Snapshot();
    ASSERT_EQ(result.members_.size(), 1U);
    const auto& member = result.members_[0];
    ASSERT_TRUE(member.completedEstimate_);
    ASSERT_EQ(member.completedEstimate_->estimate_.mean_, old.estimate_.mean_);
    ASSERT_EQ(member.completedEstimate_->estimate_.standardError_, old.estimate_.standardError_);
    ASSERT_EQ(member.consumedCoordinates_, old.consumedCoordinates_);
    ASSERT_EQ(member.batches_[0].firstNormals_, old.firstNormals_[0]);
    ASSERT_EQ(result.coefficientCovariance_(0, 0), old.coefficientCovariance_(0, 0));
    for (size_t b = 0; b < old.estimate_.batches_.size(); ++b)
        ASSERT_EQ(member.completedEstimate_->estimate_.batches_[b].value_, old.estimate_.batches_[b].value_);
}

TEST(XccyMcBasketTest, TestAadSingletonRetainsAllLegacyBatchCoefficientsAndGradients) {
    const auto spec = NonflatSpec();
    const auto model = Data(spec);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const auto context = Context(spec, Empty());
    auto settings = Settings(4, 3);
    settings.aad_ = true;
    settings.parameterLabels_ = {"fxspot:EUR/USD", model->market_->basis_->nativeLabels_.back()};
    const auto old = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
    const auto value = PreparedXccyMcBasket_({{"first", instrument, "Price"}}, context, model, settings).Evaluate();
    const auto& member = value.Snapshot().members_[0];
    const auto& e = member.completedEstimate_->estimate_;
    ASSERT_EQ(e.gradient_, old.estimate_.gradient_);
    ASSERT_EQ(e.gradientStandardError_, old.estimate_.gradientStandardError_);
    for (size_t b = 0; b < e.batches_.size(); ++b) {
        ASSERT_EQ(e.batches_[b].value_, old.estimate_.batches_[b].value_);
        ASSERT_EQ(e.batches_[b].derivatives_, old.estimate_.batches_[b].derivatives_);
        ASSERT_EQ(e.batches_[b].coefficients_->da_, old.estimate_.batches_[b].coefficients_->da_);
        ASSERT_EQ(member.batches_[b].firstNormals_, old.firstNormals_[b]);
    }
}
