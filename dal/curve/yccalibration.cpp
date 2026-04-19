//
// Created by wegam on 2026/4/19.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/yccalibration.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/utilities/dictionary.hpp>
#include <memory>

namespace Dal {

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
            Vector_<DepositInstrument_> deposits_;
            Vector_<SwapInstrument_> swaps_;
            Vector_<Date_> knotDates_;
            DayBasis_ basis_;

        public:
            YieldCurveCalibrationFunc_(const Date_& today,
                                       const Vector_<DepositInstrument_>& deposits,
                                       const Vector_<SwapInstrument_>& swaps,
                                       const Vector_<Date_>& knotDates,
                                       const DayBasis_& basis)
                : today_(today), deposits_(deposits), swaps_(swaps), knotDates_(knotDates), basis_(basis) {}

            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
                const int nKnots = static_cast<int>(knotDates_.size());
                Vector_<> fLeft(nKnots), fRight(nKnots);
                for (int i = 0; i < nKnots; ++i) {
                    fLeft[i] = x[2 * i];
                    fRight[i] = x[2 * i + 1];
                }

                PiecewiseLinear_ pwl(knotDates_, fLeft, fRight);
                std::unique_ptr<DiscountCurve_> dc(NewDiscountPWLF(String_("calib"), pwl));

                const int nDeposits = static_cast<int>(deposits_.size());
                const int nSwaps = static_cast<int>(swaps_.size());
                Vector_<> result(nDeposits + nSwaps);

                for (int i = 0; i < nDeposits; ++i)
                    result[i] = DepositRate(*dc, today_, deposits_[i].maturity_, basis_) - deposits_[i].marketRate_;

                for (int i = 0; i < nSwaps; ++i)
                    result[nDeposits + i] = SwapRate(*dc, today_, swaps_[i].maturity_, swaps_[i].freqMonths_, basis_) - swaps_[i].marketRate_;

                return result;
            }
        };
    } // namespace

    DiscountCurve_* CalibrateYieldCurve(const Date_& today,
                                         const Vector_<DepositInstrument_>& deposits,
                                         const Vector_<SwapInstrument_>& swaps,
                                         const Vector_<Date_>& knotDates,
                                         const DayBasis_& basis,
                                         double smoothingWeight,
                                         double tolerance,
                                         int maxEvaluations,
                                         int maxRestarts,
                                         Matrix_<>* effJacobianInverse) {
        const int nParams = 2 * static_cast<int>(knotDates.size());
        const int nInstruments = static_cast<int>(deposits.size() + swaps.size());

        Vector_<> guess(nParams, 0.05);

        std::unique_ptr<Sparse::TriDiagonal_> weights(BuildSmoothingWeights(nParams, smoothingWeight));
        std::unique_ptr<Sparse::SymmetricDecomposition_> wDecomp(weights->DecomposeSymmetric());

        Vector_<> tol(nInstruments, tolerance);

        Dictionary_ ctrlDict;
        ctrlDict.Insert(String_("MAXEVALUATIONS"), Cell_(static_cast<double>(maxEvaluations)));
        ctrlDict.Insert(String_("MAXRESTARTS"), Cell_(static_cast<double>(maxRestarts)));
        UnderdeterminedControls_ controls(ctrlDict);

        YieldCurveCalibrationFunc_ func(today, deposits, swaps, knotDates, basis);
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
