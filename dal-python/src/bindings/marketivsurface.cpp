#include "bindings.h"

#include <cmath>
#include <pybind11/stl.h>

#include <dal-public/src/marketivsurface.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    constexpr const char* IDENTIFIER = "InvalidMarketIvRequest";
    double Real(const py::handle& x, const char* field) {
        const auto context = InputContext(x, field, "finite int/float excluding bool and enums", IDENTIFIER);
        if (PyBool_Check(x.ptr()) || IsEnum(x) || (!PyLong_Check(x.ptr()) && !PyFloat_Check(x.ptr())))
            throw py::type_error(context);
        const double value = PyFloat_AsDouble(x.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            throw py::value_error(context);
        }
        REQUIRE(std::isfinite(value), String_(context));
        return value;
    }
    template <class T_> T_ Input(const py::handle& x, const char* field) {
        return RequiredRiskInput<T_>(x, field, "matching DAL value type", IDENTIFIER);
    }
    template <class T_> Vector_<T_> Values(const py::handle& x, const char* field) {
        Vector_<T_> values;
        const auto sequence = RequestSequence(x, {field, IDENTIFIER});
        values.reserve(sequence.size());
        for (const auto& item : sequence)
            values.push_back(Input<T_>(item, field));
        return values;
    }
    Vector_<> Numbers(const py::handle& x, const char* field) {
        Vector_<> values;
        const auto sequence = RequestSequence(x, {field, IDENTIFIER});
        values.reserve(sequence.size());
        for (const auto& item : sequence)
            values.push_back(Real(item, field));
        return values;
    }
    template <class E_> E_ Enum(const py::handle& x, const char* field, std::initializer_list<const char*> names) {
        if (py::isinstance<typename E_::Value_>(x))
            return E_(py::cast<typename E_::Value_>(x));
        const auto value = SettingStringInput(x, field, IDENTIFIER);
        bool found = false;
        for (const auto name : names)
            found |= value == name;
        REQUIRE(found, String_("InvalidMarketIvRequest: ") + field + "; expected canonical enum value");
        return E_(value);
    }
    template <class S_> bool Set(const std::string& name, const py::handle& value, S_* s) {
        for (const auto& f : MarketIvSettings::RealFields(*s))
            if (name == f.name_) {
                s->*(f.member_) = Real(value, f.name_);
                return true;
            }
        for (const auto& f : MarketIvSettings::CountFields(*s))
            if (name == f.name_) {
                s->*(f.member_) = RequestUnsigned(value, {f.name_, IDENTIFIER});
                return true;
            }
        for (const auto& f : MarketIvSettings::IntegerFields(*s))
            if (name == f.name_) {
                s->*(f.member_) =
                    static_cast<int>(IntegerInput(value, InputContext(value, f.name_, "positive int excluding bool and enums", IDENTIFIER), 1,
                                                  std::numeric_limits<int>::max(), "; int overflow"));
                return true;
            }
        return false;
    }
    void Validate(const MarketIvFitSettings_& s) { ValidateMarketIvFitSettings(s); }
    void Validate(const MarketIvEvaluationSettings_& s) { ValidateMarketIvEvaluationSettings(s); }
    void Validate(const MarketIvDupireSettings_& s) {
        Validate(s.evaluation_);
        REQUIRE(s.maxGridPoints_ > 0, "InvalidMarketIvRequest: max_grid_points must be positive");
    }
    void Validate(const MarketIvRefitRiskSettings_& s) {
        Validate(s.evaluation_);
        REQUIRE(s.maxSelectedQuotes_ > 0 && s.maxOutputs_ > 0, "InvalidMarketIvRequest: refit size limits must be positive");
    }
    template <class S_> S_ NewSettings(const py::kwargs& kwargs) {
        S_ s;
        for (const auto& entry : kwargs) {
            const std::string key = py::cast<std::string>(entry.first);
            if (Set(key, entry.second, &s))
                continue;
            if constexpr (std::is_same_v<S_, MarketIvDupireSettings_> || std::is_same_v<S_, MarketIvRefitRiskSettings_>) {
                if (Set(key, entry.second, &s.evaluation_))
                    continue;
            }
            if constexpr (std::is_same_v<S_, MarketIvRefitRiskSettings_>) {
                if (key == "max_total_model_row_evaluations") {
                    s.maxTotalModelRowEvaluations_ =
                        entry.second.is_none() ? std::nullopt : std::optional<size_t>(RequestUnsigned(entry.second, {key.c_str(), IDENTIFIER}));
                    continue;
                }
            }
            throw py::type_error("InvalidMarketIvRequest: unknown settings key=" + key);
        }
        Validate(s);
        return s;
    }
    template <class T_, class V_> void ReadOnly(py::class_<T_>* type, const char* name, V_ T_::* member) {
        type->def_property_readonly(name, [member](const T_& x) { return x.*member; });
    }
    template <class T_> void TextField(py::class_<T_>* type, const char* name, String_ T_::* member) {
        type->def_property_readonly(name, [member](const T_& x) { return Text(x.*member); });
    }
    template <class T_> void VectorField(py::class_<T_>* type, const char* name, Vector_<> T_::* member) {
        type->def_property_readonly(name, [member](const T_& x) { return CopyRiskVector(x.*member); });
    }
    template <class S_> void Settings(py::module_& m, const char* name) {
        auto type = WithCopies(py::class_<S_>(m, name));
        type.def(py::init(&NewSettings<S_>));
        for (const auto& f : MarketIvSettings::RealFields(S_{}))
            ReadOnly(&type, f.name_, f.member_);
        for (const auto& f : MarketIvSettings::CountFields(S_{}))
            ReadOnly(&type, f.name_, f.member_);
        for (const auto& f : MarketIvSettings::IntegerFields(S_{}))
            ReadOnly(&type, f.name_, f.member_);
        if constexpr (std::is_same_v<S_, MarketIvDupireSettings_> || std::is_same_v<S_, MarketIvRefitRiskSettings_>)
            for (const auto& f : MarketIvSettings::RealFields(MarketIvEvaluationSettings_{}))
                type.def_property_readonly(f.name_, [member = f.member_](const S_& x) { return x.evaluation_.*member; });
        if constexpr (std::is_same_v<S_, MarketIvRefitRiskSettings_>)
            ReadOnly(&type, "max_total_model_row_evaluations", &S_::maxTotalModelRowEvaluations_);
    }
    template <class S_> S_ OptionalSettings(const py::handle& x) { return x.is_none() ? S_{} : Input<S_>(x, "settings"); }
    MarketIvQuote_ CheckedQuote(MarketIvQuote_ q) {
        REQUIRE(!q.id_.empty() && q.id_.find('\0') == String_::npos && q.expiry_.IsValid(), "InvalidMarketIvRequest: quote.id/expiry is invalid");
        REQUIRE(q.strike_ > 0. && q.weight_ > 0., "InvalidMarketIvRequest: quote.strike/weight must be positive");
        const bool iv = q.quoteType_ == MarketIvQuoteType_::Value_::DECIMAL_IV;
        REQUIRE(iv ? q.mid_ > 0. : q.mid_ >= 0., "InvalidMarketIvRequest: quote.mid must be positive IV or nonnegative PRICE");
        REQUIRE(q.bid_.has_value() == q.ask_.has_value(), "InvalidMarketIvRequest: quote.bid/ask must both be present or empty");
        if (q.bid_) {
            REQUIRE(*q.bid_ >= 0. && *q.bid_ <= q.mid_ && q.mid_ <= *q.ask_, "InvalidMarketIvRequest: quote.bid/mid/ask order is invalid");
        }
        return q;
    }
    class Legacy_ final : public AAD::IVS_ {
        Handle_<AAD::IVS_> ivs_;

    public:
        explicit Legacy_(const MarketIvCalibrationSnapshot_& s)
            : IVS_(s.Request().market_.Spot(), s.Request().market_.Rate(), s.Request().market_.DividendYield()), ivs_(MarketIvSurfaceAsLegacyIVS(s)) {
        }
        double ImpliedVol(double k, double t) const override { return ivs_->ImpliedVol(k, t); }
    };
} // namespace

