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