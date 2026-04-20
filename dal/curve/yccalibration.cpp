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
    // CalibratedYieldCurve_

    CalibratedYieldCurve_::CalibratedYieldCurve_(const DiscountCurve_& dc)
        : YieldCurve_(dc.name_, dc.ccy_.String()), dc_(dc) {}

    const DiscountCurve_& CalibratedYieldCurve_::Discount(const CollateralType_&) const { return dc_; }

    double CalibratedYieldCurve_::FwdLibor(const PeriodLength_&, const Date_&) const {
        REQUIRE(false, "CalibratedYieldCurve_ does not support FwdLibor");
        return 0.0;
    }

    void CalibratedYieldCurve_::Write(Archive::Store_&) const {
        REQUIRE(false, "CalibratedYieldCurve_ is not serializable");
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
            Ccy_ ccy_;
            Vector_<Handle_<YCInstrument_>> instruments_;
            Vector_<Handle_<YCInstrument_::Rate_>> rates_;
            Vector_<double> marketRates_;
            Vector_<Date_> knotDates_;

        public:
            YieldCurveCalibrationFunc_(const Date_& today,
                                       const String_& ccy,
                                       const Vector_<Handle_<YCInstrument_>>& instruments,
                                       const Vector_<Date_>& knotDates)
                : today_(today), ccy_(ccy), instruments_(instruments), knotDates_(knotDates) {
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
                std::unique_ptr<DiscountCurve_> dc(NewDiscountPWLF(String_("calib"), ccy_.String(), pwl));
                CalibratedYieldCurve_ yc(*dc);

                const int nInst = static_cast<int>(instruments_.size());
                Vector_<> result(nInst);
                for (int i = 0; i < nInst; ++i)
                    result[i] = (*rates_[i])(yc) - marketRates_[i];

                return result;
            }
        };
    } // namespace

    DiscountCurve_* CalibrateYieldCurve(const Date_& today,
                                        const String_& ccy,
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

        YieldCurveCalibrationFunc_ func(today, ccy, instruments, knotDates);
        Vector_<> result = Underdetermined::Find(func, guess, tol, *wDecomp, controls, effJacobianInverse);

        const int nKnots = static_cast<int>(knotDates.size());
        Vector_<> fLeft(nKnots), fRight(nKnots);
        for (int i = 0; i < nKnots; ++i) {
            fLeft[i] = result[2 * i];
            fRight[i] = result[2 * i + 1];
        }

        PiecewiseLinear_ pwl(knotDates, fLeft, fRight);
        return NewDiscountPWLF(String_("calibrated"), ccy, pwl);
    }

} // namespace Dal
