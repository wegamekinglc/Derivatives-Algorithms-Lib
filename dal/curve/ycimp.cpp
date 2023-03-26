//
// Created by wegam on 2023/3/26.
//

#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/storage/archive.hpp>

/*IF--------------------------------------------------------------------------
storable DiscountPWLF
   Discount curve based on piecewise linear forward rates
version 1
manual
&members
name is ?string
knotDates is date[]
leftVals is number

rightVals is number[]
base is ?handle DiscountCurve
-IF-------------------------------------------------------------------------*/

namespace Dal {

    class FittableCurve_ {
    public:
        [[nodiscard]] virtual int NX() const = 0;
        [[nodiscard]] virtual void ApplyDX(Vector_<>::const_iterator dx, double leverage) = 0;
    };

    namespace {
        double LiborForecastFromDiscounts(const DiscountCurve_ &dc,
                                          const Date_ &fix_date,
                                          int tenor_months,
                                          int tenor_weeks,
                                          const DayBasis_ &daycount) {
            auto end = fix_date.AddDays((365 * tenor_months) / 12 + 7 * tenor_weeks);
            const double df = dc(fix_date, end);
            return (1.0 / df - 1.0) / daycount(fix_date, end, nullptr);
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
        DiscountPWLF_(const String_ &name, const PiecewiseLinear_ &fwds,
                      const Handle_ <DiscountCurve_> &base = Handle_<DiscountCurve_>())
                      : CurveWithBase_<DiscountCurve_>(name, base), fwds_(fwds) {}

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
            DiscountPWLF_v1::XWrite(dst, name_, fwds_.knotDates_, fwds_.fLeft_, fwds_.fRight_, base_);
        }

        DiscountPWLF_ *Clone(const String_ &new_name, const substitutions_t &base_changes) const override {
            return new DiscountPWLF_(new_name, fwds_, NewBase(base_changes));
        }
    };

    DiscountCurve_* NewDiscountPWLF(const String_ &name,
                                    const PiecewiseLinear_& fwds,
                                    const Handle_ <DiscountCurve_>& base) {
        return new DiscountPWLF_(name, fwds, base);
    }
    #include <dal/auto/MG_DiscountPWLF_v1_Read.inc>

    Storable_ *DiscountPWLF_v1::Reader_::Build() const {
        return new DiscountPWLF_(name_, PiecewiseLinear_(knotDates_, leftVals_, rightVals_), base_);
    }
}