//
// Created by wegam on 2023/3/26.
//

#pragma once

#include <dal/storage/storable.hpp>
#include <dal/currency/currency.hpp>

namespace Dal {
    class CollateralType_;
    class PeriodLength_;
    class DiscountCurve_;

    class YieldCurve_ : public Storable_ {
    public:
        const Ccy_ ccy_;
        YieldCurve_(const String_ &name, const String_ &ccy);
        [[nodiscard]] virtual const DiscountCurve_ &Discount(const CollateralType_ &collateral) const = 0;
        [[nodiscard]] virtual double FwdLibor(const PeriodLength_ &tenor, const Date_ &fixing_date) const = 0;
    };
}
