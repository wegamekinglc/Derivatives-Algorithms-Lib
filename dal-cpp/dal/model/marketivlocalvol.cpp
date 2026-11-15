#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <tuple>

#include <dal/currency/currency.hpp>
#include <dal/model/correlatedblackscholes.hpp>
#include <dal/model/marketivlocalvolinternal.hpp>
#include <dal/model/surface/marketivsurfaceinternal.hpp>
#include <dal/storage/canonicaljson_internal.hpp>

namespace Dal {
    MarketIvLocalVolError_::MarketIvLocalVolError_(
        const String_& code, const String_& stage, const String_& field, const String_& expected, const String_& actual)
        : ScriptError_(__FILE__, __LINE__, __func__, code + ": field=" + field + "; expected=" + expected + "; actual=" + actual), code_(code),
          stage_(stage), field_(field), expected_(expected), actual_(actual) {}

    namespace {
        void ValidateText(const String_& value, const String_& field) {
            if (value.empty() || value.find('\0') != String_::npos || !CanonicalJson::ValidUtf8(std::string(value.data(), value.size())))
                MarketIvLocalVolDetail::Fail("InvalidMarketIvLocalVolRequest", "Input", field, "nonempty UTF-8 without NUL", "invalid text");
        }
    } // namespace

    void ValidateMarketIvLocalVolModelSettings(const MarketIvLocalVolModelSettings_& settings) {
        using namespace MarketIvLocalVolDetail;
        ValidateText(settings.index_, "/settings/index");
        ValidateText(settings.currency_, "/settings/currency");
        ValidateText(settings.factor_, "/settings/factor");
        try {
            static_cast<void>(CanonicalCorrelatedBSAssetNames({settings.index_}));
        } catch (const Exception_&) {
            Fail("InvalidMarketIvLocalVolRequest", "Input", "/settings/index", "one plain EQ index", "invalid index");
        }
        try {
            static_cast<void>(Ccy_(settings.currency_));
        } catch (const Exception_&) {
            Fail("InvalidMarketIvLocalVolRequest", "Input", "/settings/currency", "canonical currency", "unknown currency");
        }
        for (const auto& item : {std::make_pair(settings.horizon_, "/settings/horizon"), {settings.maxStep_, "/settings/max_step"}})
            if (!std::isfinite(item.first) || item.first <= 0.)
                Fail("InvalidMarketIvLocalVolRequest", "Input", item.second, "finite positive years", ToString(item.first));
    }
} // namespace Dal

namespace Dal::MarketIvLocalVolDetail {
    [[noreturn]] void Fail(const String_& code, const String_& stage, const String_& field, const String_& expected, const String_& actual) {
        throw MarketIvLocalVolError_(code, stage, field, expected, actual);
    }
    size_t CheckedAdd(size_t a, size_t b) {
        if (b > std::numeric_limits<size_t>::max() - a)
            Fail("MarketIvLocalVolBudgetExceeded", "Build", "/ledger", "representable sum", "overflow");
        return a + b;
    }
    size_t CheckedMultiply(size_t a, size_t b) {
        if (a && b > std::numeric_limits<size_t>::max() / a)
            Fail("MarketIvLocalVolBudgetExceeded", "Build", "/ledger", "representable product", "overflow");
        return a * b;
    }
    void CheckLimit(size_t value, size_t limit, const String_& field, const String_& stage) {
        if (value > limit)
            Fail("MarketIvLocalVolBudgetExceeded", stage, field, String_(std::to_string(limit)), String_(std::to_string(value)));
    }
    double CheckedExp(double value, const String_& field, const String_& stage) {
        const double result = std::exp(value);
        if (!std::isfinite(value) || !std::isfinite(result) || result <= 0.)
            Fail("MarketIvLocalVolNumericRange", stage, field, "finite positive exponential", ToString(value));
        return result;
    }

