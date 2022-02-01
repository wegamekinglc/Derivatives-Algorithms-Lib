//
// Created by wegam on 2020/10/2.
//

#include <dal/utilities/environment.hpp>

namespace Dal {
    void Environment_::Iterator_::operator++() {
        if (IsValid())
            return imp_.reset(imp_->Next());
    }
} // namespace Dal