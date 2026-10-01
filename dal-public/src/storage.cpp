//
// Created by Codex on 2026/10/1.
//

#include <dal-public/src/storage.hpp>
#include <dal/storage/json.hpp>

namespace Dal {
    String_ WriteObjectJson(const Storable_& value) { return JSON::WriteString(value); }
    Handle_<Storable_> ReadObjectJson(const char* data, size_t length) { return JSON::ReadString(data, length, JSONReadOptions_()); }
    Handle_<Bag_> NewBag(const String_& name, const Bag_::map_t& contents) { return Handle_<Bag_>(std::make_shared<Bag_>(name, contents)); }
} // namespace Dal
