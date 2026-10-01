//
// Created by wegam on 2022/12/1.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal-public/src/repository.hpp>
#include <dal/storage/_repository.hpp>

namespace Dal {

    namespace {
        const ObjectAccess_* RequireRepo_(const Environment_* environment) {
            const auto* repo = Environment::Find<ObjectAccess_>(environment);
            REQUIRE(repo, "no repo found in the environment");
            return repo;
        }

        const ObjectAccess_* RequireRepo_() {
            ENV_SEED_TYPE(ObjectAccess_);
            return RequireRepo_(_env);
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

    int EraseRepositoryMatching(const String_& pattern) { return RequireRepo_()->Erase(pattern); }

    Handle_<Storable_> FetchRepository(_ENV, const String_& tag) { return RequireRepo_(_env)->Fetch(tag); }

    String_ StoreRepository(_ENV, const Handle_<Storable_>& object) {
        REQUIRE(object, "Output handle is NULL");
        return RequireRepo_(_env)->Add(object, RepositoryErase_());
    }
} // namespace Dal
