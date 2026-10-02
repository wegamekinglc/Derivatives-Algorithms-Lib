//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

#include <dal/math/random/pseudorandom.hpp>
#include <dal/model/gsrslv.hpp>
#include <dal/model/gsrslvpricinginternal.hpp>

namespace Dal {
    namespace {
        struct Term_ {
            double coefficient_;
            Vector_<std::pair<size_t, int>> powers_;
        };

        struct Payoff_ {
            size_t sample_;
            Vector_<Term_> terms_;

            [[nodiscard]] double Value(const AAD::Scenario_<>& path) const {
                const auto& sample = path[sample_];
                double value = 0.0;
                for (const auto& term : terms_) {
                    double amount = term.coefficient_;
                    for (const auto& [slot, power] : term.powers_)
                        if (slot != std::numeric_limits<size_t>::max())
                            amount = power == 1 ? amount * sample.observations_[slot] : amount / sample.observations_[slot];
                    value += amount;
                }
                REQUIRE(std::isfinite(value), "InvalidGSRSLVPricing: payoff overflow");
                value = std::max(0.0, value) / sample.numeraire_;
                REQUIRE(std::isfinite(value), "InvalidGSRSLVPricing: payoff overflow");
                return value;
            }
        };

        class PayoffBuilder_ {
            const GSRSLVModelData_& data_;
            AAD::GSR_<> rates_;
            Vector_<Date_> expiries_;
            Vector_<AAD::SampleDef_> definitions_;

            double Time(const Date_& date) const {
                REQUIRE(date.IsValid() && date >= data_.gaussian_->curve_->evaluationDate_ && date <= data_.gaussian_->curve_->nodeDates_.back(),
                        "InvalidGSRSLVPricing: date outside curve horizon");
                return (date - data_.gaussian_->curve_->evaluationDate_) / DAYS_PER_YEAR;
            }

            size_t Slot(size_t sample, const Date_& maturity) {
                REQUIRE(maturity >= expiries_[sample], "InvalidGSRSLVPricing: maturity precedes exercise");
                static_cast<void>(Time(maturity));
                if (maturity == expiries_[sample])
                    return std::numeric_limits<size_t>::max();
                const String_ name = "IR[" + data_.gaussian_->curve_->currency_ + ",DF," + Date::ToString(maturity) + "]";
                auto& names = definitions_[sample].indexNames_;
                const auto found = std::find(names.begin(), names.end(), name);
                if (found != names.end())
                    return static_cast<size_t>(found - names.begin());
                names.push_back(name);
                return names.size() - 1;
            }

            double ProjectionScale(const GSRFloatingCoupon_& coupon) const {
                REQUIRE(coupon.fixing_.IsValid() && coupon.fixing_ <= coupon.start_ && coupon.start_ < coupon.end_ && coupon.end_ <= coupon.payment_,
                        "InvalidGSRSLVPricing: invalid floating coupon schedule");
                REQUIRE(std::isfinite(coupon.indexAccrual_) && coupon.indexAccrual_ > 0.0 && std::isfinite(coupon.couponAccrual_) &&
                            coupon.couponAccrual_ > 0.0,
                        "InvalidGSRSLVPricing: accruals must be finite and positive");
                const double start = Time(coupon.start_), end = Time(coupon.end_);
                const double scale = std::exp(rates_.InitialLogProjection(start, coupon.tenor_) - rates_.InitialLogProjection(end, coupon.tenor_) -
                                              rates_.InitialLogDiscount(start) + rates_.InitialLogDiscount(end));
                REQUIRE(std::isfinite(scale) && scale > 0.0, "InvalidGSRSLVPricing: projection scale overflow or underflow");
                return scale;
            }

