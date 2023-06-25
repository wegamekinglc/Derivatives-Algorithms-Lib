//
// Created by wegam on 2022/4/3.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <public/excel/__platform.hpp>
#include <public/src/repository.hpp>

/*IF--------------------------------------------------------------------------
public Repository_Erase
        Erase objects from repository
&inputs
objects is handle[]
        The objects to remove(their handles will be invalidated)
&outputs
num_erased is integer
        The number of objects removed
-IF-------------------------------------------------------------------------*/


/*IF--------------------------------------------------------------------------
public Repository_Find
        Find existing objects in the repository
&inputs
match is string
        A pattern to match (by search)
&outputs
objects is handle[]
        The repository objects matching the pattern
-IF-------------------------------------------------------------------------*/


/*IF--------------------------------------------------------------------------
public Repository_Size
        Number of objects in the repository
&outputs
size is integer
        The number of stored objects
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {

        void Repository_Erase(const Vector_<Handle_<Storable_>>& objects, int* num_erased) {
            *num_erased = EraseRepository(objects);
        }

        void Repository_Find(const String_& pattern, Vector_<Handle_<Storable_>>* objects) {
            *objects = FindRepository(pattern);
        }

        void Repository_Size(int* size) {
            *size = SizeRepository();
        }
    } // namespace

#ifdef _WIN32
#include "public/auto/MG_Repository_Erase_public.inc"
#include "public/auto/MG_Repository_Find_public.inc"
#include "public/auto/MG_Repository_Size_public.inc"
#endif
} // namespace Dal
