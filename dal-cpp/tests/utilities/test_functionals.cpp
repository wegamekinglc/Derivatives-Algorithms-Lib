//
// Created by wegam on 2020/11/18.
//

#include <vector>
#include <type_traits>
#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/utilities/functionals.hpp>


using namespace Dal;

inline double func(int n) {
    return n * 2.0;
}


struct B_ {
    double n_;
    explicit B_(double n): n_(n) {}
};
struct D_: public B_ {
    explicit D_(double n): B_(n) {}
};


TEST(FunctionalsTest, TestGetFirstAndSecond) {
    auto s = std::make_pair('s', 1.);
    auto gf = GetFirst(s);
    auto gs = GetSecond(s);
    ASSERT_EQ(gf(s), 's');
    ASSERT_DOUBLE_EQ(gs(s), 1.);
}

TEST(FunctionalsTest, TestLinearIncrement) {
    auto inc = LinearIncrement(2);
    auto incr = 2.0;
    auto pv = 3.0;
    auto val = inc(pv, incr);
    ASSERT_DOUBLE_EQ(val, 7.);
}

TEST(FunctionalsTest, TestAverageIn) {
    auto avg = AverageIn(0.3);
    auto lhs = 3.0;
    auto rhs = 4.0;
    auto val = avg(lhs, rhs);
    ASSERT_DOUBLE_EQ(val, 3.3);
}

TEST(FunctionalsTest, TestDereference) {
    int* pp = nullptr;
    int v = 3;
    ASSERT_DOUBLE_EQ(Dereference(pp, v), 3.);

    int p = 2;
    pp = &p;
    ASSERT_DOUBLE_EQ(Dereference(pp, v), 2.);
}

TEST(FunctionalsTest, TestAsFunctor) {
    auto s = AsFunctor(func);
    auto flag = ::testing::StaticAssertTypeEq<std::function<double(int)>, decltype(s)>();
    ASSERT_TRUE(flag);
    ASSERT_DOUBLE_EQ(s(3), 6.0);
}

TEST(FunctionalsTest, TestGetShared) {
    auto s = std::make_shared<D_>(2.0);
    B_* val = GetShared<B_, D_>(s);

    ASSERT_DOUBLE_EQ(val->n_, 2.0);
}

TEST(FunctionalsTest, TestIdentity) {
    Identity_<double> s;
    ASSERT_DOUBLE_EQ(s(2.), 2.0);
}

TEST(FunctionlsTest, TestXLookupIn) {
    std::vector<int> s = {1, 2, 3, 4, 5};
    auto lookup = XLookupIn(s);
    ASSERT_DOUBLE_EQ(lookup(3), 4);
}

TEST(FunctionlsTest, TestLookupIn) {
    std::vector<int> s = {1, 2, 3, 4, 5};
    auto lookup = LookupIn(s);
    ASSERT_DOUBLE_EQ(lookup(3), 4);
}

TEST(FunctionalsTest, TestConstructCast) {

    auto flag = ::testing::StaticAssertTypeEq<int,
                                              std::invoke_result_t<ConstructCast_<double, int>, double>
                                              >();
    ASSERT_TRUE(flag);
    auto s = ConstructCast_<double, int>();
    auto val = 3.0;
    ASSERT_EQ(s(val), 3);
}