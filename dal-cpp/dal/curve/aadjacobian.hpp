//
// Created by dal-implementer on 2026/7/12.
//

#pragma once

#include <dal/math/aad/aad.hpp>
#include <dal/math/matrix/matrixs.hpp>

namespace Dal {
    Matrix_<> HarvestCurveJacobian(AAD::Tape_& tape,
                                   Vector_<AAD::Number_>& independents,
                                   Vector_<AAD::Number_>& residuals,
                                   const Vector_<int>& rowWidths = Vector_<int>());
} // namespace Dal
