//
// Created by wegam on 2026/5/9.
//

// Platform defines the default DAL container element types used by the curve headers.
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <cmath>
#include <type_traits>

#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/tapediscount.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/storage/archive.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    #include <dal/auto/MG_DiscountPWC_v1_Write.inc>

    namespace Tape {
        template <class T_, class B_>
        DiscountPWC_<T_, B_>::DiscountPWC_(
            const String_& name, const String_& ccy, const Vector_<Date_>& knotDates, const Vector_<T_>& fRightT, const Handle_<B_>& base)
            : CurveWithBase_<DiscountCurve_<T_>, B_>(name, ccy, base), knotDates_(knotDates), fRightT_(fRightT), sofarT_(knotDates.size()) {
            REQUIRE(!knotDates_.empty(), "DiscountPWC_: knot dates must not be empty");
            REQUIRE(fRightT_.size() == knotDates_.size(), "DiscountPWC_: forward length must equal knot count");
            REQUIRE(IsMonotonic(knotDates_), "DiscountPWC_: knot dates must be strictly increasing");
            for (const auto& value : fRightT_)
                REQUIRE(std::isfinite(AAD::Value(value)), "DiscountPWC_: forward values must be finite");
            UpdateT();
        }

        template <class T_, class B_> void DiscountPWC_<T_, B_>::UpdateT() {
            if constexpr (std::is_same_v<T_, double>) {
                sofarT_ = PiecewiseConstant_(knotDates_, fRightT_).Sofar();
            } else {
                sofarT_.Fill(T_(0.0));
                for (int i = 1; i < static_cast<int>(knotDates_.size()); ++i) {
                    const double elapsed = knotDates_[i] - knotDates_[i - 1];
                    sofarT_[i] = sofarT_[i - 1] + elapsed * fRightT_[i - 1];
                }
            }
        }

        template <class T_, class B_> T_ DiscountPWC_<T_, B_>::IntegralTo(const Date_& date) const {
            const int iGE = static_cast<int>(LowerBound(knotDates_, date) - knotDates_.begin());
            if (iGE <= 0)
                return -fRightT_.front() * static_cast<double>(knotDates_.front() - date);
            if (iGE < static_cast<int>(knotDates_.size()) && knotDates_[iGE] == date)
                return sofarT_[iGE];
            const int iLT = iGE - 1;
            const double elapsed = date - knotDates_[iLT];
            return sofarT_[iLT] + elapsed * fRightT_[iLT];
        }

        template <class T_, class B_> T_ DiscountPWC_<T_, B_>::operator()(const Date_& from, const Date_& to) const {
            const T_ logDf = -(IntegralTo(to) - IntegralTo(from)) / DAYS_PER_YEAR;
            return DiscountFromLogDf(logDf, this->base_, from, to);
        }

        template <class T_, class B_> int DiscountPWC_<T_, B_>::NX() const { return static_cast<int>(knotDates_.size()); }

        template <class T_, class B_> void DiscountPWC_<T_, B_>::ApplyDX(Vector_<>::const_iterator dx, double leverage) {
            for (auto& value : fRightT_)
                value += leverage * *dx++;
            UpdateT();
        }

        template <class T_, class B_> void DiscountPWC_<T_, B_>::Write(Archive::Store_& dst) const {
            if constexpr (IsDoubleSerializable<T_, B_>()) {
                DiscountPWC_v1::XWrite(dst, this->name_, this->ccy_.String(), knotDates_, fRightT_, this->base_);
            } else {
                REQUIRE(false, "Tape::DiscountPWC_ is only serializable for <double, DiscountCurve_<double>>");
                static_cast<void>(dst);
            }
        }

        template <class T_, class B_>
        std::unique_ptr<YCComponent_> DiscountPWC_<T_, B_>::Clone(const String_& newName, const YCComponent_::substitutions_t& baseChanges) const {
            return std::make_unique<DiscountPWC_<T_, B_>>(newName, this->ccy_.String(), knotDates_, fRightT_, this->NewBase(baseChanges));
        }

        template class DiscountPWC_<double>;
        template class DiscountPWC_<AAD::Number_>;
        template class DiscountPWC_<AAD::Number_, DiscountCurve_<AAD::Number_>>;
    } // namespace Tape

    std::unique_ptr<DiscountCurve_> NewDiscountPWC(const String_& name,
                                                   const String_& ccy,
                                                   const PiecewiseConstant_& fwds,
                                                   const Handle_<DiscountCurve_>& base) {
        return std::make_unique<Tape::DiscountPWC_<double>>(name, ccy, fwds.knotDates_, fwds.fRight_, base);
    }

} // namespace Dal