            void Floating(Payoff_* payoff, const GSRFloatingCoupon_& coupon, double sign) {
                REQUIRE(coupon.fixing_ >= expiries_[payoff->sample_], "InvalidGSRSLVPricing: floating fixing precedes exercise");
                REQUIRE(coupon.fixing_ == expiries_[payoff->sample_] || (coupon.fixing_ == coupon.start_ && coupon.payment_ == coupon.end_),
                        "InvalidGSRSLVPricing: future floating lag requires conditional valuation");
                const double scale = ProjectionScale(coupon);
                const double accrual = sign * coupon.couponAccrual_ / coupon.indexAccrual_;
                REQUIRE(std::isfinite(accrual), "InvalidGSRSLVPricing: coupon accrual ratio overflow");
                const auto start = Slot(payoff->sample_, coupon.start_), end = Slot(payoff->sample_, coupon.end_),
                           pay = Slot(payoff->sample_, coupon.payment_);
                payoff->terms_.push_back({accrual * scale, {{start, 1}, {end, -1}, {pay, 1}}});
                payoff->terms_.push_back({-accrual, {{pay, 1}}});
            }

            void Build(Payoff_* payoff, const GSRBondOption_& option, double sign) {
                REQUIRE(option.strike_ >= 0.0, "InvalidGSRSLVPricing: bond strike must be nonnegative");
                payoff->terms_ = {{sign, {{Slot(payoff->sample_, option.maturity_), 1}}}, {-sign * option.strike_, {}}};
            }

            void Build(Payoff_* payoff, const GSRCaplet_& option, double sign) {
                Floating(payoff,
                         {option.expiry_, option.start_, option.end_, option.payment_, option.indexAccrual_, option.couponAccrual_, option.tenor_},
                         sign);
                payoff->terms_.push_back({-sign * option.couponAccrual_ * option.strike_, {{Slot(payoff->sample_, option.payment_), 1}}});
            }

            void Build(Payoff_* payoff, const GSRSwaption_& option, double sign) {
                REQUIRE(!option.fixed_.empty() && !option.floating_.empty(), "InvalidGSRSLVPricing: swap legs must be nonempty");
                for (const auto& coupon : option.floating_)
                    Floating(payoff, coupon, sign);
                for (const auto& coupon : option.fixed_) {
                    REQUIRE(std::isfinite(coupon.accrual_) && coupon.accrual_ > 0.0,
                            "InvalidGSRSLVPricing: fixed accrual must be finite and positive");
                    payoff->terms_.push_back({-sign * option.strike_ * coupon.accrual_, {{Slot(payoff->sample_, coupon.payment_), 1}}});
                }
            }

        public:
            PayoffBuilder_(const GSRSLVModelData_& data, const Vector_<GSREuropeanOption_>& options) : data_(data), rates_(*data.gaussian_) {
                REQUIRE(!options.empty(), "InvalidGSRSLVPricing: options must be nonempty");
                for (const auto& option : options)
                    std::visit([&](const auto& input) { expiries_.push_back(input.expiry_); }, option);
                std::sort(expiries_.begin(), expiries_.end());
                expiries_.erase(std::unique(expiries_.begin(), expiries_.end()), expiries_.end());
                definitions_.Resize(expiries_.size());
            }

            Payoff_ Build(const GSREuropeanOption_& option) {
                return std::visit(
                    [&](const auto& input) {
                        REQUIRE(std::isfinite(input.strike_), "InvalidGSRSLVPricing: strike must be finite");
                        REQUIRE(input.type_ == OptionType_("CALL") || input.type_ == OptionType_("PUT"),
                                "InvalidGSRSLVPricing: option type must be CALL or PUT");
                        static_cast<void>(Time(input.expiry_));
                        Payoff_ payoff{static_cast<size_t>(std::lower_bound(expiries_.begin(), expiries_.end(), input.expiry_) - expiries_.begin()),
                                       {}};
                        Build(&payoff, input, input.type_ == OptionType_("CALL") ? 1.0 : -1.0);
                        return payoff;
                    },
                    option);
            }

            Vector_<> Timeline() const {
                Vector_<> times;
                for (const auto& expiry : expiries_)
                    times.push_back(Time(expiry));
                return times;
            }
            const Vector_<AAD::SampleDef_>& Definitions() const { return definitions_; }
        };
    } // namespace

    namespace GSRSLVPricingInternal {
        struct PreparedPricer_::Data_ {
            Vector_<Payoff_> payoffs_;
            Vector_<> timeline_;
            Vector_<AAD::SampleDef_> definitions_;
            Vector_<Vector_<>> normals_;
            size_t dimension_;
            GSRMonteCarloSettings_ settings_;
        };

