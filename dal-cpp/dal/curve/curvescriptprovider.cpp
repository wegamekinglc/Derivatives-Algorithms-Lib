#include <dal/platform/platform.hpp>

#include <set>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/curveinstrumentcontext_internal.hpp>
#include <dal/curve/curvescriptevaluation_internal.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycpwlf.hpp>

namespace Dal {
    using namespace CalibrationInternal;
    namespace {
        void TargetRoute(const CurveCalibrationSpec_& geometry, const Vector_<CurveScriptRoute_>& routes, const String_& overnightTarget) {
            if (overnightTarget.empty()) {
                if (!geometry.calibrateDiscountCurve_)
                    Check(geometry.targetTenor_.Months() > 0, "InvalidCalibrationPlan", "/curve/target_tenor", "positive projection target tenor");
                return;
            }
            Check(!geometry.calibrateDiscountCurve_ && !geometry.targetTenor_.String(), "InvalidCalibrationPlan", "/curve/target_overnight_route",
                  "one explicit ON target without discount or tenor target");
            Check(std::any_of(routes.begin(), routes.end(),
                              [&](const auto& route) {
                                  return route.overnight_ && Bytes(route.id_) == Bytes(overnightTarget) &&
                                         route.collateral_ == geometry.targetCollateral_;
                              }),
                  "CalibrationContextMismatch", "/curve/target_overnight_route", "explicit matching ON route and collateral");
        }
        void RouteRole(const CurveScriptRoute_& route, const CurveCalibrationSpec_& geometry) {
            Token(route.collateral_, "InvalidCalibrationPlan", "/routes/collateral");
            Check(!route.overnight_ || (route.projection_ && !route.tenor_.String()), "InvalidCalibrationPlan", "/routes/overnight",
                  "explicit ON projection without a tenor");
            if (route.projection_ && !route.overnight_)
                Check(route.tenor_.Months() > 0, "InvalidCalibrationPlan", "/routes/tenor", "explicit positive projection tenor");
            if (route.useTargetCurve_)
                Check((!route.projection_ || route.overnight_) && route.collateral_ == geometry.targetCollateral_, "CalibrationContextMismatch",
                      "/routes/use_target_curve", "explicit discount or ON use with target collateral");
        }
        void ProviderRoutes(const CurveCalibrationSpec_& geometry, const Vector_<CurveScriptRoute_>& routes, const String_& overnightTarget = "") {
            Check(geometry.today_.IsValid(), "InvalidCalibrationPlan", "/curve/today", "valid explicit valuation date");
            Token(Ccy_(geometry.ccy_), "InvalidCalibrationPlan", "/curve/currency");
            Token(geometry.targetCollateral_, "InvalidCalibrationPlan", "/curve/target_collateral");
            TargetRoute(geometry, routes, overnightTarget);
            std::set<std::string> ids;
            for (const auto& route : routes) {
                Check(!route.id_.empty() && ids.insert(Bytes(route.id_)).second, "InvalidCalibrationPlan", "/routes/id", "unique explicit route");
                RouteRole(route, geometry);
            }
            if (geometry.baseCurve_)
                Check(geometry.baseCurve_->ccy_ == Ccy_(geometry.ccy_), "CalibrationContextMismatch", "/curve/base", "same-currency frozen base");
        }
        bool IsTarget(const CurveCalibrationSpec_& geometry, const CurveScriptRoute_& route, const String_& overnightTarget) {
            if (route.useTargetCurve_)
                return true;
            if (route.overnight_)
                return !geometry.calibrateDiscountCurve_ && Bytes(route.id_) == Bytes(overnightTarget);
            return route.projection_ ? overnightTarget.empty() && !geometry.calibrateDiscountCurve_ && route.tenor_ == geometry.targetTenor_
                                     : geometry.calibrateDiscountCurve_ && route.collateral_ == geometry.targetCollateral_;
        }
        template <class K_> Handle_<DiscountCurve_> BoundCurve(const std::map<K_, Handle_<DiscountCurve_>>& curves, const K_& key) {
            const auto found = curves.find(key);
            return found == curves.end() ? Handle_<DiscountCurve_>() : found->second;
        }
        Date_ DomainAnchor(const DiscountCurve_& curve, const Date_& today) {
            // Forward curves store knots, not an anchor; old knots do not invalidate live coverage.
            if (const auto* pwc = dynamic_cast<const Tape::DiscountPWC_<double>*>(&curve))
                return std::min(today, pwc->KnotDates().front());
            if (const auto* pwl = dynamic_cast<const Tape::DiscountPWLF_<double>*>(&curve))
                return std::min(today, pwl->KnotDates().front().AddDays(-1));
            return today;
        }
        std::pair<Date_, Date_> CurveDomain(const DiscountCurve_& curve, const Date_& today) try {
            const auto state = InspectCurveParameters(curve, DomainAnchor(curve, today));
            auto domain = std::make_pair(state.definition_.anchorDate_, state.definition_.nodeDates_.back());
            if (state.passiveBase_) {
                const auto base = CurveDomain(*state.passiveBase_, today);
                domain.first = std::max(domain.first, base.first);
                domain.second = std::min(domain.second, base.second);
            }
            return domain;
        } catch (const std::exception& error) {
            Fail("CalibrationContextMismatch", "/market/curve_domain", "native curve with explicit date coverage", String_(error.what()));
        }
        template <class K_>
        void CurveDomains(const std::map<K_, Handle_<DiscountCurve_>>& curves,
                          const Date_& today,
                          std::map<const DiscountCurve_*, std::pair<Date_, Date_>>* domains) {
            for (const auto& [key, curve] : curves) {
                Check(bool(curve), "InvalidCalibrationPlan", "/market", "nonnull background curve");
                domains->emplace(curve.get(), CurveDomain(*curve, today));
            }
        }
    } // namespace

