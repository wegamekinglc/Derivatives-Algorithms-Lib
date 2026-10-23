#pragma once

#include <array>

#include <dal/curve/xccyscript.hpp>
#include <dal/model/xccygaussiandata.hpp>

namespace Dal {
    struct XccyGaussianMomentTerm_ {
        double weight_ = 0.;
        bool annuity_ = false, foreignPayment_ = false;
        Date_ payment_;
        std::optional<Date_> reset_;
        std::optional<XccyCouponPeriod_> growth_;
        bool foreignRate_ = false;
        String_ projectionRoute_;
    };
    class XccyGaussianMomentProgram_ {
        PreparedCalibrationInstrument_ prepared_;
        Vector_<XccyGaussianMomentTerm_> terms_;

        XccyGaussianMomentProgram_(PreparedCalibrationInstrument_ prepared, Vector_<XccyGaussianMomentTerm_> terms)
            : prepared_(std::move(prepared)), terms_(std::move(terms)) {}
        friend XccyGaussianMomentProgram_ LowerXccyGaussianMoments(const PreparedCalibrationInstrument_&);

    public:
        [[nodiscard]] const PreparedCalibrationInstrument_& Prepared() const { return prepared_; }
        [[nodiscard]] const Vector_<XccyGaussianMomentTerm_>& Terms() const { return terms_; }
    };
    struct XccyGaussianMomentInputs_ {
        Vector_<String_> labels_;
        Vector_<> values_;
        Vector_<Handle_<XccyGaussianCurveData_>> curves_;
        Vector_<size_t> curveOffsets_;
        std::array<size_t, 3> sigmaOffsets_{};
    };
    template <class T_> struct XccyGaussianMomentValues_ {
        T_ a_, b_, price_;
        [[nodiscard]] T_ Value(const PreparedCalibrationInstrument_& prepared) const;
    };

    [[nodiscard]] XccyGaussianMomentProgram_ LowerXccyGaussianMoments(const PreparedCalibrationInstrument_& prepared);
    [[nodiscard]] XccyGaussianMomentInputs_ XccyGaussianMomentInputs(const XccyGaussianModelData_& model);
    [[nodiscard]] Matrix_<> XccyGaussianMomentNativeMap(const XccyGaussianModelData_& model, Vector_<String_>* labels);
    template <class T_>
    [[nodiscard]] XccyGaussianMomentValues_<T_>
    EvaluateXccyGaussianMoments(const XccyGaussianMomentProgram_& program, const XccyGaussianModelData_& model, const Vector_<T_>& parameters = {});
    template <class T_>
    [[nodiscard]] XccyGaussianMomentValues_<T_> EvaluateXccyGaussianMoments(const PreparedCalibrationInstrument_& prepared,
                                                                            const XccyGaussianModelData_& model,
                                                                            const Vector_<T_>& parameters = {}) {
        return EvaluateXccyGaussianMoments<T_>(LowerXccyGaussianMoments(prepared), model, parameters);
    }
} // namespace Dal
