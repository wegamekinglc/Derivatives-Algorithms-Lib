#include <dal/platform/platform.hpp>

#include <dal/model/marketivlocalvolinternal.hpp>
#include <dal/model/surface/marketivsurfaceinternal.hpp>
#include <dal/storage/canonicaljson_internal.hpp>

namespace Dal::MarketIvLocalVolDetail {
    namespace {
        using Json_ = CanonicalJson::Json_;

        Json_ Object(std::initializer_list<std::pair<const std::string, Json_>> members) {
            auto result = Json_::Object();
            result.object_ = members;
            return result;
        }
        Json_ Count(size_t value) {
            // Decimal strings retain all size_t bits beyond JCS's binary64 integer range.
            return Json_::String(String_(std::to_string(value)));
        }
        Json_ Optional(std::optional<double> value) { return value ? Json_::Number(*value) : Json_(); }
        template <class T_> Json_ Numbers(const Vector_<T_>& values) {
            auto result = Json_::Array();
            result.array_.reserve(values.size());
            for (const auto& value : values)
                result.array_.push_back(Json_::Number(value));
            return result;
        }

        Json_ FitSettings(const MarketIvFitSettings_& s) {
            return Object({{"rho_limit", Json_::Number(s.rhoLimit_)},
                           {"constraint_margin", Json_::Number(s.constraintMargin_)},
                           {"theta_floor", Json_::Number(s.thetaFloor_)},
                           {"max_variance_increment", Json_::Number(s.maxVarianceIncrement_)},
                           {"initial_volatility", Json_::Number(s.initialVolatility_)},
                           {"projected_gradient_tolerance", Json_::Number(s.projectedGradientTolerance_)},
                           {"parameter_step_tolerance", Json_::Number(s.parameterStepTolerance_)},
                           {"objective_tie_tolerance", Json_::Number(s.objectiveTieTolerance_)},
                           {"normalized_price_tolerance", Json_::Number(s.normalizedPriceTolerance_)},
                           {"decimal_iv_tolerance", Json_::Number(s.decimalIvTolerance_)},
                           {"normalized_price_scale_floor", Json_::Number(s.normalizedPriceScaleFloor_)},
                           {"feasibility_tolerance", Json_::Number(s.feasibilityTolerance_)},
                           {"max_iterations_per_start", Json_::Number(s.maxIterationsPerStart_)},
                           {"max_line_search_trials", Json_::Number(s.maxLineSearchTrials_)},
                           {"max_model_row_evaluations", Count(s.maxModelRowEvaluations_)},
                           {"max_quotes", Count(s.maxQuotes_)},
                           {"max_expiries", Count(s.maxExpiries_)},
                           {"max_market_terms", Count(s.maxMarketTerms_)},
                           {"numeric_payload_budget_bytes", Count(s.numericPayloadBudgetBytes_)},
                           {"workspace_budget_bytes", Count(s.workspaceBudgetBytes_)}});
        }
        Json_ Certificate(const MarketIvCertificate_& c) {
            return Object({{"method", Json_::String(c.method_)},
                           {"domain", Json_::String(c.domain_)},
                           {"rho_margin", Json_::Number(c.rhoMargin_)},
                           {"eta_margin", Json_::Number(c.etaMargin_)},
                           {"butterfly_bound", Json_::Number(c.butterflyBound_)},
                           {"wing_slope_bound", Json_::Number(c.wingSlopeBound_)},
                           {"derivative_lower_bounds", Numbers(c.derivativeLowerBounds_)},
                           {"increments", Numbers(c.increments_)},
                           {"limiter_branches", Numbers(c.limiterBranches_)},
                           {"certified", Json_::Boolean(c.certified_)}});
        }
        Json_ Market(const MarketIvMarket_& market) {
            auto terms = Json_::Array();
            terms.array_.reserve(market.Terms().size());
            for (const auto& term : market.Terms())
                terms.array_.push_back(Object({{"date", Json_::String(Date::ToString(term.expiry_))},
                                               {"forward", Json_::Number(term.forward_)},
                                               {"discount", Json_::Number(term.discount_)}}));
            return Object({{"evaluation_date", Json_::String(Date::ToString(market.EvaluationDate()))},
                           {"spot", Json_::Number(market.Spot())},
                           {"carry_mode", Json_::String(market.CarryMode())},
                           {"rate", market.IsFlatCarry() ? Json_::Number(market.Rate()) : Json_()},
                           {"dividend_yield", market.IsFlatCarry() ? Json_::Number(market.DividendYield()) : Json_()},
                           {"terms", std::move(terms)}});
        }
        Json_ Quotes(const Vector_<MarketIvQuote_>& quotes) {
            auto result = Json_::Array();
            result.array_.reserve(quotes.size());
            for (const auto& q : quotes)
                result.array_.push_back(Object({{"id", Json_::String(q.id_)},
                                                {"expiry", Json_::String(Date::ToString(q.expiry_))},
                                                {"strike", Json_::Number(q.strike_)},
                                                {"option_type", Json_::String(q.optionType_.String())},
                                                {"quote_type", Json_::String(q.quoteType_.String())},
                                                {"mid", Json_::Number(q.mid_)},
                                                {"bid", Optional(q.bid_)},
                                                {"ask", Optional(q.ask_)},
                                                {"weight", Json_::Number(q.weight_)}}));
            return result;
        }
        Json_ GridRequest(const MarketIvDupireRequest_& request) {
            const auto& s = request.settings_;
            return Object({{"spots", Numbers(request.spots_)},
                           {"times", Numbers(request.times_)},
                           {"settings", Object({{"density_factor_floor", Json_::Number(s.evaluation_.densityFactorFloor_)},
                                                {"min_query_time", Json_::Number(s.evaluation_.minQueryTime_)},
                                                {"min_total_variance", Json_::Number(s.evaluation_.minTotalVariance_)},
                                                {"max_grid_points", Count(s.maxGridPoints_)},
                                                {"numeric_payload_budget_bytes", Count(s.numericPayloadBudgetBytes_)},
                                                {"workspace_budget_bytes", Count(s.workspaceBudgetBytes_)}})}});
        }
        Json_ Settings(const MarketIvLocalVolModelSettings_& s) {
            return Object({{"index", Json_::String(s.index_)},
                           {"currency", Json_::String(s.currency_)},
                           {"factor", Json_::String(s.factor_)},
                           {"horizon", Json_::Number(s.horizon_)},
                           {"max_step", Json_::Number(s.maxStep_)},
                           {"max_internal_steps", Count(s.maxInternalSteps_)},
                           {"max_simulation_dimension", Count(s.maxSimulationDimension_)},
                           {"numeric_payload_budget_bytes", Count(s.numericPayloadBudgetBytes_)},
                           {"workspace_budget_bytes", Count(s.workspaceBudgetBytes_)}});
        }
        size_t TextBound(const Json_& value) {
            if (value.kind_ == Json_::Kind_::Value_::STRING)
                return CheckedAdd(2, CheckedMultiply(6, value.string_.size()));
            size_t bytes = 32;
            for (const auto& item : value.array_)
                bytes = CheckedAdd(bytes, CheckedAdd(1, TextBound(item)));
            for (const auto& item : value.object_)
                bytes = CheckedAdd(bytes, CheckedAdd(CheckedAdd(4, CheckedMultiply(6, item.first.size())), TextBound(item.second)));
            return bytes;
        }
        void CheckTextBound(size_t bytes) {
            // Include JCS output growth and SHA padding/copies in the representability preflight.
            CheckLimit(CheckedMultiply(4, CheckedAdd(bytes, 128)), std::string().max_size(), "/identity/text_bytes");
        }
        String_ CheckedFingerprint(const Json_& value) {
            CheckTextBound(TextBound(value));
            return CanonicalJson::Fingerprint(value);
        }
        void PreflightSourceText(const MarketIvCalibrationSnapshot_& snapshot, const MarketIvDupireRequest_& request) {
            size_t bytes = 32768;
            const auto& raw = snapshot.Request();
            for (const auto& quote : raw.quotes_)
                bytes = CheckedAdd(bytes, CheckedAdd(2048, CheckedMultiply(6, quote.id_.size())));
            const auto& certificate = snapshot.Certificate();
            bytes = CheckedAdd(bytes, CheckedMultiply(6, CheckedAdd(certificate.method_.size(), certificate.domain_.size())));
            const size_t terms = raw.market_.Terms().size(), fitTimes = snapshot.Times().size();
            const size_t gridValues =
                CheckedAdd(CheckedMultiply(request.spots_.size(), request.times_.size()), CheckedAdd(request.spots_.size(), request.times_.size()));
            bytes = CheckedAdd(bytes,
                               CheckedAdd(CheckedMultiply(512, terms), CheckedAdd(CheckedMultiply(1024, fitTimes), CheckedMultiply(64, gridValues))));
            CheckTextBound(bytes);
        }
        void ValidateUtf8(const String_& value, const String_& field) {
            if (!CanonicalJson::ValidUtf8(std::string(value.data(), value.size())))
                Fail("InvalidMarketIvLocalVolRequest", "Build", field, "UTF-8 text", "invalid UTF-8");
        }
    } // namespace

