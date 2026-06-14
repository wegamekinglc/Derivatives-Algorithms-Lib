//
// Created by dal-implementer on 2026/6/14.
//

#include <algorithm>
#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/curve/discount.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/archive.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
storable DiscountLogDF
   Discount curve on explicit (node date, log DF) pairs with a pluggable interpolation rule
version 1
manual
&members
name is ?string
ccy is ?string
nodeDates is date[]
logDF is number[]
dayCount is string
interp is handle Interp1
base is ?handle DiscountCurve
-IF-------------------------------------------------------------------------*/

namespace Dal {

#include <dal/auto/MG_DiscountLogDF_v1_Write.inc>

    DiscountLogDF_::DiscountLogDF_(const String_& name,
                                   const String_& ccy,
                                   const Vector_<Date_>& nodeDates,
                                   const Vector_<>& logDF,
                                   const DayBasis_& dayCount,
                                   const Handle_<Interp1_>& interp,
                                   const Handle_<DiscountCurve_>& base)
        : CurveWithBase_<DiscountCurve_>(name, ccy, base),
          nodeDates_(nodeDates),
          dayCount_(dayCount),
          yf_(nodeDates.size()),
          logDF_(logDF),
          interp_(interp) {
        REQUIRE(nodeDates_.size() == logDF_.size(),
                "log-DF discount curve: nodeDates and logDF must have equal length");
        REQUIRE(nodeDates_.size() >= 2,
                "log-DF discount curve: need at least 2 nodes (anchor + one free)");
        REQUIRE(IsMonotonic(nodeDates_), "log-DF discount curve: node dates must be strictly increasing");
        REQUIRE(interp_, "log-DF discount curve: interpolation handle is required");
        const Date_& anchor = nodeDates_.front();
        for (int i = 0; i < static_cast<int>(nodeDates_.size()); ++i) {
            yf_[i] = dayCount_(anchor, nodeDates_[i], nullptr);
            REQUIRE(std::isfinite(logDF_[i]),
                    String_("log-DF discount curve: logDF[") + String::FromInt(i) + "] is not finite");
        }
        REQUIRE(IsMonotonic(yf_), "log-DF discount curve: year-fractions must be strictly increasing");
    }

    double DiscountLogDF_::operator()(const Date_& from, const Date_& to) const {
        const double yfFrom = dayCount_(nodeDates_.front(), from, nullptr);
        const double yfTo = dayCount_(nodeDates_.front(), to, nullptr);
        const double logDfFrom = (*interp_)(yfFrom);
        const double logDfTo = (*interp_)(yfTo);
        return std::exp(logDfTo - logDfFrom) * (base_ ? (*base_)(from, to) : 1.0);
    }

    int DiscountLogDF_::NX() const { return static_cast<int>(logDF_.size()); }

    void DiscountLogDF_::ApplyDX(Vector_<>::const_iterator dx, double leverage) {
        for (auto& v : logDF_)
            v += leverage * *dx++;
    }

    void DiscountLogDF_::Write(Archive::Store_& dst) const {
        DiscountLogDF_v1::XWrite(dst,
                                 this->Name(),
                                 this->ccy_.String(),
                                 nodeDates_,
                                 logDF_,
                                 dayCount_.String(),
                                 interp_,
                                 base_);
    }

    DiscountLogDF_* DiscountLogDF_::Clone(const String_& new_name,
                                          const YCComponent_::substitutions_t& base_changes) const {
        return new DiscountLogDF_(new_name, this->ccy_.String(), nodeDates_, logDF_, dayCount_, interp_, NewBase(base_changes));
    }

    Vector_<> DiscountLogDF_::NodeDF() const {
        Vector_<> retval(logDF_.size());
        for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
            retval[i] = std::exp(logDF_[i]);
        return retval;
    }

    DiscountCurve_* NewDiscountLogDF(const String_& name,
                                     const String_& ccy,
                                     const Vector_<Date_>& nodeDates,
                                     const Vector_<>& logDF,
                                     const DayBasis_& dayCount,
                                     const Handle_<Interp1_>& interp,
                                     const Handle_<DiscountCurve_>& base) {
        return new DiscountLogDF_(name, ccy, nodeDates, logDF, dayCount, interp, base);
    }

#include <dal/auto/MG_DiscountLogDF_v1_Read.inc>

    Storable_* DiscountLogDF_v1::Reader_::Build() const {
        Handle_<Interp1_> interp = interp_;
        if (!interp) {
            Vector_<> yf(nodeDates_.size());
            const Date_& anchor = nodeDates_.front();
            const DayBasis_ basis(dayCount_);
            for (int i = 0; i < static_cast<int>(nodeDates_.size()); ++i)
                yf[i] = basis(anchor, nodeDates_[i], nullptr);
            interp.reset(Interp::NewLinear("logdf_default", yf, logDF_));
        }
        return new DiscountLogDF_(name_, ccy_, nodeDates_, logDF_, DayBasis_(dayCount_), interp, base_);
    }
} // namespace Dal
