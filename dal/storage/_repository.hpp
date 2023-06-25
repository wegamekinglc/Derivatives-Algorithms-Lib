//
// Created by wegam on 2022/4/3.
//

// global in-process store of objects
// allows objects to be identified with string tags, e.g. for Excel use
// provide this as an environment entry, so it can be shared by callers

#pragma once

#include <dal/string/strings.hpp>
#include <dal/utilities/environment.hpp>

/*IF--------------------------------------------------------------------------
enumeration RepositoryErase
    help: Controls what is erased when a new tag is added to the repository
switchable
alternative NONE
    help: Erase nothing, just add
alternative NAME_NONEMPTY
    default:1
    help: Erase object of same type and name, iff name is nonempty
alternative NAME
    help: Erase object of same type and name
alternative TYPE
    help: Erase all objects of the same type
-IF-------------------------------------------------------------------------*/

namespace Dal {
    class Storable_;

#include <dal/auto/MG_RepositoryErase_enum.hpp>

    class ObjectAccess_ : public Environment::Entry_ {
        [[nodiscard]] String_ AddBase(const Handle_<Storable_>& object, const RepositoryErase_& erase) const;

    public:
        [[nodiscard]] Handle_<Storable_> Fetch(const String_& tag, bool quiet = false) const;

        // count handles
        [[nodiscard]] int Size() const;
        // find a handle based on an input string which may be incomplete
        [[nodiscard]] Handle_<Storable_> LowerBound(const String_& partial_name) const;
        // return all matching handles
        [[nodiscard]] Vector_<Handle_<Storable_>> Find(const String_& pattern) const;
        // erase all matching handles, return number erased
        [[nodiscard]] int Erase(const String_& pattern) const;
        // erase one handle
        [[nodiscard]] bool Erase(const Storable_& object) const;

        template <class T_> String_ Add(const Handle_<T_>& object, const RepositoryErase_& erase) const {
            return AddBase(handle_cast<Storable_>(object), erase);
        }
    };
}
