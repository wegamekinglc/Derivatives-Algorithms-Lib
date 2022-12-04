//
// Created by wegam on 2022/12/1.
//

#include <public/src/repository.hpp>


namespace Dal {

    int EraseRepository(const Vector_<Handle_<Storable_>>& objects) {
        ENV_SEED_TYPE(ObjectAccess_); // POSTPONED -- mark this function as taking _ENV input
        auto repo = Environment::Find<ObjectAccess_>(_env);
        REQUIRE(repo, "no repo found");

        int num_erased = 0;
        for (const auto& obj : objects)
            if (repo->Erase(*obj))
                ++num_erased;
        return num_erased;
    }


    Vector_<Handle_<Storable_>> FindRepository(const String_& pattern) {
        ENV_SEED_TYPE(ObjectAccess_); // POSTPONED -- mark this function as taking _ENV input
        auto repo = Environment::Find<ObjectAccess_>(_env);
        REQUIRE(repo, "no repo found");
        Vector_<Handle_<Storable_>> objects = std::move(repo->Find(pattern));
        return objects;
    }


    int SizeRepository() {
        ENV_SEED_TYPE(ObjectAccess_); // POSTPONED -- mark this function as taking _ENV input
        auto repo = Environment::Find<ObjectAccess_>(_env);
        REQUIRE(repo, "no repo found");
        return repo->Size();
    }
}