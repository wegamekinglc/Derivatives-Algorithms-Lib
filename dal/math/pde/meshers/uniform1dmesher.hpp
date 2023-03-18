//
// Created by wegam on 2023/3/18.
//

#pragma once

#include <dal/math/pde/meshers/fdm1dmesher.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    class Uniform1DMesher_ : public FDM1DMesher_ {
    public:
        Uniform1DMesher_(double start, double end, int size) :FDM1DMesher_(size) {
            REQUIRE(end > start, "end must be large than start");
            const auto dx = (end - start) / (size - 1);

            for (auto i = 0; i < size - 1; ++i) {
                locations_[i] = start + i * dx;
                dplus_[i] = dminus_[i + 1] = dx;
            }

            locations_.back() = end;
            dplus_.back() = dminus_.front() = Null_<double>();
        }
    };
}
