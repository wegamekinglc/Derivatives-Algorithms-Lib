//
// Created by wegam on 2023/3/26.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/archive.hpp>

/*IF--------------------------------------------------------------------------
storable DiscountPWLF
   Discount curve based on piecewise linear forward rates
version 1
manual
&members
name is ?string
ccy is ?string
knotDates is date[]
leftVals is number[]
rightVals is number[]
base is ?handle DiscountCurve
-IF-------------------------------------------------------------------------*/

namespace Dal {

    namespace {
        double LiborForecastFromDiscounts(const DiscountCurve_ &dc,
                                          const Date_ &fixDate,
                                          int tenor_months,
                                          int tenor_weeks,
                                          const DayBasis_ &daycount) {
            auto end = fixDate.AddDays((365 * tenor_months) / 12 + 7 * tenor_weeks);
            const double df = dc(fixDate, end);
            return (1.0 / df - 1.0) / daycount(fixDate, end, nullptr);
        }
    }

    #include <dal/auto/MG_DiscountPWLF_v1_Write.inc>

    inline void ApplyDX_PWLF(PiecewiseLinear_ *pwl, Vector_<>::const_iterator dx, double leverage) {
        auto pl = pwl->fLeft_.begin(), pr = pwl->fRight_.begin();
        while (pl != pwl->fLeft_.end()) {
            *pl++ += leverage * *dx++;
            *pr++ += leverage * *dx++;
        }
        pwl->Update();
    }

    class DiscountPWLF_ : public CurveWithBase_<DiscountCurve_>, public FittableCurve_ {
        PiecewiseLinear_ fwds_;
    public:
        DiscountPWLF_(const String_ &name, const String_& ccy, const PiecewiseLinear_ &fwds,
                      const Handle_ <DiscountCurve_> &base = Handle_<DiscountCurve_>())
                      : CurveWithBase_<DiscountCurve_>(name, ccy, base), fwds_(fwds) {}

        double operator()(const Date_ &from, const Date_ &to) const override {
            const double integral = fwds_.IntegralTo(to) - fwds_.IntegralTo(from);
            return exp(-integral / 365.0) * (base_ ? (*base_)(from, to) : 1.0);
        }

        [[nodiscard]] int NX() const override {
            return static_cast<int>(2 * fwds_.knotDates_.size());
        }

        void ApplyDX(Vector_<>::const_iterator dx, double leverage) override {
            ApplyDX_PWLF(&fwds_, dx, leverage);
        }

        void Write(Archive::Store_ &dst) const override {
            DiscountPWLF_v1::XWrite(dst, name_, ccy_.String(), fwds_.knotDates_, fwds_.fLeft_, fwds_.fRight_, base_);
        }

        [[nodiscard]] DiscountPWLF_ *Clone(const String_ &newName, const substitutions_t &baseChanges) const override {
            return new DiscountPWLF_(newName, ccy_.String(), fwds_, NewBase(baseChanges));
        }
    };

    DiscountCurve_* NewDiscountPWLF(const String_ &name,
                                    const String_ &ccy,
                                    const PiecewiseLinear_& fwds,
                                    const Handle_ <DiscountCurve_>& base) {
        return new DiscountPWLF_(name, ccy, fwds, base);
    }
    #include <dal/auto/MG_DiscountPWLF_v1_Read.inc>

    Storable_ *DiscountPWLF_v1::Reader_::Build() const {
        return new DiscountPWLF_(name_, ccy_, PiecewiseLinear_(knotDates_, leftVals_, rightVals_), base_);
    }
} // namespace Dal