    CandidateCurveGeometry_::CandidateCurveGeometry_(const CurveCalibrationSpec_& geometry, const Vector_<CurveScriptRoute_>& routes)
        : geometry_(geometry), routes_(routes) {
        Check(std::none_of(routes.begin(), routes.end(), [](const auto& route) { return route.overnight_ || route.useTargetCurve_; }),
              "InvalidCalibrationPlan", "/routes", "typed curve-instrument context for ON routes");
        ProviderRoutes(geometry_, routes_);
        definition_ = MakeCurveDefinition(geometry.curveName_, geometry.ccy_, geometry.parameterization_, geometry.logDfScheme_, geometry.knotDates_,
                                          geometry.today_, geometry.liborBasis_);
    }

    CandidateCurveGeometry_::CandidateCurveGeometry_(const CurveInstrumentCalibrationSpec_& original) {
        const auto spec = CurveInstrumentInternal::FreezeOvernightContext(original);
        geometry_ = CurveInstrumentInternal::NativeGeometry(spec);
        routes_ = spec.market_.routes_;
        targetOvernightRoute_ = spec.geometry_.targetOvernightRoute_;
        overnightCurves_ = spec.market_.overnightCurves_;
        ProviderRoutes(geometry_, routes_, targetOvernightRoute_);
        definition_ = MakeCurveDefinition(geometry_.curveName_, geometry_.ccy_, geometry_.parameterization_, geometry_.logDfScheme_,
                                          geometry_.knotDates_, geometry_.today_, geometry_.liborBasis_);
        identity_ = Fingerprint(CurveInstrumentInternal::ContextJson(spec, false));
        if (CurveInstrumentInternal::HasOvernightContext(spec)) {
            targetDomain_ = {geometry_.today_, geometry_.knotDates_.back()};
            if (geometry_.baseCurve_) {
                const auto base = CurveDomain(*geometry_.baseCurve_, geometry_.today_);
                targetDomain_->first = std::max(targetDomain_->first, base.first);
                targetDomain_->second = std::min(targetDomain_->second, base.second);
            }
            CurveDomains(geometry_.discountCurves_, geometry_.today_, &backgroundDomains_);
            CurveDomains(geometry_.forwardCurves_, geometry_.today_, &backgroundDomains_);
            CurveDomains(overnightCurves_, geometry_.today_, &backgroundDomains_);
        }
    }

    template <class T_>
    CandidateCurveProvider_<T_>::CandidateCurveProvider_(const CurveCalibrationSpec_& geometry,
                                                         const Vector_<CurveScriptRoute_>& routes,
                                                         const Vector_<T_>& parameters)
        : CandidateCurveProvider_(std::make_shared<const CandidateCurveGeometry_>(geometry, routes), parameters) {}

    template <class T_>
    CandidateCurveProvider_<T_>::CandidateCurveProvider_(std::shared_ptr<const CandidateCurveGeometry_> geometry, const Vector_<T_>& parameters)
        : geometry_(std::move(geometry)) {
        REQUIRE(geometry_, "Candidate curve geometry is required");
        const auto& definition = geometry_->definition_;
        for (size_t j = 0; j < parameters.size(); ++j)
            if (!std::isfinite(static_cast<double>(parameters[j]))) {
                const auto labels = DescribeCurveFreeParameters(definition);
                CalibrationDiagnostic_ d;
                if (j < labels.size())
                    d.parameterKey_ = String_(labels[j].component_.String()) + ":" + Date::ToString(labels[j].date_);
                Fail("CalibrationNumericalFailure", "/candidate/parameters/" + OrdinalText(j), "finite native parameter",
                     NumberText(static_cast<double>(parameters[j])), d);
            }
        target_ = BuildDiscountCurveT<T_>(definition, parameters, geometry_->geometry_.baseCurve_);
    }
    template <class T_> const CurveScriptRoute_& CandidateCurveProvider_<T_>::Route(const String_& id, bool projection) const {
        const auto& routes = geometry_->routes_;
        const auto found = std::find_if(routes.begin(), routes.end(), [&](const auto& route) {
            return route.id_.size() == id.size() && std::equal(route.id_.begin(), route.id_.end(), id.begin());
        });
        if (found == routes.end())
            Fail("CalibrationContextMismatch", "/routes/" + id, "explicitly bound curve route");
        Check(found->projection_ == projection, "CalibrationContextMismatch", "/routes/" + id, "matching discount/projection role");
        if (projection)
            Check(found->collateral_ == geometry_->geometry_.targetCollateral_, "CalibrationContextMismatch", "/routes/" + id,
                  "projection collateral matching the native market binding");
        return *found;
    }
    template <class T_> typename CandidateCurveProvider_<T_>::CurveBinding_ CandidateCurveProvider_<T_>::Bind(const CurveScriptRoute_& route) const {
        const auto& geometry = geometry_->geometry_;
        if (IsTarget(geometry, route, geometry_->targetOvernightRoute_))
            return {target_.get(), nullptr, geometry_->targetDomain_};
        const auto background = route.overnight_    ? BoundCurve(geometry_->overnightCurves_, Bytes(route.id_))
                                : route.projection_ ? BoundCurve(geometry.forwardCurves_, route.tenor_)
                                                    : BoundCurve(geometry.discountCurves_, route.collateral_);
        Check(background && background->ccy_ == Ccy_(geometry.ccy_), "CalibrationContextMismatch", "/routes/" + route.id_,
              "explicit same-currency background curve; no market fallback");
        const auto domain = geometry_->backgroundDomains_.find(background.get());
        return {nullptr, background.get(), domain == geometry_->backgroundDomains_.end() ? std::nullopt : std::optional(domain->second)};
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
