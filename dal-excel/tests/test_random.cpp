#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string_view>

#include <dal-excel/src/__random_test_api.hpp>

using namespace Dal;

TEST(ExcelRandomTest, TestFactoriesRejectInvalidNumbersWithoutReplacingHandles) {
    Handle_<PseudoRSG_> pseudo;
    Handle_<SobolRSG_> sobol;
    PseudoRSG_New("IRN", -1., 1., &pseudo);
    SobolRSG_New("label", 0., 1., false, false, &sobol);
    const auto originalPseudo = pseudo;
    const auto originalSobol = sobol;
    for (double value : {.5, -1., std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity(), 1e100,
                         static_cast<double>(std::numeric_limits<int>::max()) + 1.}) {
        if (value != -1.) {
            ASSERT_THROW(PseudoRSG_New("IRN", value, 1., &pseudo), Exception_);
            ASSERT_EQ(pseudo.get(), originalPseudo.get());
        }
        ASSERT_THROW(PseudoRSG_New("IRN", 1., value, &pseudo), Exception_);
        ASSERT_EQ(pseudo.get(), originalPseudo.get());
        ASSERT_THROW(SobolRSG_New("label", value, 1., false, false, &sobol), Exception_);
        ASSERT_EQ(sobol.get(), originalSobol.get());
        ASSERT_THROW(SobolRSG_New("label", 0., value, false, false, &sobol), Exception_);
        ASSERT_EQ(sobol.get(), originalSobol.get());
    }
    for (int seed : {0, -1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
        ASSERT_NO_THROW(PseudoRSG_New("MRG32k32a", seed, 0., &pseudo));
    ASSERT_NO_THROW(SobolRSG_New("label", std::numeric_limits<int>::max(), 0., true, true, &sobol));
    ASSERT_THROW(SobolRSG_New("label", 0., 21201., false, false, &sobol), Exception_);
}

namespace {
    constexpr auto SOBOL_MAX = std::numeric_limits<uint32_t>::max();

    template <class F_> void AssertRandomError(F_ call, std::string_view message) {
        try {
            call();
            FAIL() << "Expected Dal::Exception_ containing " << message;
        } catch (const Exception_& error) {
            ASSERT_NE(std::string_view(error.what()).find(message), std::string_view::npos) << error.what();
        }
    }

    template <class T_> void AssertRandomGetterInputs(const Handle_<T_>& generator, void (*get)(const Handle_<T_>&, double, Matrix_<>*)) {
        Matrix_<> output(1, 1, 7.0);
        for (const double count : {-0.5, -1.0, 1.5, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                                   -std::numeric_limits<double>::infinity(), static_cast<double>(std::numeric_limits<int>::max()) + 1.0}) {
            ASSERT_THROW(get(generator, count, &output), Exception_) << "count=" << count;
            ASSERT_EQ(output.Rows(), 1);
            ASSERT_EQ(output.Cols(), 1);
            ASSERT_DOUBLE_EQ(output(0, 0), 7.0);
        }
        get(generator, 0.0, &output);
        ASSERT_EQ(output.Rows(), 0);
        ASSERT_EQ(output.Cols(), 3);
        get(generator, 2.0, &output);
        ASSERT_EQ(output.Rows(), 2);
        ASSERT_EQ(output.Cols(), 3);
    }
} // namespace

TEST(ExcelRandomTest, TestPseudoGettersValidateWorksheetCountsBeforeConversion) {
    Handle_<PseudoRSG_> generator;
    PseudoRSG_New("IRN", 1024, 3, &generator);
    for (const auto get : {PseudoRSG_Get_Uniform, PseudoRSG_Get_Normal})
        AssertRandomGetterInputs(generator, get);
}

TEST(ExcelRandomTest, TestSobolGettersValidateWorksheetCountsBeforeConversion) {
    Handle_<SobolRSG_> generator;
    SobolRSG_New("Sobol", 0, 3, false, false, &generator);
    for (const auto get : {SobolRSG_Get_Uniform, SobolRSG_Get_Normal})
        AssertRandomGetterInputs(generator, get);
}

namespace {
    void AssertSobolBatchExhaustionAllowsRetry(decltype(&SobolRSG_Get_Uniform) get) {
        const Handle_<SobolRSG_> generator(new SobolRSG_("retry", SOBOL_MAX - 1, 3, true, true));
        const Handle_<SobolRSG_> direct(new SobolRSG_("direct", SOBOL_MAX - 1, 3, true, true));
        Matrix_<> output(1, 1, 123.);
        AssertRandomError([&] { get(generator, 2., &output); }, "RandomSequenceExhausted: current=4294967294; maximum=4294967295");
        EXPECT_EQ(output.Rows(), 1);
        EXPECT_EQ(output.Cols(), 1);
        if (output.Rows() == 1 && output.Cols() == 1)
            EXPECT_EQ(output(0, 0), 123.);

        get(generator, 0., &output);
        ASSERT_EQ(output.Rows(), 0);
        ASSERT_EQ(output.Cols(), 3);
        Matrix_<> expected;
        get(direct, 1., &expected);
        ASSERT_NO_THROW(get(generator, 1., &output));
        ASSERT_EQ(output.Rows(), 1);
        ASSERT_EQ(output.Cols(), 3);
        for (int col = 0; col < output.Cols(); ++col)
            ASSERT_EQ(output(0, col), expected(0, col));

        AssertRandomError([&] { get(generator, 1., &output); }, "RandomSequenceExhausted: current=4294967295; maximum=4294967295");
        ASSERT_EQ(output.Rows(), 1);
        ASSERT_EQ(output.Cols(), 3);
        for (int col = 0; col < output.Cols(); ++col)
            ASSERT_EQ(output(0, col), expected(0, col));
        get(generator, 0., &output);
        ASSERT_EQ(output.Rows(), 0);
        ASSERT_EQ(output.Cols(), 3);
    }
} // namespace

TEST(ExcelRandomTest, TestSobolUniformBatchExhaustionAllowsRetry) { AssertSobolBatchExhaustionAllowsRetry(SobolRSG_Get_Uniform); }

TEST(ExcelRandomTest, TestSobolNormalBatchExhaustionAllowsRetry) { AssertSobolBatchExhaustionAllowsRetry(SobolRSG_Get_Normal); }

TEST(ExcelRandomTest, TestSobolBatchValidatesWorksheetCountBeforeHandleAndExhaustion) {
    const Handle_<SobolRSG_> exhausted(new SobolRSG_("exhausted", SOBOL_MAX, 3));
    const auto aboveIntMax = static_cast<double>(std::numeric_limits<int>::max()) + 1.;
    Matrix_<> output(1, 1, 123.);
    for (const auto get : {SobolRSG_Get_Uniform, SobolRSG_Get_Normal}) {
        for (double count : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
            AssertRandomError([&] { get({}, count, nullptr); }, "number of paths must be finite");
        AssertRandomError([&] { get({}, -0.5, nullptr); }, "number of paths must be exactly integral");
        AssertRandomError([&] { get({}, -1., nullptr); }, "number of paths is outside the supported integer range");
        AssertRandomError([&] { get(exhausted, aboveIntMax, nullptr); }, "number of paths is outside the supported integer range");
        AssertRandomError([&] { get(exhausted, -1., &output); }, "number of paths is outside the supported integer range");
        AssertRandomError([&] { get({}, 0., nullptr); }, "Random generator handle must not be null");
        AssertRandomError([&] { get(exhausted, 0., nullptr); }, "Random generation requires an output matrix");
    }
    ASSERT_EQ(output.Rows(), 1);
    ASSERT_EQ(output.Cols(), 1);
    ASSERT_EQ(output(0, 0), 123.);
}
