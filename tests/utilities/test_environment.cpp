//
// Created by wegam on 2020/10/2.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/utilities/environment.hpp>\

using Dal::Vector_;
using Dal::Handle_;
using Dal::Environment::Entry_;
using Dal::Environment::Base_;
using Dal::Environment::Find;
using Dal::Environment::Collect;

class SampleEntry_: public Entry_ {
    int val_;
public:
    explicit SampleEntry_(int val): val_(val) {}
    int value() const { return val_;}
};

TEST(EnvironmentTest, TestEnvSeed) {
    SampleEntry_ e(1);
    ENV_SEED(e);

    auto expected = 1;
    auto calculated =Find<SampleEntry_>(_env)->value();
    ASSERT_EQ(1, calculated);
}

TEST(EnvironmentTest, TestEnvAdd) {
    SampleEntry_ e1(1);
    ENV_SEED(e1);

    {
        SampleEntry_ e2(2);
        ENV_ADD(e2);
        auto expected = 2;
        auto calculated = Find<SampleEntry_>(_env)->value();
        ASSERT_EQ(expected, calculated);
    }

    auto expected = 1;
    auto calculated = Find<SampleEntry_>(_env)->value();
    ASSERT_EQ(expected, calculated);
}

TEST(EnvironmentTest, TestCollect) {
    SampleEntry_ e1(1);
    ENV_SEED(e1);

    {
    SampleEntry_ e2(2);
    ENV_ADD(e2);
    auto calculated = Collect<SampleEntry_>(_env);
    ASSERT_EQ(2, calculated.size());
    }

    auto calculated = Collect<SampleEntry_>(_env);
    ASSERT_EQ(1, calculated.size());
}


TEST(EnvironmentTest, TestBase) {
    Vector_<Handle_<Entry_>> records;
    records.push_back(Handle_<Entry_>(new SampleEntry_(1)));
    records.push_back(Handle_<Entry_>(new SampleEntry_(2)));
    records.push_back(Handle_<Entry_>(new SampleEntry_(3)));

    Base_ env(records);
    auto me = env.Begin();

    ASSERT_TRUE(me.IsValid());

    ++me;
    ++me;
    ++me;
    ASSERT_FALSE(me.IsValid());
}


TEST(EnvironmentTest, TestBaseIterDereference) {
    Vector_<Handle_<Entry_>> records;
    records.push_back(Handle_<Entry_>(new SampleEntry_(1)));

    Base_ env(records);
    const SampleEntry_& me = dynamic_cast<const SampleEntry_&>(*(env.Begin()));

    ASSERT_EQ(1, me.value());
}
