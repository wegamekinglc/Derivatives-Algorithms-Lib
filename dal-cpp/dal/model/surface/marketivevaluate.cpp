#include <dal/platform/platform.hpp>

#include <dal/model/surface/marketivsurfaceinternal.hpp>

namespace Dal {
    namespace {
        std::optional<double> Representable(long double value) {
            const double result = static_cast<double>(value);
            if (!std::isfinite(result) || (value != 0. && result == 0.))
                return std::nullopt;
            return result;
        }
    } // namespace
    namespace MarketIvDetail {
        namespace {
            struct PointContext_ {
                MarketIvQuery_ query_;
                Carry_ carry_;
                Smile_ smile_;
                double k_, thetaSlope_;
                long double df_, wt_, g_, pdf_, root_, ck_, ckk_, ct_;
            };
            bool AllRepresented(std::initializer_list<std::optional<double>> values) {
                return std::all_of(values.begin(), values.end(), [](const auto& value) { return value.has_value(); });
            }
            PointContext_ Derivatives(PointContext_ x, double call) {
                const long double w = x.smile_.w_, wk = x.smile_.wk_, wkk = x.smile_.wkk_;
                x.wt_ = static_cast<long double>(x.smile_.wTheta_) * x.thetaSlope_;
                x.root_ = std::sqrt(w);
                const long double d1 = -x.k_ / x.root_ + .5L * x.root_, d2 = d1 - x.root_;
                x.pdf_ = std::exp(-.5L * d1 * d1) / std::sqrt(2.L * std::acos(-1.L));
                const long double nd2 = .5L * std::erfc(-d2 / std::sqrt(2.L));
                const long double a = 1.L - x.k_ * wk / (2.L * w), b = wk * wk / 4.L * (1.L / w + .25L), c = wkk / 2.L;
                x.g_ = a * a - b + c;
                x.ck_ = -x.carry_.discount_ * nd2 + x.df_ * x.pdf_ * wk / (2.L * x.root_ * x.query_.strike_);
                x.ckk_ = x.df_ * x.pdf_ / x.root_ * x.g_ / x.query_.strike_ / x.query_.strike_;
                x.ct_ = -x.carry_.yield_ * call - (x.carry_.rate_ - x.carry_.yield_) * x.query_.strike_ * x.ck_ +
                        x.df_ * x.pdf_ / (2.L * x.root_) * x.wt_;
                return x;
            }
            bool DensityCertified(const PointContext_& x, const MarketIvEvaluationSettings_& s) {
                const long double w = x.smile_.w_, wk = x.smile_.wk_, wkk = x.smile_.wkk_;
                const long double a = 1.L - x.k_ * wk / (2.L * w), b = wk * wk / 4.L * (1.L / w + .25L), c = wkk / 2.L;
                const long double error = 128.L * std::numeric_limits<double>::epsilon() * (a * a + std::abs(b) + std::abs(c));
                return x.query_.time_ >= s.minQueryTime_ && w >= s.minTotalVariance_ && x.g_ >= s.densityFactorFloor_ &&
                       error <= 1e-8L * std::abs(x.g_) && x.wt_ >= 0.;
            }
        } // namespace
        size_t EvaluationWorkspaceBytes() {
            return sizeof(PointContext_) + 2 * sizeof(MarketIvPointResult_) + sizeof(Smile_) + sizeof(Carry_) + 32 * sizeof(long double);
        }
        struct PointAccess_ {
            static bool Values(MarketIvPointResult_* p, const PointContext_& x) {
                p->totalVariance_ = x.smile_.w_;
                p->impliedVol_ = Representable(std::sqrt(static_cast<long double>(x.smile_.w_) / x.query_.time_));
                try {
                    p->callPrice_ = Representable(x.df_ * NormalizedBlack(x.k_, x.smile_.w_, true));
                    p->putPrice_ = Representable(x.df_ * NormalizedBlack(x.k_, x.smile_.w_, false));
                } catch (const Exception_&) {
                    return false;
                }
                if (!AllRepresented({p->impliedVol_, p->callPrice_, p->putPrice_}))
                    return false;
                p->valueStatus_ = "SUCCESS";
                return true;
            }
            static bool PopulateDerivatives(MarketIvPointResult_* p, const PointContext_& x) {
                if (x.smile_.derivativeRange_)
                    return false;
                p->varianceK_ = Representable(x.smile_.wk_);
                p->varianceKK_ = Representable(x.smile_.wkk_);
                p->varianceT_ = Representable(x.wt_);
                p->fixedKVarianceT_ = Representable(x.wt_ - (x.carry_.rate_ - x.carry_.yield_) * x.smile_.wk_);
                p->densityFactor_ = Representable(x.g_);
                p->callK_ = Representable(x.ck_);
                p->callKK_ = Representable(x.ckk_);
                p->callT_ = Representable(x.ct_);
                if (!AllRepresented(
                        {p->varianceK_, p->varianceKK_, p->varianceT_, p->fixedKVarianceT_, p->densityFactor_, p->callK_, p->callKK_, p->callT_}))
                    return false;
                if (!(x.pdf_ > 0.) || !(x.ckk_ > 0.))
                    return false;
                p->derivativeStatus_ = "SUCCESS";
                return true;
            }
            static MarketIvPointResult_
            Evaluate(const MarketIvCalibrationSnapshot_& snapshot, const MarketIvQuery_& query, const MarketIvEvaluationSettings_& settings) {
                MarketIvPointResult_ p;
                PointContext_ x{};
                x.query_ = query;
                x.carry_ = CarryAt(snapshot.Request().market_, query.time_);
                p.carryDerivative_ = x.carry_.knot_ ? "RIGHT_CARRY_DERIVATIVE" : "SMOOTH_CARRY_DERIVATIVE";
                if (!(x.carry_.forward_ > 0.) || !(x.carry_.discount_ > 0.) || !std::isfinite(x.carry_.forward_ * x.carry_.discount_))
                    return p;
                x.k_ = std::log(query.strike_) - std::log(x.carry_.forward_);
                const auto theta = ThetaAt(MarketIvData(snapshot).curve_, query.time_);
                if (!(theta.first > 0.) || !(theta.second >= 0.) || !std::isfinite(theta.first) || !std::isfinite(theta.second))
                    return p;
                x.thetaSlope_ = theta.second;
                x.smile_ = Smile(theta.first, snapshot.Rho(), snapshot.Eta(), x.k_);
                if (!(x.smile_.w_ > 0.) || !std::isfinite(x.smile_.w_))
                    return p;
                x.df_ = static_cast<long double>(x.carry_.forward_) * x.carry_.discount_;
                if (!Values(&p, x))
                    return p;
                x = Derivatives(x, *p.callPrice_);
                if (!PopulateDerivatives(&p, x))
                    return p;
                p.dupireStatus_ = "SINGULAR_DENSITY";
                if (!DensityCertified(x, settings))
                    return p;
                p.localVariance_ = Representable(x.wt_ / x.g_);
                p.localVol_ = Representable(std::sqrt(x.wt_ / x.g_));
                p.dupireStatus_ = AllRepresented({p.localVariance_, p.localVol_}) ? "SUCCESS" : "NUMERIC_RANGE";
                return p;
            }
        };
    } // namespace MarketIvDetail

