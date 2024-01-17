//
// Created by wegam on 2024/1/18.
//

#pragma once
#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/indice/indexpath.hpp>
#include <dal/time/datetime.hpp>
#include <optional>

namespace Dal {
    namespace Valuation {
        using address_t = int;

        std::optional<double> KnownValue(address_t loc);
        std::optional<address_t> FixedLoc(double value);

        struct IndexAddress_ {
            int val_;
            explicit IndexAddress_(int val): val_(val) {}
        };
    }

    class UpdateToken_ {
        using indices_t = Vector_<Handle_<IndexPath_>>;
        Vector_<>::const_iterator begin_;
        indices_t::const_iterator indexBegin_;
        const int valMask_, dateMask_;

    public:
        const DateTime_ eventTime_;
        UpdateToken_(Vector_<>::const_iterator begin, indices_t::const_iterator index_begin, int val_mast, int date_mask, const DateTime_& event_time);

        [[nodiscard]] FORCE_INLINE const double& operator[](const Valuation::address_t& loc) const {
            return *(begin_ + (loc & valMask_));
        }

        [[nodiscard]] FORCE_INLINE const IndexPath_& Index(const Valuation::IndexAddress_& loc) const {
            return **(indexBegin_ + (loc.val_ & valMask_));
        }
    };
}
