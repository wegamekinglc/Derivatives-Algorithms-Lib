#include <dal/platform/platform.hpp>

#include <set>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/curvescriptevaluation_internal.hpp>

namespace Dal {
    using namespace CalibrationInternal;
    namespace {
        void ProviderRoutes(const CurveCalibrationSpec_& geometry, const Vector_<CurveScriptRoute_>& routes) {
            Check(geometry.today_.IsValid(), "InvalidCalibrationPlan", "/curve/today", "valid explicit valuation date");
            Token(Ccy_(geometry.ccy_), "InvalidCalibrationPlan", "/curve/currency");
            Token(geometry.targetCollateral_, "InvalidCalibrationPlan", "/curve/target_collateral");
            if (!geometry.calibrateDiscountCurve_)
                Check(geometry.targetTenor_.Months() > 0, "InvalidCalibrationPlan", "/curve/target_tenor", "positive projection target tenor");
            std::set<std::string> ids;
            for (const auto& route : routes) {
                Check(!route.id_.empty() && ids.insert(Bytes(route.id_)).second, "InvalidCalibrationPlan", "/routes/id", "unique explicit route");
                Token(route.collateral_, "InvalidCalibrationPlan", "/routes/collateral");
                if (route.projection_)
                    Check(route.tenor_.Months() > 0, "InvalidCalibrationPlan", "/routes/tenor", "explicit positive projection tenor");
            }
            if (geometry.baseCurve_)
                Check(geometry.baseCurve_->ccy_ == Ccy_(geometry.ccy_), "CalibrationContextMismatch", "/curve/base", "same-currency frozen base");
        }
        bool IsTarget(const CurveCalibrationSpec_& geometry, const CurveScriptRoute_& route) {
            return route.projection_ ? !geometry.calibrateDiscountCurve_ && route.tenor_ == geometry.targetTenor_
                                     : geometry.calibrateDiscountCurve_ && route.collateral_ == geometry.targetCollateral_;
        }
        template <class K_> Handle_<DiscountCurve_> BoundCurve(const std::map<K_, Handle_<DiscountCurve_>>& curves, const K_& key) {
            const auto found = curves.find(key);
            return found == curves.end() ? Handle_<DiscountCurve_>() : found->second;
        }
    } // namespace

    template <class T_>
    CandidateCurveProvider_<T_>::CandidateCurveProvider_(const CurveCalibrationSpec_& geometry,
                                                         const Vector_<CurveScriptRoute_>& routes,
                                                         const Vector_<T_>& parameters)
        : geometry_(geometry), routes_(routes) {
        ProviderRoutes(geometry, routes);
        const auto definition = MakeCurveDefinition(geometry.curveName_, geometry.ccy_, geometry.parameterization_, geometry.logDfScheme_,
                                                    geometry.knotDates_, geometry.today_, geometry.liborBasis_);
        for (size_t j = 0; j < parameters.size(); ++j)
            if (!std::isfinite(static_cast<double>(parameters[j]))) {
                const auto labels = DescribeCurveFreeParameters(definition);
                CalibrationDiagnostic_ d;
                if (j < labels.size())
                    d.parameterKey_ = String_(labels[j].component_.String()) + ":" + Date::ToString(labels[j].date_);
                Fail("CalibrationNumericalFailure", "/candidate/parameters/" + OrdinalText(j), "finite native parameter",
                     NumberText(static_cast<double>(parameters[j])), d);
            }
        target_ = BuildDiscountCurveT<T_>(definition, parameters, geometry.baseCurve_);
    }
    template <class T_> const CurveScriptRoute_& CandidateCurveProvider_<T_>::Route(const String_& id, bool projection) const {
        const auto found = std::find_if(routes_.begin(), routes_.end(), [&](const auto& route) { return Bytes(route.id_) == Bytes(id); });
        Check(found != routes_.end(), "CalibrationContextMismatch", "/routes/" + id, "explicitly bound curve route");
        Check(found->projection_ == projection, "CalibrationContextMismatch", "/routes/" + id, "matching discount/projection role");
        if (projection)
            Check(found->collateral_ == geometry_.targetCollateral_, "CalibrationContextMismatch", "/routes/" + id,
                  "projection collateral matching the native market binding");
        return *found;
    }
    template <class T_> typename CandidateCurveProvider_<T_>::CurveBinding_ CandidateCurveProvider_<T_>::Bind(const CurveScriptRoute_& route) const {
        if (IsTarget(geometry_, route))
            return {target_.get(), nullptr};
        const auto background =
            route.projection_ ? BoundCurve(geometry_.forwardCurves_, route.tenor_) : BoundCurve(geometry_.discountCurves_, route.collateral_);
        Check(background && background->ccy_ == Ccy_(geometry_.ccy_), "CalibrationContextMismatch", "/routes/" + route.id_,
              "explicit same-currency background curve; no market fallback");
        return {nullptr, background.get()};
    }
    template <class T_> T_ CandidateCurveProvider_<T_>::Discount(const CurveScriptRoute_& route, const Date_& from, const Date_& to) const {
        return Discount(Bind(route), route, from, to);
    }
    template <class T_> T_ CandidateCurveProvider_<T_>::Discount(const String_& route, const Date_& from, const Date_& to) const {
        return Discount(Route(route, false), from, to);
    }
    template <class T_>
    T_ CandidateCurveProvider_<T_>::Forward(const String_& route, const Date_& start, const Date_& end, const DayBasis_& basis) const {
        const auto& boundRoute = Route(route, true);
        return Forward(Bind(boundRoute), boundRoute, start, end, basis);
    }
    template class CandidateCurveProvider_<double>;
    template class CandidateCurveProvider_<AAD::Number_>;
} // namespace Dal
