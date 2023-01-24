//
// Created by wegam on 2022/1/20.
//

#pragma once

#include <dal/indice/fixings.hpp>
#include <dal/utilities/environment.hpp>

namespace Dal {
    struct FixingsAccess_ : Environment_::Entry_ {
        std::map<String_, Handle_<Fixings_>> fixings_;
        Handle_<Fixings_> Fetch(const String_& name) const {
            auto iter = fixings_.find(name);
            if (iter != fixings_.end())
                return iter->second;
            else
                return Handle_<Fixings_>();
        }
    };

    namespace Index {
        double PastFixing(_ENV, const String_& index_name, const DateTime_& fixing_time, bool quiet = false);
    }

    class Index_ : noncopyable {
    public:
        virtual ~Index_() = default;
        virtual String_ Name() const = 0;
        virtual double Fixing(_ENV, const DateTime_& fixing_time) const;
    };

    struct IndexKey_ {
        const Handle_<Index_> val_;
        const String_ name_;
        IndexKey_(const Handle_<Index_>& val) : val_(val), name_(val_.IsEmpty() ? String_() : val->Name()) {}
        const Index_* operator->() const { return val_.get(); }
    };

    inline bool operator<(const IndexKey_& lhs, const IndexKey_& rhs) { return lhs.name_ < rhs.name_; }

    inline bool operator==(const IndexKey_& lhs, const IndexKey_& rhs) { return lhs.name_ == rhs.name_; }
} // namespace Dal
