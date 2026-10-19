#include <dal/platform/platform.hpp>

#include <algorithm>
#include <dal/model/surface/marketivsurfaceinternal.hpp>

namespace Dal::MarketIvDetail {
    namespace {
        double Endpoint(double h0, double h1, double d0, double d1, int* branch) {
            const long double raw = ((2.L * h0 + h1) / (h0 + h1)) * d0 - (static_cast<long double>(h0) / (h0 + h1)) * d1;
            if (!std::isfinite(raw))
                return std::numeric_limits<double>::quiet_NaN();
            if (raw <= 0.) {
                *branch = 1;
                return 0.;
            }
            if (raw >= 3.L * d0) {
                *branch = 2;
                return std::nextafter(3. * d0, 0.);
            }
            *branch = 3;
            const double represented = static_cast<double>(raw);
            return represented > 0. ? represented : std::numeric_limits<double>::quiet_NaN();
        }
        double InteriorDerivativeBound(long double b, double m0, double m1, long double error) {
            const long double a = m0 - 2.L * b + m1;
            if (!std::isfinite(a))
                return -1.;
            const long double u = a > 0. ? (m0 - b) / a : -1.;
            if (!(u > 0. && u < 1.))
                return -1.;
            const long double lower =
                std::min(static_cast<long double>(std::min(m0, m1)), m0 * (1.L - u) * (1.L - u) + 2.L * b * u * (1.L - u) + m1 * u * u - 8.L * error);
            return lower == 0. ? 0. : std::nextafter(static_cast<double>(lower), -std::numeric_limits<double>::infinity());
        }
        double FlatDerivativeBound(double m0, double m1) { return m0 == 0. && m1 == 0. ? 0. : -1.; }
        double DerivativeBound(long double h, long double dy, double m0, double m1) {
            if (dy == 0.)
                return FlatDerivativeBound(m0, m1);
            const long double d = dy / h;
            const long double b = 3.L * d - m0 - m1;
            const long double error = 64.L * std::numeric_limits<long double>::epsilon() * (3.L * std::abs(d) + m0 + m1) +
                                      64.L * std::numeric_limits<long double>::denorm_min();
            if (!std::isfinite(d) || !std::isfinite(b) || !std::isfinite(error))
                return -1.;
            if (b - error < 0.)
                return InteriorDerivativeBound(b, m0, m1, error);
            const long double lower = std::min({static_cast<long double>(m0), static_cast<long double>(m1), b - error});
            return lower == 0. ? 0. : std::nextafter(static_cast<double>(lower), 0.);
        }
        long double LogCdf(long double x) {
            if (x > -10.L)
                return std::log(.5L * std::erfc(-x / std::sqrt(2.L)));
            const long double z = -x;
            long double cf = 0.;
            for (int n = 96; n > 0; --n)
                cf = n / (z + cf);
            return -.5L * z * z - .5L * std::log(2.L * std::acos(-1.L)) - std::log(z + cf);
        }
    } // namespace

