//
// Created by wegam on 2024/1/18.
//

#include <dal/protocol/assetvalue.hpp>
#include <utility>

namespace Dal {
    UpdateToken_::UpdateToken_(Vector_<>::const_iterator begin,
                               indices_t::const_iterator index_begin,
                               const DateTime_& event_time)
    : begin_(begin), indexBegin_(index_begin), eventTime_(event_time) {
    }

    const uint64_t UpdateToken_::valMask_ = 0b0000000000000000000000000011111111111111111111111111111111111111;
    const uint64_t UpdateToken_::dateMask_ = 0b1111111111111111111111111100000000000000000000000000000000000000;
    const int UpdateToken_::dateOffSet_ = 38;
} // namespace Dal