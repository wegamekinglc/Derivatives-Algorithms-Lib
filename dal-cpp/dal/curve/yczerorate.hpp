//
// Created by dal-implementer on 2026/7/12.
//

#pragma once

#include <memory>

#include <dal/curve/discount.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/logdfinterp.hpp>
#include <dal/curve/logdfscheme.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/time/daybasis.hpp>

namespace Dal {
    namespace Tape {
        template <class T_, class B_ = DiscountCurve_<double>>
        class DiscountZeroRate_ : public CurveWithBase_<DiscountCurve_<T_>, B_>, public FittableCurve_ {
            Date_ anchorDate_;
            Vector_<Date_> nodeDates_;
            DayBasis_ dayCount_;
            Vector_<> yearFractions_;
            Vector_<T_> zeroRates_;
            LogDfScheme_ scheme_;
            std::unique_ptr<LogDfInterpolation_> interpolation_;

            [[nodiscard]] T_ LogDfAt(double yearFraction) const;

        public:
            DiscountZeroRate_(const String_& name,
                              const String_& ccy,
                              const Date_& anchorDate,
                              Vector_<Date_> nodeDates,
                              const Vector_<T_>& zeroRates,
                              const DayBasis_& dayCount,
                              LogDfScheme_ scheme,
                              const Handle_<B_>& base = Handle_<B_>());

            T_ operator()(const Date_& from, const Date_& to) const override;
            [[nodiscard]] int NX() const override;
            void ApplyDX(Vector_<>::const_iterator dx, double leverage) override;
            void Write(Archive::Store_& dst) const override;
            [[nodiscard]] std::unique_ptr<YCComponent_> Clone(const String_& newName, const YCComponent_::substitutions_t& baseChanges) const override;

            [[nodiscard]] const Date_& AnchorDate() const { return anchorDate_; }
            [[nodiscard]] const Vector_<Date_>& NodeDates() const { return nodeDates_; }
            [[nodiscard]] Vector_<> NodeZeroRates() const;
            [[nodiscard]] const DayBasis_& DayCount() const { return dayCount_; }
            [[nodiscard]] LogDfScheme_ Scheme() const { return scheme_; }
        };
    } // namespace Tape

    using DiscountZeroRate_ = Tape::DiscountZeroRate_<double>;

    std::unique_ptr<DiscountCurve_> NewDiscountZeroRate(const String_& name,
                                        const String_& ccy,
                                        const Date_& anchorDate,
                                        const Vector_<Date_>& nodeDates,
                                        const Vector_<>& zeroRates,
                                        const DayBasis_& dayCount,
                                        LogDfScheme_ scheme,
                                        const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>());
} // namespace Dal