    namespace {
        bool Slopes(const Vector_<>& h, const Vector_<>& d, ThetaCurve_* c) {
            const size_t n = c->times_.size();
            if (n == 2) {
                c->slopes_[0] = c->slopes_[1] = d[0];
                c->branches_[0] = c->branches_[1] = 4;
            } else {
                c->slopes_[0] = Endpoint(h[0], h[1], d[0], d[1], &c->branches_[0]);
                c->slopes_[n - 1] = Endpoint(h[n - 2], h[n - 3], d[n - 2], d[n - 3], &c->branches_[n - 1]);
                for (size_t i = 1; i + 1 < n; ++i) {
                    if (d[i - 1] == 0. || d[i] == 0.)
                        continue;
                    const long double w1 = 2.L * h[i] + h[i - 1], w2 = h[i] + 2.L * h[i - 1];
                    const long double minimum = std::min(d[i - 1], d[i]);
                    const long double denominator = (w1 / (w1 + w2)) * (minimum / d[i - 1]) + (w2 / (w1 + w2)) * (minimum / d[i]);
                    c->slopes_[i] = static_cast<double>(minimum / denominator);
                    if (!(c->slopes_[i] > 0.) || !std::isfinite(c->slopes_[i]))
                        return false;
                    c->branches_[i] = 5;
                }
            }
            return std::isfinite(c->slopes_.front()) && std::isfinite(c->slopes_.back());
        }
    } // namespace
    namespace {
        bool ValidSecant(double h, double d, double from, double to) {
            return h > 0. && d >= 0. && !(to > from && d == 0.) && std::isfinite(d) && std::isfinite(to);
        }
        bool ValidThetaAxis(const Vector_<>& times, const Vector_<>& values) {
            return times.size() >= 2 && times.size() == values.size() && times.front() == 0. && values.front() == 0.;
        }
        bool CertifyIntervals(ThetaCurve_* c) {
            for (size_t i = 0; i + 1 < c->times_.size(); ++i) {
                c->lowerBounds_[i] = DerivativeBound(static_cast<long double>(c->times_[i + 1]) - c->times_[i],
                                                     static_cast<long double>(c->values_[i + 1]) - c->values_[i], c->slopes_[i], c->slopes_[i + 1]);
                if (!(c->lowerBounds_[i] >= 0.) || !std::isfinite(c->lowerBounds_[i]))
                    return false;
            }
            return true;
        }
    } // namespace
    ThetaCurve_ NewThetaCurve(const Vector_<>& times, const Vector_<>& values) {
        ThetaCurve_ c;
        if (!ValidThetaAxis(times, values))
            return c;
        c.times_ = times;
        c.values_ = values;
        const size_t n = times.size();
        c.slopes_ = Vector_<>(n, 0.);
        c.branches_ = Vector_<int>(n, 0);
        c.lowerBounds_ = Vector_<>(n - 1, 0.);
        Vector_<> h(n - 1), d(n - 1);
        c.peakWorkspaceBytes_ = sizeof(c) + 2 * sizeof(Vector_<>) + Bytes(c.times_) + Bytes(c.values_) + Bytes(c.slopes_) + Bytes(c.lowerBounds_) +
                                Bytes(c.branches_) + Bytes(h) + Bytes(d);
        for (size_t i = 0; i + 1 < n; ++i) {
            h[i] = times[i + 1] - times[i];
            d[i] = (values[i + 1] - values[i]) / h[i];
            if (!ValidSecant(h[i], d[i], values[i], values[i + 1]))
                return c;
        }
        if (!(values[1] > 0.))
            return c;
        if (!Slopes(h, d, &c))
            return c;
        c.certified_ = CertifyIntervals(&c);
        return c;
    }
    std::pair<double, double> ThetaAt(const ThetaCurve_& c, double time) {
        REQUIRE(c.certified_, "MarketIvConstraintFailure: uncertified theta representation");
        const auto it = std::lower_bound(c.times_.begin(), c.times_.end(), time);
        if (it == c.times_.end())
            return {c.values_.back() + c.slopes_.back() * (time - c.times_.back()), c.slopes_.back()};
        const size_t hi = static_cast<size_t>(it - c.times_.begin());
        if (*it == time)
            return {c.values_[hi], c.slopes_[hi]};
        const size_t lo = hi - 1;
        const long double h = static_cast<long double>(c.times_[hi]) - c.times_[lo], u = (static_cast<long double>(time) - c.times_[lo]) / h,
                          v = 1.L - u;
        const long double m0 = c.slopes_[lo], m1 = c.slopes_[hi], d = (static_cast<long double>(c.values_[hi]) - c.values_[lo]) / h;
        const long double value = v * v * (1.L + 2.L * u) * c.values_[lo] + u * u * (3.L - 2.L * u) * c.values_[hi] + h * u * v * (v * m0 - u * m1);
        const long double slope = m0 * v * v + 2.L * (3.L * d - m0 - m1) * u * v + m1 * u * u;
        const double representedSlope = static_cast<double>(slope);
        if (representedSlope == 0. && c.values_[hi] > c.values_[lo])
            return {static_cast<double>(value), -1.};
        return {static_cast<double>(value), representedSlope};
    }
    namespace {
        long double PriceAsNormalizedCall(double premium, bool call, double forward, double strike, double discount, long double df) {
            long double normalized;
            if (call)
                normalized = premium / df;
            else if (strike >= forward)
                normalized = 1.L + static_cast<long double>(std::fma(-discount, strike, premium)) / df;
            else
                normalized = premium / df + (static_cast<long double>(forward) - strike) / forward;
            return normalized;
        }
        long double BlackIntrinsic(long double x, bool call) { return call ? (x < 0. ? -std::expm1(x) : 0.) : (x > 0. ? std::expm1(x) : 0.); }
    } // namespace
    double NormalizePrice(double premium, bool call, double forward, double strike, double discount, double maxError) {
        const long double df = static_cast<long double>(discount) * forward;
        REQUIRE(df > 0. && std::isfinite(df), "MarketIvNumericRange: PRICE normalization scale cannot be represented");
        const long double normalized = PriceAsNormalizedCall(premium, call, forward, strike, discount, df);
        REQUIRE(std::isfinite(normalized) && normalized >= 0. && normalized <= 1.,
                "MarketIvNumericRange: PRICE parity conversion cannot represent a legal normalized call");
        const long double quantum = std::numeric_limits<double>::denorm_min();
        const long double scaledQuantum = quantum / df;
        // Cover absolute rounding of the double FMA and scale even when long double has only double precision.
        const long double error = 8.L * std::numeric_limits<double>::epsilon() *
                                      (call ? std::abs(normalized) : 1.L + std::abs(static_cast<long double>(premium) / df - strike / forward)) +
                                  8.L * scaledQuantum * (1.L + std::abs(normalized)) + 8.L * quantum +
                                  (!call && strike < forward ? 8.L * quantum / forward : 0.L);
        REQUIRE(scaledQuantum < .125L && error <= maxError, "MarketIvNumericRange: PRICE parity conversion error exceeds normalized precision");
        return static_cast<double>(normalized);
    }
    double NormalizedBlack(double k, double variance, bool call) {
        const long double x = k, w = variance;
        if (w == 0.)
            return static_cast<double>(BlackIntrinsic(x, call));
        REQUIRE(w > 0. && std::isfinite(w), "MarketIvNumericRange: invalid Black total variance");
        const long double root = std::sqrt(w), d1 = -x / root + .5L * root, d2 = d1 - root;
        const bool otmCall = x >= 0.;
        const long double a = otmCall ? LogCdf(d1) : x + LogCdf(-d2);
        const long double b = otmCall ? x + LogCdf(d2) : LogCdf(-d1);
        REQUIRE(b <= a, "MarketIvNumericRange: unresolved Black tail difference");
        const long double otm = std::exp(a) * (-std::expm1(b - a));
        REQUIRE(otm > 0. || call != otmCall, "MarketIvNumericRange: Black OTM price underflow");
        const long double intrinsic = BlackIntrinsic(x, call);
        const long double price = otm + intrinsic;
        REQUIRE(std::isfinite(price) && (price == 0. || price >= std::numeric_limits<double>::denorm_min()),
                "MarketIvNumericRange: Black price overflow or underflow");
        return static_cast<double>(price);
    }
    Smile_ Smile(double theta, double rho, double eta, double k) {
        if (eta == 0.)
            return {theta, 0., 0., 1., 0., rho * k * std::sqrt(theta / (1. + theta))};
        const long double th = theta, r = rho, e = eta, x = k;
        const long double phi = e / std::sqrt(th * (1.L + th)), z = phi * x + r, s = std::sqrt((1.L - r) * (1.L + r));
        const long double h = std::hypot(z, s), a = phi * x, b0 = 1.L + r * a;
        const long double b = b0 >= 0. ? b0 + h : s * s * a * a / (h - b0), thetaPhi = th * phi;
        const long double dPhi = -phi * (1.L + 2.L * th) / (2.L * th * (1.L + th));
        const long double wk = .5L * thetaPhi * (r + z / h);
        const std::array<long double, 5> derivatives{{wk, .5L * th * phi * phi * s * s / (h * h * h), .5L * b + .5L * th * x * dPhi * (r + z / h),
                                                      .5L * th * phi * x * (1.L + 1.L / h), wk * x / e}};
        const bool range = std::any_of(derivatives.begin(), derivatives.end(), [](long double value) {
            const double represented = static_cast<double>(value);
            return !std::isfinite(represented) || (value != 0. && represented == 0.);
        });
        return {static_cast<double>(.5L * th * b),
                static_cast<double>(derivatives[0]),
                static_cast<double>(derivatives[1]),
                static_cast<double>(derivatives[2]),
                static_cast<double>(derivatives[3]),
                static_cast<double>(derivatives[4]),
                range};
    }
    double EtaMax(const MarketIvFitSettings_& s) {
        const double lower = std::nextafter(1. - s.constraintMargin_, 0.);
        const double upper = std::nextafter(1. + s.rhoLimit_, std::numeric_limits<double>::infinity());
        const double ratio = std::nextafter(lower / upper, 0.);
        double e = std::nextafter(2. * std::nextafter(std::sqrt(ratio), 0.), 0.);
        while (GlobalBounds(e, s.rhoLimit_).first > 4. * lower)
            e = std::nextafter(e, 0.);
        return e;
    }
    std::pair<double, double> GlobalBounds(double eta, double rhoLimit) {
        if (eta == 0.)
            return {0., 0.};
        const double infinity = std::numeric_limits<double>::infinity();
        const double sum = std::nextafter(1. + rhoLimit, infinity);
        const double square = std::nextafter(eta * eta, infinity);
        return {std::nextafter(square * sum, infinity), std::nextafter(eta * sum, infinity) / 2.};
    }
    bool ParametersCertified(const MarketIvFitSettings_& s, double rho, double eta) {
        if (!std::isfinite(rho) || !std::isfinite(eta) || std::abs(rho) > s.rhoLimit_ || eta < 0. || eta > EtaMax(s))
            return false;
        const auto bound = GlobalBounds(eta, s.rhoLimit_);
        return bound.first <= 4. * std::nextafter(1. - s.constraintMargin_, 0.) && bound.second < 2.;
    }
    size_t CheckedAdd(size_t a, size_t b) {
        REQUIRE(b <= std::numeric_limits<size_t>::max() - a, "MarketIvBudgetExceeded: byte/count addition overflow");
        return a + b;
    }
    size_t CheckedMultiply(size_t a, size_t b) {
        REQUIRE(a == 0 || b <= std::numeric_limits<size_t>::max() / a, "MarketIvBudgetExceeded: byte/count multiplication overflow");
        return a * b;
    }
    void CheckBudget(size_t p, size_t w, size_t pb, size_t wb) {
        REQUIRE(p <= pb, "MarketIvBudgetExceeded: numeric_payload_budget_bytes; required=" + String_(std::to_string(p)) +
                             "; limit=" + String_(std::to_string(pb)));
        REQUIRE(w <= wb,
                "MarketIvBudgetExceeded: workspace_budget_bytes; required=" + String_(std::to_string(w)) + "; limit=" + String_(std::to_string(wb)));
    }
    MarketIvExecution_ PlanFit(size_t q, size_t m, size_t c, const MarketIvFitSettings_& s) {
        const size_t p = CheckedAdd(m, 2), h = CheckedMultiply(4, CheckedAdd(static_cast<size_t>(s.maxIterationsPerStart_), 1));
        auto sum = [](std::initializer_list<size_t> values) {
            size_t n = 0;
            for (const auto v : values)
                n = CheckedAdd(n, v);
            return n;
        };
        MarketIvExecution_ result;
        result.plannedNumericPayloadBytes_ = sum(
            {8192, CheckedMultiply(512, q), CheckedMultiply(256, c), CheckedMultiply(256, m + 1), CheckedMultiply(128, h), CheckedMultiply(64, p)});
        result.plannedWorkspaceBytes_ = sum({8192, CheckedMultiply(32, CheckedMultiply(q, p)), CheckedMultiply(128, q),
                                             CheckedMultiply(128, CheckedMultiply(p, p)), CheckedMultiply(256, c), CheckedMultiply(128, h)});
        CheckBudget(result.plannedNumericPayloadBytes_, result.plannedWorkspaceBytes_, s.numericPayloadBudgetBytes_, s.workspaceBudgetBytes_);
        return result;
    }
} // namespace Dal::MarketIvDetail
