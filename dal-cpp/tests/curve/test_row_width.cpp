//
// Created by dal-implementer on 2026/6/28.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/time/date.hpp>
#include <dal/math/vectors.hpp>

using namespace Dal;

// The lower-triangular Jacobian optimization maps each instrument's maturity to the count of free
// knots at or before it: that count is the row width (the number of leading columns that can be
// nonzero for that row). Columns at or beyond the row width are guaranteed structural zero and are
// never harvested. These tests pin the maturity -> row-width mapping for the LOG_DISCOUNT knot
// layout (anchor at knot 0, free knots at indices 1..nKnots-1 mapping to solver columns 0..nCols-1).

TEST(RowWidthTest, TestMaturityAtFreeKnotReturnsKnotIndex) {
    const Vector_<Date_> knots = {
        Date_(2022, 1, 1), Date_(2022, 4, 1), Date_(2022, 7, 1), Date_(2023, 1, 1), Date_(2024, 1, 1), Date_(2025, 1, 1),
    };
    // 3M swap maturing exactly at knot 1 (2022-04-01): only column 0 (knot 1) is touched.
    ASSERT_EQ(RowWidthForMaturity(knots, Date_(2022, 4, 1)), 1);
    // 6M swap maturing exactly at knot 2 (2022-07-01): columns 0..1 (knots 1..2).
    ASSERT_EQ(RowWidthForMaturity(knots, Date_(2022, 7, 1)), 2);
    // 5Y swap maturing at the last knot: all 5 free columns.
    ASSERT_EQ(RowWidthForMaturity(knots, Date_(2025, 1, 1)), 5);
}

TEST(RowWidthTest, TestMaturityBetweenKnotsRoundsDownToPriorKnot) {
    const Vector_<Date_> knots = {
        Date_(2022, 1, 1), Date_(2022, 4, 1), Date_(2022, 7, 1), Date_(2023, 1, 1), Date_(2024, 1, 1), Date_(2025, 1, 1),
    };
    // Maturity 2022-05-01 falls between knot 1 (2022-04-01) and knot 2 (2022-07-01): cashflows can
    // only land on or before knot 1, so the row width is 1 (column 0 only).
    ASSERT_EQ(RowWidthForMaturity(knots, Date_(2022, 5, 1)), 1);
}

TEST(RowWidthTest, TestMaturityBeforeFirstFreeKnotIsZeroWidth) {
    const Vector_<Date_> knots = {Date_(2022, 1, 1), Date_(2022, 4, 1)};
    // Maturity at the anchor itself: no free knot is at or before it (the anchor is excluded), so
    // the row width is 0 -- the harvest loop is skipped entirely and the row stays zero-filled.
    ASSERT_EQ(RowWidthForMaturity(knots, Date_(2022, 1, 1)), 0);
}

TEST(RowWidthTest, TestMaturityAfterAnchorBeforeFirstFreeKnotIsWidthOne) {
    const Vector_<Date_> knots = {
        Date_(2022, 1, 1), Date_(2022, 4, 1), Date_(2022, 7, 1), Date_(2023, 1, 1), Date_(2024, 1, 1), Date_(2025, 1, 1),
    };
    // A cashflow strictly after the anchor but before the first free knot (e.g. a 1-business-day
    // swap maturing 2022-01-03, before the 2022-04-01 knot) still couples to the first free knot:
    // the DF is bracketed by the anchor (DF=1) and the first free knot, so short-end interpolation
    // gives nonzero sensitivity to column 0. The width is therefore 1, not 0.
    ASSERT_EQ(RowWidthForMaturity(knots, Date_(2022, 1, 3)), 1);
    // Maturity at the anchor: no coupling (the anchor DF is pinned at 1), so width 0.
    ASSERT_EQ(RowWidthForMaturity(knots, Date_(2022, 1, 1)), 0);
}

TEST(RowWidthTest, TestMaturityBeyondLastKnotClampsToColumnCount) {
    const Vector_<Date_> knots = {Date_(2022, 1, 1), Date_(2022, 4, 1), Date_(2022, 7, 1)};
    // Maturity after the last knot: every free column (0..1) is at or before it. Clamped to nCols.
    ASSERT_EQ(RowWidthForMaturity(knots, Date_(2023, 1, 1)), 2);
}