    void ValidateIdentityInput(const MarketIvCalibrationSnapshot_& snapshot, const MarketIvDupireRequest_& request) {
        PreflightSourceText(snapshot, request);
        const auto& quotes = snapshot.Request().quotes_;
        for (size_t i = 0; i < quotes.size(); ++i)
            ValidateUtf8(quotes[i].id_, "/grid/snapshot/quotes/" + String_(std::to_string(i)) + "/id");
        ValidateUtf8(snapshot.Certificate().method_, "/grid/snapshot/certificate/method");
        ValidateUtf8(snapshot.Certificate().domain_, "/grid/snapshot/certificate/domain");
    }

    String_ SourceIdentity(const MarketIvCalibrationSnapshot_& snapshot, const MarketIvDupireRequest_& request, const LocalVolSurfaceData_& surface) {
        ValidateIdentityInput(snapshot, request);
        const auto& fit = MarketIvData(snapshot);
        auto vols = Json_::Array();
        vols.array_.reserve(CheckedMultiply(surface.spots_.size(), surface.times_.size()));
        for (int row = 0; row < surface.vols_.Rows(); ++row)
            for (int col = 0; col < surface.vols_.Cols(); ++col)
                vols.array_.push_back(Json_::Number(surface.vols_(row, col)));
        return CheckedFingerprint(Object({{"schema", Json_::String("dal.market-iv-local-vol-source/1")},
                                          {"market", Market(snapshot.Request().market_)},
                                          {"quotes", Quotes(snapshot.Request().quotes_)},
                                          {"fit_settings", FitSettings(snapshot.Request().settings_)},
                                          {"fit", Object({{"rho", Json_::Number(snapshot.Rho())},
                                                          {"eta", Json_::Number(snapshot.Eta())},
                                                          {"times", Numbers(snapshot.Times())},
                                                          {"theta", Numbers(snapshot.Theta())},
                                                          {"slopes", Numbers(snapshot.Slopes())},
                                                          {"certificate", Certificate(snapshot.Certificate())},
                                                          {"regime_signature", Numbers(snapshot.RegimeSignature())},
                                                          {"selected_start", Json_::Number(fit.selectedStart_)},
                                                          {"status", Json_::String(fit.status_)},
                                                          {"underidentified", Json_::Boolean(fit.underidentified_)},
                                                          {"near_tie", Json_::Boolean(fit.nearTie_)}})},
                                          {"grid_request", GridRequest(request)},
                                          {"local_vol", std::move(vols)},
                                          {"interpolation", Json_::String("VOL_LINEAR_LOG_SPOT_TIME_FLAT/1")}}));
    }

