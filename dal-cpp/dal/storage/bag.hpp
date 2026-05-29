//
// Created by wegam on 2023/1/20.
//

#include <map>
#include <dal/math/vectors.hpp>
#include <dal/storage/storable.hpp>

/*IF--------------------------------------------------------------------------
storable Bag
    Holder for storable objects
manual
&members
name is ?string
contents is *handle
    Objects in the bag
keys is ?string[]
    Keys of the map in the bag
-IF-------------------------------------------------------------------------*/

namespace Dal {
    struct Bag_ : Storable_ {
        using map_t = std::multimap<String_, Handle_<Storable_>>;
        map_t contents_;
        Bag_(const String_& name, const std::multimap<String_, Handle_<Storable_>>& objects)
        : Storable_("Bag", name), contents_(objects) {}
        void Write(Archive::Store_& dst) const override;
    };

}