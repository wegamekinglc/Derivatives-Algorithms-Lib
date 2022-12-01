//
// Created by wegam on 2022/12/1.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/storage/_reader.hpp>
#include <dal/storage/_repository.hpp>
#include <dal/utilities/environment.hpp>
#include <dal/utilities/exceptions.hpp>


namespace Dal {

    int EraseRepository(const Vector_<Handle_<Storable_>>& objects);
    Vector_<Handle_<Storable_>> FindRepository(const String_& pattern);
    int SizeRepository();
}
