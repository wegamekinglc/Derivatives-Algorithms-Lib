#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/yccomponent.hpp>

namespace Dal {

    namespace {
        class DiscountPWC_ : public CurveWithBase_<DiscountCurve_>, public FittableCurve_ {
            Vector_<Date_> knotDates_;
            Vector_<> fRight_;

            [[nodiscard]] PiecewiseConstant_ Fwds() const { return PiecewiseConstant_(knotDates_, fRight_); }

        public:
            DiscountPWC_(const String_& name,
                         const String_& ccy,
                         const PiecewiseConstant_& fwds,
                         const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>())
                : CurveWithBase_<DiscountCurve_>(name, ccy, base), knotDates_(fwds.knotDates_), fRight_(fwds.fRight_) {}

            double operator()(const Date_& from, const Date_& to) const override {
                const PiecewiseConstant_ fwds = Fwds();
                const double integral = fwds.IntegralTo(to) - fwds.IntegralTo(from);
                return exp(-integral / 365.0) * (base_ ? (*base_)(from, to) : 1.0);
            }

            [[nodiscard]] int NX() const override { return static_cast<int>(knotDates_.size()); }

            void ApplyDX(Vector_<>::const_iterator dx, double leverage) override {
                for (auto& val : fRight_)
                    val += leverage * *dx++;
            }

            void Write(Archive::Store_&) const override { REQUIRE(false, "DiscountPWC_ is not serializable"); }

            [[nodiscard]] DiscountPWC_* Clone(const String_& new_name,
                                              const YCComponent_::substitutions_t& base_changes) const override {
                return new DiscountPWC_(new_name, this->ccy_.String(), Fwds(), NewBase(base_changes));
            }
        };
    } // namespace

    DiscountCurve_* NewDiscountPWC(const String_& name,
                                   const String_& ccy,
                                   const PiecewiseConstant_& fwds,
                                   const Handle_<DiscountCurve_>& base) {
        return new DiscountPWC_(name, ccy, fwds, base);
    }

} // namespace Dal