    MarketIvPointResult_
    EvaluateMarketIvSurface(const MarketIvCalibrationSnapshot_& snapshot, const MarketIvQuery_& query, const MarketIvEvaluationSettings_& settings) {
        MarketIvDetail::ValidateQuery(query);
        ValidateMarketIvEvaluationSettings(settings);
        return MarketIvDetail::PointAccess_::Evaluate(snapshot, query, settings);
    }
    namespace {
        class MarketIvLegacy_ final : public AAD::IVS_ {
            MarketIvCalibrationSnapshot_ snapshot_;

        public:
            explicit MarketIvLegacy_(const MarketIvCalibrationSnapshot_& snapshot)
                : IVS_(snapshot.Request().market_.Spot(), snapshot.Request().market_.Rate(), snapshot.Request().market_.DividendYield()),
                  snapshot_(snapshot) {}
            double ImpliedVol(double strike, double time) const override {
                const auto point = EvaluateMarketIvSurface(snapshot_, {strike, time});
                REQUIRE(point.ValueStatus() == "SUCCESS", "MarketIvNumericRange: legacy implied vol unavailable");
                return *point.ImpliedVol();
            }
        };
    } // namespace
    Handle_<AAD::IVS_> MarketIvSurfaceAsLegacyIVS(const MarketIvCalibrationSnapshot_& snapshot) {
        REQUIRE(snapshot.Request().market_.IsFlatCarry(), "InvalidMarketIvRequest: legacy IVS adapter requires explicit FLAT_CARRY snapshot");
        return Handle_<AAD::IVS_>(new MarketIvLegacy_(snapshot));
    }
    namespace MarketIvDetail {
        std::optional<double> Quantity(const MarketIvPointResult_& point, const MarketIvQuantity_& quantity) {
            switch (quantity.Switch()) {
            case MarketIvQuantity_::Value_::TOTAL_VARIANCE:
                return point.ValueStatus() == "SUCCESS" ? point.TotalVariance() : std::nullopt;
            case MarketIvQuantity_::Value_::IMPLIED_VOL:
                return point.ValueStatus() == "SUCCESS" ? point.ImpliedVol() : std::nullopt;
            case MarketIvQuantity_::Value_::CALL_PRICE:
                return point.ValueStatus() == "SUCCESS" ? point.CallPrice() : std::nullopt;
            case MarketIvQuantity_::Value_::PUT_PRICE:
                return point.ValueStatus() == "SUCCESS" ? point.PutPrice() : std::nullopt;
            case MarketIvQuantity_::Value_::LOCAL_VOL:
                return point.DupireStatus() == "SUCCESS" ? point.LocalVol() : std::nullopt;
            default:
                THROW("InvalidMarketIvRequest: unknown output quantity");
            }
        }
    } // namespace MarketIvDetail
} // namespace Dal
