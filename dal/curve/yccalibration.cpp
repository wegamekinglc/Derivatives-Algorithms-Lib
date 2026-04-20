//
// Created by wegam on 2026/4/19.
//

#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/yccalibration.hpp>
#include <dal/curve/yc.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/utilities/dictionary.hpp>

namespace Dal {

    YCInstrument_::~YCInstrument_() = default;
    YCInstrument_::Rate_::~Rate_() = default;

    // CalibrationYieldCurve_

    CalibrationYieldCurve_::CalibrationYieldCurve_(const DiscountCurve_& dc)
        : YieldCurve_(String_("CalibYC"), String_("USD")), dc_(dc) {}

    const DiscountCurve_& CalibrationYieldCurve_::Discount(const CollateralType_&) const { return dc_; }
    double CalibrationYieldCurve_::FwdLibor(const PeriodLength_&, const Date_&) const { return 0.0; }
    void CalibrationYieldCurve_::Write(Archive::Store_&) const {}

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
    } // namespace

    // Deposit_

    Deposit_::Deposit_(const Date_& today, const Date_& maturity, double marketRate, const DayBasis_& basis)
        : today_(today), maturity_(maturity), marketRate_(marketRate), basis_(basis) {}

    Deposit_::~Deposit_() = default;

    String_ Deposit_::Name() const { return String_("Deposit"); }

    pair<Date_, Date_> Deposit_::TimeSpan() const { return {today_, maturity_}; }

    Handle_<YCInstrument_::Rate_> Deposit_::Precompute(const Handle_<YCInstrument_>&,
                                                                    const Handle_<YieldCurve_>&) const {
        return Handle_<Rate_>(new DepositRate_(today_, maturity_, basis_));
    }

    // Swap_

    Swap_::Swap_(const Date_& today, const Date_& maturity, double marketRate, int freqMonths, const DayBasis_& basis)
        : today_(today), maturity_(maturity), marketRate_(marketRate), freqMonths_(freqMonths), basis_(basis) {}

    Swap_::~Swap_() = default;

    String_ Swap_::Name() const { return String_("Swap"); }

    pair<Date_, Date_> Swap_::TimeSpan() const { return {today_, maturity_}; }

    Handle_<YCInstrument_::Rate_> Swap_::Precompute(const Handle_<YCInstrument_>&,
                                                                 const Handle_<YieldCurve_>&) const {
        return Handle_<Rate_>(new SwapRate_(today_, maturity_, freqMonths_, basis_));
    }

    // Free functions

    double DepositRate(const DiscountCurve_& dc, const Date_& today, const Date_& maturity, const DayBasis_& basis) {
        const double df = dc(today, maturity);
        return (1.0 / df - 1.0) / basis(today, maturity, nullptr);
    }

    double SwapRate(const DiscountCurve_& dc, const Date_& today, const Date_& maturity, int freqMonths, const DayBasis_& basis) {
        double annuity = 0.0;
        Date_ d = today;
        while (d < maturity) {
            Date_ next = Date::AddMonths(d, freqMonths);
            if (next > maturity)
                next = maturity;
            annuity += basis(d, next, nullptr) * dc(today, next);
            d = next;
        }
        return (1.0 - dc(today, maturity)) / annuity;
    }

    // Calibration

    namespace {
        Sparse::TriDiagonal_* BuildSmoothingWeights(int nParams, double tau) {
            auto* w = new Sparse::TriDiagonal_(nParams);
            for (int i = 0; i < nParams; ++i)
                w->Set(i, i, tau);
            for (int i = 0; i < nParams - 1; ++i) {
                w->Add(i, i, tau);
                w->Add(i + 1, i + 1, tau);
                w->Set(i, i + 1, -tau);
                w->Set(i + 1, i, -tau);
            }
            return w;
        }

        class YieldCurveCalibrationFunc_ : public Underdetermined::Function_ {
            Date_ today_;
            Vector_<Handle_<YCInstrument_>> instruments_;
            Vector_<Handle_<YCInstrument_::Rate_>> rates_;
            Vector_<double> marketRates_;
            Vector_<Date_> knotDates_;

        public:
            YieldCurveCalibrationFunc_(const Date_& today,
                                       const Vector_<Handle_<YCInstrument_>>& instruments,
                                       const Vector_<Date_>& knotDates)
                : today_(today), instruments_(instruments), knotDates_(knotDates) {
                Handle_<YieldCurve_> dummyYC;
                for (const auto& inst : instruments_) {
                    rates_.push_back(inst->Precompute(inst, dummyYC));
                    marketRates_.push_back(inst->MarketRate());
                }
            }

            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
                const int nKnots = static_cast<int>(knotDates_.size());
                Vector_<> fLeft(nKnots), fRight(nKnots);
                for (int i = 0; i < nKnots; ++i) {
                    fLeft[i] = x[2 * i];
                    fRight[i] = x[2 * i + 1];
                }

                PiecewiseLinear_ pwl(knotDates_, fLeft, fRight);
                std::unique_ptr<DiscountCurve_> dc(NewDiscountPWLF(String_("calib"), pwl));
                CalibrationYieldCurve_ yc(*dc);

                const int nInst = static_cast<int>(instruments_.size());
                Vector_<> result(nInst);
                for (int i = 0; i < nInst; ++i)
                    result[i] = (*rates_[i])(yc) - marketRates_[i];

                return result;
            }
        };
    } // namespace

    DiscountCurve_* CalibrateYieldCurve(const Date_& today,
                                         const Vector_<Handle_<YCInstrument_>>& instruments,
                                         const Vector_<Date_>& knotDates,
                                         double smoothingWeight,
                                         double tolerance,
                                         int maxEvaluations,
                                         int maxRestarts,
                                         Matrix_<>* effJacobianInverse) {
        const int nParams = 2 * static_cast<int>(knotDates.size());
        const int nInstruments = static_cast<int>(instruments.size());

        Vector_<> guess(nParams, 0.05);

        std::unique_ptr<Sparse::TriDiagonal_> weights(BuildSmoothingWeights(nParams, smoothingWeight));
        std::unique_ptr<Sparse::SymmetricDecomposition_> wDecomp(weights->DecomposeSymmetric());

        Vector_<> tol(nInstruments, tolerance);

        Dictionary_ ctrlDict;
        ctrlDict.Insert(String_("MAXEVALUATIONS"), Cell_(static_cast<double>(maxEvaluations)));
        ctrlDict.Insert(String_("MAXRESTARTS"), Cell_(static_cast<double>(maxRestarts)));
        UnderdeterminedControls_ controls(ctrlDict);

        YieldCurveCalibrationFunc_ func(today, instruments, knotDates);
        Vector_<> result = Underdetermined::Find(func, guess, tol, *wDecomp, controls, effJacobianInverse);

        const int nKnots = static_cast<int>(knotDates.size());
        Vector_<> fLeft(nKnots), fRight(nKnots);
        for (int i = 0; i < nKnots; ++i) {
            fLeft[i] = result[2 * i];
            fRight[i] = result[2 * i + 1];
        }

        PiecewiseLinear_ pwl(knotDates, fLeft, fRight);
        return NewDiscountPWLF(String_("calibrated"), pwl);
    }

} // namespace Dal
