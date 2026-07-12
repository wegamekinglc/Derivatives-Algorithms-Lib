//
// Created by dal-implementer on 2026/7/12.
//

// Platform defines the default DAL container element types used by the curve headers.
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <cmath>
#include <type_traits>

#include <dal/curve/yczerorate.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/storage/archive.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace Tape {
        template <class T_, class B_>
        DiscountZeroRate_<T_, B_>::DiscountZeroRate_(const String_& name,
                                                     const String_& ccy,
                                                     const Date_& anchorDate,
                                                     const Vector_<Date_>& nodeDates,
                                                     const Vector_<T_>& zeroRates,
                                                     const DayBasis_& dayCount,
                                                     LogDfScheme_ scheme,
                                                     const Handle_<B_>& base)
            : CurveWithBase_<DiscountCurve_<T_>, B_>(name, ccy, base), anchorDate_(anchorDate), nodeDates_(nodeDates), dayCount_(dayCount),
              yearFractions_(nodeDates.size() + 1, 0.0), zeroRates_(zeroRates), scheme_(scheme) {
            REQUIRE(!nodeDates_.empty(), "zero-rate discount curve: need at least one future node");
            REQUIRE(nodeDates_.size() == zeroRates_.size(), "zero-rate discount curve: nodeDates and zeroRates must have equal length");
            REQUIRE(IsMonotonic(nodeDates_), "zero-rate discount curve: node dates must be strictly increasing");
            REQUIRE(nodeDates_.front() > anchorDate_, "zero-rate discount curve: every node date must be strictly after the anchor");

            if constexpr (std::is_same_v<T_, double>) {
                for (int i = 0; i < static_cast<int>(zeroRates_.size()); ++i)
                    REQUIRE(std::isfinite(zeroRates_[i]), String_("zero-rate discount curve: zeroRates[") + String::FromInt(i) + "] is not finite");
            }

            for (int i = 0; i < static_cast<int>(nodeDates_.size()); ++i) {
                const double yearFraction = dayCount_(anchorDate_, nodeDates_[i], nullptr);
                REQUIRE(std::isfinite(yearFraction), "zero-rate discount curve: year-fraction geometry must be finite");
                REQUIRE(yearFraction > 0.0, "zero-rate discount curve: future-node year fractions must be positive");
                yearFractions_[i + 1] = yearFraction;
            }
            REQUIRE(IsMonotonic(yearFractions_), "zero-rate discount curve: year fractions must be strictly increasing");
            interpolation_ = std::make_unique<LogDfInterpolation_>(yearFractions_, scheme_);
        }

        template <class T_, class B_> T_ DiscountZeroRate_<T_, B_>::LogDfAt(double yearFraction) const {
            T_ result(0.0);
            for (const auto& [index, weight] : interpolation_->WeightsAt(yearFraction)) {
                if (index > 0)
                    result -= weight * zeroRates_[index - 1] * yearFractions_[index];
            }
            return result;
        }

        template <class T_, class B_> T_ DiscountZeroRate_<T_, B_>::operator()(const Date_& from, const Date_& to) const {
            const double yfFrom = dayCount_(anchorDate_, from, nullptr);
            const double yfTo = dayCount_(anchorDate_, to, nullptr);
            if constexpr (std::is_same_v<T_, double>) {
                const double logDfFrom = LogDfAt(yfFrom);
                const double logDfTo = LogDfAt(yfTo);
                return std::exp(logDfTo - logDfFrom) * (this->base_ ? (*this->base_)(from, to) : 1.0);
            } else {
                const T_ logDfFrom = LogDfAt(yfFrom);
                const T_ logDfTo = LogDfAt(yfTo);
                if (this->base_)
                    return AAD::exp(logDfTo - logDfFrom) * (*this->base_)(from, to);
                return AAD::exp(logDfTo - logDfFrom);
            }
        }

        template <class T_, class B_> int DiscountZeroRate_<T_, B_>::NX() const { return static_cast<int>(zeroRates_.size()); }

        template <class T_, class B_> void DiscountZeroRate_<T_, B_>::ApplyDX(Vector_<>::const_iterator dx, double leverage) {
            for (auto& zeroRate : zeroRates_)
                zeroRate += leverage * *dx++;
        }

        template <class T_, class B_> void DiscountZeroRate_<T_, B_>::Write(Archive::Store_& dst) const {
            static_cast<void>(dst);
            THROW("DiscountZeroRate_ persistence is not supported until DiscountZeroRate_v1 is generated");
        }

        template <class T_, class B_>
        DiscountZeroRate_<T_, B_>* DiscountZeroRate_<T_, B_>::Clone(const String_& newName, const YCComponent_::substitutions_t& baseChanges) const {
            return new DiscountZeroRate_<T_, B_>(newName, this->ccy_.String(), anchorDate_, nodeDates_, zeroRates_, dayCount_, scheme_,
                                                 this->NewBase(baseChanges));
        }

        template <class T_, class B_> Vector_<> DiscountZeroRate_<T_, B_>::NodeZeroRates() const {
            Vector_<> result(zeroRates_.size());
            for (int i = 0; i < static_cast<int>(zeroRates_.size()); ++i)
                result[i] = AAD::Value(zeroRates_[i]);
            return result;
        }

        template class DiscountZeroRate_<double>;
        template class DiscountZeroRate_<AAD::Number_>;
        template class DiscountZeroRate_<AAD::Number_, DiscountCurve_<AAD::Number_>>;
    } // namespace Tape

    DiscountCurve_* NewDiscountZeroRate(const String_& name,
                                        const String_& ccy,
                                        const Date_& anchorDate,
                                        const Vector_<Date_>& nodeDates,
                                        const Vector_<>& zeroRates,
                                        const DayBasis_& dayCount,
                                        LogDfScheme_ scheme,
                                        const Handle_<DiscountCurve_>& base) {
        return new Tape::DiscountZeroRate_<double>(name, ccy, anchorDate, nodeDates, zeroRates, dayCount, scheme, base);
    }
} // namespace Dal
