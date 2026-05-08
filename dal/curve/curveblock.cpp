//
// Created by wegam on 2026/4/19.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/time/periodlength.hpp>

namespace Dal {

    namespace {
        Handle_<DiscountCurve_> MakeNonOwningHandle(const DiscountCurve_& curve) {
            return Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(&curve, [](const DiscountCurve_*) {}));
        }
    } // namespace

    CurveBlock_::CurveBlock_(const DiscountCurve_& dc)
        : CurveBlock_(MakeNonOwningHandle(dc)) {}

    CurveBlock_::CurveBlock_(const Handle_<DiscountCurve_>& dc, const DayBasis_& liborBasis)
        : CurveBlock_([&dc]() -> const DiscountCurve_& {
                          REQUIRE(dc, "CurveBlock_ requires a non-empty discount curve handle");
                          return *dc;
                      }(),
                      liborBasis) {}

    CurveBlock_::CurveBlock_(const DiscountCurve_& dc, const DayBasis_& liborBasis)
        : CurveBlock_(dc.name_,
                      dc.ccy_.String(),
                      {{CollateralType_(CollateralType_::Value_::OIS), MakeNonOwningHandle(dc)}},
                      {},
                      liborBasis) {}

    CurveBlock_::CurveBlock_(const String_& name,
                             const String_& ccy,
                             const std::map<CollateralType_, Handle_<DiscountCurve_>>& discountCurves,
                             const std::map<PeriodLength_, Handle_<DiscountCurve_>>& forwardCurves,
                             const DayBasis_& liborBasis)
        : YieldCurve_(name, ccy), discountCurves_(discountCurves), forwardCurves_(forwardCurves), liborBasis_(liborBasis) {
        REQUIRE(!discountCurves_.empty(), "CurveBlock_ requires at least one discount curve");
        for (const auto& [_, curve] : discountCurves_) {
            REQUIRE(curve, "CurveBlock_ discount curve handles must not be empty");
            REQUIRE(curve->ccy_ == ccy_, "CurveBlock_ discount curves must share the block currency");
        }
        for (const auto& [_, curve] : forwardCurves_) {
            REQUIRE(curve, "CurveBlock_ forward curve handles must not be empty");
            REQUIRE(curve->ccy_ == ccy_, "CurveBlock_ forward curves must share the block currency");
        }
    }

    const DiscountCurve_& CurveBlock_::Discount(const CollateralType_& collateral) const {
        const auto found = discountCurves_.find(collateral);
        if (found != discountCurves_.end())
            return *found->second;
        const auto ois = discountCurves_.find(CollateralType_(CollateralType_::Value_::OIS));
        REQUIRE(ois != discountCurves_.end(), "CurveBlock_ cannot route collateral without an OIS discount curve");
        return *ois->second;
    }

    double CurveBlock_::FwdLibor(const PeriodLength_& tenor, const Date_& fixing_date) const {
        const auto found = forwardCurves_.find(tenor);
        const DiscountCurve_& forecast = found == forwardCurves_.end()
                                             ? Discount(CollateralType_(CollateralType_::Value_::OIS))
                                             : *found->second;
        const Date_ maturity = Date::NominalMaturity(fixing_date, tenor, ccy_);
        REQUIRE(maturity > fixing_date, "FwdLibor requires fixing date before maturity");
        const double df = forecast(fixing_date, maturity);
        REQUIRE(df > 0.0, "FwdLibor requires positive forecast discount factor");
        return (1.0 / df - 1.0) / liborBasis_(fixing_date, maturity, nullptr);
    }

    void CurveBlock_::Write(Archive::Store_&) const {
        REQUIRE(false, "CurveBlock_ is not serializable");
    }

    DiscountCurve_* CalibrateYieldCurve(const Date_& today,
                                        const String_& ccy,
                                        const Vector_<Handle_<YCInstrument_>>& instruments,
                                        const Vector_<Date_>& knotDates,
                                        double smoothingWeight,
                                        double tolerance,
                                        int maxEvaluations,
                                        int maxRestarts,
                                        Matrix_<>* effJacobianInverse) {
        CurveCalibrationSpec_ spec;
        spec.today_ = today;
        spec.ccy_ = ccy;
        spec.instruments_ = instruments;
        spec.knotDates_ = knotDates;
        spec.smoothingWeight_ = smoothingWeight;
        spec.tolerance_ = tolerance;
        spec.maxEvaluations_ = maxEvaluations;
        spec.maxRestarts_ = maxRestarts;

        auto result = CalibrateYieldCurve(spec);
        if (effJacobianInverse)
            *effJacobianInverse = result.diagnostics_.effJacobianInverse_;
        return result.curve_.release();
    }

} // namespace Dal
