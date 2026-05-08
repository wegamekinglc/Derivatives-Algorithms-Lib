#include <algorithm>
#include <cmath>
#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/optimization/underdeterminedutils.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/dictionary.hpp>

namespace Dal {

    namespace {
        constexpr const char* KEY_MAX_EVALUATIONS = "MAXEVALUATIONS";
        constexpr const char* KEY_MAX_RESTARTS = "MAXRESTARTS";

        Vector_<Handle_<YCInstrument_>> OrderInstruments(const Vector_<Handle_<YCInstrument_>>& instruments) {
            auto ordered = instruments;
            std::sort(ordered.begin(),
                      ordered.end(),
                      [](const Handle_<YCInstrument_>& lhs, const Handle_<YCInstrument_>& rhs) {
                          const auto lhsSpan = lhs->TimeSpan();
                          const auto rhsSpan = rhs->TimeSpan();
                          if (lhsSpan.second != rhsSpan.second)
                              return lhsSpan.second < rhsSpan.second;
                          if (lhsSpan.first != rhsSpan.first)
                              return lhsSpan.first < rhsSpan.first;
                          return lhs->Name() < rhs->Name();
                      });
            return ordered;
        }

        Vector_<Date_> UniqueSortedDates(const Vector_<Date_>& dates) {
            auto sorted = dates;
            std::sort(sorted.begin(), sorted.end());
            sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
            return sorted;
        }

        Vector_<Date_> InstrumentDates(const Date_& today, const Vector_<Handle_<YCInstrument_>>& instruments) {
            Vector_<Date_> retval;
            retval.reserve(instruments.size() * 2);
            for (const auto& inst : instruments) {
                const auto span = inst->TimeSpan();
                if (span.first > today)
                    retval.push_back(span.first);
                if (span.second > today)
                    retval.push_back(span.second);
            }
            return UniqueSortedDates(retval);
        }

        int ParamsPerKnot(CurveParameterization_ parameterization) {
            switch (parameterization) {
            case CurveParameterization_::PIECEWISE_LINEAR_FWD:
                return 2;
            case CurveParameterization_::PIECEWISE_CONSTANT_FWD:
                return 1;
            case CurveParameterization_::ZERO_RATE:
            case CurveParameterization_::LOG_DISCOUNT:
                REQUIRE(false, "Requested curve parameterization is not implemented");
                return 0;
            default:
                REQUIRE(false, "Unknown curve parameterization");
                return 0;
            }
        }

