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
using Dal::Cell::Type_;
using Dal::Cell::CoerceToString;
using Dal::Cell::FromOptionalDouble;
using Dal::Cell::ConvertString;
using Dal::DateTime_;
using Dal::Date_;
using Dal::Cell_;
using Dal::String_;

TEST(CellUtilsTest, TestTypeCheck) {
    TypeCheck_ checker;
    checker = checker.Add(Type_::BOOLEAN);

    Cell_ cell(true);
    ASSERT_TRUE(checker(cell));

    cell = 2.;
    ASSERT_FALSE(checker(cell));

    checker = checker.Add(Type_::NUMBER);
    ASSERT_TRUE(checker(cell));
}

TEST(CellUtilsTest, TestTypeCheckCanConvertCase) {
    TypeCheck_ checker;
    checker = checker.Add(Type_::DATE);

    Cell_ cell(44149.);
    ASSERT_TRUE(checker(cell));


    cell = 44149.5;
    ASSERT_FALSE(checker(cell));
    checker = checker.Add(Type_::DATETIME);
    ASSERT_TRUE(checker(cell));
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

    cell = DateTime_(cell.dt_.Date(), 0.5);
    ASSERT_EQ(CoerceToString(cell), "2020-11-14 12:00:00");
}

TEST(CellUtilsTest, TestFromOptionalDouble) {
    boost::optional<double> s;
    auto cell = FromOptionalDouble(s);
    ASSERT_EQ(cell.type_, Type_::EMPTY);

    s = 2.0;
    cell = FromOptionalDouble(s);
    ASSERT_EQ(cell.type_,Type_::NUMBER);
    ASSERT_DOUBLE_EQ(cell.d_, 2.0);
}

TEST(CellUtilsTest, TestConvertString) {
    String_ s("sos");
    auto cell = ConvertString(s);
    ASSERT_EQ(cell.type_, Type_::STRING);
    ASSERT_EQ(cell.s_, "sos");

    s = "2.0";
    cell = ConvertString(s);
    ASSERT_EQ(cell.type_, Type_::NUMBER);
    ASSERT_DOUBLE_EQ(cell.d_, 2.0);

    s = "2020-11-14";
    cell = ConvertString(s);
    ASSERT_EQ(cell.type_, Type_::DATE);
    ASSERT_EQ(cell.dt_.Date(), Dal::Date::FromString(s));

    s = "2020-11-14 12:00:00";
    cell = ConvertString(s);
    ASSERT_EQ(cell.type_, Type_::DATETIME);
    ASSERT_EQ(cell.dt_, Dal::DateTime::FromString(s));
}