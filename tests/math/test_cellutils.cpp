//
// Created by wegam on 2020/11/14.
//

#include <gtest/gtest.h>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/time/datetimeutils.hpp>
#include <dal/math/cellutils.hpp>

using Dal::Cell::TypeCheck_;
using Dal::Cell::CoerceToString;
using Dal::Cell::FromOptionalDouble;
using Dal::Cell::ConvertString;
using Dal::DateTime_;
using Dal::Date_;
using Dal::Cell_;
using Dal::String_;

TEST(CellUtilsTest, TestTypeCheck) {

    Cell_ cell(true);
    ASSERT_TRUE(TypeCheck_<bool>()(cell));

    cell = 2.;
    ASSERT_FALSE(TypeCheck_<bool>()(cell));
    ASSERT_TRUE(TypeCheck_<double>()(cell));
}

TEST(CellUtilsTest, TestTypeCheckCanConvertCase) {
    Cell_ cell(Dal::Date::FromExcel(44149));
    ASSERT_TRUE(TypeCheck_<Date_>()(cell));
    ASSERT_FALSE(TypeCheck_<DateTime_>()(cell));

    cell = Dal::DateTime_(Dal::Cell::ToDate(cell), 0.5);
    ASSERT_FALSE(TypeCheck_<Date_>()(cell));
    ASSERT_TRUE(TypeCheck_<DateTime_>()(cell));
}

TEST(CellUtilsTest, TestTypeCheckChain) {
    Cell_ cell(1.0);
    auto res = TypeCheck_<double, bool>()(cell);
    ASSERT_TRUE(res);
}

TEST(CellUtilsTest, TestCoerceToString) {
    Cell_ cell("Hi");
    ASSERT_EQ(CoerceToString(cell), "hi");

    cell = 2.;
    ASSERT_EQ(CoerceToString(cell), "2");

    cell = 2.499;
    ASSERT_EQ(CoerceToString(cell), "2.499000");

    cell = Date_(2020, 11 ,14);
    ASSERT_EQ(CoerceToString(cell), "2020-11-14");

    cell = DateTime_(Dal::Cell::ToDate(cell), 0.5);
    ASSERT_EQ(CoerceToString(cell), "2020-11-14 12:00:00");
}

TEST(CellUtilsTest, TestFromOptionalDouble) {
    std::optional<double> s;
    auto cell = FromOptionalDouble(s);
    ASSERT_TRUE(Dal::Cell::IsEmpty(cell));

    s = 2.0;
    cell = FromOptionalDouble(s);
    ASSERT_TRUE(Dal::Cell::IsDouble(cell));
    ASSERT_DOUBLE_EQ(Dal::Cell::ToDouble(cell), 2.0);
}

TEST(CellUtilsTest, TestConvertString) {
    String_ s("sos");
    auto cell = ConvertString(s);
    ASSERT_TRUE(Dal::Cell::IsString(cell));
    ASSERT_EQ(Dal::Cell::ToString(cell), "sos");

    s = "2.0";
    cell = ConvertString(s);
    ASSERT_TRUE(Dal::Cell::IsDouble(cell));
    ASSERT_DOUBLE_EQ(Dal::Cell::ToDouble(cell), 2.0);

    s = "2020-11-14";
    cell = ConvertString(s);
    ASSERT_TRUE(Dal::Cell::IsDate(cell));
    ASSERT_EQ(Dal::Cell::ToDate(cell), Dal::Date::FromString(s));

    s = "2020-11-14 12:00:00";
    cell = ConvertString(s);
    ASSERT_TRUE(Dal::Cell::IsDateTime(cell));
    ASSERT_EQ(Dal::Cell::ToDateTime(cell), Dal::DateTime::FromString(s));
}