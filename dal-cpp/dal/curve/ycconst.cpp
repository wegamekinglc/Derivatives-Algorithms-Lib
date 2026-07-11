//
// Created by wegam on 2026/5/9.
//

// Platform defines the default DAL container element types used by the curve headers.
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/curve/fittable.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {

    namespace {
        constexpr double DAYS_PER_YEAR = 365.0;

        class DiscountPWC_ : public CurveWithBase_<DiscountCurve_>, public FittableCurve_ {
            Vector_<Date_> knotDates_;
            Vector_<> fRight_;
            Vector_<> sofar_;

            [[nodiscard]] PiecewiseConstant_ Fwds() const { return PiecewiseConstant_(knotDates_, fRight_); }

            [[nodiscard]] double IntegralTo(const Date_& dt) const {
                const auto iGE = LowerBound(knotDates_, dt) - knotDates_.begin();
                if (iGE <= 0)
                    return -fRight_.front() * (knotDates_.front() - dt);
                if (iGE < knotDates_.size() && knotDates_[iGE] == dt)
                    return sofar_[iGE];
                const auto iLT = iGE - 1;
                const double elapsed = dt - knotDates_[iLT];
                return sofar_[iLT] + elapsed * fRight_[iLT];
            }

        public:
            DiscountPWC_(const String_& name,
                         const String_& ccy,
                         const PiecewiseConstant_& fwds,
                         const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>())
                : CurveWithBase_<DiscountCurve_>(name, ccy, base), knotDates_(fwds.knotDates_), fRight_(fwds.fRight_), sofar_(fwds.sofar_) {}

            double operator()(const Date_& from, const Date_& to) const override {
                const double integral = IntegralTo(to) - IntegralTo(from);
                return exp(-integral / DAYS_PER_YEAR) * (base_ ? (*base_)(from, to) : 1.0);
            }

            [[nodiscard]] int NX() const override { return static_cast<int>(knotDates_.size()); }

            void ApplyDX(Vector_<>::const_iterator dx, double leverage) override {
                for (auto& val : fRight_)
                    val += leverage * *dx++;
                PiecewiseConstant_ fwds(knotDates_, fRight_);
                sofar_ = fwds.sofar_;
            }

            void Write(Archive::Store_&) const override { THROW("DiscountPWC_ persistence is not supported"); }

            [[nodiscard]] DiscountPWC_* Clone(const String_& newName, const YCComponent_::substitutions_t& baseChanges) const override {
                return new DiscountPWC_(newName, this->ccy_.String(), Fwds(), NewBase(baseChanges));
            }
        };
    } // namespace

    DiscountCurve_* NewDiscountPWC(const String_& name, const String_& ccy, const PiecewiseConstant_& fwds, const Handle_<DiscountCurve_>& base) {
        return new DiscountPWC_(name, ccy, fwds, base);
    }

} // namespace Dal
