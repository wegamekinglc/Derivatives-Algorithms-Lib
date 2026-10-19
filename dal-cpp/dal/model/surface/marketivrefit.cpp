#include <dal/platform/platform.hpp>

#include <algorithm>
#include <numeric>

#include <dal/model/surface/marketivsurfaceinternal.hpp>

namespace Dal {
    namespace {
        void ValidateIds(const Vector_<String_>& ids) {
            for (const auto& id : ids)
                REQUIRE(!id.empty() && id.find('\0') == String_::npos, "InvalidMarketIvRequest: quote_ids must be nonempty strings without NUL");
            Vector_<size_t> order(ids.size());
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return ids[a] < ids[b]; });
            for (size_t i = 1; i < order.size(); ++i)
                REQUIRE(ids[order[i]] != ids[order[i - 1]], "InvalidMarketIvRequest: duplicate refit quote ID=" + ids[order[i]]);
        }
        struct Selection_ {
            size_t quote_, requested_;
            double step_;
        };
        std::optional<double> Slope(double high, double low, double h) {
            const long double value = (static_cast<long double>(high) - low) / h;
            const double result = static_cast<double>(value);
            return std::isfinite(result) && (value == 0. || result != 0.) ? std::optional<double>(result) : std::nullopt;
        }
        void Bump(MarketIvQuote_* q, double step) {
            auto change = [step](double* value) {
                const double next = *value + step;
                REQUIRE(std::isfinite(next) && next != *value, "BUMP_INVALID: bump is nonfinite or not representable in original quote units");
                *value = next;
            };
            change(&q->mid_);
            if (q->bid_) {
                change(&*q->bid_);
                change(&*q->ask_);
            }
        }
        void Direction(const MarketIvCalibrationSnapshot_& base,
                       const MarketIvRefitRiskRequest_& request,
                       size_t axis,
                       bool plus,
                       size_t totalLimit,
                       MarketIvDetail::RiskData_* data) {
            using namespace MarketIvDetail;
            auto& a = data->axis_[axis];
            auto& status = plus ? a.plusStatus_ : a.minusStatus_;
            auto& failure = plus ? a.plusFailure_ : a.minusFailure_;
            if (data->execution_.modelRowEvaluations_ >= totalLimit) {
                status = "WORK_LIMIT";
                return;
            }
            auto fitRequest = base.Request();
            try {
                Bump(&fitRequest.quotes_[a.quoteIndex_], plus ? a.step_ : -a.step_);
                ValidateMarketIvFitRequest(fitRequest);
            } catch (const Exception_& error) {
                status = "BUMP_INVALID";
                failure = error.what();
                return;
            }
            const auto fit = Access_::Result(Fit(fitRequest, totalLimit - data->execution_.modelRowEvaluations_));
            data->execution_.modelRowEvaluations_ += fit.Execution().modelRowEvaluations_;
            data->execution_.quoteConversions_ += fit.Execution().quoteConversions_;
            data->execution_.peakWorkspaceBytes_ = std::max(data->execution_.peakWorkspaceBytes_,
                                                            fit.Execution().peakWorkspaceBytes_ + fit.Execution().numericPayloadBytes_ +
                                                                sizeof(fitRequest) + Bytes(fitRequest.quotes_) + Bytes(fitRequest.market_.Terms()));
            status = fit.Status();
            failure = fit.Failure();
            if (!fit.IsSuccess())
                return;
            const auto snapshot = fit.Snapshot();
            const bool changed = snapshot.RegimeSignature() != base.RegimeSignature();
            a.regimeChanged_ |= changed;
            (plus ? a.plusStart_ : a.minusStart_) = fit.SelectedStart();
            (plus ? a.plusRank_ : a.minusRank_) = fit.Starts()[fit.SelectedStart()].rank_;
            for (size_t o = 0; o < request.outputs_.size(); ++o) {
                auto& response = data->responses_[axis * request.outputs_.size() + o];
                const auto& output = request.outputs_[o];
                const auto point = EvaluateMarketIvSurface(snapshot, output.query_, request.settings_.evaluation_);
                ++data->execution_.outputEvaluations_;
                (plus ? response.plus_ : response.minus_) = Quantity(point, output.quantity_);
                const auto value = plus ? response.plus_ : response.minus_;
                (plus ? response.plusOutputCode_ : response.minusOutputCode_) = value ? 1 : point.DupireStatus() == "SINGULAR_DENSITY" ? 3 : 2;
                response.regimeChanged_ |= changed;
            }
        }
        Vector_<Selection_> Select(const MarketIvCalibrationSnapshot_& base, const MarketIvRefitRiskRequest_& request) {
            using namespace MarketIvDetail;
            const size_t a = request.quoteIds_.size();
            Vector_<Selection_> selected;
            selected.reserve(a);
            for (size_t i = 0; i < a; ++i) {
                const auto& quotes = base.Request().quotes_;
                const auto it = std::find_if(quotes.begin(), quotes.end(), [&](const auto& q) { return q.id_ == request.quoteIds_[i]; });
                REQUIRE(it != quotes.end(), "InvalidMarketIvRequest: unknown refit quote ID=" + request.quoteIds_[i]);
                const auto carry = CarryAt(base.Request().market_, (it->expiry_ - base.Request().market_.EvaluationDate()) / 365.);
                const double step =
                    request.steps_.empty()
                        ? (it->quoteType_ == MarketIvQuoteType_::Value_::DECIMAL_IV ? 1e-4 : (carry.discount_ * carry.forward_) * 1e-4)
                        : request.steps_[i];
                REQUIRE(std::isfinite(step) && step > 0., "InvalidMarketIvRequest: refit step must be finite positive; ID=" + it->id_);
                selected.push_back({static_cast<size_t>(it - quotes.begin()), i, step});
            }
            std::sort(selected.begin(), selected.end(), [](const auto& x, const auto& y) { return x.quote_ < y.quote_; });
            return selected;
        }
        void Slopes(MarketIvDetail::RiskData_* data, size_t i, size_t o) {
            for (size_t j = 0; j < o; ++j) {
                auto& row = data->responses_[i * o + j];
                if (row.base_ && row.plus_ && row.minus_) {
                    row.symmetricBumpSlope_ = Slope(*row.plus_, *row.minus_, 2. * row.step_);
                    row.plusSlope_ = Slope(*row.plus_, *row.base_, row.step_);
                    row.minusSlope_ = Slope(*row.base_, *row.minus_, row.step_);
                }
                if (!row.symmetricBumpSlope_ || !row.plusSlope_ || !row.minusSlope_)
                    data->complete_ = false;
            }
        }
    } // namespace

    void ValidateMarketIvRefitRiskRequest(const MarketIvRefitRiskRequest_& request) {
        using namespace MarketIvDetail;
        const auto& s = request.settings_;
        const size_t a = request.quoteIds_.size(), o = request.outputs_.size();
        REQUIRE(a > 0 && a <= s.maxSelectedQuotes_ && o > 0 && o <= s.maxOutputs_,
                "InvalidMarketIvRequest: refit quote_ids/outputs; empty or exceeds configured count limit");
        REQUIRE(request.steps_.empty() || request.steps_.size() == a, "InvalidMarketIvRequest: refit steps must match quote_ids");
        ValidateMarketIvEvaluationSettings(s.evaluation_);
        ValidateIds(request.quoteIds_);
        for (const auto step : request.steps_)
            REQUIRE(std::isfinite(step) && step > 0., "InvalidMarketIvRequest: refit steps must be finite positive");
        for (const auto& output : request.outputs_) {
            ValidateQuery(output.query_);
            REQUIRE(output.quantity_.Switch() >= MarketIvQuantity_::Value_::TOTAL_VARIANCE &&
                        output.quantity_.Switch() < MarketIvQuantity_::Value_::_N_VALUES,
                    "InvalidMarketIvRequest: invalid output quantity");
        }
    }
    MarketIvRefitRiskResult_ EvaluateMarketIvRefitRisk(const MarketIvCalibrationSnapshot_& base, const MarketIvRefitRiskRequest_& request) {
        using namespace MarketIvDetail;
        REQUIRE(request.quoteIds_.size() <= base.Request().quotes_.size(), "InvalidMarketIvRequest: selected quote count exceeds base quote axis");
        ValidateMarketIvRefitRiskRequest(request);
        const auto& s = request.settings_;
        const size_t a = request.quoteIds_.size(), o = request.outputs_.size();
        const size_t totalLimit = s.maxTotalModelRowEvaluations_
                                      ? *s.maxTotalModelRowEvaluations_
                                      : CheckedMultiply(CheckedMultiply(2, a), base.Request().settings_.maxModelRowEvaluations_);
        const size_t p = base.Execution().plannedNumericPayloadBytes_, w = base.Execution().plannedWorkspaceBytes_;
        const size_t payload = CheckedAdd(
            p,
            CheckedAdd(8192, CheckedAdd(CheckedMultiply(512, a), CheckedAdd(CheckedMultiply(128, CheckedMultiply(a, o)), CheckedMultiply(256, o)))));
        const size_t workspace = CheckedAdd(w, CheckedAdd(CheckedMultiply(2, p), CheckedMultiply(1024, o)));
        CheckBudget(payload, workspace, s.numericPayloadBudgetBytes_, s.workspaceBudgetBytes_);
        const auto selected = Select(base, request);
        auto data = std::make_shared<RiskData_>(base, request);
        data->execution_.plannedNumericPayloadBytes_ = payload;
        data->execution_.plannedWorkspaceBytes_ = workspace;
        data->execution_.peakWorkspaceBytes_ = 8192 + EvaluationWorkspaceBytes();
        data->axis_ = Vector_<MarketIvRefitAxis_>(a);
        data->responses_ = Vector_<MarketIvRefitResponse_>(CheckedMultiply(a, o));
        Vector_<std::optional<double>> baseValues(o);
        for (size_t j = 0; j < o; ++j) {
            baseValues[j] = Quantity(EvaluateMarketIvSurface(base, request.outputs_[j].query_, s.evaluation_), request.outputs_[j].quantity_);
            ++data->execution_.outputEvaluations_;
        }
        for (size_t i = 0; i < a; ++i) {
            auto& axis = data->axis_[i];
            axis.quoteIndex_ = selected[i].quote_;
            axis.step_ = selected[i].step_;
            axis.inputUnit_ = base.Request().quotes_[axis.quoteIndex_].quoteType_ == MarketIvQuoteType_::Value_::PRICE ? "premium" : "decimal_iv";
            for (size_t j = 0; j < o; ++j) {
                auto& row = data->responses_[i * o + j];
                row.quoteIndex_ = axis.quoteIndex_;
                row.outputIndex_ = j;
                row.step_ = axis.step_;
                row.base_ = baseValues[j];
            }
            Direction(base, request, i, true, totalLimit, data.get());
            Direction(base, request, i, false, totalLimit, data.get());
            Slopes(data.get(), i, o);
        }
        data->execution_.numericPayloadBytes_ = base.Execution().numericPayloadBytes_ + sizeof(*data) + Bytes(data->axis_) + Bytes(data->responses_) +
                                                Bytes(data->request_.quoteIds_) + Bytes(data->request_.outputs_) + Bytes(data->request_.steps_);
        data->execution_.peakWorkspaceBytes_ += Bytes(selected) + Bytes(baseValues) + sizeof(selected) + sizeof(baseValues);
        CheckBudget(data->execution_.numericPayloadBytes_, data->execution_.peakWorkspaceBytes_, payload, workspace);
        return MarketIvRefitRiskResult_(data);
    }
    bool MarketIvRefitRiskResult_::Complete() const { return data_->complete_; }
    const MarketIvCalibrationSnapshot_& MarketIvRefitRiskResult_::Snapshot() const { return data_->snapshot_; }
    const MarketIvRefitRiskRequest_& MarketIvRefitRiskResult_::Request() const { return data_->request_; }
    const Vector_<MarketIvRefitResponse_>& MarketIvRefitRiskResult_::Responses() const { return data_->responses_; }
    const Vector_<MarketIvRefitAxis_>& MarketIvRefitRiskResult_::Axis() const { return data_->axis_; }
    const MarketIvExecution_& MarketIvRefitRiskResult_::Execution() const { return data_->execution_; }
} // namespace Dal