void init_bindings_marketivsurface(py::module_& m) {
    py::enum_<MarketIvQuoteType_::Value_>(m, "MarketIvQuoteType_")
        .value("PRICE", MarketIvQuoteType_::Value_::PRICE)
        .value("DECIMAL_IV", MarketIvQuoteType_::Value_::DECIMAL_IV);
    py::enum_<MarketIvQuantity_::Value_>(m, "MarketIvQuantity_")
        .value("TOTAL_VARIANCE", MarketIvQuantity_::Value_::TOTAL_VARIANCE)
        .value("IMPLIED_VOL", MarketIvQuantity_::Value_::IMPLIED_VOL)
        .value("CALL_PRICE", MarketIvQuantity_::Value_::CALL_PRICE)
        .value("PUT_PRICE", MarketIvQuantity_::Value_::PUT_PRICE)
        .value("LOCAL_VOL", MarketIvQuantity_::Value_::LOCAL_VOL);
    Settings<MarketIvFitSettings_>(m, "MarketIvFitSettings_");
    Settings<MarketIvEvaluationSettings_>(m, "MarketIvEvaluationSettings_");
    Settings<MarketIvDupireSettings_>(m, "MarketIvDupireSettings_");
    Settings<MarketIvRefitRiskSettings_>(m, "MarketIvRefitRiskSettings_");
    auto term = WithCopies(py::class_<MarketIvTerm_>(m, "MarketIvTerm_"));
    term.def(py::init([](const py::object& date, const py::object& f, const py::object& d) {
                 MarketIvTerm_ t{Input<Date_>(date, "expiry"), Real(f, "forward"), Real(d, "discount")};
                 REQUIRE(t.expiry_.IsValid() && t.forward_ > 0. && t.discount_ > 0., "InvalidMarketIvRequest: invalid term");
                 return t;
             }),
             py::arg("expiry"), py::arg("forward"), py::arg("discount"));
    ReadOnly(&term, "expiry", &MarketIvTerm_::expiry_);
    ReadOnly(&term, "forward", &MarketIvTerm_::forward_);
    ReadOnly(&term, "discount", &MarketIvTerm_::discount_);
    auto market = WithCopies(py::class_<MarketIvMarket_>(m, "MarketIvMarket_"));
    market.def(py::init([](const py::object& date, const py::object& spot, const py::object& terms) {
                   return MarketIvMarket_(Input<Date_>(date, "evaluation_date"), Real(spot, "spot"), Values<MarketIvTerm_>(terms, "terms"));
               }),
               py::arg("evaluation_date"), py::arg("spot"), py::arg("terms"));
    market.def_property_readonly("evaluation_date", &MarketIvMarket_::EvaluationDate)
        .def_property_readonly("spot", &MarketIvMarket_::Spot)
        .def_property_readonly("terms", [](const MarketIvMarket_& x) { return CopyRiskVector(x.Terms()); })
        .def_property_readonly("carry_mode", [](const MarketIvMarket_& x) { return Text(x.CarryMode()); });
    m.def(
        "MarketIvFlatMarket_New",
        [](const py::object& date, const py::object& spot, const py::object& r, const py::object& q, const py::object& dates) {
            return NewFlatMarketIvMarket(Input<Date_>(date, "evaluation_date"), Real(spot, "spot"), Real(r, "rate"), Real(q, "dividend_yield"),
                                         Values<Date_>(dates, "expiries"));
        },
        py::arg("evaluation_date"), py::arg("spot"), py::arg("rate"), py::arg("dividend_yield"), py::arg("expiries"));
    auto quote = WithCopies(py::class_<MarketIvQuote_>(m, "MarketIvQuote_"));
    quote.def(py::init([](const py::object& id, const py::object& date, const py::object& strike, const py::object& option, const py::object& type,
                          const py::object& mid, const py::object& bid, const py::object& ask, const py::object& weight) {
                  return CheckedQuote(MarketIvQuote_{SettingStringInput(id, "id", IDENTIFIER), Input<Date_>(date, "expiry"), Real(strike, "strike"),
                                                     Enum<OptionType_>(option, "option_type", {"CALL", "PUT"}),
                                                     Enum<MarketIvQuoteType_>(type, "quote_type", {"PRICE", "DECIMAL_IV"}), Real(mid, "mid"),
                                                     bid.is_none() ? std::nullopt : std::optional<double>(Real(bid, "bid")),
                                                     ask.is_none() ? std::nullopt : std::optional<double>(Real(ask, "ask")), Real(weight, "weight")});
              }),
              py::kw_only(), py::arg("id"), py::arg("expiry"), py::arg("strike"), py::arg("option_type"), py::arg("quote_type"), py::arg("mid"),
              py::arg("bid") = py::none(), py::arg("ask") = py::none(), py::arg("weight") = 1.);
    TextField(&quote, "id", &MarketIvQuote_::id_);
    ReadOnly(&quote, "expiry", &MarketIvQuote_::expiry_);
    ReadOnly(&quote, "strike", &MarketIvQuote_::strike_);
    ReadOnly(&quote, "mid", &MarketIvQuote_::mid_);
    ReadOnly(&quote, "bid", &MarketIvQuote_::bid_);
    ReadOnly(&quote, "ask", &MarketIvQuote_::ask_);
    ReadOnly(&quote, "weight", &MarketIvQuote_::weight_);
    quote.def_property_readonly("option_type", [](const MarketIvQuote_& x) { return Text(x.optionType_.String()); })
        .def_property_readonly("quote_type", [](const MarketIvQuote_& x) { return Text(x.quoteType_.String()); });
    auto query = WithCopies(py::class_<MarketIvQuery_>(m, "MarketIvQuery_"));
    query.def(py::init([](const py::object& k, const py::object& t) {
                  const double strike = Real(k, "strike"), time = Real(t, "time");
                  REQUIRE(strike > 0. && time > 0., "InvalidMarketIvRequest: query must be positive");
                  return MarketIvQuery_{strike, time};
              }),
              py::arg("strike"), py::arg("time"));
    ReadOnly(&query, "strike", &MarketIvQuery_::strike_);
    ReadOnly(&query, "time", &MarketIvQuery_::time_);
    auto request = WithCopies(py::class_<MarketIvFitRequest_>(m, "MarketIvFitRequest_"));
    request.def(py::init([](const MarketIvMarket_& market, const py::object& quotes, const py::object& settings) {
                    MarketIvFitRequest_ r{market, Values<MarketIvQuote_>(quotes, "quotes"), OptionalSettings<MarketIvFitSettings_>(settings)};
                    ValidateMarketIvFitRequest(r);
                    return r;
                }),
                py::arg("market"), py::arg("quotes"), py::kw_only(), py::arg("settings") = py::none());
    ReadOnly(&request, "market", &MarketIvFitRequest_::market_);
    ReadOnly(&request, "settings", &MarketIvFitRequest_::settings_);
    request.def_property_readonly("quotes", [](const MarketIvFitRequest_& x) { return CopyRiskVector(x.quotes_); });
    auto gridRequest = WithCopies(py::class_<MarketIvDupireRequest_>(m, "MarketIvDupireRequest_"));
    gridRequest.def(py::init([](const py::object& spots, const py::object& times, const py::object& settings) {
                        MarketIvDupireRequest_ request{Numbers(spots, "spots"), Numbers(times, "times"),
                                                       OptionalSettings<MarketIvDupireSettings_>(settings)};
                        ValidateMarketIvDupireRequest(request);
                        return request;
                    }),
                    py::arg("spots"), py::arg("times"), py::kw_only(), py::arg("settings") = py::none());
    VectorField(&gridRequest, "spots", &MarketIvDupireRequest_::spots_);
    VectorField(&gridRequest, "times", &MarketIvDupireRequest_::times_);
    ReadOnly(&gridRequest, "settings", &MarketIvDupireRequest_::settings_);
    auto output = WithCopies(py::class_<MarketIvRefitOutput_>(m, "MarketIvRefitOutput_"));
    output.def(py::init([](const MarketIvQuery_& query, const py::object& quantity) {
                   return MarketIvRefitOutput_{
                       query,
                       Enum<MarketIvQuantity_>(quantity, "quantity", {"TOTAL_VARIANCE", "IMPLIED_VOL", "CALL_PRICE", "PUT_PRICE", "LOCAL_VOL"})};
               }),
               py::arg("query"), py::arg("quantity"));
    ReadOnly(&output, "query", &MarketIvRefitOutput_::query_);
    output.def_property_readonly("quantity", [](const MarketIvRefitOutput_& x) { return Text(x.quantity_.String()); });
    auto riskRequest = WithCopies(py::class_<MarketIvRefitRiskRequest_>(m, "MarketIvRefitRiskRequest_"));
    riskRequest.def(py::init([](const py::object& ids, const py::object& outputs, const py::object& settings, const py::object& steps) {
                        const auto names = RequestIds(ids, {"quote_ids", IDENTIFIER});
                        REQUIRE(names, "InvalidMarketIvRequest: quote_ids required");
                        MarketIvRefitRiskRequest_ request{*names, Values<MarketIvRefitOutput_>(outputs, "outputs"),
                                                          OptionalSettings<MarketIvRefitRiskSettings_>(settings),
                                                          steps.is_none() ? Vector_<>{} : Numbers(steps, "steps")};
                        ValidateMarketIvRefitRiskRequest(request);
                        return request;
                    }),
                    py::arg("quote_ids"), py::arg("outputs"), py::kw_only(), py::arg("settings") = py::none(), py::arg("steps") = py::none());
    riskRequest.def_property_readonly("quote_ids", [](const MarketIvRefitRiskRequest_& x) { return RiskTextVector(x.quoteIds_); })
        .def_property_readonly("outputs", [](const MarketIvRefitRiskRequest_& x) { return CopyRiskVector(x.outputs_); });
    ReadOnly(&riskRequest, "settings", &MarketIvRefitRiskRequest_::settings_);
    VectorField(&riskRequest, "steps", &MarketIvRefitRiskRequest_::steps_);
    auto execution = WithCopies(py::class_<MarketIvExecution_>(m, "MarketIvExecution_"));
    ReadOnly(&execution, "planned_numeric_payload_bytes", &MarketIvExecution_::plannedNumericPayloadBytes_);
    ReadOnly(&execution, "planned_workspace_bytes", &MarketIvExecution_::plannedWorkspaceBytes_);
    ReadOnly(&execution, "numeric_payload_bytes", &MarketIvExecution_::numericPayloadBytes_);
    ReadOnly(&execution, "peak_workspace_bytes", &MarketIvExecution_::peakWorkspaceBytes_);
    ReadOnly(&execution, "model_row_evaluations", &MarketIvExecution_::modelRowEvaluations_);
    ReadOnly(&execution, "quote_conversions", &MarketIvExecution_::quoteConversions_);
    ReadOnly(&execution, "output_evaluations", &MarketIvExecution_::outputEvaluations_);
    ReadOnly(&execution, "quote_capacity", &MarketIvExecution_::quoteCapacity_);
    ReadOnly(&execution, "iteration_capacity", &MarketIvExecution_::iterationCapacity_);
    ReadOnly(&execution, "jacobian_capacity", &MarketIvExecution_::jacobianCapacity_);
    auto iteration = WithCopies(py::class_<MarketIvIteration_>(m, "MarketIvIteration_"));
    ReadOnly(&iteration, "start", &MarketIvIteration_::start_);
    ReadOnly(&iteration, "iteration", &MarketIvIteration_::iteration_);
    ReadOnly(&iteration, "trials", &MarketIvIteration_::trials_);
    ReadOnly(&iteration, "objective", &MarketIvIteration_::objective_);
    ReadOnly(&iteration, "projected_gradient", &MarketIvIteration_::projectedGradient_);
    ReadOnly(&iteration, "gradient_threshold", &MarketIvIteration_::gradientThreshold_);
    ReadOnly(&iteration, "damping", &MarketIvIteration_::damping_);
    auto start = WithCopies(py::class_<MarketIvStart_>(m, "MarketIvStart_"));
    ReadOnly(&start, "projected_seed", &MarketIvStart_::projectedSeed_);
    ReadOnly(&start, "converged", &MarketIvStart_::converged_);
    ReadOnly(&start, "certified", &MarketIvStart_::certified_);
    ReadOnly(&start, "quality_accepted", &MarketIvStart_::qualityAccepted_);
    ReadOnly(&start, "iterations", &MarketIvStart_::iterations_);
    start.def_property_readonly("rank", [](const MarketIvStart_& x) { return x.converged_ ? std::optional<int>(x.rank_) : std::nullopt; });
    ReadOnly(&start, "objective", &MarketIvStart_::objective_);
    ReadOnly(&start, "projected_gradient", &MarketIvStart_::projectedGradient_);
    ReadOnly(&start, "condition_estimate", &MarketIvStart_::conditionEstimate_);
    auto diagnostic = WithCopies(py::class_<MarketIvQuoteDiagnostic_>(m, "MarketIvQuoteDiagnostic_"));
    ReadOnly(&diagnostic, "input_row", &MarketIvQuoteDiagnostic_::inputRow_);
    ReadOnly(&diagnostic, "expiry_index", &MarketIvQuoteDiagnostic_::expiryIndex_);
    ReadOnly(&diagnostic, "time", &MarketIvQuoteDiagnostic_::time_);
    ReadOnly(&diagnostic, "forward", &MarketIvQuoteDiagnostic_::forward_);
    ReadOnly(&diagnostic, "discount", &MarketIvQuoteDiagnostic_::discount_);
    ReadOnly(&diagnostic, "log_moneyness", &MarketIvQuoteDiagnostic_::logMoneyness_);
    ReadOnly(&diagnostic, "normalized_mid", &MarketIvQuoteDiagnostic_::normalizedMid_);
    ReadOnly(&diagnostic, "normalized_bid", &MarketIvQuoteDiagnostic_::normalizedBid_);
    ReadOnly(&diagnostic, "normalized_ask", &MarketIvQuoteDiagnostic_::normalizedAsk_);
    ReadOnly(&diagnostic, "scale", &MarketIvQuoteDiagnostic_::scale_);
    ReadOnly(&diagnostic, "normalized_weight", &MarketIvQuoteDiagnostic_::normalizedWeight_);
    ReadOnly(&diagnostic, "fitted_normalized_call", &MarketIvQuoteDiagnostic_::fittedNormalizedCall_);
    ReadOnly(&diagnostic, "normalized_residual", &MarketIvQuoteDiagnostic_::normalizedResidual_);
    ReadOnly(&diagnostic, "native_fitted", &MarketIvQuoteDiagnostic_::nativeFitted_);
    ReadOnly(&diagnostic, "native_residual", &MarketIvQuoteDiagnostic_::nativeResidual_);
    ReadOnly(&diagnostic, "band_violation", &MarketIvQuoteDiagnostic_::bandViolation_);
    ReadOnly(&diagnostic, "parity_gap", &MarketIvQuoteDiagnostic_::parityGap_);
    ReadOnly(&diagnostic, "within_band", &MarketIvQuoteDiagnostic_::withinBand_);
    ReadOnly(&diagnostic, "band_branch", &MarketIvQuoteDiagnostic_::bandBranch_);
    auto cert = WithCopies(py::class_<MarketIvCertificate_>(m, "MarketIvCertificate_"));
    ReadOnly(&cert, "rho_margin", &MarketIvCertificate_::rhoMargin_);
    ReadOnly(&cert, "eta_margin", &MarketIvCertificate_::etaMargin_);
    ReadOnly(&cert, "butterfly_bound", &MarketIvCertificate_::butterflyBound_);
    ReadOnly(&cert, "wing_slope_bound", &MarketIvCertificate_::wingSlopeBound_);
    ReadOnly(&cert, "certified", &MarketIvCertificate_::certified_);
    auto response = WithCopies(py::class_<MarketIvRefitResponse_>(m, "MarketIvRefitResponse_"));
    ReadOnly(&response, "quote_index", &MarketIvRefitResponse_::quoteIndex_);
    ReadOnly(&response, "output_index", &MarketIvRefitResponse_::outputIndex_);
    ReadOnly(&response, "step", &MarketIvRefitResponse_::step_);
    ReadOnly(&response, "base", &MarketIvRefitResponse_::base_);
    ReadOnly(&response, "plus", &MarketIvRefitResponse_::plus_);
    ReadOnly(&response, "minus", &MarketIvRefitResponse_::minus_);
    ReadOnly(&response, "symmetric_bump_slope", &MarketIvRefitResponse_::symmetricBumpSlope_);
    ReadOnly(&response, "plus_slope", &MarketIvRefitResponse_::plusSlope_);
    ReadOnly(&response, "minus_slope", &MarketIvRefitResponse_::minusSlope_);
    ReadOnly(&response, "regime_changed", &MarketIvRefitResponse_::regimeChanged_);
    auto axis = WithCopies(py::class_<MarketIvRefitAxis_>(m, "MarketIvRefitAxis_"));
    ReadOnly(&axis, "quote_index", &MarketIvRefitAxis_::quoteIndex_);
    ReadOnly(&axis, "step", &MarketIvRefitAxis_::step_);
    ReadOnly(&axis, "regime_changed", &MarketIvRefitAxis_::regimeChanged_);
    axis.def_property_readonly("plus_start",
                               [](const MarketIvRefitAxis_& x) { return x.plusStart_ >= 0 ? std::optional<int>(x.plusStart_) : std::nullopt; });
    axis.def_property_readonly("minus_start",
                               [](const MarketIvRefitAxis_& x) { return x.minusStart_ >= 0 ? std::optional<int>(x.minusStart_) : std::nullopt; });
    axis.def_property_readonly("plus_rank",
                               [](const MarketIvRefitAxis_& x) { return x.plusStart_ >= 0 ? std::optional<int>(x.plusRank_) : std::nullopt; });
    axis.def_property_readonly("minus_rank",
                               [](const MarketIvRefitAxis_& x) { return x.minusStart_ >= 0 ? std::optional<int>(x.minusRank_) : std::nullopt; });
    TextField(&start, "termination", &MarketIvStart_::termination_);
    TextField(&start, "failure", &MarketIvStart_::failure_);
    TextField(&cert, "method", &MarketIvCertificate_::method_);
    TextField(&cert, "domain", &MarketIvCertificate_::domain_);
    TextField(&axis, "plus_status", &MarketIvRefitAxis_::plusStatus_);
    TextField(&axis, "minus_status", &MarketIvRefitAxis_::minusStatus_);
    TextField(&axis, "plus_failure", &MarketIvRefitAxis_::plusFailure_);
    TextField(&axis, "minus_failure", &MarketIvRefitAxis_::minusFailure_);
    TextField(&axis, "input_unit", &MarketIvRefitAxis_::inputUnit_);
    VectorField(&start, "parameters", &MarketIvStart_::parameters_);
    VectorField(&cert, "derivative_lower_bounds", &MarketIvCertificate_::derivativeLowerBounds_);
    VectorField(&cert, "increments", &MarketIvCertificate_::increments_);
    cert.def_property_readonly("limiter_branches", [](const MarketIvCertificate_& x) { return CopyRiskVector(x.limiterBranches_); });
    response.def_property_readonly("status", [](const MarketIvRefitResponse_& x) { return Text(x.Status()); });
    response.def_property_readonly("plus_output_status", [](const MarketIvRefitResponse_& x) { return Text(x.PlusOutputStatus()); });
    response.def_property_readonly("minus_output_status", [](const MarketIvRefitResponse_& x) { return Text(x.MinusOutputStatus()); });
    auto snapshot = WithCopies(py::class_<MarketIvCalibrationSnapshot_>(m, "MarketIvCalibrationSnapshot_"));
    snapshot.def_property_readonly("request", [](const MarketIvCalibrationSnapshot_& x) { return x.Request(); })
        .def_property_readonly("market", [](const MarketIvCalibrationSnapshot_& x) { return x.Request().market_; })
        .def_property_readonly("times", [](const MarketIvCalibrationSnapshot_& x) { return CopyRiskVector(x.Times()); })
        .def_property_readonly("theta", [](const MarketIvCalibrationSnapshot_& x) { return CopyRiskVector(x.Theta()); })
        .def_property_readonly("slopes", [](const MarketIvCalibrationSnapshot_& x) { return CopyRiskVector(x.Slopes()); })
        .def_property_readonly("certificate", [](const MarketIvCalibrationSnapshot_& x) { return x.Certificate(); })
        .def_property_readonly("rho", &MarketIvCalibrationSnapshot_::Rho)
        .def_property_readonly("eta", &MarketIvCalibrationSnapshot_::Eta)
        .def_property_readonly("regime_signature", [](const MarketIvCalibrationSnapshot_& x) { return CopyRiskVector(x.RegimeSignature()); });
    auto fit = WithCopies(py::class_<MarketIvFitResult_>(m, "MarketIvFitResult_"));
    fit.def_property_readonly("is_success", &MarketIvFitResult_::IsSuccess)
        .def_property_readonly("snapshot", &MarketIvFitResult_::Snapshot)
        .def_property_readonly("status", [](const MarketIvFitResult_& x) { return Text(x.Status()); })
        .def_property_readonly("failure", [](const MarketIvFitResult_& x) { return Text(x.Failure()); })
        .def_property_readonly("method", [](const MarketIvFitResult_& x) { return Text(x.Method()); })
        .def_property_readonly("request", [](const MarketIvFitResult_& x) { return x.Request(); })
        .def_property_readonly("quotes", [](const MarketIvFitResult_& x) { return CopyRiskVector(x.Quotes()); })
        .def_property_readonly("iterations", [](const MarketIvFitResult_& x) { return CopyRiskVector(x.Iterations()); })
        .def_property_readonly("starts", [](const MarketIvFitResult_& x) { return CopyRiskVector(x.Starts()); })
        .def_property_readonly("execution", [](const MarketIvFitResult_& x) { return x.Execution(); })
        .def_property_readonly(
            "selected_start",
            [](const MarketIvFitResult_& x) { return x.SelectedStart() >= 0 ? std::optional<int>(x.SelectedStart()) : std::nullopt; })
        .def_property_readonly("underidentified", &MarketIvFitResult_::Underidentified)
        .def_property_readonly("near_tie", &MarketIvFitResult_::NearTie);
    auto point = WithCopies(py::class_<MarketIvPointResult_>(m, "MarketIvPointResult_"));
    point.def_property_readonly("total_variance", &MarketIvPointResult_::TotalVariance);
    point.def_property_readonly("variance_k", &MarketIvPointResult_::VarianceK);
    point.def_property_readonly("variance_kk", &MarketIvPointResult_::VarianceKK);
    point.def_property_readonly("variance_t", &MarketIvPointResult_::VarianceT);
    point.def_property_readonly("fixed_k_variance_t", &MarketIvPointResult_::FixedKVarianceT);
    point.def_property_readonly("implied_vol", &MarketIvPointResult_::ImpliedVol);
    point.def_property_readonly("call_price", &MarketIvPointResult_::CallPrice);
    point.def_property_readonly("put_price", &MarketIvPointResult_::PutPrice);
    point.def_property_readonly("call_k", &MarketIvPointResult_::CallK);
    point.def_property_readonly("call_kk", &MarketIvPointResult_::CallKK);
    point.def_property_readonly("call_t", &MarketIvPointResult_::CallT);
    point.def_property_readonly("density_factor", &MarketIvPointResult_::DensityFactor);
    point.def_property_readonly("local_variance", &MarketIvPointResult_::LocalVariance);
    point.def_property_readonly("local_vol", &MarketIvPointResult_::LocalVol);
    point.def_property_readonly("value_status", [](const MarketIvPointResult_& x) { return Text(x.ValueStatus()); });
    point.def_property_readonly("derivative_status", [](const MarketIvPointResult_& x) { return Text(x.DerivativeStatus()); });
    point.def_property_readonly("dupire_status", [](const MarketIvPointResult_& x) { return Text(x.DupireStatus()); });
    point.def_property_readonly("carry_derivative", [](const MarketIvPointResult_& x) { return Text(x.CarryDerivative()); });
    auto grid = WithCopies(py::class_<MarketIvDupireResult_>(m, "MarketIvDupireResult_"));
    grid.def_property_readonly("is_success", &MarketIvDupireResult_::IsSuccess)
        .def_property_readonly("failure", [](const MarketIvDupireResult_& x) { return Text(x.Failure()); })
        .def_property_readonly("snapshot", [](const MarketIvDupireResult_& x) { return x.Snapshot(); })
        .def_property_readonly("request", [](const MarketIvDupireResult_& x) { return x.Request(); })
        .def_property_readonly("nodes", [](const MarketIvDupireResult_& x) { return CopyRiskVector(x.Nodes()); })
        .def_property_readonly("execution", [](const MarketIvDupireResult_& x) { return x.Execution(); })
        .def_property_readonly("surface", [](const MarketIvDupireResult_& x) {
            const auto& s = x.SurfaceData();
            return std::make_shared<LocalVolSurfaceData_>(s.Name(), s.spots_, s.times_, s.vols_);
        });
    auto risk = WithCopies(py::class_<MarketIvRefitRiskResult_>(m, "MarketIvRefitRiskResult_"));
    risk.def_property_readonly("complete", &MarketIvRefitRiskResult_::Complete)
        .def_property_readonly("method", [](const MarketIvRefitRiskResult_& x) { return Text(x.Method()); })
        .def_property_readonly("snapshot", [](const MarketIvRefitRiskResult_& x) { return x.Snapshot(); })
        .def_property_readonly("request", [](const MarketIvRefitRiskResult_& x) { return x.Request(); })
        .def_property_readonly("responses", [](const MarketIvRefitRiskResult_& x) { return CopyRiskVector(x.Responses()); })
        .def_property_readonly("axis", [](const MarketIvRefitRiskResult_& x) { return CopyRiskVector(x.Axis()); })
        .def_property_readonly("execution", [](const MarketIvRefitRiskResult_& x) { return x.Execution(); });
    m.def(
        "MarketIvFitResult_New",
        [](const MarketIvFitRequest_& x) {
            const auto request = x;
            py::gil_scoped_release release;
            return FitMarketIvSurface(request);
        },
        py::arg("request"));
    m.def(
        "MarketIvPointResult_New",
        [](const MarketIvCalibrationSnapshot_& s, const MarketIvQuery_& q, const py::object& settings) {
            const auto evaluation = OptionalSettings<MarketIvEvaluationSettings_>(settings);
            const auto snapshot = s;
            const auto query = q;
            py::gil_scoped_release release;
            return EvaluateMarketIvSurface(snapshot, query, evaluation);
        },
        py::arg("snapshot"), py::arg("query"), py::kw_only(), py::arg("settings") = py::none());
    m.def(
        "MarketIvDupireResult_New",
        [](const MarketIvCalibrationSnapshot_& s, const MarketIvDupireRequest_& x) {
            const auto snapshot = s;
            const auto request = x;
            py::gil_scoped_release release;
            return CalibrateMarketIvDupire(snapshot, request);
        },
        py::arg("snapshot"), py::arg("request"));
    m.def(
        "MarketIvRefitRiskResult_New",
        [](const MarketIvCalibrationSnapshot_& s, const MarketIvRefitRiskRequest_& x) {
            const auto snapshot = s;
            const auto request = x;
            py::gil_scoped_release release;
            return EvaluateMarketIvRefitRisk(snapshot, request);
        },
        py::arg("snapshot"), py::arg("request"));
    m.def(
        "RefitMarketIvSurface",
        [](const MarketIvCalibrationSnapshot_& s, const py::object& quotes) {
            const auto snapshot = s;
            const auto rows = Values<MarketIvQuote_>(quotes, "quotes");
            py::gil_scoped_release release;
            return RefitMarketIvSurface(snapshot, rows);
        },
        py::arg("snapshot"), py::arg("quotes"));
    m.def(
        "MarketIvSurfaceAsLegacyIVS",
        [](const MarketIvCalibrationSnapshot_& s) -> std::unique_ptr<AAD::IVS_> { return std::make_unique<Legacy_>(s); }, py::arg("snapshot"));
}
