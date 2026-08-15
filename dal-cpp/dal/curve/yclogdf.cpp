//
// Created by dal-implementer on 2026/6/14.
//

#include <cmath>
#include <dal/curve/discount.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/tapediscount.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/storage/archive.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>
#include <type_traits>

/*IF--------------------------------------------------------------------------
storable DiscountLogDF
   Discount curve on explicit (node date, log DF) pairs with a pluggable interpolation rule
version 2
manual
&members
name is ?string
ccy is ?string
nodeDates is date[]
logDF is number[]
dayCount is string
scheme is string
base is ?handle DiscountCurve
-IF-------------------------------------------------------------------------*/

namespace Dal {

#include <dal/auto/MG_DiscountLogDF_v2_Write.inc>

    namespace Tape {

        // Runtime validation (isfinite, logDF[0]==0) is double-only -- Number_ value extraction
        // would record spurious tape nodes. The Number_ factory pins logDF[0]=0 before construction.
        template <class T_, class B_>
        DiscountLogDF_<T_, B_>::DiscountLogDF_(const String_& name,
                                               const String_& ccy,
                                               const Vector_<Date_>& nodeDates,
                                               const Vector_<T_>& logDF,
                                               const DayBasis_& dayCount,
                                               LogDfScheme_ scheme,
                                               const Handle_<B_>& base)
            : CurveWithBase_<DiscountCurve_<T_>, B_>(name, ccy, base), nodeDates_(nodeDates), dayCount_(dayCount), yf_(nodeDates.size()),
              logDF_(logDF), scheme_(scheme) {
            REQUIRE(nodeDates_.size() == logDF_.size(), "log-DF discount curve: nodeDates and logDF must have equal length");
            REQUIRE(nodeDates_.size() >= 2, "log-DF discount curve: need at least 2 nodes (anchor + one free)");
            REQUIRE(IsMonotonic(nodeDates_), "log-DF discount curve: node dates must be strictly increasing");
            if constexpr (std::is_same_v<T_, double>) {
                REQUIRE(!logDF_.empty() && std::abs(logDF_[0]) < 1e-15, "log-DF discount curve: anchor node (index 0) must be pinned at logDF = 0");
                for (int i = 0; i < static_cast<int>(nodeDates_.size()); ++i)
                    REQUIRE(std::isfinite(logDF_[i]), String_("log-DF discount curve: logDF[") + String::FromInt(i) + "] is not finite");
            }
            const Date_& anchor = nodeDates_.front();
            for (int i = 0; i < static_cast<int>(nodeDates_.size()); ++i)
                yf_[i] = dayCount_(anchor, nodeDates_[i], nullptr);
            REQUIRE(IsMonotonic(yf_), "log-DF discount curve: year-fractions must be strictly increasing");
            interpolation_ = std::make_unique<LogDfInterpolation_>(yf_, scheme_);
        }

        template <class T_, class B_> T_ DiscountLogDF_<T_, B_>::operator()(const Date_& from, const Date_& to) const {
            const double yfFrom = dayCount_(nodeDates_.front(), from, nullptr);
            const double yfTo = dayCount_(nodeDates_.front(), to, nullptr);
            const T_ logDfFrom = LogDfAt(yfFrom);
            const T_ logDfTo = LogDfAt(yfTo);
            const T_ logDf = logDfTo - logDfFrom;
            return DiscountFromLogDf(logDf, this->base_, from, to);
        }

        template <class T_, class B_> T_ DiscountLogDF_<T_, B_>::LogDfAt(double yf) const { return interpolation_->Evaluate(logDF_, yf); }

        // Free-node count: anchor (node 0) is pinned, so solver dimension = n-1.
        template <class T_, class B_> int DiscountLogDF_<T_, B_>::NX() const { return static_cast<int>(logDF_.size()) - 1; }

        template <class T_, class B_> void DiscountLogDF_<T_, B_>::ApplyDX(Vector_<>::const_iterator dx, double leverage) {
            for (int i = 1; i < static_cast<int>(logDF_.size()); ++i)
                logDF_[i] += leverage * *dx++;
        }

        template <class T_, class B_> void DiscountLogDF_<T_, B_>::Write(Archive::Store_& dst) const {
            if constexpr (IsDoubleSerializable<T_, B_>()) {
                Vector_<> logDFDouble(logDF_.size());
                for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
                    logDFDouble[i] = Dal::AAD::Value(logDF_[i]);
                DiscountLogDF_v2::XWrite(dst, this->Name(), this->ccy_.String(), nodeDates_, logDFDouble, dayCount_.String(), scheme_.String(),
                                         this->base_);
            } else {
                REQUIRE(false, "Tape::DiscountLogDF_ is only serializable for <double, DiscountCurve_<double>>");
                static_cast<void>(dst);
            }
        }

        template <class T_, class B_>
        std::unique_ptr<YCComponent_> DiscountLogDF_<T_, B_>::Clone(const String_& newName, const YCComponent_::substitutions_t& baseChanges) const {
            return std::make_unique<DiscountLogDF_<T_, B_>>(
                newName, this->ccy_.String(), nodeDates_, logDF_, dayCount_, scheme_, this->NewBase(baseChanges));
        }

        template <class T_, class B_> Vector_<> DiscountLogDF_<T_, B_>::NodeDF() const {
            Vector_<> retval(logDF_.size());
            for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
                retval[i] = std::exp(Dal::AAD::Value(logDF_[i]));
            return retval;
        }

        template <class T_, class B_> Vector_<> DiscountLogDF_<T_, B_>::NodeLogDF() const {
            Vector_<> retval(logDF_.size());
            for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
                retval[i] = Dal::AAD::Value(logDF_[i]);
            return retval;
        }

        template class DiscountLogDF_<double>;
        template class DiscountLogDF_<Dal::AAD::Number_>;
        template class DiscountLogDF_<Dal::AAD::Number_, DiscountCurve_<Dal::AAD::Number_>>;

    } // namespace Tape

    std::unique_ptr<DiscountCurve_> NewDiscountLogDF(const String_& name,
                                                      const String_& ccy,
                                                      const Vector_<Date_>& nodeDates,
                                                      const Vector_<>& logDF,
                                                      const DayBasis_& dayCount,
                                                      LogDfScheme_ scheme,
                                                      const Handle_<DiscountCurve_>& base) {
        return std::make_unique<Tape::DiscountLogDF_<double>>(name, ccy, nodeDates, logDF, dayCount, scheme, base);
    }

#include <dal/auto/MG_DiscountLogDF_v1_Read.inc>
#include <dal/auto/MG_DiscountLogDF_v2_Read.inc>

    Storable_* DiscountLogDF_v1::Reader_::Build() const {
        // Legacy v1 stored a built Interp1_ handle and did NOT persist LogDfScheme_.
        // The scheme cannot be recovered from the handle (all interp subtypes report
        // type "Interp1"), so v1 always reconstructs as LOG_LINEAR.
        // v2 (the canonical format) carries the scheme by name.
        return new Tape::DiscountLogDF_<double>(name_, ccy_, nodeDates_, logDF_, DayBasis_(dayCount_), LogDfScheme_::Value_::LOG_LINEAR, base_);
    }

    Storable_* DiscountLogDF_v2::Reader_::Build() const {
        return new Tape::DiscountLogDF_<double>(name_, ccy_, nodeDates_, logDF_, DayBasis_(dayCount_), LogDfScheme_(scheme_), base_);
    }
} // namespace Dal
