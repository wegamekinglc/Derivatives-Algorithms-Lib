//
// Created by wegam on 2026/4/19.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/yc.hpp>
#include <dal/curve/discount.hpp>
#include <dal/protocol/collateraltype.hpp>

namespace Dal {

    namespace {
        class DepositRate_ : public YCInstrument_::Rate_ {
            Date_ today_;
            Date_ maturity_;
            DayBasis_ basis_;
        public:
            DepositRate_(const Date_& today, const Date_& maturity, const DayBasis_& basis)
                : today_(today), maturity_(maturity), basis_(basis) {}

            double operator()(const YieldCurve_& yc) const override {
                const auto& dc = yc.Discount(CollateralType_::Value_::OIS);
                double df = dc(today_, maturity_);
                return (1.0 / df - 1.0) / basis_(today_, maturity_, nullptr);
            }
        };

        class SwapRate_ : public YCInstrument_::Rate_ {
            Date_ today_;
            Date_ maturity_;
            int freqMonths_;
            DayBasis_ basis_;
        public:
            SwapRate_(const Date_& today, const Date_& maturity, int freqMonths, const DayBasis_& basis)
                : today_(today), maturity_(maturity), freqMonths_(freqMonths), basis_(basis) {}

            double operator()(const YieldCurve_& yc) const override {
                const auto& dc = yc.Discount(CollateralType_::Value_::OIS);
                double annuity = 0.0;
                Date_ d = today_;
                while (d < maturity_) {
                    Date_ next = Date::AddMonths(d, freqMonths_);
                    if (next > maturity_)
                        next = maturity_;
                    annuity += basis_(d, next, nullptr) * dc(today_, next);
                    d = next;
                }
                return (1.0 - dc(today_, maturity_)) / annuity;
            }
        };

        class STIRRate_ : public YCInstrument_::Rate_ {
            Date_ today_;
            Date_ start_;
            Date_ maturity_;
            DayBasis_ basis_;
        public:
            STIRRate_(const Date_& today, const Date_& start, const Date_& maturity, const DayBasis_& basis)
                : today_(today), start_(start), maturity_(maturity), basis_(basis) {}

            double operator()(const YieldCurve_& yc) const override {
                const auto& dc = yc.Discount(CollateralType_::Value_::OIS);
                double fwdDf = dc(today_, maturity_) / dc(today_, start_);
                return (1.0 / fwdDf - 1.0) / basis_(start_, maturity_, nullptr);
            }
        };
    } // namespace

    // Deposit_

    Deposit_::Deposit_(const Date_& today, const Date_& maturity, double marketRate, const DayBasis_& basis)
        : today_(today), maturity_(maturity), marketRate_(marketRate), basis_(basis) {}

    Deposit_::~Deposit_() = default;

    String_ Deposit_::Name() const { return "Deposit"; }

    pair<Date_, Date_> Deposit_::TimeSpan() const { return {today_, maturity_}; }

    Handle_<YCInstrument_::Rate_> Deposit_::Precompute(const Handle_<YCInstrument_>&,
                                                        const Handle_<YieldCurve_>&) const {
        return Handle_<Rate_>(new DepositRate_(today_, maturity_, basis_));
    }

    // Swap_

    Swap_::Swap_(const Date_& today, const Date_& maturity, double marketRate, int freqMonths, const DayBasis_& basis)
        : today_(today), maturity_(maturity), marketRate_(marketRate), freqMonths_(freqMonths), basis_(basis) {}

    Swap_::~Swap_() = default;

    String_ Swap_::Name() const { return "Swap"; }

    pair<Date_, Date_> Swap_::TimeSpan() const { return {today_, maturity_}; }

    Handle_<YCInstrument_::Rate_> Swap_::Precompute(const Handle_<YCInstrument_>&,
                                                     const Handle_<YieldCurve_>&) const {
        return Handle_<Rate_>(new SwapRate_(today_, maturity_, freqMonths_, basis_));
    }

    // STIR_

    STIR_::STIR_(const Date_& today,
                 const Date_& start,
                 const Date_& maturity,
                 double marketRate,
                 const DayBasis_& basis)
        : today_(today), start_(start), maturity_(maturity), marketRate_(marketRate), basis_(basis) {}

    STIR_::~STIR_() = default;

    String_ STIR_::Name() const { return "STIR"; }

    pair<Date_, Date_> STIR_::TimeSpan() const { return {start_, maturity_}; }

    Handle_<YCInstrument_::Rate_> STIR_::Precompute(const Handle_<YCInstrument_>&,
                                                    const Handle_<YieldCurve_>&) const {
        return Handle_<Rate_>(new STIRRate_(today_, start_, maturity_, basis_));
    }

} // namespace Dal
