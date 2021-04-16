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
using Dal::Cell::Type_;
using Dal::Cell::IsEmpty;

TEST(CellTest, TestCellEmpty) {
    auto cell = Cell_();
    ASSERT_EQ(cell.type_, Type_::EMPTY);
}

TEST(CellTest, TestCellBool) {
    auto cell = Cell_(true);
    ASSERT_TRUE(cell.b_);
    ASSERT_EQ(cell.type_, Type_::BOOLEAN);
}

TEST(CellTest, TestCellDouble) {
    auto cell = Cell_(2.5);
    ASSERT_DOUBLE_EQ(cell.d_, 2.5);
    ASSERT_EQ(cell.type_, Type_::NUMBER);
}

TEST(CellTest, TestCellString) {
    auto cell = Cell_(String_("hello"));
    ASSERT_EQ(cell.type_, Type_::STRING);
    ASSERT_EQ(cell.s_, "hello");
}

TEST(CellTest, TestCellDate) {
    auto cell = Cell_(Date_(2020, 5, 3));
    ASSERT_EQ(cell.type_, Type_::DATE);
    ASSERT_EQ(cell.dt_.Date(), Date_(2020, 5, 3));
}

TEST(CellTest, TestCellDateTime) {
    Date_ dt(2020, 5, 3);
    auto cell = Cell_(DateTime_(dt, 12, 0, 0));
    ASSERT_EQ(cell.type_, Type_::DATETIME);
    ASSERT_EQ(cell.dt_.Date(), Date_(2020, 5, 3));
    ASSERT_DOUBLE_EQ(cell.dt_.Frac(), 0.5);
}

TEST(CellTest, TestCellCharArray) {
    auto cell = Cell_("hello");
    ASSERT_EQ(cell.type_, Type_::STRING);
    ASSERT_EQ(cell.s_, "hello");
}

TEST(CellTest, TestCellClear) {
    auto cell = Cell_(true);
    cell.Clear();
    ASSERT_EQ(cell.type_, Type_::EMPTY);
}

TEST(CellTest, TestAssignBool) {
    auto cell = Cell_();
    cell = true;
    ASSERT_TRUE(cell.b_);
    ASSERT_EQ(cell.type_, Type_::BOOLEAN);
}

TEST(CellTest, TestAssignInt) {
    auto cell = Cell_();
    cell = 2;
    ASSERT_DOUBLE_EQ(cell.d_, 2.);
    ASSERT_EQ(cell.type_, Type_::NUMBER);
}

TEST(CellTest, TestAssignDouble) {
    auto cell = Cell_();
    cell = 2.5;
    ASSERT_DOUBLE_EQ(cell.d_, 2.5);
    ASSERT_EQ(cell.type_, Type_::NUMBER);
}

TEST(CellTest, TestAssignDate) {
    auto cell = Cell_();
    cell = Date_(2020, 5, 3);
    ASSERT_EQ(cell.dt_.Date(), Date_(2020, 5, 3));
    ASSERT_EQ(cell.type_, Type_::DATE);
}

TEST(CellTest, TestAssignDateTime) {
    auto cell = Cell_();
    Date_ dt(2020, 5, 3);
    cell = DateTime_(dt, 12, 0, 0);
    ASSERT_EQ(cell.dt_.Date(), Date_(2020, 5, 3));
    ASSERT_DOUBLE_EQ(cell.dt_.Frac(), 0.5);
    ASSERT_EQ(cell.type_, Type_::DATETIME);
}

TEST(CellTest, TestAssignString) {
    auto cell = Cell_();
    cell = String_("hello");
    ASSERT_EQ(cell.type_, Type_::STRING);
    ASSERT_EQ(cell.s_, "hello");
}

TEST(CellTest, TestAssignCharArray) {
    auto cell = Cell_();
    cell = "hello";
    ASSERT_EQ(cell.type_, Type_::STRING);
    ASSERT_EQ(cell.s_, "hello");
}

TEST(CellTest, TestAssignCell) {
    auto cell = Cell_();
    Cell_ rhs(true);

    cell = rhs;
    ASSERT_TRUE(cell.b_);
    ASSERT_EQ(cell.type_, Type_::BOOLEAN);

    rhs = Cell_(2.5);
    cell = rhs;
    ASSERT_DOUBLE_EQ(cell.d_, 2.5);
    ASSERT_EQ(cell.type_, Type_::NUMBER);

    rhs = Cell_("hello");
    cell = rhs;
    ASSERT_EQ(cell.s_, String_("hello"));
    ASSERT_EQ(cell.type_, Type_::STRING);

    rhs = Cell_(Date_(2020, 5, 3));
    cell = rhs;
    ASSERT_EQ(cell.dt_.Date(), Date_(2020, 5, 3));
    ASSERT_EQ(cell.type_, Type_::DATE);

    rhs = Cell_(DateTime_(Date_(2020, 5, 3), 0.25));
    cell = rhs;
    ASSERT_EQ(cell.dt_, DateTime_(Date_(2020, 5, 3), 6, 0, 0));
    ASSERT_EQ(cell.type_, Type_::DATETIME);

    rhs = Cell_();
    cell = rhs;
    ASSERT_EQ(cell.type_, Type_::EMPTY);
}

TEST(CellTest, TestIsEmpty) {
    Cell_ cell;
    ASSERT_TRUE(IsEmpty(cell));

    cell = Cell_("");
    ASSERT_TRUE(IsEmpty(cell));

    cell = Cell_("not empty");
    ASSERT_FALSE(IsEmpty(cell));
}