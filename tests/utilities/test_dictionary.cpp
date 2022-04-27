//
// Created by wegam on 2020/11/20.
//

#include <dal/math/interp/interp.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/dictionary.hpp>
#include <gtest/gtest.h>

using namespace Dal;

TEST(DictionaryTest, TestDictionaryInsert) {
    Dictionary_ dict;
    dict.Insert("a", Cell_(1.));

    ASSERT_DOUBLE_EQ(Cell::ToDouble(dict.At("a")), 1.);
    ASSERT_TRUE(dict.Has("a"));
    ASSERT_FALSE(dict.Has("b"));
    ASSERT_THROW(dict.At("b"), Exception_);
    ASSERT_TRUE(Cell::IsEmpty(dict.At("b", true)));
    ASSERT_EQ(dict.Size(), 1);
    ASSERT_NO_THROW(for(const auto& v: dict) {});
}

TEST(DictionaryTest, TestDictionaryExtract) {
    Dictionary_ dict;
    dict.Insert("a", Cell_(2.));

    auto translate = [](const Cell_& src) {
        const double d = Cell::ToDouble(src);
        return d * d;
    };
    auto val = Dictionary::Extract(dict, "a", translate);
    ASSERT_DOUBLE_EQ(val, 4.);
    ASSERT_THROW(Dictionary::Extract(dict, "b", translate), Exception_);

    val = Dictionary::Extract(dict, "b", translate, 6.);
    ASSERT_EQ(val, 6.);
}

TEST(DictionaryTest, TestDictionaryFindHandle) {
    std::map<String_, Handle_<Storable_>> handles;

    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};
    auto name = "interp";

    handles.insert(std::make_pair(name, Handle_<Storable_>(new Interp1Linear_(name, x, f))));
    auto val = Dictionary::FindHandle<Interp1Linear_>(handles, name);
    ASSERT_THROW(Dictionary::FindHandle<Interp1Linear_>(handles, "not_found"), Exception_);
}

TEST(DictionaryTest, TestDictionaryToString) {
    Dictionary_ dict;
    dict.Insert("a", Cell_(2.));
    dict.Insert("b", Cell_(true));
    dict.Insert("c", Cell_("hello world"));
    dict.Insert("d", Cell_(DateTime_(Date_(2020, 11, 21), 0.5)));

    auto val = Dictionary::ToString(dict);
    ASSERT_EQ(val, "A=2;B=true;C=hello world;D=2020-11-21 12:00:00");
}

TEST(DictionaryTest, TestDictionaryFromString) {
    String_ s = "A=2;B=true;C=hello world;D=2020-11-21 12:00:00";
    auto dict = Dictionary::FromString(s);
    auto val = Dictionary::ToString(dict);
    ASSERT_EQ(val, s);
}