        std::unique_ptr<DiscountCurve_> BuildDiscountCurve(const String_& name,
                                                           const String_& ccy,
                                                           CurveParameterization_ parameterization,
                                                           const Vector_<Date_>& knotDates,
                                                           const Vector_<>& x) {
            switch (parameterization) {
            case CurveParameterization_::PIECEWISE_LINEAR_FWD: {
                Vector_<> fLeft(knotDates.size());
                Vector_<> fRight(knotDates.size());
                for (int i = 0; i < static_cast<int>(knotDates.size()); ++i) {
                    fLeft[i] = x[2 * i];
                    fRight[i] = x[2 * i + 1];
                }
                return std::unique_ptr<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knotDates, fLeft, fRight)));
            }
            case CurveParameterization_::PIECEWISE_CONSTANT_FWD:
                return std::unique_ptr<DiscountCurve_>(NewDiscountPWC(name, ccy, PiecewiseConstant_(knotDates, x)));
            case CurveParameterization_::ZERO_RATE:
            case CurveParameterization_::LOG_DISCOUNT:
                REQUIRE(false, "Requested curve parameterization is reserved for future implementation");
                return nullptr;
            default:
                REQUIRE(false, "Unknown curve parameterization");
                return nullptr;
            }
        }

        class YieldCurveCalibrationFunc_ : public Underdetermined::Function_ {
            String_ ccy_;
            CurveParameterization_ parameterization_;
            Vector_<Handle_<YCInstrument_>> instruments_;
            Vector_<Handle_<YCInstrument_::Rate_>> rates_;
            Vector_<> marketRates_;
            Vector_<Date_> knotDates_;

        public:
            YieldCurveCalibrationFunc_(const String_& ccy,
                                       CurveParameterization_ parameterization,
                                       const Vector_<Handle_<YCInstrument_>>& instruments,
                                       const Vector_<Date_>& knotDates)
                : ccy_(ccy), parameterization_(parameterization), instruments_(instruments), knotDates_(knotDates) {
                Handle_<YieldCurve_> dummyYC;
                rates_.reserve(instruments_.size());
                marketRates_.reserve(instruments_.size());
                for (const auto& inst : instruments_) {
                    rates_.push_back(inst->Precompute(inst, dummyYC));
                    marketRates_.push_back(inst->MarketRate());
                }
            }

            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
                auto dc = BuildDiscountCurve("calib", ccy_, parameterization_, knotDates_, x);
                CurveBlock_ yc(*dc);

                Vector_<> result(instruments_.size());
                for (int i = 0; i < static_cast<int>(instruments_.size()); ++i)
                    result[i] = (*rates_[i])(yc) - marketRates_[i];
                return result;
            }
        };

        Vector_<> ModelRates(const Vector_<Handle_<YCInstrument_>>& instruments, const YieldCurve_& curve) {
            Vector_<> modelRates(instruments.size());
            Handle_<YieldCurve_> dummyYC;
            for (int i = 0; i < static_cast<int>(instruments.size()); ++i) {
                auto rate = instruments[i]->Precompute(instruments[i], dummyYC);
                modelRates[i] = (*rate)(curve);
            }
            return modelRates;
        }

        CurveCalibrationDiagnostics_ BuildDiagnostics(const Vector_<Handle_<YCInstrument_>>& instruments,
                                                      const YieldCurve_& curve,
                                                      bool usedApproximateFit,
                                                      const Matrix_<>* effJacobianInverse) {
            CurveCalibrationDiagnostics_ retval;
            retval.usedApproximateFit_ = usedApproximateFit;
            retval.modelRates_ = ModelRates(instruments, curve);
            retval.instrumentNames_.reserve(instruments.size());
            retval.marketRates_.reserve(instruments.size());
            retval.residuals_.reserve(instruments.size());
            double sqResidual = 0.0;
            for (int i = 0; i < static_cast<int>(instruments.size()); ++i) {
                retval.instrumentNames_.push_back(instruments[i]->Name());
                retval.marketRates_.push_back(instruments[i]->MarketRate());
                const double residual = retval.modelRates_[i] - retval.marketRates_[i];
                retval.residuals_.push_back(residual);
                retval.maxAbsResidual_ = std::max(retval.maxAbsResidual_, std::fabs(residual));
                sqResidual += residual * residual;
            }
            retval.rmsResidual_ = retval.residuals_.empty() ? 0.0 : std::sqrt(sqResidual / retval.residuals_.size());
            if (effJacobianInverse)
                retval.effJacobianInverse_ = *effJacobianInverse;
            return retval;
        }
    } // namespace

    Sparse::TriDiagonal_* BuildCurveCalibrationWeights(const Vector_<Date_>& knotDates,
                                                       int paramsPerKnot,
                                                       double smoothingWeight) {
        REQUIRE(!knotDates.empty(), "Curve calibration weights require knot dates");
        REQUIRE(paramsPerKnot > 0, "Curve calibration weights require positive params-per-knot");
        REQUIRE(smoothingWeight > 0.0, "Curve calibration smoothing weight must be positive");
        Vector_<DateTime_> expandedKnots;
        expandedKnots.reserve(knotDates.size() * paramsPerKnot);
        for (const auto& knot : knotDates)
            for (int i = 0; i < paramsPerKnot; ++i)
                expandedKnots.push_back(DateTime_(knot));
        return Underdetermined::WeightsPWC(expandedKnots, smoothingWeight);
    }

    Vector_<Date_> BuildCurveCalibrationKnots(const Date_& today,
                                              const Vector_<Handle_<YCInstrument_>>& instruments,
                                              const Vector_<Date_>& inputKnots,
                                              CurveKnotPolicy_ policy) {
        const Vector_<Date_> instrumentKnots = InstrumentDates(today, instruments);
        switch (policy) {
        case CurveKnotPolicy_::INPUT:
            return UniqueSortedDates(inputKnots);
        case CurveKnotPolicy_::INSTRUMENTS:
            return instrumentKnots;
        case CurveKnotPolicy_::AUGMENTED:
            return UniqueSortedDates(Vector::Join(UniqueSortedDates(inputKnots), instrumentKnots));
        default:
            REQUIRE(false, "Unknown curve knot policy");
            return {};
        }
    }

    void ValidateCurveCalibrationSpec(const CurveCalibrationSpec_& spec) {
        REQUIRE(!spec.instruments_.empty(), "Curve calibration requires at least one instrument");
        REQUIRE(spec.smoothingWeight_ > 0.0, "Curve calibration smoothing weight must be positive");
        REQUIRE(spec.tolerance_ > 0.0, "Curve calibration tolerance must be positive");
        REQUIRE(spec.fitTolerance_ > 0.0, "Curve calibration fit tolerance must be positive");
        REQUIRE(spec.maxEvaluations_ > 0, "Curve calibration max evaluations must be positive");
        REQUIRE(spec.maxRestarts_ > 0, "Curve calibration max restarts must be positive");
        REQUIRE(std::isfinite(spec.initialGuess_), "Curve calibration initial guess must be finite");

        const Vector_<Date_> knotDates = BuildCurveCalibrationKnots(spec.today_, spec.instruments_, spec.knotDates_, spec.knotPolicy_);
        REQUIRE(!knotDates.empty(), "Curve calibration requires at least one knot date");
        REQUIRE(knotDates.front() > spec.today_, "Curve calibration knot dates must be after today");

        Date_ latestEnd = spec.today_;
        for (const auto& inst : spec.instruments_) {
            const auto span = inst->TimeSpan();
            REQUIRE(span.second > span.first || span.first == spec.today_, "Curve instrument maturity must follow its start date");
            REQUIRE(span.second > spec.today_, "Curve instrument maturity must be after today");
            REQUIRE(std::isfinite(inst->MarketRate()), "Curve instrument market rate must be finite");
            latestEnd = std::max(latestEnd, span.second);
        }
        REQUIRE(knotDates.back() >= latestEnd, "Curve calibration knots must span all instrument maturities");
        static_cast<void>(ParamsPerKnot(spec.parameterization_));
    }

    void ValidateNoArbitrage(const DiscountCurve_& curve, const Date_& today, const Vector_<Date_>& checkDates) {
        for (const auto& checkDate : checkDates) {
            REQUIRE(checkDate > today, "No-arbitrage checks require future dates");
            const double df = curve(today, checkDate);
            REQUIRE(std::isfinite(df), "Discount factor must be finite");
            REQUIRE(df > 0.0, "Discount factor must stay positive");
        }
    }

    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec) {
        ValidateCurveCalibrationSpec(spec);

        const Vector_<Handle_<YCInstrument_>> instruments = OrderInstruments(spec.instruments_);
        const Vector_<Date_> knotDates = BuildCurveCalibrationKnots(spec.today_, instruments, spec.knotDates_, spec.knotPolicy_);
        const int paramsPerKnot = ParamsPerKnot(spec.parameterization_);
        const int nParams = paramsPerKnot * static_cast<int>(knotDates.size());

        Vector_<> guess(nParams, spec.initialGuess_);
        Vector_<> tol(instruments.size(), spec.tolerance_);
        std::unique_ptr<Sparse::TriDiagonal_> weights(BuildCurveCalibrationWeights(knotDates, paramsPerKnot, spec.smoothingWeight_));

        Dictionary_ ctrlDict;
        ctrlDict.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(spec.maxEvaluations_)));
        ctrlDict.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(spec.maxRestarts_)));
        UnderdeterminedControls_ controls(ctrlDict);

        YieldCurveCalibrationFunc_ func(spec.ccy_, spec.parameterization_, instruments, knotDates);
        Vector_<> result;
        Matrix_<> effJacobianInverse;
        if (spec.solveMode_ == CurveSolveMode_::EXACT) {
            std::unique_ptr<Sparse::SymmetricDecomposition_> wDecomp(weights->DecomposeSymmetric());
            result = Underdetermined::Find(func, guess, tol, *wDecomp, controls, &effJacobianInverse);
        } else {
            result = Underdetermined::Approximate(func, guess, tol, spec.fitTolerance_, *weights, controls);
        }

        CurveCalibrationResult_ retval;
        retval.curve_ = BuildDiscountCurve("calibrated", spec.ccy_, spec.parameterization_, knotDates, result);
        ValidateNoArbitrage(*retval.curve_, spec.today_, knotDates);
        CurveBlock_ curveView(*retval.curve_);
        retval.diagnostics_ = BuildDiagnostics(instruments,
                                               curveView,
                                               spec.solveMode_ == CurveSolveMode_::APPROXIMATE,
                                               spec.solveMode_ == CurveSolveMode_::EXACT ? &effJacobianInverse : nullptr);
        return retval;
    }

} // namespace Dal
