//
// Created by dal-tester on 2026/8/15.
//

#include <gtest/gtest.h>

#include <dal/math/interp/interplinear.hpp>
#include <dal/storage/_repository.hpp>

#include <dal-public/src/repository.hpp>

using Dal::Handle_;
using Dal::Interp1_;
using Dal::Storable_;
using Dal::String_;
using Dal::Vector_;

namespace {
    Handle_<Storable_> InsertInterp(const char* name) {
        const Vector_<> x = {1., 2., 3.};
        const Vector_<> y = {2.5, 3.5, 1.7};
        Handle_<Storable_> object(Dal::Interp::NewLinear(String_(name), x, y));
        Dal::ObjectAccess_::Add(object, Dal::RepositoryErase_::Value_::NONE);
        return object;
    }
} // namespace

TEST(RepositoryTest, TestSizeChangesAroundInsertAndErase) {
    const int baseline = Dal::SizeRepository();
    const auto first = InsertInterp("dal_public_repo_size_a");
    const auto second = InsertInterp("dal_public_repo_size_b");

    ASSERT_EQ(Dal::SizeRepository(), baseline + 2);

    const Vector_<Handle_<Storable_>> eraseFirst = {first};
    ASSERT_EQ(Dal::EraseRepository(eraseFirst), 1);
    ASSERT_EQ(Dal::SizeRepository(), baseline + 1);

    // erasing the same object again removes nothing
    ASSERT_EQ(Dal::EraseRepository(eraseFirst), 0);
    ASSERT_EQ(Dal::SizeRepository(), baseline + 1);

    const Vector_<Handle_<Storable_>> eraseSecond = {second};
    ASSERT_EQ(Dal::EraseRepository(eraseSecond), 1);
    ASSERT_EQ(Dal::SizeRepository(), baseline);
}

TEST(RepositoryTest, TestFindByNamePattern) {
    const auto object = InsertInterp("dal_public_repo_find_me");

    const auto matched = Dal::FindRepository(String_("dal_public_repo_find_me"));
    ASSERT_EQ(matched.size(), 1);
    ASSERT_EQ(matched[0].get(), object.get());

    ASSERT_TRUE(Dal::FindRepository(String_("dal_public_repo_no_such_object_zzz")).empty());

    const Vector_<Handle_<Storable_>> cleanup = {object};
    ASSERT_EQ(Dal::EraseRepository(cleanup), 1);
    ASSERT_TRUE(Dal::FindRepository(String_("dal_public_repo_find_me")).empty());
}

TEST(RepositoryTest, TestEraseEmptyVectorRemovesNothing) {
    const int baseline = Dal::SizeRepository();
    const Vector_<Handle_<Storable_>> nothing;

    ASSERT_EQ(Dal::EraseRepository(nothing), 0);
    ASSERT_EQ(Dal::SizeRepository(), baseline);
}

TEST(RepositoryTest, TestEraseObjectNeverInsertedReturnsZero) {
    const Vector_<> x = {1., 2., 3.};
    const Vector_<> y = {2.5, 3.5, 1.7};
    const Handle_<Storable_> notStored(Dal::Interp::NewLinear(String_("dal_public_repo_never_stored"), x, y));
    const int baseline = Dal::SizeRepository();

    const Vector_<Handle_<Storable_>> objects = {notStored};
    ASSERT_EQ(Dal::EraseRepository(objects), 0);
    ASSERT_EQ(Dal::SizeRepository(), baseline);
}
