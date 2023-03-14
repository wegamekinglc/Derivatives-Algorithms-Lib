//
// Created by wegam on 2023/3/15.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {
    class FDM1DMesher_ {
    public:
        explicit FDM1DMesher_(int size): locations_(size), dplus_(size), dminus_(size) {}

        int Size() const { return locations_.size(); }
        double DPlus(int i) const { return dplus_[i]; }
        double DMinus(int i) const { return dminus_[i]; }
        double Location(int i) const { return locations_[i]; }
        const Vector_<>& Locations() const { return locations_; }

    private:
        Vector_<> locations_;
        Vector_<> dplus_;
        Vector_<> dminus_;
    };
}