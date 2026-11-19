//
// Created by dal-tester on 2026/8/15.
//

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string_view>

#include <dal-public/src/random.hpp>
#include <dal/math/buffercapacity.hpp>

namespace {
    constexpr auto SOBOL_MAX = std::numeric_limits<uint32_t>::max();
    constexpr auto SOBOL_LAST_DRAW = "RandomSequenceExhausted: current=4294967294; maximum=4294967295";
    constexpr auto SOBOL_EXHAUSTED = "RandomSequenceExhausted: current=4294967295; maximum=4294967295";

    template <class F_> void AssertRandomError(F_ call, std::string_view message) {
        try {
            call();
            FAIL() << "Expected Dal::Exception_ containing " << message;
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string_view(error.what()).find(message), std::string_view::npos) << error.what();
        }
    }
} // namespace

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
    AssertRandomError([&] { Dal::FillRSG_(wide, -1, &WideGenerator_::FillUniform, &output); }, "Random path count must be nonnegative");
    AssertRandomError([&] { Dal::FillRSG_(wide, 0, &WideGenerator_::FillUniform, &output); }, "InvalidRandomInput: field=/n_dim");
    ASSERT_EQ(output.Rows(), 1);
    ASSERT_EQ(output.Cols(), 1);
    ASSERT_EQ(output(0, 0), 7.);
}

using Dal::Matrix_;
using Dal::String_;

namespace {
    void AssertSobolBatchExhaustionAllowsRetry(decltype(&Dal::GetSobolRSGUniform) get) {
        const Dal::Handle_<Dal::SobolRSG_> generator(new Dal::SobolRSG_("retry", SOBOL_MAX - 1, 3, true, true));
        const Dal::Handle_<Dal::SobolRSG_> direct(new Dal::SobolRSG_("direct", SOBOL_MAX - 1, 3, true, true));
        Matrix_<> output(1, 1, 123.);
        AssertRandomError([&] { get(generator, 2, &output); }, SOBOL_LAST_DRAW);
        EXPECT_EQ(output.Rows(), 1);
        EXPECT_EQ(output.Cols(), 1);
        if (output.Rows() == 1 && output.Cols() == 1)
            EXPECT_EQ(output(0, 0), 123.);

        get(generator, 0, &output);
        ASSERT_EQ(output.Rows(), 0);
        ASSERT_EQ(output.Cols(), 3);
        Matrix_<> expected;
        get(direct, 1, &expected);
        ASSERT_NO_THROW(get(generator, 1, &output));
        ASSERT_EQ(output.Rows(), 1);
        ASSERT_EQ(output.Cols(), 3);
        for (int col = 0; col < output.Cols(); ++col)
            ASSERT_EQ(output(0, col), expected(0, col));

        AssertRandomError([&] { get(generator, 1, &output); }, SOBOL_EXHAUSTED);
        ASSERT_EQ(output.Rows(), 1);
        ASSERT_EQ(output.Cols(), 3);
        for (int col = 0; col < output.Cols(); ++col)
            ASSERT_EQ(output(0, col), expected(0, col));
        get(generator, 0, &output);
        ASSERT_EQ(output.Rows(), 0);
        ASSERT_EQ(output.Cols(), 3);
    }
} // namespace

TEST(RandomTest, TestSobolUniformBatchExhaustionAllowsRetry) { AssertSobolBatchExhaustionAllowsRetry(Dal::GetSobolRSGUniform); }

TEST(RandomTest, TestSobolNormalBatchExhaustionAllowsRetry) { AssertSobolBatchExhaustionAllowsRetry(Dal::GetSobolRSGNormal); }

