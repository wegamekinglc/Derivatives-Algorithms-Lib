//
// Created by wegam on 2023/1/22.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/utilities/environment.hpp>
#include <dal/storage/audit.hpp>
#include <dal/math/interp/interplinear.hpp>

using namespace Dal;

TEST(StorageTest, TestAudit) {
    AuditorImp_ auditor;
    ENV_SEED(auditor);
    auditor.mode_ = AuditorMode_::Value_::READING;

    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1_> src(Interp::NewLinear("interp", x, f));

    Environment::Audit(_env, String_("sample_data"), src);

    auditor.mode_ = AuditorMode_::Value_::SHOWING;
    Handle_<Interp1_> bak;
    Environment::Recall(_env, String_("sample_data"), &bak);
    ASSERT_DOUBLE_EQ((*src)(2.5), (*bak)(2.5));
}
