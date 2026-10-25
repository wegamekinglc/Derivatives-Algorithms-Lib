#include <gtest/gtest.h>

#include "xccymcbasketfixture.hpp"

using namespace Dal;
using namespace Dal::TestXccyBasket;
using namespace Dal::XccyMcBasketInternal;

namespace {
    double Covariance(const Vector_<Vector_<>>& rows, size_t i, size_t j) {
        long double x = 0., y = 0., total = 0.;
        for (const auto& row : rows) {
            x += row[i] / static_cast<long double>(rows.size());
            y += row[j] / static_cast<long double>(rows.size());
        }
        for (const auto& row : rows)
            total += (row[i] - x) * (row[j] - y);
        return static_cast<double>(total / (rows.size() - 1));
    }
    double Project(const Matrix_<>& c, const Matrix_<>& j, int output) {
        long double result = 0.;
        for (int r = 0; r < c.Rows(); ++r)
            for (int s = 0; s < c.Cols(); ++s)
                result += static_cast<long double>(j(output, r)) * c(r, s) * j(output, s);
        return static_cast<double>(result);
    }
    bool NonzeroCross(int i, int j, double c) { return i < 8 && j >= 8 && c != 0.; }
} // namespace

TEST(XccyMcBasketStatisticsTest, TestEveryFullCovarianceEntryAndBatchNormalization) {
    const auto spec = NonflatSpec();
    const auto result = PreparedXccyMcBasket_(Members(), Context(spec, Empty()), Data(spec), AadSettings()).Evaluate();
    const auto& r = result.Snapshot();
    ASSERT_EQ(r.coefficientRows_.size(), 4U);
    ASSERT_EQ(r.coefficientAxes_.size(), 24U);
    bool nonzeroCross = false;
    for (int i = 0; i < 24; ++i)
        for (int j = 0; j < 24; ++j) {
            const double c = Covariance(r.coefficientRows_, i, j);
            ASSERT_NEAR(r.coefficientCovariance_(i, j), c, 1e-9);
            ASSERT_EQ(r.meanCovariance_(i, j), r.coefficientCovariance_(i, j) / 4.);
            if (NonzeroCross(i, j, c))
                nonzeroCross = true;
        }
    ASSERT_TRUE(nonzeroCross);
    for (size_t m = 0; m < 3; ++m)
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 8; ++j)
                ASSERT_EQ(r.coefficientCovariance_(8 * static_cast<int>(m) + i, 8 * static_cast<int>(m) + j),
                          r.members_[m].completedEstimate_->coefficientCovariance_(i, j));
}

TEST(XccyMcBasketStatisticsTest, TestPriceAndFullParGradientJacobianUseSameRows) {
    const auto spec = NonflatSpec();
    for (const String_ measure : {"Price", "BasisSpread"}) {
        const auto result = PreparedXccyMcBasket_(Members(measure), Context(spec, Empty()), Data(spec), AadSettings()).Evaluate();
        const auto& r = result.Snapshot();
        ASSERT_EQ(r.outputAxes_.size(), 12U);
        for (size_t m = 0; m < 3; ++m) {
            const auto& e = r.members_[m].completedEstimate_->estimate_;
            for (int j = 0; j < 4; ++j) {
                const int row = 4 * static_cast<int>(m) + j;
                const double variance = Project(r.meanCovariance_, r.outputJacobian_, row);
                ASSERT_NEAR(r.outputCovariance_(row, row), variance, 1e-9);
                const double se = j == 0 ? e.standardError_ : e.gradientStandardError_[j - 1];
                ASSERT_NEAR(se * se, variance, 1e-9);
            }
        }
        ASSERT_NE(r.outputCovariance_(0, 4), 0.);
    }
}

TEST(XccyMcBasketStatisticsTest, TestDuplicateEconomicRowsUseDistinctBlocksAndByteExactKeys) {
    const auto spec = ModelSpec();
    auto members = Members();
    members.Resize(2);
    members[1] = members[0];
    members[1].memberId_ = "Member-0";
    const PreparedXccyMcBasket_ plan(members, Context(spec, Empty()), Data(spec), Settings(2, 2));
    const auto value = plan.Evaluate();
    ASSERT_EQ(plan.Allocation().members_[0].firstCoordinate_, 0U);
    ASSERT_EQ(plan.Allocation().members_[1].firstCoordinate_, 70778880U);
    ASSERT_NE(value.Snapshot().members_[0].batches_[0].firstNormals_, value.Snapshot().members_[1].batches_[0].firstNormals_);
    auto reversedMembers = members;
    std::reverse(reversedMembers.begin(), reversedMembers.end());
    const auto reversed = PreparedXccyMcBasket_(reversedMembers, Context(spec, Empty()), Data(spec), Settings(2, 2)).Evaluate();
    ASSERT_THROW(RequireXccyMcBasketIndependent(value, reversed, true), CalibrationError_);
}

