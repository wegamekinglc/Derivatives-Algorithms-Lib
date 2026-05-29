//
// Created by wegam on 2024/1/19.
//

#include <gtest/gtest.h>
#include <dal/indice/indexpath.hpp>

using namespace Dal;

TEST(IndexTest, TestIndexPathHistorical) {
    IndexPathHistorical_ index_path;
    std::map<DateTime_, double> fixings;
    DateTime_ dt(Date_(2024, 1, 18), 0.5);
    fixings[dt] = 1.0;
    index_path.fixings_ = fixings;

    ASSERT_NEAR(index_path.FixInRangeProb(dt, std::make_pair<double, double>(0.0, 2.0), 0.0), 1.0, 1e-4);
    ASSERT_NEAR(index_path.FixInRangeProb(dt, std::make_pair<double, double>(3.0, 4.0), 0.0), 0.0, 1e-4);
}
