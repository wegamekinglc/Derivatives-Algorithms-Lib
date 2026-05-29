//
// Created by wegam on 2022/1/20.
//

#pragma once

#include <dal/indice/index.hpp>

namespace Dal::Index {
    class Composite_ : public Index_ {
    public:
        using component_t = std::pair<Handle_<Index_>, double>;

    private:
        Vector_<component_t> components_;
        double Fixing(_ENV, const DateTime_&) const override;
        String_ Name() const override;
    };
} // namespace Dal::Index
