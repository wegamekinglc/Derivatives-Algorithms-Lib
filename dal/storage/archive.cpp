//
// Created by wegamekinglc on 2020/5/1.
//

#include <map>
#include <dal/storage/archive.hpp>

namespace {
    std::map<Dal::String_, const Dal::Archive::Reader_*>& TheBuilders() {
        RETURN_STATIC(std::map<Dal::String_, const Dal::Archive::Reader_*>);
    }
}

namespace Dal {
    void Archive::Utils::SetStorable(Archive::Store_ &dst, const String_ &name, const Storable_ &value) {
        auto& child = dst.Child(name);
        if(child.StoreRef(&value))
            child.Done();
        else
            value.Write(child);
    }

    Archive::Store_& Archive::Store_::Element(int index) {
        return Child(String::FromInt(index));
    }

    Handle_<Storable_> Archive::Extract(const View_ &src, Built_ &built) {
        Handle_<Storable_>& ret_val = src.Known(built);

        if(!ret_val) {
            const String_& type = src.Type();
            REQUIRE(!type.empty(), "No type supplied: can't extract a handle");
            NOTICE(type);
            auto pb = TheBuilders().equal_range(type);
            REQUIRE(pb.first != pb.second, "Type has bo builder");
            REQUIRE(pb.first == --pb.second, "Builder is not unique");
            ret_val.reset(pb.first->second->Build(src, built));
        }
        return ret_val;
    }

    const Archive::View_& Archive::View_::Element(int index) const {
        return Child(String::FromInt(index));
    }

    bool Archive::View_::HasElement(int index) const {
        return HasChild(String::FromInt(index));
    }
}