    Carry_::Carry_(const MarketIvMarket_& market)
        : logSpot_(std::log(market.Spot())), rate_(market.Rate()), yield_(market.DividendYield()), flat_(market.IsFlatCarry()) {
        const size_t count = CheckedAdd(market.Terms().size(), 1);
        CheckLimit(count, static_cast<size_t>(std::numeric_limits<int>::max()), "/carry/terms");
        times_.reserve(count);
        logForward_.reserve(count);
        logDiscount_.reserve(count);
        times_.push_back(0.);
        logForward_.push_back(0.);
        logDiscount_.push_back(0.);
        for (const auto& term : market.Terms()) {
            times_.push_back((term.expiry_ - market.EvaluationDate()) / 365.);
            logForward_.push_back(std::log(term.forward_) - logSpot_);
            logDiscount_.push_back(std::log(term.discount_));
        }
    }

    CarryPoint_ Carry_::At(double time) const {
        if (!std::isfinite(time) || time < 0.)
            Fail("InvalidMarketIvLocalVolRequest", "Build", "/carry/time", "finite nonnegative time", ToString(time));
        const auto next = std::upper_bound(times_.begin(), times_.end(), time);
        const size_t hi = std::min(static_cast<size_t>(next - times_.begin()), times_.size() - 1), lo = hi - 1;
        const double dt = times_[hi] - times_[lo], weight = (time - times_[lo]) / dt;
        const double drift = flat_ ? rate_ - yield_ : (logForward_[hi] - logForward_[lo]) / dt;
        const double rate = flat_ ? rate_ : -(logDiscount_[hi] - logDiscount_[lo]) / dt;
        const bool tail = time >= times_.back();
        const double l = flat_  ? drift * time
                         : tail ? logForward_[hi] + drift * (time - times_[hi])
                                : (1. - weight) * logForward_[lo] + weight * logForward_[hi];
        const double h = flat_  ? -rate_ * time
                         : tail ? logDiscount_[hi] - rate * (time - times_[hi])
                                : (1. - weight) * logDiscount_[lo] + weight * logDiscount_[hi];
        const double forward = CheckedExp(logSpot_ + l, "/carry/forward"), discount = CheckedExp(h, "/carry/discount");
        static_cast<void>(CheckedExp(-h, "/carry/numeraire"));
        if (!std::isfinite(drift) || !std::isfinite(rate) || !std::isfinite(rate - drift))
            Fail("MarketIvLocalVolNumericRange", "Build", "/carry/slopes", "finite r and q", ToString(time));
        return {forward, discount, l, h, rate, flat_ ? yield_ : rate - drift};
    }

    IntegratedCarry_ Carry_::Integrated(double from, double to) const {
        if (to < from)
            Fail("InvalidMarketIvLocalVolRequest", "Build", "/carry/interval", "from <= to", ToString(to));
        const auto a = At(from), b = At(to);
        const double r = a.logDiscount_ - b.logDiscount_, q = r - (b.logForwardOverSpot_ - a.logForwardOverSpot_);
        static_cast<void>(CheckedExp(-r, "/carry/discount_ratio"));
        if (!std::isfinite(q))
            Fail("MarketIvLocalVolNumericRange", "Build", "/carry/integral", "finite R and Q", ToString(to));
        return {r, q};
    }

    size_t Carry_::NumericBytes() const {
        return CheckedMultiply(sizeof(double), CheckedAdd(times_.capacity(), CheckedAdd(logForward_.capacity(), logDiscount_.capacity())));
    }

    struct SourceData_ {
        MarketIvCalibrationSnapshot_ snapshot_;
        MarketIvDupireRequest_ request_;
        Vector_<MarketIvPointResult_> nodes_;
        LocalVolSurfaceData_ surface_;
        MarketIvLocalVolModelSettings_ settings_;
        Carry_ carry_;
        Vector_<MarketIvLocalVolCarryTerm_> terms_;
        Vector_<MarketIvLocalVolWarning_> warnings_;
        MarketIvLocalVolConstructionExecution_ execution_;
        String_ sourceIdentity_, modelIdentity_;

