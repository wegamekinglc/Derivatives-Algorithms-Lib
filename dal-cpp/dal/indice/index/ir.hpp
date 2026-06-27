//
// Created by wegam on 2022/1/28.
//

#pragma once

#include <utility>
#include <dal/indice/index.hpp>
#include <dal/math/cell.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/couponrate.hpp>
#include <dal/currency/currency.hpp>
#include <dal/utilities/functionals.hpp>

namespace Dal::Index {
    class IRForward_ : public Index_ {
    public:
        const Ccy_ ccy_;
        const Cell_ start_;

        explicit IRForward_(const Ccy_& ccy, const Cell_& start = Cell_()) : ccy_(ccy), start_(start) {}
        [[nodiscard]] Date_ StartDate(const DateTime_& fixingTime) const;
    };

    class Libor_ : public IRForward_ {
    public:
        const TradedRate_ tenor_;
        Libor_(const Ccy_& ccy, const TradedRate_& tenor, const Cell_& start = Cell_())
            : IRForward_(ccy, start), tenor_(tenor) {}

        [[nodiscard]] String_ Name() const override;
    };

    class Swap_ : public IRForward_ {
    public:
        const String_ tenor_;
        Swap_(const Ccy_& ccy, String_ tenor, const Cell_& start = Cell_())
            : IRForward_(ccy, start), tenor_(std::move(tenor)) {}

        [[nodiscard]] String_ Name() const override;
    };

    class DF_ : public Index_ {
    public:
        const Ccy_ ccy_;
        // maturity/start offsets are from the fixing date; see docs/methodology/yield_curve.md §"Multi-Curve Framework".
        const Cell_ maturity_;
        const CollateralType_ collateral_;
        Cell_ start_;

        DF_(const Ccy_& ccy,
            const Cell_& maturity,
            const Cell_* start = nullptr,
            const CollateralType_& coll = CollateralType_())
            : ccy_(ccy), maturity_(maturity), start_(Dereference(start, Cell_())) {}

        [[nodiscard]] String_ Name() const override;
        [[nodiscard]] Date_ Maturity(const DateTime_& event_time) const;
        [[nodiscard]] Date_ StartDate(const DateTime_& event_time) const;
    };
} // namespace Dal::Index
