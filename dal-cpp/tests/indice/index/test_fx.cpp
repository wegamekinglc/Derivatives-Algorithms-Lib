//
// Created by wegam on 2023/1/23.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/indice/index.hpp>
#include <dal/indice/index/fx.hpp>
#include <dal/indice/parser/fx.hpp>
#include <dal/utilities/environment.hpp>

using namespace Dal;

TEST(IndexTest, TestIndexFxFixing) {
    String_ name("FX[USD/GBP]");
    std::unique_ptr<Index_> index(Index::FxParser(name));

    DateTime_ fixing_time(Date_(2023, 1, 24));
    {
        FixingsAccess_ fixings_access;
        ENV_SEED(fixings_access);
        ASSERT_THROW(index->Fixing(_env, fixing_time), std::runtime_error);
    }

    {
        FixingsAccess_ fixings_access;
        std::map<DateTime_, double> vals;
        vals.insert(std::make_pair(fixing_time, 2.0));
        Handle_<Fixings_> fixings(new Fixings_(name, vals));
        fixings_access.fixings_.insert(std::make_pair(name, fixings));
        ENV_SEED(fixings_access);
        ASSERT_DOUBLE_EQ(index->Fixing(_env, fixing_time), 2.0);
    }
}