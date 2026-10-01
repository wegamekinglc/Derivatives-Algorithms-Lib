//
// Created by Codex on 2026/10/1.
//

#pragma once

#include <dal-public/src/types.hpp>
#include <dal/storage/bag.hpp>

namespace Dal {
    String_ WriteObjectJson(const Storable_& value);
    Handle_<Storable_> ReadObjectJson(const char* data, size_t length);
    Handle_<Bag_> NewBag(const String_& name, const Bag_::map_t& contents);
} // namespace Dal