        SourceData_(const MarketIvDupireResult_& grid, const MarketIvLocalVolModelSettings_& settings, Carry_ carry)
            : snapshot_(grid.Snapshot()), request_(grid.Request()), nodes_(grid.Nodes()),
              surface_("market-iv-local-vol", grid.SurfaceData().spots_, grid.SurfaceData().times_, grid.SurfaceData().vols_), settings_(settings),
              carry_(std::move(carry)) {}
    };

    namespace {
        template <class T_> size_t Bytes(const Vector_<T_>& values) { return CheckedMultiply(sizeof(T_), values.capacity()); }

        void ValidateGrid(const MarketIvDupireResult_& grid) {
            if (!grid.IsSuccess())
                Fail("MarketIvDupireUnavailable", "Build", "/grid", "successful Dupire result", grid.Failure().substr(0, 160));
            const auto& snapshot = grid.Snapshot();
            const auto& fit = MarketIvData(snapshot);
            if ((fit.status_ != "SUCCESS" && fit.status_ != "SUCCESS_WITH_WARNINGS") || !snapshot.Certificate().certified_)
                Fail("MarketIvDupireUnavailable", "Build", "/grid/snapshot", "successful certified fit", fit.status_);
            const auto& request = grid.Request();
            const auto& surface = grid.SurfaceData();
            if (surface.spots_ != request.spots_ || surface.times_ != request.times_)
                Fail("MarketIvSourceMismatch", "Build", "/grid/surface/axes", "requested axes", "different axes");
            const auto rows = request.spots_.size(), cols = request.times_.size(), count = CheckedMultiply(rows, cols);
            CheckLimit(rows, static_cast<size_t>(std::numeric_limits<int>::max()), "/grid/spots");
            CheckLimit(cols, static_cast<size_t>(std::numeric_limits<int>::max()), "/grid/times");
            if (!rows || !cols || surface.vols_.Rows() != static_cast<int>(rows) || surface.vols_.Cols() != static_cast<int>(cols) ||
                grid.Nodes().size() != count)
                Fail("MarketIvSourceMismatch", "Build", "/grid/surface/shape", "spot-major requested grid", "different shape");
            for (size_t i = 0; i < rows; ++i)
                for (size_t j = 0; j < cols; ++j) {
                    const auto& node = grid.Nodes()[i * cols + j];
                    const String_ field = "/grid/nodes/" + String_(std::to_string(i * cols + j)) + "/local_vol";
                    if (node.DupireStatus() != "SUCCESS" || !node.LocalVol() || !std::isfinite(*node.LocalVol()) || *node.LocalVol() < 0.)
                        Fail("MarketIvDupireUnavailable", "Build", field, "successful finite nonnegative local vol", node.DupireStatus());
                    if (surface.vols_(static_cast<int>(i), static_cast<int>(j)) != *node.LocalVol())
                        Fail("MarketIvSourceMismatch", "Build", "/grid/surface/vols/" + String_(std::to_string(i)) + "/" + String_(std::to_string(j)),
                             "authoritative node local vol", "different value");
                }
        }

        size_t RetainedBytes(const SourceData_& data) {
            size_t result = CheckedAdd(sizeof(SourceData_), data.snapshot_.Execution().numericPayloadBytes_);
            for (const size_t bytes : {Bytes(data.request_.spots_), Bytes(data.request_.times_), Bytes(data.nodes_), Bytes(data.surface_.spots_),
                                       Bytes(data.surface_.times_), CheckedMultiply(sizeof(double), data.surface_.vols_.Capacity()),
                                       data.carry_.NumericBytes(), Bytes(data.terms_), Bytes(data.warnings_)})
                result = CheckedAdd(result, bytes);
            return result;
        }

