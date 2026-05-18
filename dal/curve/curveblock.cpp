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
        const DiscountCurve_& CheckedCurve(const Handle_<DiscountCurve_>& curve) {
            REQUIRE(curve, "CurveBlock_ requires a non-empty discount curve handle");
            return *curve;
        }
    } // namespace

    CurveBlock_::CurveBlock_(const DiscountCurve_& dc)
        : YieldCurve_(dc.name_, dc.ccy_.String()), dc_(&dc), liborBasis_("ACT_365F") {}

    CurveBlock_::CurveBlock_(const Handle_<DiscountCurve_>& dc, const DayBasis_& liborBasis)
        : CurveBlock_(CheckedCurve(dc).name_,
                      CheckedCurve(dc).ccy_.String(),
                      {{CollateralType_(CollateralType_::Value_::OIS), dc}},
                      {},
                      liborBasis) {}

    CurveBlock_::CurveBlock_(const DiscountCurve_& dc, const DayBasis_& liborBasis)
        : YieldCurve_(dc.name_, dc.ccy_.String()), dc_(&dc), liborBasis_(liborBasis) {}

    CurveBlock_::CurveBlock_(const String_& name,
                             const String_& ccy,
                             const std::map<CollateralType_, Handle_<DiscountCurve_>>& discountCurves,
                             const std::map<PeriodLength_, Handle_<DiscountCurve_>>& forwardCurves,
                             const DayBasis_& liborBasis)
        : YieldCurve_(name, ccy), dc_(nullptr), discountCurves_(discountCurves), forwardCurves_(forwardCurves), liborBasis_(liborBasis) {
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

    bool CurveBlock_::HasDiscount(const CollateralType_& collateral) const {
        if (dc_)
            return true;
        return discountCurves_.find(collateral) != discountCurves_.end()
               || discountCurves_.find(CollateralType_(CollateralType_::Value_::OIS)) != discountCurves_.end();
    }

    bool CurveBlock_::HasForward(const PeriodLength_& tenor) const {
        if (dc_)
            return true;
        return forwardCurves_.find(tenor) != forwardCurves_.end();
    }

    const DiscountCurve_& CurveBlock_::Discount(const CollateralType_& collateral) const {
        if (dc_)
            return *dc_;
        const auto found = discountCurves_.find(collateral);
        if (found != discountCurves_.end())
            return *found->second;
        const auto ois = discountCurves_.find(CollateralType_(CollateralType_::Value_::OIS));
        REQUIRE(ois != discountCurves_.end(), "CurveBlock_ cannot route collateral without an OIS discount curve");
        return *ois->second;
    }

    const DiscountCurve_& CurveBlock_::Forward(const PeriodLength_& tenor, const CollateralType_& collateral) const {
        if (dc_)
            return *dc_;
        const auto found = forwardCurves_.find(tenor);
        if (found != forwardCurves_.end())
            return *found->second;
        return Discount(collateral);
    }

    double CurveBlock_::FwdLibor(const PeriodLength_& tenor, const Date_& fixing_date) const {
        const DiscountCurve_& forecast = Forward(tenor, CollateralType_(CollateralType_::Value_::OIS));
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
