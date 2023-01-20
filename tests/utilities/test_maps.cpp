//
// Created by wegam on 2023/1/21.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/utilities/maps.hpp>

using namespace Dal;

TEST(MapsTest, TestZipToMap) {
    Vector_<String_> c1 = {"a", "b", "c"};
    Vector_<> c2 = {1.0, 2.0, 3.0};

    std::map<String_, double> map = ZipToMap(c1, c2);
    for(int i = 0; i < c1.size(); ++i)
        ASSERT_DOUBLE_EQ(map[c1[i]], c2[i]);
}

TEST(MapsTest, TestZipToMultimap) {
    Vector_<String_> c1 = {"a", "b", "b", "c"};
    Vector_<> c2 = {1.0, 2.0, 3.0, 4.0};

    std::multimap<String_, double> map = ZipToMultimap(c1, c2);
    auto range = map.equal_range("b");
    int j = 1;
    for(auto i = range.first; i != range.second; ++i) {
        ASSERT_DOUBLE_EQ(i->second, c2[j]);
        ++j;
    }

}