        MarketIvLocalVolConstructionExecution_ Plan(const MarketIvDupireResult_& grid, const MarketIvLocalVolModelSettings_& settings) {
            MarketIvLocalVolConstructionExecution_ plan;
            const size_t terms = CheckedAdd(grid.Snapshot().Request().market_.Terms().size(), 2);
            const size_t extra = CheckedAdd(
                sizeof(SourceData_), CheckedAdd(CheckedMultiply(24, terms), CheckedAdd(CheckedMultiply(sizeof(MarketIvLocalVolCarryTerm_), terms),
                                                                                       CheckedMultiply(5, sizeof(MarketIvLocalVolWarning_)))));
            plan.plannedNumericPayloadBytes_ = CheckedAdd(grid.Execution().plannedNumericPayloadBytes_, extra);
            plan.plannedWorkspaceBytes_ =
                CheckedAdd(grid.Execution().plannedNumericPayloadBytes_ - grid.Snapshot().Execution().plannedNumericPayloadBytes_, extra);
            CheckLimit(plan.plannedNumericPayloadBytes_, settings.numericPayloadBudgetBytes_, "/settings/numeric_payload_budget_bytes");
            CheckLimit(plan.plannedWorkspaceBytes_, settings.workspaceBudgetBytes_, "/settings/workspace_budget_bytes");
            return plan;
        }

        void ValidateCarryRange(const Carry_& carry, double horizon) {
            double minLog = 0., maxLog = 0.;
            const auto visit = [&](double time) {
                const auto point = carry.At(time);
                if (time <= horizon) {
                    minLog = std::min(minLog, point.logDiscount_);
                    maxLog = std::max(maxLog, point.logDiscount_);
                }
            };
            for (const double time : carry.Times())
                visit(time);
            visit(horizon);
            static_cast<void>(CheckedExp(minLog - maxLog, "/carry/discount_ratios"));
            static_cast<void>(CheckedExp(maxLog - minLog, "/carry/discount_ratios"));
        }

