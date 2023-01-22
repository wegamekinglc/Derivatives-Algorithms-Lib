//
// Created by wegam on 2023/1/22.
//

#include <gtest/gtest.h>
#include <dal/storage/box.hpp>
#include <dal/storage/json.hpp>
#include <dal/time/datetime.hpp>

using namespace Dal;

TEST(BoxTest, TestBox) {
    Matrix_<Cell_> mat(2, 3);
    mat(0, 0) = 1.0;
    mat(0, 1) = String_("hello");
    mat(0, 2) = 1;
    mat(1, 0) = Date_(2023, 1, 22);
    mat(1, 1) = true;
    mat(1, 2) = DateTime_(Date_(2023, 1, 22), 0.5);

    Box_ box("mybox", mat);
    auto dst = JSON::WriteString(box);
    Handle_<Storable_> rtn = JSON::ReadString(dst, true);
    Handle_<Box_> val(std::dynamic_pointer_cast<const Box_>(rtn));
    Matrix_<Cell_> contents = val->contents_;

    ASSERT_EQ(contents.Rows(), 2);
    ASSERT_EQ(contents.Cols(), 3);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(contents(0, 0)), Cell::ToDouble(mat(0, 0)));
    ASSERT_EQ(Cell::ToString(contents(0, 1)), Cell::ToString(mat(0, 1)));
    ASSERT_EQ(Cell::ToInt(contents(0, 2)), Cell::ToInt(mat(0, 2)));
    ASSERT_EQ(Cell::ToDate(contents(1, 0)), Cell::ToDate(mat(1, 0)));
    ASSERT_EQ(Cell::ToBool(contents(1, 1)), Cell::ToBool(mat(1, 1)));
    ASSERT_EQ(Cell::ToDateTime(contents(1, 2)), Cell::ToDateTime(mat(1, 2)));
}