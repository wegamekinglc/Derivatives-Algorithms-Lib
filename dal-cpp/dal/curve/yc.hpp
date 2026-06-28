//
// Created by wegam on 2023/3/26.
//

#pragma once

#include <dal/storage/storable.hpp>
#include <dal/currency/currency.hpp>

namespace Dal {
    class CollateralType_;
    class PeriodLength_;
    namespace Tape { template <class T_> class DiscountCurve_; }
    using DiscountCurve_ = Tape::DiscountCurve_<double>;

    class YieldCurve_ : public Storable_ {
    public:
        const Ccy_ ccy_;
        YieldCurve_(const String_ &name, const String_ &ccy);
        [[nodiscard]] virtual bool HasDiscount(const CollateralType_& collateral) const = 0;
        [[nodiscard]] virtual bool HasForward(const PeriodLength_& tenor) const = 0;
        [[nodiscard]] virtual const DiscountCurve_ &Discount(const CollateralType_ &collateral) const = 0;
        [[nodiscard]] virtual const DiscountCurve_& Forward(const PeriodLength_& tenor,
                                                            const CollateralType_& collateral) const = 0;
        [[nodiscard]] virtual double FwdLibor(const PeriodLength_ &tenor, const Date_ &fixingDate) const = 0;
    };
} // namespace Dal
