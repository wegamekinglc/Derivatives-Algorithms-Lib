//
// Created by Codex on 2026/10/1.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal-excel/src/__random_test_api.hpp>

using namespace Dal;

namespace {
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
    const auto generator = NewPseudoRSG("IRN", 1024, 3);
    for (const auto get : {PseudoRSG_Get_Uniform, PseudoRSG_Get_Normal})
        AssertRandomGetterInputs(generator, get);
}

TEST(ExcelRandomTest, TestSobolGettersValidateWorksheetCountsBeforeConversion) {
    const auto generator = NewSobolRSG("Sobol", 0, 3);
    for (const auto get : {SobolRSG_Get_Uniform, SobolRSG_Get_Normal})
        AssertRandomGetterInputs(generator, get);
}
