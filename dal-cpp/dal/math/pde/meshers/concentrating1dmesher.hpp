//
// Created by wegam on 2023/3/18.
//

#pragma once

#include <dal/math/pde/meshers/fdm1dmesher.hpp>


namespace Dal {
    class Concentrating1dMesher_ : public FDM1DMesher_ {
    public:
        Concentrating1dMesher_(double start,
                               double end,
                               int size,
                               const std::pair<double, double>& cPoints,
                               bool requireCPoint = false);
    };
}
