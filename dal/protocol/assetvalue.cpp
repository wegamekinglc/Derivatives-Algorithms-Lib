//
// Created by wegam on 2024/1/18.
//

#include <dal/protocol/assetvalue.hpp>
#include <utility>

namespace Dal {
    UpdateToken_::UpdateToken_(Vector_<>::const_iterator begin,
                               indices_t::const_iterator index_begin,
                               const DateTime_& event_time)
        :begin_(std::move(begin)), indexBegin_(std::move(index_begin)), eventTime_(event_time) {}
}