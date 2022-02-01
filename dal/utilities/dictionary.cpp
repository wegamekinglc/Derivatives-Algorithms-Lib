//
// Created by wegam on 2020/10/25.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/utilities/dictionary.hpp>

#include <dal/math/cellutils.hpp>
#include <dal/time/datetimeutils.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    void Dictionary_::Insert(const String_& key, const Cell_& val) {
        const String_& k = String::Condensed(key);
        REQUIRE(!val_.count(k), String_("Duplicate key '" + k + "'"));
        val_.insert(make_pair(k, val));
    }

    const Cell_& Dictionary::BlankCell() { RETURN_STATIC(Cell_); }

    const Cell_& Dictionary_::At(const String_& key, bool optional) const {
        const String_& k = String::Condensed(key);
        auto p = val_.find(k);
        if (p != val_.end())
            return p->second;
        REQUIRE(optional, String_("No value for key '" + k + "'"));
        return Dictionary::BlankCell();
    }

    bool Dictionary_::Has(const String_& key) const { return val_.count(String::Condensed(key)) > 0; }

    Handle_<Storable_>
    Dictionary::FindHandleBase(const std::map<String_, Handle_<Storable_>>& handles, const String_& key, bool quiet) {
        auto p = handles.find(String::Condensed(key));
        if (p == handles.end()) {
            REQUIRE(quiet, String_("No handle found at '" + key + "'"));
            return Handle_<Storable_>();
        }
        return p->second;
    }

    String_ Dictionary::ToString(const Dictionary_& d) {
        String_ ret_val;
        for (const auto& kv : d) {
            if (!ret_val.empty())
                ret_val += ';';
            ret_val += kv.first;
            ret_val += '=';
            ret_val += Cell::CoerceToString(kv.second);
        }
        return ret_val;
    }
    Dictionary_ Dictionary::FromString(const String_& s) {
        Vector_<String_> entries = String::Split(s, ';', false);
        Dictionary_ ret_val;
        for (const auto& kev : entries) {
            Vector_<String_> kv = String::Split(kev, '=', true);
            REQUIRE(kv.size() == 2, "Dictionary entry must have the form 'key=value'");
            REQUIRE(!kv.front().empty(), "Empty dictionary key");
            const String_& vs = kv.back();
            Cell_ value;
            if (String::IsNumber(vs))
                value = String::ToDouble(vs);
            else if (Date::IsDateString(vs))
                value = Date::FromString(vs);
            else if (DateTime::IsDateTimeString(vs))
                value = DateTime::FromString(vs);
            else if (!vs.empty())
                value = vs;
            ret_val.Insert(kv.front(), value);
        }
        return ret_val;
    }
} // namespace Dal