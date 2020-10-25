//
// Created by wegam on 2020/10/25.
//

#pragma once

// basically a map<String_, Cell_>, with uppercase strings and protection against duplicates

#include <map>
#include <dal/math//cell.hpp>
#include <dal/storage/storable.hpp>

namespace Dal {
    class Dictionary_ {
        std::map<String_, Cell_> val_;

    public:
        Dictionary_() {}

        void Insert(const String_& key, const Cell_& val);
        bool Has(const String_& key) const;
        const Cell_& At(const String_& key, bool optional = false) const;
        int Size() const { return static_cast<int>(val_.size()); }
        std::map<String_, Cell_>::const_iterator begin() const { return val_.begin(); }
        std::map<String_, Cell_>::const_iterator end() const { return val_.end(); }
    };

    namespace Dictionary {
        const Cell_& BlankCell(); // returned when At can't find key

        template <class F_, class R_>
        auto Extract(const Dictionary_& src, const String_& key, F_ translate, const R_& default_val)
            -> decltype(translate(Cell_())) {
            return src.Has(key) ? translate(src.At(key)) : default_val;
        }

        template <class F_>
        auto Extract(const Dictionary_& src, const String_& key, F_ translate) -> decltype(translate(Cell_())) {
            return translate(src.At(key, false));
        }

        // deal with non-atomic types too
        Handle_<Storable_>
        FindHandleBase(const std::map<String_, Handle_<Storable_>>& handles, const String_& key, bool quiet = false);
        template <class T_>
        Handle_<T_> FindHandle(const std::map<String_, Handle_<Storable_>>& handles, const String_& key) {
            auto retval = handle_cast<T_>(FindHandleBase(handles, key));
            REQUIRE(retval, "Object with key '" + key + "' has invalid type");
            return retval;
        }

        // convert dict to/from string (';' separator, '=' between key and value)
        String_ ToString(const Dictionary_& dict);
        Dictionary_ FromString(const String_& src);
    }

// sometimes we need to provide information about how to save a settings
    template <class T_> class StoreAsDictionary_ {}; // no content

}