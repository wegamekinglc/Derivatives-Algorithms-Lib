//
// Created by wegam on 2022/12/1.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal-public/src/repository.hpp>
#include <dal/storage/_repository.hpp>

namespace Dal {

    namespace {
        const ObjectAccess_* RequireRepo_() {
            ENV_SEED_TYPE(ObjectAccess_); // POSTPONED -- mark this function as taking _ENV input
            auto repo = Environment::Find<ObjectAccess_>(_env);
            REQUIRE(repo, "no repo found");
            return repo;
        }
    } // namespace

    int EraseRepository(const Vector_<Handle_<Storable_>>& objects) {
        auto* repo = RequireRepo_();
        int num_erased = 0;
        for (const auto& obj : objects)
            if (repo->Erase(*obj))
                ++num_erased;
        return num_erased;
    }


    Vector_<Handle_<Storable_>> FindRepository(const String_& pattern) {
        auto* repo = RequireRepo_();
        return repo->Find(pattern);
    }


    int SizeRepository() {
        auto* repo = RequireRepo_();
        return repo->Size();
    }
} // namespace Dal