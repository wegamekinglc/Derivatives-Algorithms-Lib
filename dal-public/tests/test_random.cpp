//
// Created by dal-tester on 2026/8/15.
//

#include <gtest/gtest.h>

#include <dal-public/src/random.hpp>

TEST(RandomTest, TestRandomZeroDimensionsAndNarrowing) {
    for (const auto& type : {"IRN", "ShuffledIRN", "MRG32", "MRG32k32a"}) {
        const auto generator = Dal::NewPseudoRSG(type, std::numeric_limits<int>::min(), 0);
        Dal::Matrix_<> output;
        Dal::GetPseudoRSGUniform(generator, 2, &output);
        ASSERT_EQ(output.Rows(), 2);
        ASSERT_EQ(output.Cols(), 0);
    }
    ASSERT_THROW(Dal::NewPseudoRSG("IRN", 0, -1), Dal::Exception_);
    ASSERT_THROW(Dal::NewSobolRSG("label", -1), Dal::Exception_);
    const auto sobol = Dal::NewSobolRSG("label", 0, 0);
    Dal::Matrix_<> output;
    Dal::GetSobolRSGNormal(sobol, 2, &output);
    ASSERT_EQ(output.Rows(), 2);
    ASSERT_EQ(output.Cols(), 0);

    struct WideGenerator_ {
        size_t NDim() const { return static_cast<size_t>(std::numeric_limits<int>::max()) + 1; }
        void FillUniform(Dal::Vector_<>*) const {}
    };
    const Dal::Handle_<WideGenerator_> wide(new WideGenerator_);
    output.Resize(1, 1);
    output(0, 0) = 7.;
    ASSERT_THROW(Dal::FillRSG_(wide, 0, &WideGenerator_::FillUniform, &output), Dal::Exception_);
    ASSERT_EQ(output.Rows(), 1);
    ASSERT_EQ(output.Cols(), 1);
    ASSERT_EQ(output(0, 0), 7.);
}

using Dal::Matrix_;
using Dal::String_;

TEST(RandomTest, TestGetRandomRejectsNullHandleAndNegativePaths) {
    Matrix_<> output;
    ASSERT_THROW(Dal::GetPseudoRSGUniform({}, 1, &output), Dal::Exception_);
    ASSERT_THROW(Dal::GetPseudoRSGNormal({}, 1, &output), Dal::Exception_);
    ASSERT_THROW(Dal::GetSobolRSGUniform({}, 1, &output), Dal::Exception_);
    ASSERT_THROW(Dal::GetSobolRSGNormal({}, 1, &output), Dal::Exception_);
    const auto pseudo = Dal::NewPseudoRSG("MRG32", 42, 3);
    const auto sobol = Dal::NewSobolRSG("sobol", 0, 3);
    ASSERT_THROW(Dal::GetPseudoRSGNormal(pseudo, -1, &output), Dal::Exception_);
    ASSERT_THROW(Dal::GetSobolRSGUniform(sobol, -1, &output), Dal::Exception_);
    ASSERT_THROW(Dal::GetPseudoRSGNormal(pseudo, 1, nullptr), Dal::Exception_);
    Dal::GetPseudoRSGNormal(pseudo, 0, &output);
    ASSERT_EQ(output.Rows(), 0);
    ASSERT_EQ(output.Cols(), 3);
}

TEST(RandomTest, TestPseudoUniformDeterministicForSameSeed) {
    const auto first = Dal::NewPseudoRSG(String_("MRG32"), 42, 3);
    const auto second = Dal::NewPseudoRSG(String_("MRG32"), 42, 3);

    Matrix_<> valuesFirst, valuesSecond;
    Dal::GetPseudoRSGUniform(first, 64, &valuesFirst);
    Dal::GetPseudoRSGUniform(second, 64, &valuesSecond);

    ASSERT_EQ(valuesFirst.Rows(), 64);
    ASSERT_EQ(valuesFirst.Cols(), 3);
    for (int i = 0; i < valuesFirst.Rows(); ++i)
        for (int j = 0; j < valuesFirst.Cols(); ++j)
            ASSERT_DOUBLE_EQ(valuesFirst(i, j), valuesSecond(i, j));
}

