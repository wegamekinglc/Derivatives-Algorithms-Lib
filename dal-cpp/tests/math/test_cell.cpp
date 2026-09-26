//
// Created by wegamekinglc on 2020/5/2.
//

#include <gtest/gtest.h>
#include <limits>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>
#include <dal/math/cell.hpp>
#include <dal/utilities/exceptions.hpp>

using Dal::Cell_;
using Dal::String_;
using Dal::Date_;
using Dal::DateTime_;

TEST(CellTest, TestCellEmpty) {
    auto cell = Cell_();
    ASSERT_TRUE(Dal::Cell::IsEmpty(cell));
}

TEST(CellTest, TestCellBool) {
    auto cell = Cell_(true);
    ASSERT_TRUE(Dal::Cell::IsBool(cell));
}

TEST(CellTest, TestCellDouble) {
    auto cell = Cell_(2.5);
    ASSERT_TRUE(Dal::Cell::IsDouble(cell));
}

TEST(CellTest, TestCellString) {
    auto cell = Cell_(String_("hello"));
    ASSERT_TRUE(Dal::Cell::IsString(cell));
}

TEST(CellTest, TestCellDate) {
    auto cell = Cell_(Date_(2020, 5, 3));
    ASSERT_TRUE(Dal::Cell::IsDate(cell));
}

TEST(CellTest, TestCellDateTime) {
    Date_ dt(2020, 5, 3);
    auto cell = Cell_(DateTime_(dt, 12, 0, 0));
    ASSERT_TRUE(Dal::Cell::IsDateTime(cell));
    ASSERT_DOUBLE_EQ(Dal::Cell::ToDateTime(cell).Frac(), 0.5);
}

TEST(CellTest, TestCellCharArray) {
    auto cell = Cell_("hello");
    ASSERT_TRUE(Dal::Cell::IsString(cell));
}

TEST(CellTest, TestCellClear) {
    auto cell = Cell_(true);
    cell.Clear();
    ASSERT_TRUE(Dal::Cell::IsEmpty(cell));
}

TEST(CellTest, TestAssignBool) {
    auto cell = Cell_();
    cell = true;
    ASSERT_TRUE(Dal::Cell::IsBool(cell));
}

TEST(CellTest, TestAssignInt) {
    auto cell = Cell_();
    cell = 2;
    ASSERT_TRUE(Dal::Cell::IsDouble(cell));
}

TEST(CellTest, TestAssignDouble) {
    auto cell = Cell_();
    cell = 2.5;
    ASSERT_TRUE(Dal::Cell::IsDouble(cell));
}

TEST(CellTest, TestAssignDate) {
    auto cell = Cell_();
    cell = Date_(2020, 5, 3);
    ASSERT_TRUE(Dal::Cell::IsDate(cell));
}

TEST(CellTest, TestAssignDateTime) {
    auto cell = Cell_();
    Date_ dt(2020, 5, 3);
    cell = DateTime_(dt, 12, 0, 0);
    ASSERT_DOUBLE_EQ(Dal::Cell::ToDateTime(cell).Frac(), 0.5);
    ASSERT_TRUE(Dal::Cell::IsDateTime(cell));
}

TEST(CellTest, TestAssignString) {
    auto cell = Cell_();
    cell = String_("hello");
    ASSERT_TRUE(Dal::Cell::IsString(cell));
}

TEST(CellTest, TestAssignCharArray) {
    auto cell = Cell_();
    cell = "hello";
    ASSERT_TRUE(Dal::Cell::IsString(cell));
}

TEST(CellTest, TestAssignCell) {
    auto cell = Cell_();
    Cell_ rhs(true);

    cell = rhs;
    ASSERT_TRUE(Dal::Cell::IsBool(cell));

    rhs = Cell_(2.5);
    cell = rhs;
    ASSERT_TRUE(Dal::Cell::IsDouble(cell));

    rhs = Cell_("hello");
    cell = rhs;
    ASSERT_TRUE(Dal::Cell::IsString(cell));

    rhs = Cell_(Date_(2020, 5, 3));
    cell = rhs;
    ASSERT_TRUE(Dal::Cell::IsDate(cell));

    rhs = Cell_(DateTime_(Date_(2020, 5, 3), 0.25));
    cell = rhs;
    ASSERT_TRUE(Dal::Cell::IsDateTime(cell));

    rhs = Cell_();
    cell = rhs;
    ASSERT_TRUE(Dal::Cell::IsEmpty(cell));
}

TEST(CellTest, TestIsEmpty) {
    Cell_ cell;
    ASSERT_TRUE(Dal::Cell::IsEmpty(cell));

    cell = Cell_("");
    ASSERT_TRUE(Dal::Cell::IsEmpty(cell));

    cell = Cell_("not empty");
    ASSERT_FALSE(Dal::Cell::IsEmpty(cell));
}

TEST(CellTest, TestToStringKeepsPrecisionAndLargeValues) {
    ASSERT_EQ(Dal::Cell::ToString(Cell_(5.0)), "5");
    ASSERT_EQ(Dal::Cell::ToString(Cell_(-7.0)), "-7");
    ASSERT_EQ(Dal::Cell::ToString(Cell_(1.5e-7)), "1.5e-07");
    ASSERT_EQ(Dal::Cell::ToString(Cell_(2.25)), "2.25");
    ASSERT_EQ(Dal::Cell::ToString(Cell_(5e9)), "5e+09");
}

TEST(CellTest, TestIntegerQueriesOutsideIntRange) {
    ASSERT_TRUE(Dal::Cell::IsInt(Cell_(2147483646.0)));
    ASSERT_FALSE(Dal::Cell::IsInt(Cell_(5e9)));
    ASSERT_FALSE(Dal::Cell::IsInt(Cell_(-5e9)));
    ASSERT_FALSE(Dal::Cell::IsInt(Cell_(std::numeric_limits<double>::quiet_NaN())));
    ASSERT_FALSE(Dal::Cell::IsInt(Cell_(2.5)));
    ASSERT_EQ(Dal::Cell::ToInt(Cell_(-12.0)), -12);
    ASSERT_THROW(Dal::Cell::ToInt(Cell_(5e9)), Dal::Exception_);
    ASSERT_THROW(Dal::Cell::ToInt(Cell_(2.5)), Dal::Exception_);
}
