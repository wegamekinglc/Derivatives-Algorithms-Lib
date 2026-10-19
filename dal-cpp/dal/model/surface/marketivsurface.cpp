#include <dal/platform/platform.hpp>

#include <algorithm>
#include <numeric>
#include <tuple>

#include <dal/model/surface/marketivsurfaceinternal.hpp>

namespace Dal {
#include <dal/auto/MG_MarketIvQuantity_enum.inc>
#include <dal/auto/MG_MarketIvQuoteType_enum.inc>

    namespace {
        void ValidateQuoteAxes(const Vector_<MarketIvQuote_>& quotes) {
            Vector_<size_t> order(quotes.size());
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return quotes[a].id_ < quotes[b].id_; });
            for (size_t i = 1; i < order.size(); ++i)
                REQUIRE(quotes[order[i]].id_ != quotes[order[i - 1]].id_, "InvalidMarketIvRequest: quotes[row=" + String_(std::to_string(order[i])) +
                                                                              ";id=" + quotes[order[i]].id_ + "]; duplicate ID");
            const auto key = [&](size_t i) { return std::make_tuple(quotes[i].expiry_, quotes[i].strike_, quotes[i].optionType_.Switch()); };
            std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return key(a) < key(b); });
            for (size_t i = 1; i < order.size(); ++i)
                REQUIRE(key(order[i]) != key(order[i - 1]), "InvalidMarketIvRequest: quotes[row=" + String_(std::to_string(order[i])) +
                                                                ";id=" + quotes[order[i]].id_ + "]; duplicate economic quote");
        }
        void Positive(double value, const String_& field) {
            REQUIRE(std::isfinite(value) && value > 0., "InvalidMarketIvRequest: " + field + "; expected finite positive; actual=" + ToString(value));
        }
        void Proportion(double value, const String_& field) {
            Positive(value, field);
            REQUIRE(value <= 1., "InvalidMarketIvRequest: " + field + "; expected <= 1");
        }
        void ValidateMarket(const MarketIvMarket_& market) {
            REQUIRE(market.EvaluationDate().IsValid(), "InvalidMarketIvRequest: market.evaluation_date; invalid date");
            Positive(market.Spot(), "market.spot");
            REQUIRE(!market.Terms().empty(), "InvalidMarketIvRequest: market.terms; at least one term required");
            auto previous = market.EvaluationDate();
            for (const auto& term : market.Terms()) {
                REQUIRE(term.expiry_.IsValid() && previous < term.expiry_,
                        "InvalidMarketIvRequest: market.terms.expiry; dates must strictly increase after evaluation_date");
                Positive(term.forward_, "market.terms.forward");
                Positive(term.discount_, "market.terms.discount");
                Positive(term.forward_ * term.discount_, "market.terms.discount*forward");
                previous = term.expiry_;
            }
        }
        void ValidateQuoteValue(double value, const MarketIvQuote_& q, const MarketIvTerm_& term, const String_& field, bool allowZero) {
            REQUIRE(std::isfinite(value), "InvalidMarketIvRequest: " + field + "; expected finite number");
            if (q.quoteType_ == MarketIvQuoteType_::Value_::DECIMAL_IV) {
                REQUIRE(allowZero ? value >= 0. : value > 0., "InvalidMarketIvRequest: " + field + "; decimal IV must be positive (bid may be zero)");
            } else {
                const bool call = q.optionType_ == OptionType_::Value_::CALL;
                const long double f = term.forward_, k = q.strike_, d = term.discount_;
                const long double lo = d * std::max(call ? f - k : k - f, 0.L), hi = d * (call ? f : k);
                REQUIRE(std::isfinite(static_cast<double>(hi)) && value >= static_cast<double>(lo) && value <= static_cast<double>(hi),
                        "InvalidMarketIvRequest: " + field + "; PRICE outside closed no-arbitrage bounds; actual=" + ToString(value));
            }
        }
    } // namespace

    MarketIvMarket_::MarketIvMarket_(const Date_& date, double spot, const Vector_<MarketIvTerm_>& terms)
        : evaluationDate_(date), spot_(spot), terms_(terms) {
        ValidateMarket(*this);
    }

    MarketIvMarket_ NewFlatMarketIvMarket(const Date_& date, double spot, double rate, double yield, const Vector_<Date_>& expiries) {
        REQUIRE(std::isfinite(rate) && std::isfinite(yield), "InvalidMarketIvRequest: market.rate/dividend_yield; expected finite numbers");
        Vector_<MarketIvTerm_> terms;
        terms.reserve(expiries.size());
        for (const auto& expiry : expiries) {
            const double time = (expiry - date) / 365.;
            terms.push_back({expiry, spot * std::exp((rate - yield) * time), std::exp(-rate * time)});
        }
        MarketIvMarket_ result(date, spot, terms);
        result.flat_ = true;
        result.rate_ = rate;
        result.dividendYield_ = yield;
        return result;
    }
    void ValidateMarketIvFitSettings(const MarketIvFitSettings_& s) {
        Proportion(s.rhoLimit_, "settings.rho_limit");
        Proportion(s.constraintMargin_, "settings.constraint_margin");
        REQUIRE(s.rhoLimit_ < 1. && s.constraintMargin_ < 1., "InvalidMarketIvRequest: rho_limit/constraint_margin; expected < 1");
        Positive(s.thetaFloor_, "settings.theta_floor");
        Positive(s.maxVarianceIncrement_, "settings.max_variance_increment");
        REQUIRE(s.thetaFloor_ <= s.maxVarianceIncrement_, "InvalidMarketIvRequest: theta_floor exceeds max_variance_increment");
        Positive(s.initialVolatility_, "settings.initial_volatility");
        Proportion(s.projectedGradientTolerance_, "settings.projected_gradient_tolerance");
        Proportion(s.parameterStepTolerance_, "settings.parameter_step_tolerance");
        REQUIRE(std::isfinite(s.objectiveTieTolerance_) && s.objectiveTieTolerance_ >= 0. && s.objectiveTieTolerance_ <= 1.,
                "InvalidMarketIvRequest: objective_tie_tolerance; expected [0,1]");
        Proportion(s.normalizedPriceTolerance_, "settings.normalized_price_tolerance");
        Proportion(s.decimalIvTolerance_, "settings.decimal_iv_tolerance");
        Proportion(s.normalizedPriceScaleFloor_, "settings.normalized_price_scale_floor");
        Proportion(s.feasibilityTolerance_, "settings.feasibility_tolerance");
        REQUIRE(s.maxIterationsPerStart_ > 0 && s.maxLineSearchTrials_ > 0, "InvalidMarketIvRequest: iteration/trial limits must be positive int");
        const size_t intMax = static_cast<size_t>(std::numeric_limits<int>::max());
        REQUIRE(s.maxQuotes_ > 0 && s.maxQuotes_ <= intMax && s.maxExpiries_ > 0 && s.maxExpiries_ <= intMax - 2 && s.maxMarketTerms_ > 0 &&
                    s.maxMarketTerms_ <= intMax,
                "InvalidMarketIvRequest: size limits must be positive representable int");
        MarketIvDetail::CheckedMultiply(4, MarketIvDetail::CheckedAdd(s.maxIterationsPerStart_, 1));
    }
    void ValidateMarketIvEvaluationSettings(const MarketIvEvaluationSettings_& s) {
        Positive(s.densityFactorFloor_, "settings.density_factor_floor");
        Positive(s.minQueryTime_, "settings.min_query_time");
        Positive(s.minTotalVariance_, "settings.min_total_variance");
    }
    void ValidateMarketIvFitRequest(const MarketIvFitRequest_& request) {
        ValidateMarketIvFitSettings(request.settings_);
        ValidateMarket(request.market_);
        const auto& quotes = request.quotes_;
        const auto& terms = request.market_.Terms();
        const auto& s = request.settings_;
        REQUIRE(!quotes.empty(), "InvalidMarketIvRequest: quotes; at least one quote required");
        REQUIRE(quotes.size() <= s.maxQuotes_ && terms.size() <= s.maxMarketTerms_,
                "MarketIvBudgetExceeded: quotes/market.terms exceeds configured count limit");
        size_t quotedTerms = 0;
        for (const auto& t : terms)
            quotedTerms += std::any_of(quotes.begin(), quotes.end(), [&](const auto& q) { return q.expiry_ == t.expiry_; });
        REQUIRE(quotedTerms <= s.maxExpiries_, "MarketIvBudgetExceeded: quoted expiries exceeds max_expiries");
        MarketIvDetail::PlanFit(quotes.size(), quotedTerms, terms.size(), s);
        for (size_t i = 0; i < quotes.size(); ++i) {
            const auto& q = quotes[i];
            const String_ path = "quotes[row=" + String_(std::to_string(i)) + ";id=" + q.id_ + "]";
            REQUIRE(!q.id_.empty() && q.id_.find('\0') == String_::npos, "InvalidMarketIvRequest: " + path + ".id; expected nonempty ID without NUL");
            const auto term = std::find_if(terms.begin(), terms.end(), [&](const auto& t) { return t.expiry_ == q.expiry_; });
            REQUIRE(term != terms.end(), "InvalidMarketIvRequest: " + path + ".expiry; not a market term");
            Positive(q.strike_, path + ".strike");
            Positive(q.weight_, path + ".weight");
            REQUIRE(q.optionType_ == OptionType_::Value_::CALL || q.optionType_ == OptionType_::Value_::PUT,
                    "InvalidMarketIvRequest: " + path + ".option_type; expected CALL/PUT");
            REQUIRE(q.quoteType_ == MarketIvQuoteType_::Value_::PRICE || q.quoteType_ == MarketIvQuoteType_::Value_::DECIMAL_IV,
                    "InvalidMarketIvRequest: " + path + ".quote_type; expected PRICE/DECIMAL_IV");
            REQUIRE(q.bid_.has_value() == q.ask_.has_value(), "InvalidMarketIvRequest: " + path + "; bid/ask must both be present or empty");
            ValidateQuoteValue(q.mid_, q, *term, path + ".mid", false);
            if (q.bid_) {
                ValidateQuoteValue(*q.bid_, q, *term, path + ".bid", true);
                ValidateQuoteValue(*q.ask_, q, *term, path + ".ask", false);
                REQUIRE(*q.bid_ <= q.mid_ && q.mid_ <= *q.ask_, "InvalidMarketIvRequest: " + path + "; expected bid <= mid <= ask");
            }
        }
        ValidateQuoteAxes(quotes);
    }

    namespace MarketIvDetail {
        Carry_ CarryAt(const MarketIvMarket_& market, double time) {
            if (market.IsFlatCarry())
                return {market.Spot() * std::exp((market.Rate() - market.DividendYield()) * time), std::exp(-market.Rate() * time), market.Rate(),
                        market.DividendYield(), false};
            const auto& terms = market.Terms();
            size_t hi = 0;
            while (hi < terms.size() && (terms[hi].expiry_ - market.EvaluationDate()) / 365. <= time)
                ++hi;
            const bool knot = hi > 0 && (terms[hi - 1].expiry_ - market.EvaluationDate()) / 365. == time;
            if (hi == terms.size())
                hi = terms.size() - 1;
            const double t1 = (terms[hi].expiry_ - market.EvaluationDate()) / 365.;
            const double t0 = hi == 0 ? 0. : (terms[hi - 1].expiry_ - market.EvaluationDate()) / 365.;
            const double d1 = std::log(terms[hi].discount_), f1 = std::log(terms[hi].forward_) - std::log(market.Spot());
            const double d0 = hi == 0 ? 0. : std::log(terms[hi - 1].discount_);
            const double f0 = hi == 0 ? 0. : std::log(terms[hi - 1].forward_) - std::log(market.Spot());
            const double ds = (d1 - d0) / (t1 - t0), fs = (f1 - f0) / (t1 - t0);
            double f = market.Spot() * std::exp(f0 + fs * (time - t0)), d = std::exp(d0 + ds * (time - t0));
            for (const auto& term : terms)
                if ((term.expiry_ - market.EvaluationDate()) / 365. == time) {
                    f = term.forward_;
                    d = term.discount_;
                    break;
                }
            return {f, d, -ds, -ds - fs, knot};
        }
        void ValidateQuery(const MarketIvQuery_& query) {
            Positive(query.strike_, "query.strike");
            Positive(query.time_, "query.time");
        }
        size_t FitPayload(const FitData_& d) {
            size_t bytes = sizeof(d) + Bytes(d.request_.quotes_) + Bytes(d.request_.market_.Terms()) + Bytes(d.quotes_) + Bytes(d.starts_) +
                           Bytes(d.iterations_) + Bytes(d.expiryTimes_) + Bytes(d.parameters_) + Bytes(d.signature_);
            bytes +=
                Bytes(d.curve_.times_) + Bytes(d.curve_.values_) + Bytes(d.curve_.slopes_) + Bytes(d.curve_.lowerBounds_) + Bytes(d.curve_.branches_);
            bytes += Bytes(d.certificate_.derivativeLowerBounds_) + Bytes(d.certificate_.increments_) + Bytes(d.certificate_.limiterBranches_);
            for (const auto& start : d.starts_)
                bytes += Bytes(start.parameters_);
            return bytes;
        }
    } // namespace MarketIvDetail

    const MarketIvDetail::FitData_& MarketIvData(const MarketIvCalibrationSnapshot_& s) { return *s.data_; }
    const MarketIvFitRequest_& MarketIvCalibrationSnapshot_::Request() const { return data_->request_; }
    const Vector_<>& MarketIvCalibrationSnapshot_::Times() const { return data_->curve_.times_; }
    const Vector_<>& MarketIvCalibrationSnapshot_::Theta() const { return data_->curve_.values_; }
    const Vector_<>& MarketIvCalibrationSnapshot_::Slopes() const { return data_->curve_.slopes_; }
    const MarketIvCertificate_& MarketIvCalibrationSnapshot_::Certificate() const { return data_->certificate_; }
    double MarketIvCalibrationSnapshot_::Rho() const { return data_->parameters_[data_->expiryTimes_.size()]; }
    double MarketIvCalibrationSnapshot_::Eta() const { return data_->parameters_[data_->expiryTimes_.size() + 1]; }
    const Vector_<int>& MarketIvCalibrationSnapshot_::RegimeSignature() const { return data_->signature_; }
    const MarketIvExecution_& MarketIvCalibrationSnapshot_::Execution() const { return data_->execution_; }
    bool MarketIvFitResult_::IsSuccess() const { return data_->status_ == "SUCCESS" || data_->status_ == "SUCCESS_WITH_WARNINGS"; }
    const String_& MarketIvFitResult_::Status() const { return data_->status_; }
    const String_& MarketIvFitResult_::Failure() const { return data_->failure_; }
    MarketIvCalibrationSnapshot_ MarketIvFitResult_::Snapshot() const {
        REQUIRE(IsSuccess(), "MarketIvSnapshotUnavailable: " + data_->status_ + "; " + data_->failure_);
        return MarketIvCalibrationSnapshot_(data_);
    }
    const MarketIvFitRequest_& MarketIvFitResult_::Request() const { return data_->request_; }
    const Vector_<MarketIvQuoteDiagnostic_>& MarketIvFitResult_::Quotes() const { return data_->quotes_; }
    const Vector_<MarketIvIteration_>& MarketIvFitResult_::Iterations() const { return data_->iterations_; }
    const Vector_<MarketIvStart_>& MarketIvFitResult_::Starts() const { return data_->starts_; }
    const MarketIvExecution_& MarketIvFitResult_::Execution() const { return data_->execution_; }
    int MarketIvFitResult_::SelectedStart() const { return data_->selectedStart_; }
    bool MarketIvFitResult_::Underidentified() const { return data_->underidentified_; }
    bool MarketIvFitResult_::NearTie() const { return data_->nearTie_; }
} // namespace Dal
