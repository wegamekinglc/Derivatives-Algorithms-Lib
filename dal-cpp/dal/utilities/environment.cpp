//
// Created by wegam on 2020/10/2.
//

#include <dal/platform/strict.hpp>
#include <dal/utilities/environment.hpp>

namespace Dal {
    void Environment_::Iterator_::operator++() {
        if (IsValid())
            imp_ = Handle_<IterImp_>(imp_->Next());
    }
} // namespace Dal
