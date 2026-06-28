//
// Created by wegam on 2022/1/20.
//

#pragma once


#include <map>
#include <dal/storage/storable.hpp>
#include <dal/time/datetime.hpp>

/*IF--------------------------------------------------------------------------
storable Fixings
     Holder for historical fixings
manual
&members
name is ?string
fixings is number[]
     Objects in the bag
fixing_times is datetime[]
     Keys of the map in the bag
-IF-------------------------------------------------------------------------*/

namespace Dal {
    class FixHistory_ {
    public:
        using vals_t = std::map<DateTime_, double>;

    private:
        vals_t vals_;

    public:
        explicit FixHistory_(const vals_t& vals) : vals_(vals) {}
        double Find(const DateTime_& fix_time, bool quiet = false) const;
    };

    namespace FixHistory {
        const FixHistory_& Empty();
    } // namespace FixHistory

    double LookupFixing(const std::map<DateTime_, double>& vals, const DateTime_& fixTime, bool quiet = false);

    class Fixings_ : public Storable_ {
    public:
        using vals_t = std::map<DateTime_, double>;
        const vals_t vals_;
        Fixings_(const String_& indexName, const vals_t& vals = vals_t())
            : Storable_("Fixings", indexName), vals_(vals) {}
        void Write(Archive::Store_& dst) const override;
    };
} // namespace Dal
