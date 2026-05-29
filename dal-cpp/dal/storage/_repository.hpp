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

    class ObjectAccess_ : public Environment::Entry_  {
        [[nodiscard]] static String_ AddBase(const Handle_<Storable_>& object, const RepositoryErase_& erase);

    public:
        [[nodiscard]] static Handle_<Storable_> Fetch(const String_& tag, bool quiet = false);

        // count handles
        [[nodiscard]] static int Size();
        // return all matching handles
        [[nodiscard]] static Vector_<Handle_<Storable_>> Find(const String_& pattern);
        // erase all matching handles, return number erased
        [[nodiscard]] static int Erase(const String_& pattern);
        // erase one handle
        [[nodiscard]] static bool Erase(const Storable_& object);

        template <class T_> static String_ Add(const Handle_<T_>& object, const RepositoryErase_& erase) {
            return AddBase(handle_cast<Storable_>(object), erase);
        }
    };
}