        PreparedPricer_::PreparedPricer_(const GSRSLVModelData_& data,
                                         const Vector_<GSREuropeanOption_>& options,
                                         const GSRMonteCarloSettings_& settings) {
            REQUIRE(settings.paths_ >= 4 && settings.paths_ % 2 == 0, "InvalidGSRSLVPricing: paths must be even and at least four");
            REQUIRE(settings.seed_ >= 0, "InvalidGSRSLVPricing: seed must be nonnegative");
            auto prepared = std::make_shared<Data_>();
            PayoffBuilder_ builder(data, options);
            for (const auto& option : options)
                prepared->payoffs_.push_back(builder.Build(option));
            prepared->timeline_ = builder.Timeline();
            prepared->definitions_ = builder.Definitions();
            prepared->settings_ = settings;
            AAD::GSRSLV_<> model(data);
            model.Allocate(prepared->timeline_, prepared->definitions_);
            prepared->dimension_ = model.SimDim();
            const size_t pairs = settings.paths_ / 2;
            if (pairs <= 16777216 / std::max(size_t(1), prepared->dimension_)) {
                auto generator = New(RNGType_("MRG32"), settings.seed_, prepared->dimension_, true);
                prepared->normals_.Resize(pairs);
                for (auto& normals : prepared->normals_) {
                    normals.Resize(prepared->dimension_);
                    generator->FillNormal(&normals);
                }
            }
            data_ = std::move(prepared);
        }

        Vector_<GSRMonteCarloPrice_> PreparedPricer_::Price(const GSRSLVModelData_& data) const {
            const auto& settings = data_->settings_;
            const auto& payoffs = data_->payoffs_;
            AAD::GSRSLV_<> model(data);
            model.Allocate(data_->timeline_, data_->definitions_);
            model.Init(data_->timeline_, data_->definitions_);
            REQUIRE(model.SimDim() == data_->dimension_, "InvalidGSRSLVPricing: prepared integration grid changed");
            auto generator = data_->normals_.empty() ? New(RNGType_("MRG32"), settings.seed_, model.SimDim(), true) : nullptr;
            Vector_<> normals(model.SimDim()), first(payoffs.size()), means(payoffs.size(), 0.0), squares(payoffs.size(), 0.0);
            AAD::Scenario_<> path;
            AAD::AllocatePath(data_->definitions_, path);
            const int pairs = settings.paths_ / 2;
            for (int pair = 0; pair < pairs; ++pair) {
                if (generator)
                    generator->FillNormal(&normals);
                else
                    normals = data_->normals_[pair];
                model.GeneratePath(normals, &path);
                for (size_t i = 0; i < payoffs.size(); ++i)
                    first[i] = payoffs[i].Value(path);
                for (auto& value : normals)
                    value = -value;
                model.GeneratePath(normals, &path);
                for (size_t i = 0; i < payoffs.size(); ++i) {
                    const double value = 0.5 * first[i] + 0.5 * payoffs[i].Value(path);
                    const double delta = value - means[i];
                    means[i] += delta / (pair + 1);
                    squares[i] += delta * (value - means[i]);
                }
            }
            Vector_<GSRMonteCarloPrice_> result;
            for (size_t i = 0; i < payoffs.size(); ++i) {
                const double error = std::sqrt(std::max(0.0, squares[i]) / (pairs * static_cast<double>(pairs - 1)));
                REQUIRE(std::isfinite(means[i]) && std::isfinite(squares[i]) && std::isfinite(error),
                        "InvalidGSRSLVPricing: sampling moments overflow");
                result.push_back({means[i], error});
            }
            return result;
        }

    } // namespace GSRSLVPricingInternal

    Vector_<GSRMonteCarloPrice_>
    PriceGSRSLVEuropeanOptions(const GSRSLVModelData_& data, const Vector_<GSREuropeanOption_>& options, const GSRMonteCarloSettings_& settings) {
        return GSRSLVPricingInternal::PreparedPricer_(data, options, settings).Price(data);
    }
} // namespace Dal
