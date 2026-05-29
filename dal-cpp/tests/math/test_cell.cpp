//
// Created by wegamekinglc on 2020/5/2.
//

#include <gtest/gtest.h>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>
#include <dal/math/cell.hpp>

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