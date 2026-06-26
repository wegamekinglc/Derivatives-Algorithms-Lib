//
// Created by dal-implementer on 2026/6/20.
//

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/curve/discount.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/archive.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace Tape {
        template <class T_, class B_>
        DiscountPWLF_<T_, B_>::DiscountPWLF_(const String_& name,
                                              const String_& ccy,
                                              const Vector_<Date_>& knotDates,
                                              const Vector_<T_>& fLeftT,
                                              const Vector_<T_>& fRightT,
                                              const Handle_<B_>& base)
            : CurveWithBase_<DiscountCurve_<T_>, B_>(name, ccy, base),
              knotDates_(knotDates),
              fLeftT_(fLeftT),
              fRightT_(fRightT),
              sofarT_(knotDates.size()),
              knotAbscissae_(knotDates.size(), 0.0) {
            sofarT_.Fill(T_(0.0));
            REQUIRE(!knotDates_.empty(), "DiscountPWLF_: knot dates must not be empty");
            REQUIRE(fLeftT_.size() == knotDates_.size(), "DiscountPWLF_: fLeft length must equal knot count");
            REQUIRE(fRightT_.size() == knotDates_.size(), "DiscountPWLF_: fRight length must equal knot count");
            // Knot abscissae are serial-day offsets from knot 0 (double, identical for any T_).
            // Mirrors the dt weights used by PiecewiseLinear_::Sofar / IntegralTo.
            for (int k = 0; k < static_cast<int>(knotDates_.size()); ++k)
                knotAbscissae_[k] = static_cast<double>(knotDates_[k] - knotDates_.front());
            UpdateT();
        }

        template <class T_, class B_>
        void DiscountPWLF_<T_, B_>::UpdateT() {
            // T_-typed running integral using fLeftT_[ii] + fRightT_[ii-1] segment indexing.
            // Abscissa weights (dt) are double; mean and sofarT_ are T_.
            const int n = static_cast<int>(knotDates_.size());
            if (static_cast<int>(sofarT_.size()) != n)
                sofarT_.Resize(n);
            sofarT_.Fill(T_(0.0));
            for (int ii = 1; ii < n; ++ii) {
                const double dt = knotAbscissae_[ii] - knotAbscissae_[ii - 1];
                const T_ mean = (fLeftT_[ii] + fRightT_[ii - 1]) * static_cast<double>(0.5);
                sofarT_[ii] = sofarT_[ii - 1] + static_cast<double>(dt) * mean;
            }
        }

        template <class T_, class B_>
        T_ DiscountPWLF_<T_, B_>::IntegralTo(double t) const {
            const int n = static_cast<int>(knotAbscissae_.size());
            const int iGE = static_cast<int>(std::lower_bound(knotAbscissae_.begin(), knotAbscissae_.end(), t) - knotAbscissae_.begin());
            if (iGE <= 0)
                return -fLeftT_.front() * static_cast<double>(knotAbscissae_.front() - t);
            if (iGE >= n)
                return sofarT_.back() + fRightT_.back() * static_cast<double>(t - knotAbscissae_.back());
            if (knotAbscissae_[iGE] == t)
                return sofarT_[iGE];
            return IntegralToInterior(t, iGE);
        }

        template <class T_, class B_>
        T_ DiscountPWLF_<T_, B_>::IntegralToInterior(double t, int iGE) const {
            const int iLT = iGE - 1;
            const double elapsed = t - knotAbscissae_[iLT];
            const double segWidth = knotAbscissae_[iGE] - knotAbscissae_[iLT];
            const double elapsedFrac = elapsed / segWidth;
            const T_ fStart = fRightT_[iLT];
            const T_ fStop = fStart + static_cast<double>(elapsedFrac) * (fLeftT_[iGE] - fStart);
            return sofarT_[iLT] + static_cast<double>(elapsed) * (fStart + fStop) * static_cast<double>(0.5);
        }

        template <class T_, class B_>
        T_ DiscountPWLF_<T_, B_>::operator()(const Date_& from, const Date_& to) const {
            const double fromT = static_cast<double>(from - knotDates_.front());
            const double toT = static_cast<double>(to - knotDates_.front());
            const T_ logDf = -(IntegralTo(toT) - IntegralTo(fromT)) / static_cast<double>(DAYS_PER_YEAR_PWLF);
            if constexpr (std::is_same_v<T_, double>) {
                const double baseFactor = this->base_ ? (*this->base_)(from, to) : 1.0;
                return std::exp(logDf) * baseFactor;
            } else {
                if (this->base_) {
                    const auto baseVal = (*this->base_)(from, to);
                    return Dal::AAD::exp(logDf) * baseVal;
                }
                return Dal::AAD::exp(logDf) * static_cast<double>(1.0);
            }
        }

        template <class T_, class B_>
        int DiscountPWLF_<T_, B_>::NX() const {
            // PWL_FWD has 2 params/knot with NO anchor exclusion (every knot free).
            return 2 * static_cast<int>(knotDates_.size());
        }

        template <class T_, class B_>
        void DiscountPWLF_<T_, B_>::ApplyDX(Vector_<>::const_iterator dx, double leverage) {
            // See docs/methodology/yield_curve_jacobian.md §Joint Multi-Curve Analytic Jacobian.
            for (int k = 0; k < static_cast<int>(fLeftT_.size()); ++k) {
                fLeftT_[k] += static_cast<double>(leverage) * *dx++;
                fRightT_[k] += static_cast<double>(leverage) * *dx++;
            }
            UpdateT();
        }

        template <class T_, class B_>
        void DiscountPWLF_<T_, B_>::Write(Archive::Store_& dst) const {
            // The Number_ specialization is never serialized (the AAD path constructs it only for
            // the duration of one Gradient sweep); this method exists to satisfy the base-class
            // virtual and is exercised only on the double specialization. Extract doubles via
            // Dal::AAD::Value so the template compiles on every T_.
            REQUIRE(false, "Tape::DiscountPWLF_<T_>::Write is not implemented -- the templated PWL-forward curve is non-storable; use the anonymous-namespace double DiscountPWLF_ at ycimp.cpp for storage");
            static_cast<void>(dst);
        }

        template <class T_, class B_>
        DiscountPWLF_<T_, B_>* DiscountPWLF_<T_, B_>::Clone(const String_& new_name,
                                                             const YCComponent_::substitutions_t& base_changes) const {
            return new DiscountPWLF_<T_, B_>(new_name, this->ccy_.String(), knotDates_, fLeftT_, fRightT_, this->NewBase(base_changes));
        }

        // See docs/methodology/yield_curve_jacobian.md §Joint Multi-Curve Analytic Jacobian.
        template class DiscountPWLF_<double>;
        template class DiscountPWLF_<Dal::AAD::Number_>;
        template class DiscountPWLF_<Dal::AAD::Number_, DiscountCurve_<Dal::AAD::Number_>>;
    } // namespace Tape
} // namespace Dal
