#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/compoundedois_internal.hpp>
#include <dal/curve/curveinstrumentcontext_internal.hpp>
#include <dal/storage/json.hpp>

namespace Dal::CurveInstrumentInternal {
    using namespace CalibrationInternal;
    namespace {
        constexpr const char* INPUT_ERROR = "InvalidScriptCurveQuoteRiskInput";
        Handle_<DiscountCurve_> CopyCurve(const Handle_<DiscountCurve_>& curve) {
            if (!curve)
                return {};
            const auto copy = handle_cast<DiscountCurve_>(JSON::ReadString(JSON::WriteString(*curve), false));
            Check(bool(copy), "InvalidCalibrationPlan", "/market", "owned native curve snapshot");
            return copy;
        }
    } // namespace

    bool HasOvernightContext(const CurveInstrumentCalibrationSpec_& spec) {
        return !spec.compoundedOis_.empty() || !spec.geometry_.targetOvernightRoute_.empty() || !spec.market_.overnightCurves_.empty() ||
               std::any_of(spec.market_.routes_.begin(), spec.market_.routes_.end(),
                           [](const auto& route) { return route.overnight_ || route.useTargetCurve_; });
    }
    CurveInstrumentCalibrationSpec_ FreezeOvernightContext(CurveInstrumentCalibrationSpec_ spec) {
        if (!HasOvernightContext(spec))
            return spec;
        spec.geometry_.baseCurve_ = CopyCurve(spec.geometry_.baseCurve_);
        for (auto& [key, curve] : spec.market_.discountCurves_)
            curve = CopyCurve(curve);
        for (auto& [key, curve] : spec.market_.forwardCurves_)
            curve = CopyCurve(curve);
        for (auto& [key, curve] : spec.market_.overnightCurves_)
            curve = CopyCurve(curve);
        if (spec.valuation_.fixings_)
            spec.valuation_.fixings_.reset(new MarketFixingSnapshot_(spec.valuation_.fixings_->Values()));
        for (auto& observation : spec.observations_) {
            Check(bool(observation), "InvalidCalibrationObservation", "/observations", "nonnull typed observation");
            observation = NewCalibrationObservation(observation->Id(), observation->Instrument(), observation->MeasureId(),
                                                    Handle_<CalibrationMarketQuote_>(new CalibrationMarketQuote_(*observation->Quote())),
                                                    observation->ErrorScale());
        }
        return spec;
    }

