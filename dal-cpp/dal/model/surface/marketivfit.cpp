#include <dal/platform/platform.hpp>

#include <algorithm>
#include <numeric>
#include <tuple>

#include <dal/model/surface/marketivsurfaceinternal.hpp>

namespace Dal::MarketIvDetail {
    namespace {
        bool Eligible(const MarketIvStart_& s) { return s.converged_ && s.certified_ && s.qualityAccepted_ && s.objective_.has_value(); }
    } // namespace
    int SelectStart(const Vector_<MarketIvStart_>& starts, double tieTolerance) {
        double minimum = std::numeric_limits<double>::infinity();
        for (const auto& start : starts)
            if (Eligible(start))
                minimum = std::min(minimum, *start.objective_);
        for (size_t i = 0; i < starts.size(); ++i)
            if (Eligible(starts[i]) && *starts[i].objective_ - minimum <= tieTolerance)
                return static_cast<int>(i);
        return -1;
    }
    namespace {
        constexpr size_t FIXED_FIT_FRAME_BYTES = 8192;
        bool NearTie(const Vector_<MarketIvStart_>& starts, int selected, double tolerance) {
            for (int i = 0; i < 4; ++i) {
                const auto& s = starts[i];
                if (i != selected && s.converged_ && s.certified_ && s.qualityAccepted_ &&
                    std::abs(*s.objective_ - *starts[selected].objective_) <= tolerance)
                    return true;
            }
            return false;
        }
        struct Workspace_ {
            Vector_<> jac_, rank_, lower_, gradient_, step_, x_, candidate_, best_, prices_, trial_, bestPrices_, theta_, candidatePrices_;
            bool pricesValid_ = false;
            Workspace_(size_t q, size_t p)
                : jac_(q * p), rank_(q * p), lower_(p * p), gradient_(p), step_(p), x_(p), candidate_(p), best_(p), prices_(q), trial_(q),
                  bestPrices_(q), theta_(p), candidatePrices_(4 * q) {}
            size_t BytesUsed() const {
                return sizeof(*this) + Bytes(jac_) + Bytes(rank_) + Bytes(lower_) + Bytes(gradient_) + Bytes(step_) + Bytes(x_) + Bytes(candidate_) +
                       Bytes(best_) + Bytes(prices_) + Bytes(trial_) + Bytes(bestPrices_) + Bytes(theta_) + Bytes(candidatePrices_);
            }
        };
        struct WorkLimit_ {};
        double Bound(const MarketIvFitSettings_& s, size_t m, size_t j, bool upper) {
            if (j < m)
                return upper ? s.maxVarianceIncrement_ : (j == 0 ? s.thetaFloor_ : 0.);
            if (j == m)
                return upper ? s.rhoLimit_ : -s.rhoLimit_;
            return upper ? EtaMax(s) : 0.;
        }
        void
        JacobianRow(Workspace_* w, const MarketIvQuoteDiagnostic_& q, const Smile_& smile, double factor, size_t row, size_t m, size_t dimension) {
            REQUIRE(!smile.derivativeRange_, "MarketIvNumericRange: analytic model Jacobian underflow/overflow");
            const double root = std::sqrt(smile.w_), d1 = -q.logMoneyness_ / root + .5 * root;
            const double vega = std::exp(-.5 * d1 * d1) / std::sqrt(2. * std::acos(-1.)) / (2. * root) * factor;
            for (size_t j = 0; j < dimension; ++j) {
                const double dw = j < m ? (j <= q.expiryIndex_ ? smile.wTheta_ : 0.) : (j == m ? smile.wRho_ : smile.wEta_);
                w->jac_[row * (m + 2) + j] = vega * dw;
                REQUIRE(std::isfinite(w->jac_[row * (m + 2) + j]), "MarketIvNumericRange: model Jacobian overflow");
            }
        }
        double ModelRows(FitData_* data, Workspace_* w, const Vector_<>& x, bool jacobian, Vector_<>* prices, size_t dimension) {
            const size_t m = data->expiryTimes_.size();
            double sum = 0., objective = 0.;
            for (size_t j = 0; j < m; ++j)
                w->theta_[j] = sum += x[j];
            for (size_t row = 0; row < data->quotes_.size(); ++row) {
                if (data->execution_.modelRowEvaluations_ == data->rowLimit_)
                    throw WorkLimit_();
                ++data->execution_.modelRowEvaluations_;
                const auto& q = data->quotes_[row];
                const auto smile = Smile(w->theta_[q.expiryIndex_], x[m], x[m + 1], q.logMoneyness_);
                const double price = NormalizedBlack(q.logMoneyness_, smile.w_);
                const double factor = std::sqrt(q.normalizedWeight_) / q.scale_;
                const double residual = factor * (price - q.normalizedMid_);
                (*prices)[row] = price;
                objective += .5 * residual * residual;
                if (jacobian)
                    JacobianRow(w, q, smile, factor, row, m, dimension);
            }
            REQUIRE(std::isfinite(objective), "MarketIvNumericRange: objective overflow");
            return objective;
        }
        bool GradientBlocked(const FitData_& d, const Workspace_& w, size_t m, size_t j) {
            return (w.x_[j] == Bound(d.request_.settings_, m, j, false) && w.gradient_[j] > 0.) ||
                   (w.x_[j] == Bound(d.request_.settings_, m, j, true) && w.gradient_[j] < 0.);
        }
        std::pair<double, double> Gradient(const FitData_& d, Workspace_* w, size_t dimension) {
            const size_t p = d.expiryTimes_.size() + 2;
            double norm = 0., scale = 1., residualNorm = 0.;
            std::fill(w->gradient_.begin(), w->gradient_.end(), 0.);
            for (size_t row = 0; row < d.quotes_.size(); ++row) {
                const auto& q = d.quotes_[row];
                const double residual = std::sqrt(q.normalizedWeight_) / q.scale_ * (w->prices_[row] - q.normalizedMid_);
                residualNorm += residual * residual;
                for (size_t j = 0; j < dimension; ++j)
                    w->gradient_[j] += w->jac_[row * p + j] * residual;
            }
            for (size_t j = 0; j < dimension; ++j) {
                const bool blocked = GradientBlocked(d, *w, p - 2, j);
                if (blocked)
                    w->gradient_[j] = 0.;
                norm = std::max(norm, std::abs(w->gradient_[j]));
                double column = 0.;
                for (size_t row = 0; row < d.quotes_.size(); ++row)
                    column += w->jac_[row * p + j] * w->jac_[row * p + j];
                scale = std::max(scale, std::sqrt(column));
            }
            const double threshold = d.request_.settings_.projectedGradientTolerance_ * scale * std::max(1., std::sqrt(residualNorm));
            REQUIRE(std::isfinite(norm) && std::isfinite(threshold), "MarketIvNumericRange: projected gradient overflow");
            return {norm, threshold};
        }
        double DampedNormalElement(double value, double damping, bool diagonal) {
            if (diagonal)
                value += damping * std::max(1., value);
            return value;
        }
        bool ValidFactor(double value, bool diagonal) { return std::isfinite(value) && (!diagonal || value > 0.); }
        void ForwardRow(Workspace_* w, size_t p, size_t i) {
            double value = -w->gradient_[i];
            for (size_t j = 0; j < i; ++j)
                value -= w->lower_[i * p + j] * w->step_[j];
            w->step_[i] = value / w->lower_[i * p + i];
        }
        void BackwardSubstitution(Workspace_* w, size_t p, size_t dimension) {
            for (size_t i = dimension; i-- > 0;) {
                for (size_t j = i + 1; j < dimension; ++j)
                    w->step_[i] -= w->lower_[j * p + i] * w->step_[j];
                w->step_[i] /= w->lower_[i * p + i];
            }
        }
        void Step(const FitData_& d, Workspace_* w, size_t dimension, double damping) {
            const size_t p = d.expiryTimes_.size() + 2;
            for (size_t i = 0; i < dimension; ++i) {
                for (size_t j = 0; j <= i; ++j) {
                    double value = 0.;
                    if (w->gradient_[i] != 0. && w->gradient_[j] != 0.)
                        for (size_t row = 0; row < d.quotes_.size(); ++row)
                            value += w->jac_[row * p + i] * w->jac_[row * p + j];
                    value = DampedNormalElement(value, damping, i == j);
                    for (size_t k = 0; k < j; ++k)
                        value -= w->lower_[i * p + k] * w->lower_[j * p + k];
                    w->lower_[i * p + j] = i == j ? std::sqrt(value) : value / w->lower_[j * p + j];
                    REQUIRE(ValidFactor(w->lower_[i * p + j], i == j), "MarketIvNumericRange: damped normal equation failed");
                }
                ForwardRow(w, p, i);
            }
            BackwardSubstitution(w, p, dimension);
        }
        double ColumnNorm(const Workspace_& w, size_t p, size_t q, size_t j) {
            double sum = 0.;
            for (size_t row = 0; row < q; ++row)
                sum += w.rank_[row * p + j] * w.rank_[row * p + j];
            return std::sqrt(sum);
        }
        size_t Pivot(const Workspace_& w, size_t p, size_t q, size_t i, size_t dimension) {
            size_t best = i;
            for (size_t j = i + 1; j < dimension; ++j)
                if (ColumnNorm(w, p, q, j) > ColumnNorm(w, p, q, best))
                    best = j;
            return best;
        }
        void Orthogonalize(Workspace_* w, size_t p, size_t q, size_t i, size_t dimension) {
            for (size_t j = i + 1; j < dimension; ++j)
                for (int pass = 0; pass < 2; ++pass) {
                    double dot = 0.;
                    for (size_t row = 0; row < q; ++row)
                        dot += w->rank_[row * p + i] * w->rank_[row * p + j];
                    for (size_t row = 0; row < q; ++row)
                        w->rank_[row * p + j] -= dot * w->rank_[row * p + i];
                }
        }
        void Rank(const FitData_& d, Workspace_* w, size_t dimension, MarketIvStart_* start) {
            const size_t p = d.expiryTimes_.size() + 2, q = d.quotes_.size();
            std::copy(w->jac_.begin(), w->jac_.end(), w->rank_.begin());
            double largest = 0., smallest = std::numeric_limits<double>::infinity();
            for (size_t j = 0; j < dimension; ++j)
                largest = std::max(largest, ColumnNorm(*w, p, q, j));
            for (size_t i = 0; i < dimension; ++i) {
                const size_t best = Pivot(*w, p, q, i, dimension);
                const double length = ColumnNorm(*w, p, q, best);
                if (length == 0. || length <= largest * 1e-8)
                    break;
                ++start->rank_;
                smallest = std::min(smallest, length);
                for (size_t row = 0; row < q; ++row) {
                    std::swap(w->rank_[row * p + best], w->rank_[row * p + i]);
                    w->rank_[row * p + i] /= length;
                }
                Orthogonalize(w, p, q, i, dimension);
            }
            if (start->rank_ == static_cast<int>(dimension))
                start->conditionEstimate_ = largest / smallest;
        }
        ThetaCurve_ Curve(const FitData_& data, const Vector_<>& x) {
            Vector_<> times(data.expiryTimes_.size() + 1), values(times.size(), 0.);
            for (size_t j = 0; j < data.expiryTimes_.size(); ++j) {
                times[j + 1] = data.expiryTimes_[j];
                values[j + 1] = values[j] + x[j];
            }
            auto curve = NewThetaCurve(times, values);
            curve.peakWorkspaceBytes_ += Bytes(times) + Bytes(values) + 2 * sizeof(Vector_<>);
            return curve;
        }
        bool Quality(const FitData_& data, const Vector_<>& prices) {
            for (size_t i = 0; i < prices.size(); ++i)
                if (prices[i] < data.quotes_[i].normalizedBid_ - data.request_.settings_.feasibilityTolerance_ ||
                    prices[i] > data.quotes_[i].normalizedAsk_ + data.request_.settings_.feasibilityTolerance_)
                    return false;
            return true;
        }
        MarketIvQuoteDiagnostic_ PrepareQuote(FitData_* data, size_t row, double maxWeight, double sumWeight) {
            const auto& request = data->request_;
            const auto& axis = data->expiryTimes_;
            const auto& q = request.quotes_[row];
            MarketIvQuoteDiagnostic_ d;
            data->execution_.peakWorkspaceBytes_ = std::max(data->execution_.peakWorkspaceBytes_, sizeof(d));
            d.inputRow_ = row;
            d.time_ = (q.expiry_ - request.market_.EvaluationDate()) / 365.;
            d.expiryIndex_ = std::lower_bound(axis.begin(), axis.end(), d.time_) - axis.begin();
            const auto carry = CarryAt(request.market_, d.time_);
            d.forward_ = carry.forward_;
            d.discount_ = carry.discount_;
            d.logMoneyness_ = std::log(q.strike_) - std::log(d.forward_);
            auto convert = [&](double value) {
                ++data->execution_.quoteConversions_;
                if (q.quoteType_ == MarketIvQuoteType_::Value_::PRICE)
                    return NormalizePrice(value, q.optionType_ == OptionType_::Value_::CALL, d.forward_, q.strike_, d.discount_,
                                          .01 * std::min({request.settings_.normalizedPriceScaleFloor_, request.settings_.feasibilityTolerance_,
                                                          request.settings_.normalizedPriceTolerance_}));
                const long double variance = static_cast<long double>(value) * value * d.time_;
                const double represented = static_cast<double>(variance);
                REQUIRE(std::isfinite(represented) && (value == 0. || represented > 0.),
                        "MarketIvNumericRange: quotes[id=" + q.id_ + "]; positive IV total variance overflow/underflow");
                return NormalizedBlack(d.logMoneyness_, represented);
            };
            d.normalizedMid_ = convert(q.mid_);
            if (q.bid_) {
                d.normalizedBid_ = convert(*q.bid_);
                d.normalizedAsk_ = convert(*q.ask_);
            } else if (q.quoteType_ == MarketIvQuoteType_::Value_::PRICE) {
                d.normalizedBid_ =
                    std::max(std::max(-std::expm1(d.logMoneyness_), 0.), d.normalizedMid_ - request.settings_.normalizedPriceTolerance_);
                d.normalizedAsk_ = std::min(1., d.normalizedMid_ + request.settings_.normalizedPriceTolerance_);
            } else {
                d.normalizedBid_ = convert(std::max(0., q.mid_ - request.settings_.decimalIvTolerance_));
                d.normalizedAsk_ = convert(q.mid_ + request.settings_.decimalIvTolerance_);
            }
            d.scale_ = std::max((d.normalizedAsk_ - d.normalizedBid_) / 2., request.settings_.normalizedPriceScaleFloor_);
            d.normalizedWeight_ = (q.weight_ / maxWeight) / sumWeight;
            return d;
        }
        std::pair<long double, long double> PremiumBand(const MarketIvQuote_& quote, long double df, long double dk, long double width, bool call) {
            const long double lo = quote.bid_ ? *quote.bid_ : std::max(std::max(call ? df - dk : dk - df, 0.L), quote.mid_ - width);
            const long double hi = quote.ask_ ? *quote.ask_ : std::min(call ? df : dk, quote.mid_ + width);
            return {lo, hi};
        }
        bool PriceParityContradiction(const MarketIvQuote_& a,
                                      const MarketIvQuote_& b,
                                      const MarketIvQuoteDiagnostic_& d,
                                      const MarketIvFitSettings_& settings) {
            if (a.quoteType_ != MarketIvQuoteType_::Value_::PRICE || b.quoteType_ != MarketIvQuoteType_::Value_::PRICE)
                return false;
            const auto& call = a.optionType_ == OptionType_::Value_::CALL ? a : b;
            const auto& put = a.optionType_ == OptionType_::Value_::PUT ? a : b;
            const long double df = static_cast<long double>(d.discount_) * d.forward_, dk = static_cast<long double>(d.discount_) * a.strike_;
            const long double width = settings.normalizedPriceTolerance_ * df;
            const auto callBand = PremiumBand(call, df, dk, width, true);
            const auto putBand = PremiumBand(put, df, dk, width, false);
            const long double callLo = callBand.first, callHi = callBand.second, putLo = putBand.first, putHi = putBand.second;
            const long double error = 64.L * std::numeric_limits<long double>::epsilon() * (std::abs(df) + std::abs(dk) + callHi + putHi);
            if (!std::isfinite(error))
                return false;
            return callLo - putHi - (df - dk) > error || callHi - putLo - (df - dk) < -error;
        }
        void CheckParity(FitData_* data) {
            const auto& quotes = data->request_.quotes_;
            Vector_<size_t> order(quotes.size());
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(), [&](size_t i, size_t j) {
                return std::tie(quotes[i].expiry_, quotes[i].strike_) < std::tie(quotes[j].expiry_, quotes[j].strike_);
            });
            data->execution_.peakWorkspaceBytes_ =
                std::max(data->execution_.peakWorkspaceBytes_, FIXED_FIT_FRAME_BYTES + sizeof(order) + Bytes(order));
            for (size_t pos = 1; pos < order.size(); ++pos) {
                const size_t i = order[pos], j = order[pos - 1];
                if (quotes[i].expiry_ != quotes[j].expiry_ || quotes[i].strike_ != quotes[j].strike_)
                    continue;
                auto& a = data->quotes_[i];
                auto& b = data->quotes_[j];
                const double gap = std::max(a.normalizedBid_, b.normalizedBid_) - std::min(a.normalizedAsk_, b.normalizedAsk_);
                a.parityGap_ = b.parityGap_ = gap;
                if (gap > 0. && PriceParityContradiction(quotes[i], quotes[j], a, data->request_.settings_)) {
                    data->status_ = "INCONSISTENT_QUOTES";
                    data->failure_ = "Disjoint call/put acceptable bands: " + quotes[i].id_ + "; " + quotes[j].id_;
                }
            }
        }
        void Prepare(FitData_* data) {
            const auto& request = data->request_;
            auto& axis = data->expiryTimes_;
            axis.reserve(request.market_.Terms().size());
            for (const auto& term : request.market_.Terms())
                if (std::any_of(request.quotes_.begin(), request.quotes_.end(), [&](const auto& q) { return q.expiry_ == term.expiry_; }))
                    axis.push_back((term.expiry_ - request.market_.EvaluationDate()) / 365.);
            data->execution_ = PlanFit(request.quotes_.size(), axis.size(), request.market_.Terms().size(), request.settings_);
            data->execution_.peakWorkspaceBytes_ = FIXED_FIT_FRAME_BYTES + sizeof(Vector_<size_t>) + sizeof(size_t) * request.quotes_.size();
            data->quotes_.reserve(request.quotes_.size());
            data->starts_ = Vector_<MarketIvStart_>(4);
            data->iterations_.reserve(4 * (static_cast<size_t>(request.settings_.maxIterationsPerStart_) + 1));
            double maxWeight = 0., sumWeight = 0.;
            for (const auto& q : request.quotes_)
                maxWeight = std::max(maxWeight, q.weight_);
            for (const auto& q : request.quotes_)
                sumWeight += q.weight_ / maxWeight;
            for (size_t row = 0; row < request.quotes_.size(); ++row)
                data->quotes_.push_back(PrepareQuote(data, row, maxWeight, sumWeight));
            CheckParity(data);
        }
        void Seed(FitData_* data, Workspace_* w, int index) {
            w->pricesValid_ = false;
            const size_t m = data->expiryTimes_.size();
            const auto& s = data->request_.settings_;
            double previous = 0.;
            for (size_t j = 0; j < m + 2; ++j) {
                const double raw = j < m    ? s.initialVolatility_ * s.initialVolatility_ * (data->expiryTimes_[j] - previous)
                                   : j == m ? (index == 0 ? 0. : .5 * (index - 2))
                                            : (index == 0 ? 0. : .5);
                REQUIRE(std::isfinite(raw), "MarketIvNumericRange: initial variance seed is not representable");
                const double projected = std::clamp(raw, Bound(s, m, j, false), Bound(s, m, j, true));
                data->starts_[index].projectedSeed_ |= projected != raw;
                w->x_[j] = projected;
                if (j < m)
                    previous = data->expiryTimes_[j];
            }
            if (index == 0)
                w->x_[m] = w->x_[m + 1] = 0.;
        }
        bool
        LineSearch(FitData_* data, Workspace_* w, size_t dimension, double objective, double* damping, bool* stalled, MarketIvIteration_* history) {
            const auto& s = data->request_.settings_;
            const size_t m = data->expiryTimes_.size();
            for (int trial = 0; trial < s.maxLineSearchTrials_; ++trial) {
                ++history->trials_;
                Step(*data, w, dimension, *damping);
                w->candidate_ = w->x_;
                double distance = 0.;
                for (size_t j = 0; j < dimension; ++j) {
                    w->candidate_[j] = std::clamp(w->x_[j] + w->step_[j], Bound(s, m, j, false), Bound(s, m, j, true));
                    distance = std::max(distance, std::abs(w->candidate_[j] - w->x_[j]));
                }
                const double next = ModelRows(data, w, w->candidate_, false, &w->trial_, dimension);
                if (next < objective) {
                    w->x_ = w->candidate_;
                    w->prices_ = w->trial_;
                    *damping = std::max(*damping / 3., 1e-12);
                    *stalled = distance <= s.parameterStepTolerance_;
                    return true;
                }
                *damping *= 10.;
            }
            return false;
        }
        void CertifyStart(FitData_* data, Workspace_* w, int index, size_t dimension) {
            auto& start = data->starts_[index];
            Rank(*data, w, dimension, &start);
            const auto curve = Curve(*data, w->x_);
            data->execution_.peakWorkspaceBytes_ =
                std::max(data->execution_.peakWorkspaceBytes_, FIXED_FIT_FRAME_BYTES + w->BytesUsed() + curve.peakWorkspaceBytes_);
            start.certified_ = curve.certified_;
            start.qualityAccepted_ = Quality(*data, w->prices_);
            if (!start.certified_ || !start.qualityAccepted_)
                return;
            std::copy(w->prices_.begin(), w->prices_.end(), w->candidatePrices_.begin() + index * data->quotes_.size());
        }
        void RunStart(FitData_* data, Workspace_* w, int index) {
            Seed(data, w, index);
            auto& start = data->starts_[index];
            const auto& s = data->request_.settings_;
            const size_t m = data->expiryTimes_.size(), dimension = index == 0 ? m : m + 2;
            double damping = 1e-3;
            bool stalled = false;
            start.termination_ = "MAX_ITERATIONS";
            for (int iteration = 0; iteration < s.maxIterationsPerStart_; ++iteration) {
                start.iterations_ = iteration + 1;
                const double objective = ModelRows(data, w, w->x_, true, &w->prices_, dimension);
                w->pricesValid_ = true;
                const auto gradient = Gradient(*data, w, dimension);
                start.objective_ = objective;
                start.projectedGradient_ = gradient.first;
                MarketIvIteration_ history{index, iteration, 0, objective, gradient.first, gradient.second, damping};
                if (gradient.first <= gradient.second) {
                    start.termination_ = "PROJECTED_GRADIENT";
                    start.converged_ = true;
                    data->iterations_.push_back(history);
                    break;
                }
                if (stalled) {
                    start.termination_ = "STEP_STALLED";
                    data->iterations_.push_back(history);
                    break;
                }
                const bool accepted = LineSearch(data, w, dimension, objective, &damping, &stalled, &history);
                data->iterations_.push_back(history);
                if (!accepted) {
                    start.termination_ = "NO_DESCENT";
                    break;
                }
            }
            start.parameters_ = w->x_;
            if (!start.converged_)
                return;
            CertifyStart(data, w, index, dimension);
        }
        void Reject(FitData_* data, Workspace_* w) {
            const bool converged = std::any_of(data->starts_.begin(), data->starts_.end(), [](const auto& s) { return s.converged_; });
            const bool certified =
                std::any_of(data->starts_.begin(), data->starts_.end(), [](const auto& s) { return s.converged_ && s.certified_; });
            const bool numeric =
                std::all_of(data->starts_.begin(), data->starts_.end(), [](const auto& s) { return s.termination_ == "NUMERIC_RANGE"; });
            data->status_ = numeric ? "NUMERIC_RANGE" : converged ? (certified ? "FIT_QUALITY_FAILURE" : "CONSTRAINT_FAILURE") : "NO_CONVERGENCE";
            data->failure_ = "No start passed projected gradient, full-domain certificate and every quote band";
            data->parameters_ = w->x_;
            if (w->pricesValid_)
                for (size_t row = 0; row < data->quotes_.size(); ++row) {
                    auto& q = data->quotes_[row];
                    q.fittedNormalizedCall_ = w->prices_[row];
                    q.normalizedResidual_ = w->prices_[row] - q.normalizedMid_;
                    q.bandViolation_ = std::max({0., q.normalizedBid_ - w->prices_[row], w->prices_[row] - q.normalizedAsk_});
                    q.withinBand_ = *q.bandViolation_ <= data->request_.settings_.feasibilityTolerance_;
                }
        }
        void CertifyWinner(FitData_* data, Workspace_* w) {
            const size_t m = data->expiryTimes_.size();
            data->parameters_ = w->best_;
            if (data->parameters_[m + 1] == 0.)
                data->parameters_[m] = 0.;
            data->curve_ = Curve(*data, data->parameters_);
            auto& cert = data->certificate_;
            const auto& s = data->request_.settings_;
            cert.rhoMargin_ = s.rhoLimit_ - std::abs(data->parameters_[m]);
            cert.etaMargin_ = EtaMax(s) - data->parameters_[m + 1];
            const auto bounds = GlobalBounds(data->parameters_[m + 1], s.rhoLimit_);
            cert.butterflyBound_ = bounds.first;
            cert.wingSlopeBound_ = bounds.second;
            cert.derivativeLowerBounds_ = data->curve_.lowerBounds_;
            cert.limiterBranches_ = data->curve_.branches_;
            cert.increments_ = Vector_<>(data->parameters_.begin(), data->parameters_.begin() + m);
            cert.certified_ = data->curve_.certified_ && ParametersCertified(s, data->parameters_[m], data->parameters_[m + 1]);
        }
        void QuoteDiagnostics(FitData_* data, Workspace_* w) {
            const size_t m = data->expiryTimes_.size();
            const auto& s = data->request_.settings_;
            for (size_t row = 0; row < data->quotes_.size(); ++row) {
                auto& q = data->quotes_[row];
                const auto& input = data->request_.quotes_[row];
                const double price = w->bestPrices_[row];
                q.fittedNormalizedCall_ = price;
                q.normalizedResidual_ = price - q.normalizedMid_;
                q.bandViolation_ = std::max({0., q.normalizedBid_ - price, price - q.normalizedAsk_});
                q.withinBand_ = *q.bandViolation_ <= s.feasibilityTolerance_;
                q.bandBranch_ = price < q.normalizedBid_                              ? -2
                                : price > q.normalizedAsk_                            ? 2
                                : price - q.normalizedBid_ <= s.feasibilityTolerance_ ? -1
                                : q.normalizedAsk_ - price <= s.feasibilityTolerance_ ? 1
                                                                                      : 0;
                data->signature_.push_back(q.bandBranch_);
                const double theta = data->curve_.values_[q.expiryIndex_ + 1];
                if (data->execution_.modelRowEvaluations_ == data->rowLimit_)
                    throw WorkLimit_();
                ++data->execution_.modelRowEvaluations_;
                const auto smile = Smile(theta, data->parameters_[m], data->parameters_[m + 1], q.logMoneyness_);
                if (input.quoteType_ == MarketIvQuoteType_::Value_::DECIMAL_IV)
                    q.nativeFitted_ = std::sqrt(smile.w_ / q.time_);
                else
                    q.nativeFitted_ =
                        q.discount_ * q.forward_ * NormalizedBlack(q.logMoneyness_, smile.w_, input.optionType_ == OptionType_::Value_::CALL);
                q.nativeResidual_ = *q.nativeFitted_ - input.mid_;
            }
        }
        void WinnerSignature(FitData_* data) {
            const size_t m = data->expiryTimes_.size();
            const auto& s = data->request_.settings_;
            const auto& selected = data->starts_[data->selectedStart_];
            data->nearTie_ = NearTie(data->starts_, data->selectedStart_, s.objectiveTieTolerance_);
            const int dimension = static_cast<int>(m + (data->selectedStart_ == 0 ? 0 : 2));
            data->underidentified_ = selected.rank_ < dimension || data->quotes_.size() < m + 2;
            data->signature_.reserve(m * 2 + data->quotes_.size() + 16);
            data->signature_.push_back(data->selectedStart_);
            data->signature_.push_back(selected.rank_);
            data->signature_.push_back(data->nearTie_);
            for (size_t j = 0; j < m + 2; ++j)
                data->signature_.push_back(data->parameters_[j] == Bound(s, m, j, false) ? -1 : data->parameters_[j] == Bound(s, m, j, true) ? 1 : 0);
            for (const auto branch : data->certificate_.limiterBranches_)
                data->signature_.push_back(branch);
        }
        void Finish(FitData_* data, Workspace_* w) {
            data->selectedStart_ = SelectStart(data->starts_, data->request_.settings_.objectiveTieTolerance_);
            if (data->selectedStart_ < 0) {
                Reject(data, w);
                return;
            }
            w->best_ = data->starts_[data->selectedStart_].parameters_;
            std::copy_n(w->candidatePrices_.begin() + data->selectedStart_ * data->quotes_.size(), data->quotes_.size(), w->bestPrices_.begin());
            CertifyWinner(data, w);
            const auto& cert = data->certificate_;
            if (!cert.certified_) {
                data->status_ = "CONSTRAINT_FAILURE";
                data->failure_ = "Final stored representation failed its full-domain certificate";
                return;
            }
            WinnerSignature(data);
            QuoteDiagnostics(data, w);
            const bool numericStart =
                std::any_of(data->starts_.begin(), data->starts_.end(), [](const auto& start) { return start.termination_ == "NUMERIC_RANGE"; });
            data->status_ = data->underidentified_ || data->nearTie_ || numericStart ? "SUCCESS_WITH_WARNINGS" : "SUCCESS";
            data->failure_.clear();
        }
    } // namespace

    std::shared_ptr<const FitData_> Fit(const MarketIvFitRequest_& request, std::optional<size_t> aggregateRemaining) {
        auto data = std::make_shared<FitData_>(request);
        data->rowLimit_ = std::min(request.settings_.maxModelRowEvaluations_, aggregateRemaining.value_or(request.settings_.maxModelRowEvaluations_));
        try {
            Prepare(data.get());
            if (data->status_ != "INCONSISTENT_QUOTES") {
                Workspace_ workspace(data->quotes_.size(), data->expiryTimes_.size() + 2);
                data->execution_.jacobianCapacity_ = workspace.jac_.capacity();
                data->execution_.peakWorkspaceBytes_ = FIXED_FIT_FRAME_BYTES + workspace.BytesUsed();
                for (int index = 0; index < 4; ++index) {
                    try {
                        RunStart(data.get(), &workspace, index);
                    } catch (const WorkLimit_&) {
                        data->starts_[index].termination_ = "WORK_LIMIT";
                        data->starts_[index].parameters_ = workspace.x_;
                        throw;
                    } catch (const Exception_& error) {
                        data->starts_[index].termination_ = "NUMERIC_RANGE";
                        data->starts_[index].failure_ = error.what();
                        data->starts_[index].parameters_ = workspace.x_;
                        data->failure_ = error.what();
                    }
                }
                Finish(data.get(), &workspace);
            }
        } catch (const WorkLimit_&) {
            data->status_ = "WORK_LIMIT";
            data->failure_ = "max_model_row_evaluations exhausted before completing fixed four-start search";
        } catch (const Exception_& error) {
            data->status_ = "NUMERIC_RANGE";
            data->failure_ = error.what();
        }
        data->execution_.quoteCapacity_ = data->quotes_.capacity();
        data->execution_.iterationCapacity_ = data->iterations_.capacity();
        data->execution_.numericPayloadBytes_ = FitPayload(*data);
        REQUIRE(data->execution_.numericPayloadBytes_ <= data->execution_.plannedNumericPayloadBytes_ &&
                    data->execution_.peakWorkspaceBytes_ <= data->execution_.plannedWorkspaceBytes_,
                "MarketIvBudgetExceeded: actual fit layout exceeds planned envelope");
        return data;
    }
} // namespace Dal::MarketIvDetail

namespace Dal {
    MarketIvFitResult_ FitMarketIvSurface(const MarketIvFitRequest_& request) {
        ValidateMarketIvFitRequest(request);
        return MarketIvFitResult_(MarketIvDetail::Fit(request));
    }
    MarketIvFitResult_ RefitMarketIvSurface(const MarketIvCalibrationSnapshot_& base, const Vector_<MarketIvQuote_>& quotes) {
        const auto& original = base.Request();
        REQUIRE(quotes.size() == original.quotes_.size(), "InvalidMarketIvRequest: refit quotes must preserve original axis");
        for (size_t i = 0; i < quotes.size(); ++i) {
            const auto& a = original.quotes_[i];
            const auto& b = quotes[i];
            REQUIRE(a.id_ == b.id_ && a.expiry_ == b.expiry_ && a.strike_ == b.strike_ && a.optionType_ == b.optionType_ &&
                        a.quoteType_ == b.quoteType_ && a.weight_ == b.weight_,
                    "InvalidMarketIvRequest: refit quote metadata/weight changed at row=" + String_(std::to_string(i)));
        }
        return FitMarketIvSurface({original.market_, quotes, original.settings_});
    }
} // namespace Dal