TEST(RandomTest, TestSobolBatchChecksLiveCursorAcrossGetters) {
    const Dal::Handle_<Dal::SobolRSG_> generator(new Dal::SobolRSG_("mixed", SOBOL_MAX - 2, 3));
    Matrix_<> output;
    Dal::GetSobolRSGUniform(generator, 1, &output);
    AssertRandomError([&] { Dal::GetSobolRSGNormal(generator, 2, &output); }, SOBOL_LAST_DRAW);
    ASSERT_NO_THROW(Dal::GetSobolRSGNormal(generator, 1, &output));
    AssertRandomError([&] { Dal::GetSobolRSGUniform(generator, 1, &output); }, SOBOL_EXHAUSTED);
}

TEST(RandomTest, TestSobolBatchCanConsumeExactlyTheRemainingPaths) {
    for (int dimensions : {0, 3}) {
        SCOPED_TRACE(dimensions);
        const Dal::Handle_<Dal::SobolRSG_> generator(new Dal::SobolRSG_("exact count", SOBOL_MAX - 2, dimensions));
        Matrix_<> output;
        Dal::GetSobolRSGNormal(generator, 2, &output);
        ASSERT_EQ(output.Rows(), 2);
        ASSERT_EQ(output.Cols(), dimensions);
        AssertRandomError([&] { Dal::GetSobolRSGUniform(generator, 1, &output); }, SOBOL_EXHAUSTED);
        Dal::GetSobolRSGUniform(generator, 0, &output);
        ASSERT_EQ(output.Rows(), 0);
        ASSERT_EQ(output.Cols(), dimensions);
    }
}

TEST(RandomTest, TestSobolBatchValidatesArgumentsBeforeExhaustion) {
    const Dal::Handle_<Dal::SobolRSG_> exhausted(new Dal::SobolRSG_("exhausted", SOBOL_MAX, 3));
    Matrix_<> output(1, 1, 123.);
    for (const auto get : {Dal::GetSobolRSGUniform, Dal::GetSobolRSGNormal}) {
        AssertRandomError([&] { get({}, -1, nullptr); }, "Random generator handle must not be null");
        AssertRandomError([&] { get(exhausted, -1, nullptr); }, "Random generation requires an output matrix");
        AssertRandomError([&] { get(exhausted, 0, nullptr); }, "Random generation requires an output matrix");
        AssertRandomError([&] { get(exhausted, -1, &output); }, "Random path count must be nonnegative");
    }
    ASSERT_EQ(output.Rows(), 1);
    ASSERT_EQ(output.Cols(), 1);
    ASSERT_EQ(output(0, 0), 123.);
}

TEST(RandomTest, TestSobolBatchExhaustionPrecedesBufferBudget) {
    Matrix_<> output(1, 1, 123.);
    const Dal::Handle_<Dal::SobolRSG_> generator(new Dal::SobolRSG_("over capacity", SOBOL_MAX - 1, 3));
    Dal::BufferCapacityBudget_ budget(0);
    const Dal::BufferCapacityScope_ scope(&budget);
    AssertRandomError([&] { Dal::GetSobolRSGUniform(generator, 2, &output); }, SOBOL_LAST_DRAW);
    ASSERT_EQ(output.Rows(), 1);
    ASSERT_EQ(output.Cols(), 1);
    ASSERT_EQ(output(0, 0), 123.);
}

TEST(RandomTest, TestSobolZeroBatchSkipsScratchAllocation) {
    const Dal::Handle_<Dal::SobolRSG_> exhausted(new Dal::SobolRSG_("exhausted", SOBOL_MAX, 3));
    Dal::BufferCapacityBudget_ budget(0);
    const Dal::BufferCapacityScope_ scope(&budget);
    for (const auto get : {Dal::GetSobolRSGUniform, Dal::GetSobolRSGNormal}) {
        Matrix_<> output;
        ASSERT_NO_THROW(get(exhausted, 0, &output));
        ASSERT_EQ(output.Rows(), 0);
        ASSERT_EQ(output.Cols(), 3);
    }
    ASSERT_EQ(budget.PeakCapacityBytes(), 0);
}

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
