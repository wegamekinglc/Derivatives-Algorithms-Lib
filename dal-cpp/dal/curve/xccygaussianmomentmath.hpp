#pragma once

#include <dal/curve/xccygaussianmoments.hpp>
#include <dal/model/xccygaussianmath.hpp>

namespace Dal::XccyMomentInternal {
    struct Loading_ {
        Date_ date_;
        int slot_;
        double weight_;
    };
    template <class T_> struct Exponent_ {
        T_ constant_ = T_(0.);
        Vector_<Loading_> loadings_;
    };
    inline double Time(const Date_& from, const Date_& to) { return static_cast<double>((to - from) / 365.L); }
    inline double H(double a, const Date_& from, const Date_& to) { return static_cast<double>(XccyGaussianDetail::H(a, (to - from) / 365.L)); }
    inline double Reversion(const XccyGaussianDynamics_& dynamics, int slot) {
        return XccyGaussianDetail::Driver(slot) == 0 ? dynamics.domesticA_ : dynamics.foreignA_;
    }
    inline std::pair<long double, long double> ShiftedKernel(int slot, double a, long double tail) {
        if (slot == 4)
            return {1.L, 0.L};
        return {slot < 2 ? 0.L : XccyGaussianDetail::H(a, tail), std::exp(-a * tail)};
    }
    inline long double CrossKernel(int i, int j, const XccyGaussianDynamics_& dynamics, long double ti, long double tj, long double h) {
        const double ai = Reversion(dynamics, i), aj = Reversion(dynamics, j);
        const auto [ci, bi] = ShiftedKernel(i, ai, ti);
        const auto [cj, bj] = ShiftedKernel(j, aj, tj);
        return ci * cj * h + ci * bj * XccyGaussianDetail::KernelWithConstant(j, aj, h) + bi * cj * XccyGaussianDetail::KernelWithConstant(i, ai, h) +
               bi * bj * XccyGaussianDetail::KernelProduct(i, j, dynamics.domesticA_, dynamics.foreignA_, h);
    }
    template <class T_> class Environment_ {
        const XccyGaussianModelData_& model_;
        const XccyGaussianMomentInputs_& inputs_;
        const Vector_<T_>& parameters_;

        [[nodiscard]] T_ LogDF(size_t curve, const Date_& date) const {
            const auto& data = *inputs_.curves_[curve];
            XccyGaussianDetail::Check(date >= data.nodeDates_.front() && date <= data.nodeDates_.back(), "CalibrationContextMismatch",
                                      "/model/market/" + data.route_ + "/coverage", "date within explicit snapshot coverage");
            const auto found = std::lower_bound(data.nodeDates_.begin(), data.nodeDates_.end(), date);
            const size_t upper = static_cast<size_t>(found - data.nodeDates_.begin());
            const auto value = [&](size_t node) { return node == 0 ? T_(0.) : parameters_[inputs_.curveOffsets_[curve] + node - 1]; };
            if (*found == date)
                return value(upper);
            const double weight = static_cast<double>(date - data.nodeDates_[upper - 1]) / (data.nodeDates_[upper] - data.nodeDates_[upper - 1]);
            return value(upper - 1) * (1. - weight) + value(upper) * weight;
        }
        [[nodiscard]] T_ EffectiveLogDF(bool foreign, const Date_& date) const {
            return foreign ? T_(LogDF(1, date) - LogDF(2, date)) : LogDF(0, date);
        }
        [[nodiscard]] T_ Sigma(int slot, size_t bucket) const {
            return parameters_[inputs_.sigmaOffsets_[XccyGaussianDetail::Driver(slot)] + bucket];
        }
        template <class F_> T_ Integrate(const Date_& from, const Date_& to, const F_& piece) const {
            const auto& ends = model_.dynamics_->bucketEndDates_;
            XccyGaussianDetail::Check(from >= model_.market_->valuationTime_.Date() && from <= to && to <= ends.back(), "InvalidCalibrationPlan",
                                      "/model/dynamics/coverage", "explicit buckets covering all live observations and maturities");
            T_ result(0.);
            Date_ current = from;
            while (current < to) {
                const auto found = std::upper_bound(ends.begin(), ends.end(), current);
                const size_t bucket = static_cast<size_t>(found - ends.begin());
                const Date_ next = std::min(to, *found);
                result += piece(current, next, bucket);
                current = next;
            }
            return result;
        }
        [[nodiscard]] T_ Phi(bool foreign, const Date_& date) const {
            const int slot = foreign ? 3 : 2;
            return -EffectiveLogDF(foreign, date) + .5 * Covariance({date, slot, 1.}, {date, slot, 1.}, model_.market_->valuationTime_.Date());
        }

    public:
        Environment_(const XccyGaussianModelData_& model, const XccyGaussianMomentInputs_& inputs, const Vector_<T_>& parameters)
            : model_(model), inputs_(inputs), parameters_(parameters) {}
        [[nodiscard]] T_ Covariance(const Loading_& left, const Loading_& right, const Date_& from) const {
            const auto& dynamics = *model_.dynamics_;
            return Integrate(from, std::min(left.date_, right.date_), [&](const Date_& l, const Date_& r, size_t bucket) {
                const auto kernel =
                    CrossKernel(left.slot_, right.slot_, dynamics, (left.date_ - r) / 365.L, (right.date_ - r) / 365.L, (r - l) / 365.L);
                return T_(Sigma(left.slot_, bucket) * Sigma(right.slot_, bucket) *
                          dynamics.correlation_(XccyGaussianDetail::Driver(left.slot_), XccyGaussianDetail::Driver(right.slot_)) *
                          static_cast<double>(kernel));
            });
        }
        [[nodiscard]] T_ Mean(const Loading_& loading) const {
            if (loading.slot_ != 1 && loading.slot_ != 3)
                return T_(0.);
            const auto& dynamics = *model_.dynamics_;
            return Integrate(model_.market_->valuationTime_.Date(), loading.date_, [&](const Date_& l, const Date_& r, size_t bucket) {
                const auto [c, b] = ShiftedKernel(loading.slot_, dynamics.foreignA_, (loading.date_ - r) / 365.L);
                const long double h = (r - l) / 365.L;
                const double kernel = static_cast<double>(c * h + b * XccyGaussianDetail::KernelWithConstant(loading.slot_, dynamics.foreignA_, h));
                return T_(-dynamics.correlation_(1, 2) * Sigma(1, bucket) * Sigma(4, bucket) * kernel);
            });
        }
        [[nodiscard]] Exponent_<T_> Discount(const Date_& date) const { return {-Phi(false, date), {{date, 2, -1.}}}; }
        [[nodiscard]] Exponent_<T_> Fx(const Date_& date) const {
            const Date_ v = model_.market_->valuationTime_.Date();
            return {Dal::log(parameters_[0]) + Phi(false, date) - Phi(true, date) - .5 * Covariance({date, 4, 1.}, {date, 4, 1.}, v),
                    {{date, 2, 1.}, {date, 3, -1.}, {date, 4, 1.}}};
        }
        [[nodiscard]] Exponent_<T_> Growth(const XccyCouponPeriod_& period, bool foreign, const String_& tenor) const {
            const auto& schedule = period.schedule_;
            const auto& currency = foreign ? model_.market_->pair_.foreign_ : model_.market_->pair_.domestic_;
            const auto& convention = model_.market_->Projection(currency.String(), tenor);
            size_t route = 3;
            while (inputs_.curves_[route].get() != &convention)
                ++route;
            const Date_ fix = period.rateFixingTime_.Date(), start = schedule.accrualStart_, end = schedule.accrualEnd_;
            const int slot = foreign ? 3 : 2;
            const double a = foreign ? model_.dynamics_->foreignA_ : model_.dynamics_->domesticA_;
            const T_ q = LogDF(route, start) - EffectiveLogDF(foreign, start) - LogDF(route, end) + EffectiveLogDF(foreign, end);
            return {Phi(foreign, end) - Phi(foreign, start) + q +
                        .5 * (Covariance({start, slot, 1.}, {start, slot, 1.}, fix) - Covariance({end, slot, 1.}, {end, slot, 1.}, fix)),
                    {{fix, foreign ? 1 : 0, H(a, fix, end) - H(a, fix, start)}}};
        }
        [[nodiscard]] T_ Expectation(const Exponent_<T_>& exponent) const {
            T_ logMean = exponent.constant_;
            const Date_ v = model_.market_->valuationTime_.Date();
            for (const auto& left : exponent.loadings_) {
                logMean += left.weight_ * Mean(left);
                for (const auto& right : exponent.loadings_)
                    logMean += .5 * left.weight_ * right.weight_ * Covariance(left, right, v);
            }
            return Dal::exp(logMean);
        }
    };
    template <class T_> Exponent_<T_> Add(Exponent_<T_> left, const Exponent_<T_>& right) {
        left.constant_ += right.constant_;
        left.loadings_.Append(right.loadings_);
        return left;
    }
} // namespace Dal::XccyMomentInternal
