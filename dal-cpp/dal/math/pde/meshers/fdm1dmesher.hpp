//
// Created by wegam on 2023/3/15.
//

#pragma once

#include <dal/math/vectors.hpp>

namespace Dal {
    class FDM1DMesher_ {
    public:
        explicit FDM1DMesher_(int size): locations_(size), dplus_(size), dminus_(size) {}
        virtual ~FDM1DMesher_() = default;

        [[nodiscard]] int Size() const { return locations_.size(); }
        [[nodiscard]] double DPlus(int i) const { return dplus_[i]; }
        [[nodiscard]] double DMinus(int i) const { return dminus_[i]; }
        [[nodiscard]] double Location(int i) const { return locations_[i]; }
        [[nodiscard]] const Vector_<>& Locations() const { return locations_; }

    protected:
        Vector_<> locations_;
        Vector_<> dplus_;
        Vector_<> dminus_;
    };
}