#pragma once

#include <dal/curve/xccybasiscalibration.hpp>
#include <dal/model/xccygaussiandata.hpp>

namespace Dal {
    [[nodiscard]] Handle_<XccyGaussianModelData_> XccyGaussianBasisCandidate(const XccyInstrumentCalibrationSpec_& spec,
                                                                             const Handle_<DiscountCurve_>& basis);
    template <class T_>
    [[nodiscard]] Vector_<T_> EvaluateXccyGaussianStaged(const XccyInstrumentCalibrationSpec_& spec,
                                                         const CurveDefinition_& definition,
                                                         const Vector_<PreparedCalibrationInstrument_>& plans,
                                                         const Vector_<T_>& parameters);
} // namespace Dal
