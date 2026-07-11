//
// Created by dal-implementer on 2026/7/12.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal/math/aad/aad.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/interp/interpweights.hpp>

using namespace Dal;

TEST(InterpWeightsTest, TestLinearWeightsReproduceLegacyValues) {
    const Vector_<> x{0.0, 1.0, 3.0};
    const Vector_<> y{2.0, 4.0, 10.0};
    const Interp::LinearWeightGeometry_ geometry(x);

    ASSERT_DOUBLE_EQ(Interp::ApplyInterpWeights(y, geometry.At(-1.0)), 2.0);
    ASSERT_DOUBLE_EQ(Interp::ApplyInterpWeights(y, geometry.At(0.0)), 2.0);
    ASSERT_DOUBLE_EQ(Interp::ApplyInterpWeights(y, geometry.At(2.0)), 7.0);
    ASSERT_DOUBLE_EQ(Interp::ApplyInterpWeights(y, geometry.At(4.0)), 10.0);
}

TEST(InterpWeightsTest, TestLinearWeightsPropagateAadOrdinateDerivatives) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);

    Vector_<AAD::Number_> y(3);
    for (int i = 0; i < 3; ++i)
        AAD::RegisterIndependent(y[i], 2.0 + static_cast<double>(i));
    AAD::NewRecording(*tape);

    const Interp::LinearWeightGeometry_ geometry(Vector_<>{0.0, 1.0, 3.0});
    AAD::Number_ result = Interp::ApplyInterpWeights(y, geometry.At(2.0));
    AAD::Adjoint(result) = 1.0;
    AAD::PropagateToStart(*tape);

    ASSERT_DOUBLE_EQ(AAD::AdjointValue(y[0]), 0.0);
    ASSERT_DOUBLE_EQ(AAD::AdjointValue(y[1]), 0.5);
    ASSERT_DOUBLE_EQ(AAD::AdjointValue(y[2]), 0.5);
    AAD::Clear(*tape);
}

TEST(InterpWeightsTest, TestNaturalCubicWeightsReproduceLegacyValues) {
    const Vector_<> x{0.0, 0.5, 1.75, 3.0, 5.0};
    const Vector_<> y{0.0, -0.1, -0.6, -1.2, -2.0};
    const Interp::Boundary_ natural(2, 0.0);
    const Handle_<Interp1_> legacy(Interp::NewCubic("legacy", x, y, natural, natural));
    const Interp::NaturalCubicWeightGeometry_ geometry(x);
    const Vector_<> queries{-0.25, 0.0, 0.2, 0.5, 1.0, 1.75, 2.4, 3.0, 4.0, 5.0, 5.25};

    for (const double query : queries)
        ASSERT_DOUBLE_EQ(Interp::ApplyInterpWeights(y, geometry.At(query)), (*legacy)(query)) << "query=" << query;
}

TEST(InterpWeightsTest, TestNaturalCubicWeightsPartitionUnityAndPropagateAad) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);

    const Vector_<> x{0.0, 0.5, 1.75, 3.0, 5.0};
    const Interp::NaturalCubicWeightGeometry_ geometry(x);
    const auto weights = geometry.At(2.4);
    double weightSum = 0.0;
    int nonzeroWeights = 0;
    for (const auto& [index, weight] : weights) {
        static_cast<void>(index);
        weightSum += weight;
        nonzeroWeights += std::abs(weight) > 1.0e-14 ? 1 : 0;
    }
    ASSERT_NEAR(weightSum, 1.0, 1.0e-14);
    ASSERT_GT(nonzeroWeights, 2);

    Vector_<AAD::Number_> y(x.size());
    for (int i = 0; i < static_cast<int>(y.size()); ++i)
        AAD::RegisterIndependent(y[i], -0.2 * static_cast<double>(i));
    AAD::NewRecording(*tape);
    AAD::Number_ result = Interp::ApplyInterpWeights(y, weights);
    AAD::Adjoint(result) = 1.0;
    AAD::PropagateToStart(*tape);
    for (const auto& [index, weight] : weights)
        ASSERT_NEAR(AAD::AdjointValue(y[index]), weight, 1.0e-14);

    AAD::Clear(*tape);
}

TEST(InterpWeightsTest, TestGeometryRejectsInvalidAbscissae) {
    ASSERT_THROW(Interp::LinearWeightGeometry_(Vector_<>{}), Exception_);
    ASSERT_THROW(Interp::LinearWeightGeometry_(Vector_<>{0.0, 0.0}), Exception_);
    ASSERT_THROW(Interp::NaturalCubicWeightGeometry_(Vector_<>{0.0, 1.0}), Exception_);
    ASSERT_THROW(Interp::NaturalCubicWeightGeometry_(Vector_<>{0.0, 1.0, 1.0}), Exception_);
}
