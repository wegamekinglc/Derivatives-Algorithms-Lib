//
// Created by wegam on 2023/1/22.
//

#include <gtest/gtest.h>
#include <dal/utilities/environment.hpp>
#include <dal/storage/audit.hpp>
#include <dal/math/interp/interp.hpp>

using namespace Dal;

TEST(AuditTest, TestAudit) {
    AuditorImp_ auditor;
    ENV_SEED(auditor);
    auditor.mode_ = AuditorImp_::READING;

    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1Linear_> src(new Interp1Linear_("interp", x, f));

    Environment::Audit(_env, String_("sample_data"), src);

    auditor.mode_ = AuditorImp_::SHOWING;
    Handle_<Interp1Linear_> bak;
    Environment::Recall(_env, String_("sample_data"), &bak);
    ASSERT_EQ(x, bak->x());
    ASSERT_EQ(f, bak->f());
}