    Json_ CurveJson(const Handle_<DiscountCurve_>& curve) { return curve ? Parse(JSON::WriteString(*curve)) : Json_(); }
    Json_ Dates(const Vector_<Date_>& dates) {
        return Array(dates, [](const auto& date) { return Str(Date::ToString(date)); });
    }
    Json_ Numbers(const Vector_<>& values) {
        return Array(values, [](double value) { return Num(value); });
    }
    Json_ GeometryJson(const CurveInstrumentGeometry_& g) {
        Check(g.today_.IsValid(), INPUT_ERROR, "/context/geometry/today", "valid valuation date");
        auto result = Obj({{"today", Str(Date::ToString(g.today_))},
                           {"currency", Str(g.ccy_)},
                           {"curve_name", Str(g.curveName_)},
                           {"knot_dates", Dates(g.knotDates_)},
                           {"knot_policy", Str(Token(g.knotPolicy_, INPUT_ERROR, "/context/geometry/knot_policy"))},
                           {"parameterization", Str(Token(g.parameterization_, INPUT_ERROR, "/context/geometry/parameterization"))},
                           {"log_df_scheme", Str(Token(g.logDfScheme_, INPUT_ERROR, "/context/geometry/log_df_scheme"))},
                           {"base_curve", CurveJson(g.baseCurve_)},
                           {"discount_target", Bool(g.calibrateDiscountCurve_)},
                           {"target_collateral", Str(Token(g.targetCollateral_, INPUT_ERROR, "/context/geometry/target_collateral"))},
                           {"target_tenor", g.targetTenor_.String() ? Str(g.targetTenor_.String()) : Json_()},
                           {"libor_basis", Str(Token(g.liborBasis_, INPUT_ERROR, "/context/geometry/libor_basis"))}});
        if (!g.targetOvernightRoute_.empty())
            result.object_["target_overnight_route"] = Str(g.targetOvernightRoute_);
        return result;
    }
    Json_ MarketJson(const CurveInstrumentMarket_& market) {
        auto discounts = Json_::Array(), forwards = Json_::Array(), overnight = Json_::Array();
        for (const auto& [key, curve] : market.discountCurves_)
            discounts.array_.push_back(Obj({{"collateral", Str(Token(key, INPUT_ERROR, "/context/market/discounts"))}, {"curve", CurveJson(curve)}}));
        for (const auto& [key, curve] : market.forwardCurves_)
            forwards.array_.push_back(Obj({{"tenor", Str(Token(key, INPUT_ERROR, "/context/market/forwards"))}, {"curve", CurveJson(curve)}}));
        for (const auto& [key, curve] : market.overnightCurves_)
            overnight.array_.push_back(Obj({{"route", Str(String_(key))}, {"curve", CurveJson(curve)}}));
        auto result = Obj({{"discounts", discounts},
                           {"forwards", forwards},
                           {"routes", Array(market.routes_, [](const auto& route) {
                                auto row = Obj({{"id", Str(route.id_)},
                                                {"projection", Bool(route.projection_)},
                                                {"collateral", Str(Token(route.collateral_, INPUT_ERROR, "/context/market/routes"))},
                                                {"tenor", route.tenor_.String() ? Str(route.tenor_.String()) : Json_()}});
                                if (route.overnight_ || route.useTargetCurve_) {
                                    row.object_["overnight"] = Bool(route.overnight_);
                                    row.object_["use_target_curve"] = Bool(route.useTargetCurve_);
                                }
                                return row;
                            })}});
        if (!market.overnightCurves_.empty())
            result.object_["overnight"] = overnight;
        return result;
    }
    Json_ ValuationJson(const Script::ScriptValuationSettings_& valuation) {
        Check(valuation.evaluationDate_ && valuation.evaluationDate_->IsValid(), INPUT_ERROR, "/context/valuation/evaluation_date",
              "explicit valid valuation date");
        Check(bool(valuation.fixings_), INPUT_ERROR, "/context/valuation/fixings", "explicit frozen fixing snapshot");
        auto fixings = Json_::Array();
        for (const auto& [index, history] : valuation.fixings_->Values())
            for (const auto& [time, value] : history)
                fixings.array_.push_back(
                    Obj({{"index", Str(index)}, {"date", Str(Date::ToString(time.Date()))}, {"fraction", Num(time.Frac())}, {"value", Num(value)}}));
        return Obj({{"evaluation_date", Str(Date::ToString(*valuation.evaluationDate_))},
                    {"today_fixing_policy", Str(Token(valuation.todayFixingPolicy_, INPUT_ERROR, "/context/valuation/today_fixing_policy"))},
                    {"fixings", fixings}});
    }
    Json_ ObservationJson(const Handle_<CalibrationObservation_>& row) {
        Check(bool(row), INPUT_ERROR, "/context/observations", "nonnull typed observation");
        const auto& instrument = *row->Instrument();
        return Obj({{"id", Str(row->Id())},
                    {"instrument_id", Str(instrument.Id())},
                    {"source_identity", Str(instrument.Definition()->SourceIdentity())},
                    {"definition_identity", Str(instrument.Definition()->DefinitionIdentity())},
                    {"binding_identity", Str(instrument.BindingIdentity())},
                    {"instrument_identity", Str(instrument.InstrumentIdentity())},
                    {"measure_id", Str(row->MeasureId())},
                    {"quote", QuoteJson(*row->Quote())},
                    {"error_scale", Num(row->ErrorScale())}});
    }
    Json_ ConfigNumber(double value, const String_& field, bool validate) {
        if (!std::isfinite(value)) {
            Check(!validate, INPUT_ERROR, field, "finite configuration number", NumberText(value));
            return Str(NumberText(value));
        }
        return Num(value);
    }
    Json_ ConfigNumbers(const Vector_<>& values, const String_& field, bool validate) {
        auto result = Json_::Array();
        for (size_t i = 0; i < values.size(); ++i)
            result.array_.push_back(ConfigNumber(values[i], field + "/" + OrdinalText(i), validate));
        return result;
    }
    Json_ SolverJson(const CurveInstrumentSolver_& solver, bool validate) {
        const auto mode = validate ? Token(solver.solveMode_, INPUT_ERROR, "/context/solver/mode")
                                   : String_(solver.solveMode_.String() ? solver.solveMode_.String() : "");
        return Obj({{"mode", Str(mode)},
                    {"initial_guess", ConfigNumber(solver.initialGuess_, "/context/solver/initial_guess", validate)},
                    {"initial_guess_per_node", ConfigNumbers(solver.initialGuessPerNode_, "/context/solver/initial_guess_per_node", validate)},
                    {"parameter_scales", ConfigNumbers(solver.parameterScales_, "/context/solver/parameter_scales", validate)},
                    {"smoothing_weight", ConfigNumber(solver.smoothingWeight_, "/context/solver/smoothing_weight", validate)},
                    {"max_evaluations", Num(solver.maxEvaluations_)},
                    {"max_restarts", Num(solver.maxRestarts_)},
                    {"fit_weight",
                     Optional(solver.fitWeight_, [validate](double value) { return ConfigNumber(value, "/context/solver/fit_weight", validate); })},
                    {"fit_norm_tolerance", Optional(solver.fitNormTolerance_, [validate](double value) {
                         return ConfigNumber(value, "/context/solver/fit_norm_tolerance", validate);
                     })}});
    }
    Json_ ContextJson(const CurveInstrumentCalibrationSpec_& spec, bool validateSolver) {
        const auto valuation = ValuationJson(spec.valuation_);
        auto result = Obj({{"geometry", GeometryJson(spec.geometry_)},
                           {"market", MarketJson(spec.market_)},
                           {"valuation", HasOvernightContext(spec) ? CompoundedOisInternal::ValuationJson(spec.valuation_) : valuation},
                           {"observations", Array(spec.observations_, ObservationJson)},
                           {"solver", SolverJson(spec.solver_, validateSolver)}});
        if (!spec.compoundedOis_.empty())
            result.object_["compounded_ois"] = Array(spec.compoundedOis_, [](const auto& tables) {
                Check(bool(tables), INPUT_ERROR, "/context/compounded_ois", "nonnull frozen OIS tables");
                return Str(tables->PreparationIdentity());
            });
        return result;
    }
    String_ ConfigurationIdentity(const CurveInstrumentCalibrationSpec_& spec, const CurveInstrumentCalibrationOptions_& options) {
        // Capture invalid solver values without replacing the solver's primary diagnostic.
        return Fingerprint(Obj({{"context", ContextJson(spec, false)},
                                {"options", Obj({{"evaluation", Str(options.evaluation_.String())},
                                                 {"derivative", Str(options.jacobianMode_.String())},
                                                 {"difference_step", Num(options.differenceStep_)},
                                                 {"quote_jacobian", Bool(options.computeQuoteJacobian_)}})}}));
    }
    String_ ObjectiveIdentity(const CurveInstrumentCalibrationResult_& result, const CurveInstrumentSolver_& solver) {
        return Fingerprint(Obj({{"plan", Str(result.planIdentity_)},
                                {"mode", Str(result.solveMode_.String())},
                                {"theta0", Numbers(result.initialParameters_)},
                                {"scales", Numbers(result.parameterScales_)},
                                {"smoothing_weight", Num(solver.smoothingWeight_)},
                                {"lambda", Optional(result.fitWeight_, [](double value) { return Num(value); })},
                                {"eta", Optional(result.fitNormTolerance_, [](double value) { return Num(value); })}}));
    }
    String_ MetricIdentity(const Matrix_<>& metric) {
        auto values = Json_::Array();
        for (int i = 0; i < metric.Rows(); ++i)
            for (int j = 0; j < metric.Cols(); ++j) {
                if (!std::isfinite(metric(i, j)))
                    return "";
                values.array_.push_back(Num(metric(i, j)));
            }
        return Fingerprint(Obj({{"rows", Num(metric.Rows())}, {"columns", Num(metric.Cols())}, {"values", values}}));
    }
} // namespace Dal::CurveInstrumentInternal