        Vector_<MarketIvLocalVolCarryTerm_> CarryTerms(const MarketIvMarket_& market, const Carry_& carry, double horizon) {
            Vector_<MarketIvLocalVolCarryTerm_> result;
            result.reserve(CheckedAdd(market.Terms().size(), 2));
            const auto add = [&](double time, std::optional<Date_> date, const String_& origin) {
                const auto point = carry.At(time);
                result.push_back({0, date, time, point.forward_, point.discount_, point.logForwardOverSpot_, point.logDiscount_, origin});
            };
            add(0., market.EvaluationDate(), "ANCHOR");
            result.back().forward_ = market.Spot();
            bool included = false;
            for (const auto& term : market.Terms()) {
                const double time = (term.expiry_ - market.EvaluationDate()) / 365.;
                add(time, term.expiry_, time == horizon ? "MARKET_AND_HORIZON" : "MARKET");
                result.back().forward_ = term.forward_;
                result.back().discount_ = term.discount_;
                included = included || time == horizon;
            }
            if (!included) {
                std::optional<Date_> date;
                const double days = horizon * 365.;
                const int maxDays = Date::Maximum() - market.EvaluationDate();
                if (std::isfinite(days) && days <= maxDays + 1.) {
                    const int lower = static_cast<int>(days);
                    // Exact year-fraction equality recognizes multiplication roundoff without rounding subdays.
                    for (const int day : {lower, lower + 1})
                        if (day <= maxDays && day / 365. == horizon)
                            date = market.EvaluationDate().AddDays(day);
                }
                add(horizon, date, "HORIZON");
            }
            std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.time_ < b.time_; });
            for (size_t i = 0; i < result.size(); ++i)
                result[i].ordinal_ = i;
            return result;
        }

        Vector_<MarketIvLocalVolWarning_> Warnings(const SourceData_& data) {
            Vector_<MarketIvLocalVolWarning_> result;
            result.reserve(5);
            const auto& fit = MarketIvData(data.snapshot_);
            if (fit.underidentified_)
                result.push_back({"FIT_UNDERIDENTIFIED", "/grid/snapshot", "Fit has underidentified parameters"});
            if (fit.nearTie_)
                result.push_back({"FIT_NEAR_TIE", "/grid/snapshot", "Fit has near-tied starts"});
            result.push_back({"LOCAL_VOL_FIRST_COLUMN_AT_ZERO", "/grid/times/0", "First local-vol column extends to zero"});
            if (data.settings_.horizon_ > data.surface_.times_.back())
                result.push_back({"LOCAL_VOL_TIME_TAIL", "/settings/horizon", "Flat local-vol time tail"});
            if (data.settings_.horizon_ > data.carry_.Times().back())
                result.push_back({"MARKET_CARRY_TAIL", "/settings/horizon", "Last-slope market carry tail"});
            std::sort(result.begin(), result.end(),
                      [](const auto& a, const auto& b) { return std::tie(a.code_, a.field_) < std::tie(b.code_, b.field_); });
            return result;
        }
    } // namespace

    std::shared_ptr<const Source_> BuildSource(const MarketIvDupireResult_& grid, const MarketIvLocalVolModelSettings_& settings) {
        ValidateMarketIvLocalVolModelSettings(settings);
        ValidateGrid(grid);
        ValidateIdentityInput(grid.Snapshot(), grid.Request());
        CheckLimit(1, settings.maxInternalSteps_, "/settings/max_internal_steps");
        CheckLimit(1, settings.maxSimulationDimension_, "/settings/max_simulation_dimension");
        const auto plan = Plan(grid, settings);
        auto resolved = settings;
        resolved.index_ = CanonicalCorrelatedBSAssetNames({settings.index_})[0];
        resolved.currency_ = Ccy_(settings.currency_).String();
        Carry_ carry(grid.Snapshot().Request().market_);
        ValidateCarryRange(carry, settings.horizon_);
        auto data = std::make_shared<SourceData_>(grid, resolved, std::move(carry));
        data->terms_ = CarryTerms(data->snapshot_.Request().market_, data->carry_, settings.horizon_);
        data->warnings_ = Warnings(*data);
        data->execution_ = plan;
        data->execution_.numericPayloadBytes_ = RetainedBytes(*data);
        data->execution_.peakWorkspaceBytes_ = data->execution_.numericPayloadBytes_ - data->snapshot_.Execution().numericPayloadBytes_;
        CheckLimit(data->execution_.numericPayloadBytes_, plan.plannedNumericPayloadBytes_, "/ledger/numeric_payload_bytes");
        CheckLimit(data->execution_.peakWorkspaceBytes_, plan.plannedWorkspaceBytes_, "/ledger/peak_workspace_bytes");
        data->sourceIdentity_ = SourceIdentity(data->snapshot_, data->request_, data->surface_);
        data->modelIdentity_ = ModelIdentity(data->sourceIdentity_, resolved);
        return std::shared_ptr<const Source_>(new Source_(std::move(data)));
    }

    String_ Source_::SourceIdentity() const { return data_->sourceIdentity_; }
    String_ Source_::ModelIdentity() const { return data_->modelIdentity_; }
    MarketIvLocalVolModelSettings_ Source_::Configuration() const { return data_->settings_; }
    Vector_<MarketIvLocalVolCarryTerm_> Source_::CarryTerms() const { return data_->terms_; }
    Vector_<MarketIvLocalVolWarning_> Source_::Warnings() const { return data_->warnings_; }
    MarketIvLocalVolConstructionExecution_ Source_::ConstructionExecution() const { return data_->execution_; }
    LocalVolSurfaceData_ Source_::SurfaceData() const {
        return LocalVolSurfaceData_(data_->surface_.Name(), data_->surface_.spots_, data_->surface_.times_, data_->surface_.vols_);
    }
    const Carry_& Source_::Carry() const { return data_->carry_; }
    const MarketIvCalibrationSnapshot_& Source_::Snapshot() const { return data_->snapshot_; }
    const LocalVolSurfaceData_& Source_::Surface() const { return data_->surface_; }
} // namespace Dal::MarketIvLocalVolDetail
