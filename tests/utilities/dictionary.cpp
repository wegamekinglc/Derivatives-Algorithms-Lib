//
// Created by wegam on 2020/11/20.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/utilities/dictionary.hpp>
#include <dal/math/interp.hpp>

using namespace Dal;

TEST(DictionaryTest, TestDictionaryInsert) {
    Dictionary_ dict;
    dict.Insert("a", Cell_(1.));

    ASSERT_DOUBLE_EQ(dict.At("a").d_, 1.);
    ASSERT_TRUE(dict.Has("a"));
    ASSERT_FALSE(dict.Has("b"));
    ASSERT_THROW(dict.At("b"), Exception_);
    ASSERT_EQ(dict.At("b", true).type_, Cell::Type_::EMPTY);
    ASSERT_EQ(dict.Size(), 1);
    ASSERT_NO_THROW(for(const auto& v: dict) {});
}

TEST(DictionaryTest, TestDictionaryExtract) {
    Dictionary_ dict;
    dict.Insert("a", Cell_(2.));

    auto translate = [](const Cell_& src) { return src.d_ * src.d_;};
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
    constexpr auto flag = ::testing::StaticAssertTypeEq<Handle_<Interp1Linear_>, decltype(val)>();
    ASSERT_TRUE(flag);

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