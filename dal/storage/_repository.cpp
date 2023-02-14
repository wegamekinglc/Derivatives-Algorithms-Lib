//
// Created by wegam on 2022/4/3.
//

#define _Regex_traits _Regex_traits_Repository
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/storage/_repository.hpp>
#include <map>
#include <mutex>
#include <regex>

#include <dal/math/matrix/matrixutils.hpp>
#include <dal/storage/box.hpp>
#include <dal/storage/globals.hpp>
#include <dal/storage/storable.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
#include <dal/auto/MG_RepositoryErase_enum.inc>
    namespace {
        // a repository of objects
        // POSTPONED -- make repository thread safe
        std::map<String_, Handle_<Storable_>>& TheObjects() { RETURN_STATIC(std::map<String_, Handle_<Storable_>>); }

        static std::mutex TheObjectsMutex;
#define LOCK_OBJECTS std::lock_guard<std::mutex> l(TheObjectsMutex)

        String_ TypeTag(const Storable_& s) { return "~" + s.type_ + "~"; }
        String_ BaseTag(const Storable_& s) { return TypeTag(s) + s.name_ + "~"; }
        String_ Uniquifier(const Storable_& s) { return String::Uniquifier(&s); }

        void EraseByStart(const String_& start) {
            LOCK_OBJECTS;
            auto pLow = TheObjects().lower_bound(start);
            auto pHigh = pLow;
            while (pHigh != TheObjects().end() && pHigh->first.substr(0, start.size()) == start)
                ++pHigh;
            TheObjects().erase(pLow, pHigh);
        }
    } // namespace

    String_ ObjectAccess_::AddBase(const Handle_<Storable_>& s, const RepositoryErase_& erase) const {
        const String_ untickered = BaseTag(*s);
        switch (erase.Switch()) {
        case RepositoryErase_::Value_::NAME_NONEMPTY:
            if (s->name_.empty())
                break;
            // else fall through
        case RepositoryErase_::Value_::NAME:
            EraseByStart(untickered); // squash anything with this name
            break;
        case RepositoryErase_::Value_::TYPE:
            EraseByStart(TypeTag(*s)); // squash anything with this type
            break;
        case RepositoryErase_::Value_::_NOT_SET:
        case RepositoryErase_::Value_::NONE:
        case RepositoryErase_::Value_::_N_VALUES:
            break;
        }
        // call to LOCK_OBJECTS must be after EraseByStart, because that will also acquire the lock
        LOCK_OBJECTS;
        const String_ retval = untickered + Uniquifier(*s);
        TheObjects().insert(make_pair(retval, s));
        return retval;
    }

    int ObjectAccess_::Erase(const String_& pattern) const {
        std::regex match(pattern);
        LOCK_OBJECTS;
        auto start = TheObjects().begin();
        // could do this with remove_if, but the lambda is ghastly
        int retval = 0;
        while (start != TheObjects().end()) {
            if (std::regex_search(start->first, match))
                ++retval, TheObjects().erase(start++);
            else
                ++start;
        }
        return retval;
    }

    bool ObjectAccess_::Erase(const Storable_& object) const {
        LOCK_OBJECTS;
        auto po = TheObjects().find(BaseTag(object) + Uniquifier(object));
        if (po == TheObjects().end())
            return false;
        TheObjects().erase(po);
        return true;
    }

    int ObjectAccess_::Size() const {
        LOCK_OBJECTS;
        return static_cast<int>(TheObjects().size());
    }

    Vector_<Handle_<Storable_>> ObjectAccess_::Find(const String_& pattern) const {
        std::regex match(pattern);
        LOCK_OBJECTS;
        auto all = TheObjects();
        Vector_<Handle_<Storable_>> retval;
        for (const auto& k_v : all) {
            if (std::regex_search(k_v.first, match))
                retval.push_back(k_v.second);
        }
        return retval;
    }

    void Repository_Erase(const Vector_<Handle_<Storable_>>& objects, int* num_erased) {
        LOCK_OBJECTS;
        *num_erased = 0;
        for (const auto& obj : objects)
            *num_erased += static_cast<int>(TheObjects().erase(BaseTag(*obj) + Uniquifier(*obj)));
    }

    // enable a store for low-level global objects

    namespace {
        static const String_ GLOBAL_TAG("##GLOBAL##");

        struct RepoStore_ : Global::Store_ {
            void Set(const String_& name, const Matrix_<Cell_>& value) override {
                Handle_<Box_> storable(new Box_(GLOBAL_TAG + name, value));
                (void)ObjectAccess_().Add(storable, RepositoryErase_("NAME"));
            }
            const Matrix_<Cell_>& Get(const String_& name) override {
                static const Matrix_<Cell_> EMPTY;
                Box_ dummy(GLOBAL_TAG + name, Matrix_<Cell_>());
                String_ tagStart = BaseTag(dummy);
                LOCK_OBJECTS;
                auto po = TheObjects().lower_bound(tagStart);
                if (po != TheObjects().end() && po->first.substr(0, tagStart.size()) == tagStart) {
                    const Box_* box = dynamic_cast<const Box_*>(po->second.get());
                    REQUIRE(box && !box->contents_.Empty(), "Internal error, or global tag meddling");
                    return box->contents_;
                }
                return EMPTY;
            }
        };

        RUN_AT_LOAD(Global::SetTheDateStore(new RepoStore_))
    } // namespace

    Handle_<Storable_> ObjectAccess_::Fetch(const String_& tag, bool quiet) const {
        NOTICE(tag);
        LOCK_OBJECTS;
        Handle_<Storable_> retval;
        auto po = TheObjects().find(tag);
        if (po != TheObjects().end()) {
            assert(po->second); // no null object in repository
            return po->second;
        }
        REQUIRE(quiet, "Repository object not found");
        return Handle_<Storable_>();
    }
} // namespace Dal