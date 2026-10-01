//
// Created by wegam on 2022/12/1.
//

#pragma once

#include <dal/storage/storable.hpp>
#include <dal/utilities/environment.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {

    int EraseRepository(const Vector_<Handle_<Storable_>>& objects);
    int EraseRepositoryMatching(const String_& pattern);
    Vector_<Handle_<Storable_>> FindRepository(const String_& pattern);
    int SizeRepository();
    Handle_<Storable_> FetchRepository(_ENV, const String_& tag);
    String_ StoreRepository(_ENV, const Handle_<Storable_>& object);
} // namespace Dal
