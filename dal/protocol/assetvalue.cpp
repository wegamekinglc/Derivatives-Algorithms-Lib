//
// Created by wegam on 2024/1/18.
//

#include <dal/protocol/assetvalue.hpp>

namespace Dal {
    UpdateToken_::UpdateToken_(Vector_<>::const_iterator begin, indices_t::const_iterator index_begin, int val_mask, int date_mask, const DateTime_& event_time)
        :begin_(begin), indexBegin_(index_begin), valMask_(val_mask), dateMask_(date_mask), eventTime_(event_time) {}
}