TEST(XccyMcBasketStatisticsTest, TestExpiredDeterministicPriceAndAllDeterministicIndependenceRefusal) {
    const auto spec = ModelSpec(Date_(2029, 1, 1));
    const auto model = Data(spec);
    const auto context = Context(spec, Empty());
    const auto value = PreparedXccyMcBasket_(Members(), context, model, Settings(2, 2)).Evaluate();
    ASSERT_EQ(value.Snapshot().consumedCoordinates_, 0U);
    ASSERT_EQ(value.Snapshot().allocation_.coordinateEnd_, 0U);
    for (const auto& member : value.Snapshot().members_) {
        ASSERT_EQ(member.completedEstimate_->estimate_.mean_, 0.);
        ASSERT_EQ(member.completedEstimate_->estimate_.standardError_, 0.);
        ASSERT_EQ(member.completedPaths_, 4U);
    }
    ASSERT_THROW(RequireXccyMcBasketIndependent(value, value), CalibrationError_);
    ASSERT_NO_THROW(RequireXccyMcBasketIndependent(value, value, true));
    ASSERT_THROW(static_cast<void>(PreparedXccyMcBasket_(Members("BasisSpread"), context, model, Settings(2, 2)).Evaluate()), XccyMcBasketError_);
}

TEST(XccyMcBasketStatisticsTest, TestMixedExpiredAndStochasticMembersKeepEmptyBlocks) {
    const auto spec = ModelSpec();
    const CrossCurrencySwap_ expired(Date_(2024, 10, 21), Date_(2025, 5, 5), Date_(2025, 11, 5), .001,
                                     TestXccy::Config(XccyNotionalMode_::Value_::FIXED));
    auto members = Members();
    members[0].instrument_ = NewXccyScriptInstrument(expired, {"expired", "USD.OIS", true, 1e-12});
    auto settings = Settings(2, 2);
    settings.aad_ = true;
    settings.parameterLabels_ = {"fxspot:EUR/USD"};
    const auto value = PreparedXccyMcBasket_(members, Context(spec, Empty()), Data(spec), settings).Evaluate();
    const auto& r = value.Snapshot();
    ASSERT_TRUE(r.allocation_.members_[0].deterministic_);
    ASSERT_EQ(r.allocation_.members_[0].coordinateEnd_, 0U);
    ASSERT_EQ(r.members_[0].consumedCoordinates_, 0U);
    ASSERT_EQ(r.allocation_.members_[1].firstCoordinate_, 0U);
    ASSERT_GT(r.members_[1].consumedCoordinates_, 0U);
    ASSERT_EQ(r.members_[0].completedEstimate_->estimate_.mean_, 0.);
    ASSERT_EQ(r.members_[0].completedEstimate_->estimate_.gradient_[0], 0.);
}

TEST(XccyMcBasketStatisticsTest, TestZeroDynamicsMatchesSameSourceCoefficientsAndPar) {
    const auto spec = ModelSpec(TestXccy::TODAY, true);
    const auto model = Data(spec);
    const auto context = Context(spec, Empty());
    for (const String_ measure : {"Price", "BasisSpread"}) {
        const auto members = Members(measure);
        const auto value = PreparedXccyMcBasket_(members, context, model, Settings(2, 2)).Evaluate();
        for (size_t i = 0; i < members.size(); ++i) {
            const auto prepared = PrepareMoment(members[i].instrument_, context, measure);
            const auto fast = EvaluateXccyGaussianMoments<double>(prepared, *model);
            const auto& e = *value.Snapshot().members_[i].completedEstimate_;
            ASSERT_NEAR(e.coefficients_.b_, fast.b_, 1e-10);
            ASSERT_NEAR(e.coefficients_.a_, fast.a_, 1e-10);
            ASSERT_NEAR(e.estimate_.mean_, fast.Value(prepared), measure == "Price" ? 1e-10 : 1e-12);
            ASSERT_EQ(e.estimate_.standardError_, 0.);
        }
    }
}

TEST(XccyMcBasketStatisticsTest, TestAllNativeAxesArePresentWithSingularFullCovariance) {
    const auto spec = NonflatSpec();
    const auto model = Data(spec);
    const auto context = Context(spec, Empty());
    auto settings = Settings(2, 2);
    settings.aad_ = true;
    static_cast<void>(XccyGaussianMomentNativeMap(*model, &settings.parameterLabels_));
    const auto value = PreparedXccyMcBasket_(Members(), context, model, settings).Evaluate();
    const auto& r = value.Snapshot();
    const size_t q = settings.parameterLabels_.size();
    ASSERT_EQ(r.coefficientAxes_.size(), 3 * (2 + 2 * q));
    ASSERT_EQ(r.header_.parameterUnits_.size(), q);
    ASSERT_EQ(r.outputCovariance_.Rows(), 3 * (1 + q));
    const auto single = PreparedXccyMc_(Members()[0].instrument_, "Price", context, model, settings).Evaluate();
    ASSERT_EQ(r.members_[0].completedEstimate_->estimate_.gradient_, single.estimate_.gradient_);
    for (const auto& member : r.members_) {
        ASSERT_EQ(member.completedEstimate_->estimate_.parameterLabels_, settings.parameterLabels_);
        ASSERT_EQ(member.completedEstimate_->coefficients_.db_.size(), q);
        ASSERT_EQ(member.completedEstimate_->coefficients_.da_.size(), q);
        ASSERT_EQ(member.batches_[0].row_.size(), 2 + 2 * q);
    }
}