    String_ ModelIdentity(const String_& sourceIdentity, const MarketIvLocalVolModelSettings_& settings) {
        const size_t text =
            CheckedAdd(CheckedAdd(sourceIdentity.size(), settings.index_.size()), CheckedAdd(settings.currency_.size(), settings.factor_.size()));
        CheckTextBound(CheckedAdd(8192, CheckedMultiply(6, text)));
        return CheckedFingerprint(Object({{"schema", Json_::String("dal.market-iv-local-vol-model/1")},
                                          {"source_identity", Json_::String(sourceIdentity)},
                                          {"settings", Settings(settings)},
                                          {"time_basis", Json_::String("ACT/365")},
                                          {"step_scheme", Json_::String("LOG_EULER_LEFT_VOL/1")},
                                          {"local_vol_scheme", Json_::String("VOL_LINEAR_LOG_SPOT_TIME_FLAT/1")},
                                          {"carry_scheme", Json_::String("LOG_F_LOG_D_LINEAR_LAST_SLOPE/1")},
                                          {"timeline_scheme", Json_::String("UNION_CARRY_GRID_EVENTS/1")},
                                          {"capabilities", Json_::String("dal.market-iv-local-vol-capabilities/1")}}));
    }
} // namespace Dal::MarketIvLocalVolDetail
