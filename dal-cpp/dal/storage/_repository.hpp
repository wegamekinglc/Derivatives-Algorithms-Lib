//
// Created by wegam on 2022/4/3.
//

// In-process store of objects identified by string tags, exposed as an environment entry.

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

        [[nodiscard]] static int Size();
        [[nodiscard]] static Vector_<Handle_<Storable_>> Find(const String_& pattern);
        [[nodiscard]] static int Erase(const String_& pattern);
        [[nodiscard]] static bool Erase(const Storable_& object);

        template <class T_> static String_ Add(const Handle_<T_>& object, const RepositoryErase_& erase) {
            return AddBase(handle_cast<Storable_>(object), erase);
        }
    };
} // namespace Dal