TEST(RandomTest, TestPseudoUniformDiffersAcrossSeeds) {
    const auto first = Dal::NewPseudoRSG(String_("MRG32"), 42, 3);
    const auto second = Dal::NewPseudoRSG(String_("MRG32"), 43, 3);

    Matrix_<> valuesFirst, valuesSecond;
    Dal::GetPseudoRSGUniform(first, 64, &valuesFirst);
    Dal::GetPseudoRSGUniform(second, 64, &valuesSecond);

    bool anyDifference = false;
    for (int i = 0; i < valuesFirst.Rows() && !anyDifference; ++i)
        for (int j = 0; j < valuesFirst.Cols() && !anyDifference; ++j)
            anyDifference = valuesFirst(i, j) != valuesSecond(i, j);
    ASSERT_TRUE(anyDifference);
}

TEST(RandomTest, TestPseudoUniformWithinUnitInterval) {
    const auto rsg = Dal::NewPseudoRSG(String_("MRG32"), 7, 4);

    Matrix_<> values;
    Dal::GetPseudoRSGUniform(rsg, 256, &values);

    ASSERT_EQ(values.Rows(), 256);
    ASSERT_EQ(values.Cols(), 4);
    for (int i = 0; i < values.Rows(); ++i)
        for (int j = 0; j < values.Cols(); ++j) {
            ASSERT_GE(values(i, j), 0.0);
            ASSERT_LT(values(i, j), 1.0);
        }
}

TEST(RandomTest, TestPseudoNormalDeterministicForSameSeed) {
    const auto first = Dal::NewPseudoRSG(String_("MRG32"), 123, 2);
    const auto second = Dal::NewPseudoRSG(String_("MRG32"), 123, 2);

    Matrix_<> valuesFirst, valuesSecond;
    Dal::GetPseudoRSGNormal(first, 64, &valuesFirst);
    Dal::GetPseudoRSGNormal(second, 64, &valuesSecond);

    ASSERT_EQ(valuesFirst.Rows(), 64);
    ASSERT_EQ(valuesFirst.Cols(), 2);
    for (int i = 0; i < valuesFirst.Rows(); ++i)
        for (int j = 0; j < valuesFirst.Cols(); ++j)
            ASSERT_DOUBLE_EQ(valuesFirst(i, j), valuesSecond(i, j));
}

TEST(RandomTest, TestSobolUniformDeterministicForSameStartPath) {
    const auto first = Dal::NewSobolRSG(String_("dal_public_sobol_u1"), 0, 3);
    const auto second = Dal::NewSobolRSG(String_("dal_public_sobol_u2"), 0, 3);

    Matrix_<> valuesFirst, valuesSecond;
    Dal::GetSobolRSGUniform(first, 32, &valuesFirst);
    Dal::GetSobolRSGUniform(second, 32, &valuesSecond);

    ASSERT_EQ(valuesFirst.Rows(), 32);
    ASSERT_EQ(valuesFirst.Cols(), 3);
    for (int i = 0; i < valuesFirst.Rows(); ++i)
        for (int j = 0; j < valuesFirst.Cols(); ++j)
            ASSERT_DOUBLE_EQ(valuesFirst(i, j), valuesSecond(i, j));
}

TEST(RandomTest, TestSobolNormalDiffersAcrossStartPaths) {
    const auto first = Dal::NewSobolRSG(String_("dal_public_sobol_n1"), 0, 2);
    const auto second = Dal::NewSobolRSG(String_("dal_public_sobol_n2"), 1024, 2);

    Matrix_<> valuesFirst, valuesSecond;
    Dal::GetSobolRSGNormal(first, 32, &valuesFirst);
    Dal::GetSobolRSGNormal(second, 32, &valuesSecond);

    bool anyDifference = false;
    for (int i = 0; i < valuesFirst.Rows() && !anyDifference; ++i)
        for (int j = 0; j < valuesFirst.Cols() && !anyDifference; ++j)
            anyDifference = valuesFirst(i, j) != valuesSecond(i, j);
    ASSERT_TRUE(anyDifference);
}
