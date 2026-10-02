//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include <dal/math/random/pseudorandom.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/model/gsrslv.hpp>
#include <dal/model/gsrslvpricinginternal.hpp>

namespace Dal {
    namespace {
        struct Term_ {
            double coefficient_;
            struct Observation_ {
                size_t sample_, slot_;
                int power_;
            };
            Vector_<Observation_> powers_;
            size_t discountFrom_ = std::numeric_limits<size_t>::max();

            template <class T_> T_ Value(const AAD::Scenario_<T_>& path, size_t exercise) const {
                T_ amount(coefficient_);
                for (const auto& observation : powers_)
                    if (observation.slot_ != std::numeric_limits<size_t>::max()) {
                        const auto& value = path[observation.sample_].observations_[observation.slot_];
                        amount = observation.power_ == 1 ? T_(amount * value) : T_(amount / value);
                    }
                if (discountFrom_ != std::numeric_limits<size_t>::max())
                    amount *= path[exercise].numeraire_ / path[discountFrom_].numeraire_;
                return amount;
            }
        };

        struct Payoff_ {
            size_t sample_;
            Vector_<Term_> terms_;
            Vector_<Term_> conditional_;

            template <class T_> T_ Sum(const Vector_<Term_>& terms, const AAD::Scenario_<T_>& path) const {
                T_ value(0.0);
                for (const auto& term : terms)
                    value += term.Value(path, sample_);
                return value;
            }

            template <class T_> T_ Positive(const T_& value, const AAD::Scenario_<T_>& path) const {
                REQUIRE(std::isfinite(AAD::Value(value)), "InvalidGSRSLVPricing: payoff overflow");
                const T_ price = AAD::Value(value) > 0.0 ? T_(value / path[sample_].numeraire_) : T_(0.0);
                REQUIRE(std::isfinite(AAD::Value(price)), "InvalidGSRSLVPricing: payoff overflow");
                return price;
            }
        };

        Vector_<GSRMonteCarloPrice_> SamplingPrices(const Vector_<>& means, const Vector_<>& squares, const Vector_<>& conditional, int pairs) {
            Vector_<GSRMonteCarloPrice_> result;
            for (size_t i = 0; i < means.size(); ++i) {
                const double error = std::sqrt(std::max(0.0, squares[i]) / (pairs * static_cast<double>(pairs - 1)));
                REQUIRE(std::isfinite(means[i]) && std::isfinite(squares[i]) && std::isfinite(error),
                        "InvalidGSRSLVPricing: sampling moments overflow");
                REQUIRE(std::isfinite(conditional[i]), "InvalidGSRSLVPricing: conditional refinement overflow");
                result.push_back({means[i], error, std::max(0.0, conditional[i])});
            }
            return result;
        }

        class PayoffBuilder_ {
            const GSRSLVModelData_& data_;
            AAD::GSR_<> rates_;
            Vector_<Date_> expiries_;
            Vector_<AAD::SampleDef_> definitions_;
            Date_ lastExercise_;

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

            size_t Sample(const Date_& date) const {
                return static_cast<size_t>(std::lower_bound(expiries_.begin(), expiries_.end(), date) - expiries_.begin());
            }

            Term_::Observation_ Observation(size_t sample, const Date_& date, int power = 1) { return {sample, Slot(sample, date), power}; }

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
                static_cast<void>(Time(coupon.fixing_));
                const double scale = ProjectionScale(coupon);
                const double accrual = sign * coupon.couponAccrual_ / coupon.indexAccrual_;
                REQUIRE(std::isfinite(accrual), "InvalidGSRSLVPricing: coupon accrual ratio overflow");
                const auto exercise = expiries_[payoff->sample_];
                const bool conditional = coupon.fixing_ > exercise && (coupon.fixing_ != coupon.start_ || coupon.payment_ != coupon.end_);
                const size_t fixing = conditional || coupon.fixing_ < exercise ? Sample(coupon.fixing_) : payoff->sample_;
                const size_t paySample = conditional ? fixing : payoff->sample_;
                const auto start = Observation(fixing, coupon.start_), end = Observation(fixing, coupon.end_, -1),
                           pay = Observation(paySample, coupon.payment_);
                auto& terms = conditional ? payoff->conditional_ : payoff->terms_;
                const size_t discount = conditional ? fixing : std::numeric_limits<size_t>::max();
                terms.push_back({accrual * scale, {start, end, pay}, discount});
                terms.push_back({-accrual, {pay}, discount});
            }

            void Build(Payoff_* payoff, const GSRBondOption_& option, double sign) {
                REQUIRE(option.strike_ >= 0.0, "InvalidGSRSLVPricing: bond strike must be nonnegative");
                payoff->terms_ = {{sign, {Observation(payoff->sample_, option.maturity_)}}, {-sign * option.strike_, {}}};
            }

            void Build(Payoff_* payoff, const GSRCaplet_& option, double sign) {
                Floating(payoff,
                         {option.expiry_, option.start_, option.end_, option.payment_, option.indexAccrual_, option.couponAccrual_, option.tenor_},
                         sign);
                payoff->terms_.push_back({-sign * option.couponAccrual_ * option.strike_, {Observation(payoff->sample_, option.payment_)}});
            }

            void Build(Payoff_* payoff, const GSRSwaption_& option, double sign) {
                REQUIRE(!option.fixed_.empty() && !option.floating_.empty(), "InvalidGSRSLVPricing: swap legs must be nonempty");
                for (const auto& coupon : option.floating_)
                    Floating(payoff, coupon, sign);
                for (const auto& coupon : option.fixed_) {
                    REQUIRE(std::isfinite(coupon.accrual_) && coupon.accrual_ > 0.0,
                            "InvalidGSRSLVPricing: fixed accrual must be finite and positive");
                    payoff->terms_.push_back({-sign * option.strike_ * coupon.accrual_, {Observation(payoff->sample_, coupon.payment_)}});
                }
            }

        public:
            PayoffBuilder_(const GSRSLVModelData_& data, const Vector_<GSREuropeanOption_>& options) : data_(data), rates_(*data.gaussian_) {
                REQUIRE(!options.empty(), "InvalidGSRSLVPricing: options must be nonempty");
                for (const auto& option : options)
                    std::visit([&](const auto& input) { expiries_.push_back(input.expiry_); }, option);
                lastExercise_ = *std::max_element(expiries_.begin(), expiries_.end());
                for (const auto& option : options)
                    if (const auto* swaption = std::get_if<GSRSwaption_>(&option))
                        for (const auto& coupon : swaption->floating_)
                            if (coupon.fixing_ < swaption->expiry_ ||
                                (coupon.fixing_ > swaption->expiry_ && (coupon.fixing_ != coupon.start_ || coupon.payment_ != coupon.end_)))
                                expiries_.push_back(coupon.fixing_);
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
                        Payoff_ payoff{Sample(input.expiry_), {}, {}};
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
            size_t OuterSamples() const { return Sample(lastExercise_) + 1; }
        };

        template <class T_>
        std::pair<T_, double> Value(const Payoff_& payoff,
                                    const AAD::Scenario_<T_>& outer,
                                    const Vector_<typename AAD::GSRSLV_<T_>::State_>& states,
                                    const AAD::GSRSLV_<T_>* inner,
                                    const Vector_<Vector_<>>& draws,
                                    AAD::Scenario_<T_>* path) {
            const T_ direct = payoff.Sum(payoff.terms_, outer);
            if (payoff.conditional_.empty())
                return {payoff.Positive(direct, outer), 0.0};
            std::copy(outer.begin(), outer.end(), path->begin());
            T_ halves[2]{T_(0.0), T_(0.0)};
            const size_t dimension = inner->SimDimFrom(payoff.sample_), offset = inner->SimDim() - dimension;
            Vector_<> normal(dimension);
            for (size_t pair = 0; pair < draws.size(); ++pair) {
                std::copy(draws[pair].begin() + offset, draws[pair].end(), normal.begin());
                for (int sign : {1, -1}) {
                    inner->GeneratePathFrom(payoff.sample_, states[payoff.sample_], normal, path);
                    halves[pair < draws.size() / 2 ? 0 : 1] += payoff.Sum(payoff.conditional_, *path) / static_cast<double>(draws.size());
                    if (sign == 1)
                        for (auto& value : normal)
                            value = -value;
                }
            }
            const T_ fine = payoff.Positive(T_(direct + 0.5 * (halves[0] + halves[1])), outer);
            const T_ coarse = 0.5 * (payoff.Positive(T_(direct + halves[0]), outer) + payoff.Positive(T_(direct + halves[1]), outer));
            return {fine, std::max(0.0, AAD::Value(coarse) - AAD::Value(fine))};
        }
    } // namespace

    namespace GSRSLVPricingInternal {
        struct PreparedPricer_::Data_ {
            Vector_<Payoff_> payoffs_;
            Vector_<> timeline_;
            Vector_<AAD::SampleDef_> definitions_;
            size_t outerSamples_;
            bool conditional_;
            Vector_<Vector_<>> normals_;
            size_t dimension_;
            GSRMonteCarloSettings_ settings_;

            template <class T_, class I_, class M_, class R_, class F_>
            void Samples(const GSRSLVModelData_& data, I_ initialize, M_ mark, R_ reset, F_ consume) const {
                AAD::GSRSLV_<T_> model(data);
                const Vector_<> outerTimeline(timeline_.begin(), timeline_.begin() + outerSamples_);
                const Vector_<AAD::SampleDef_> outerDefinitions(definitions_.begin(), definitions_.begin() + outerSamples_);
                model.Allocate(outerTimeline, outerDefinitions);
                std::unique_ptr<AAD::GSRSLV_<T_>> inner;
                if (conditional_) {
                    inner = std::make_unique<AAD::GSRSLV_<T_>>(data);
                    inner->Allocate(timeline_, definitions_);
                }
                initialize(&model, inner.get());
                model.Init(outerTimeline, outerDefinitions);
                if (inner)
                    inner->Init(timeline_, definitions_);
                REQUIRE(model.SimDim() == dimension_, "InvalidGSRSLVPricing: prepared integration grid changed");
                auto generator = normals_.empty() ? New(RNGType_("MRG32"), settings_.seed_, dimension_, true) : nullptr;
                auto innerGenerator = inner ? New(RNGType_("MRG32"), static_cast<int>((static_cast<uint64_t>(settings_.seed_) + 104729) % 2147483647),
                                                  inner->SimDim(), true)
                                            : nullptr;
                Vector_<Vector_<>> draws;
                if (inner)
                    draws = Vector_<Vector_<>>(settings_.conditionalPaths_ / 2, Vector_<>(inner->SimDim()));
                Vector_<> normals(dimension_), errors(payoffs_.size());
                Vector_<T_> values(payoffs_.size());
                AAD::Scenario_<T_> path, continuation;
                AAD::AllocatePath(outerDefinitions, path);
                AAD::AllocatePath(definitions_, continuation);
                Vector_<typename AAD::GSRSLV_<T_>::State_> states;
                mark();
                for (int pair = 0; pair < settings_.paths_ / 2; ++pair) {
                    if (generator)
                        generator->FillNormal(&normals);
                    else
                        normals = normals_[pair];
                    for (auto& draw : draws)
                        innerGenerator->FillNormal(&draw);
                    for (int sign : {1, -1}) {
                        reset();
                        if (inner)
                            model.GeneratePathWithStates(normals, &path, &states);
                        else
                            model.GeneratePath(normals, &path);
                        for (size_t i = 0; i < payoffs_.size(); ++i) {
                            const auto value = Value(payoffs_[i], path, states, inner.get(), draws, &continuation);
                            values[i] = value.first;
                            errors[i] = value.second;
                        }
                        consume(&values, errors, pair, sign);
                        if (sign == 1)
                            for (auto& value : normals)
                                value = -value;
                    }
                }
            }
        };

        PreparedPricer_::PreparedPricer_(const GSRSLVModelData_& data,
                                         const Vector_<GSREuropeanOption_>& options,
                                         const GSRMonteCarloSettings_& settings) {
            REQUIRE(settings.paths_ >= 4 && settings.paths_ % 2 == 0, "InvalidGSRSLVPricing: paths must be even and at least four");
            REQUIRE(settings.seed_ >= 0, "InvalidGSRSLVPricing: seed must be nonnegative");
            REQUIRE(settings.conditionalPaths_ >= 4 && settings.conditionalPaths_ % 4 == 0,
                    "InvalidGSRSLVPricing: conditional paths must be divisible by four and at least four");
            auto prepared = std::make_shared<Data_>();
            PayoffBuilder_ builder(data, options);
            for (const auto& option : options)
                prepared->payoffs_.push_back(builder.Build(option));
            prepared->timeline_ = builder.Timeline();
            prepared->definitions_ = builder.Definitions();
            prepared->outerSamples_ = builder.OuterSamples();
            prepared->conditional_ =
                std::any_of(prepared->payoffs_.begin(), prepared->payoffs_.end(), [](const auto& payoff) { return !payoff.conditional_.empty(); });
            prepared->settings_ = settings;
            AAD::GSRSLV_<> model(data);
            const Vector_<> outerTimeline(prepared->timeline_.begin(), prepared->timeline_.begin() + prepared->outerSamples_);
            const Vector_<AAD::SampleDef_> outerDefinitions(prepared->definitions_.begin(), prepared->definitions_.begin() + prepared->outerSamples_);
            model.Allocate(outerTimeline, outerDefinitions);
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
            const size_t count = data_->payoffs_.size();
            Vector_<> first(count), firstConditional(count), means(count, 0.0), squares(count, 0.0), conditional(count, 0.0);
            data_->Samples<double>(
                data, [](auto*, auto*) {}, [] {}, [] {},
                [&](auto* values, const auto& errors, int pair, int sign) {
                    if (sign == 1) {
                        first = *values;
                        firstConditional = errors;
                        return;
                    }
                    for (size_t i = 0; i < count; ++i) {
                        const double value = 0.5 * first[i] + 0.5 * (*values)[i];
                        const double delta = value - means[i];
                        means[i] += delta / (pair + 1);
                        squares[i] += delta * (value - means[i]);
                        conditional[i] += (0.5 * (firstConditional[i] + errors[i]) - conditional[i]) / (pair + 1);
                    }
                });
            return SamplingPrices(means, squares, conditional, data_->settings_.paths_ / 2);
        }

        Matrix_<> PreparedPricer_::Jacobian(const GSRSLVModelData_& data, const Vector_<String_>& labels) const {
            std::set<String_> unique;
            for (const auto& label : labels)
                REQUIRE(unique.insert(label).second, "InvalidGSRSLVPricing: duplicate Jacobian parameter " + label);
            const TapeGuard_ guard(AAD::Tape());
            Vector_<Vector_<AAD::Number_*>> selected(labels.size());
            Matrix_<> result(data_->payoffs_.size(), labels.size(), 0.0);
            const auto initialize = [&](auto* outer, auto* inner) {
                for (auto* model : {outer, inner}) {
                    if (!model)
                        continue;
                    for (size_t col = 0; col < labels.size(); ++col) {
                        const auto& names = model->ParameterLabels();
                        const auto found = std::find(names.begin(), names.end(), labels[col]);
                        REQUIRE(found != names.end(), "InvalidGSRSLVPricing: unknown Jacobian parameter " + labels[col]);
                        const size_t firstSLV =
                            model->Parameters().size() - 2 - static_cast<size_t>(data.leverage_->values_.Rows()) * data.leverage_->values_.Cols();
                        REQUIRE(static_cast<size_t>(found - names.begin()) >= firstSLV,
                                "InvalidGSRSLVPricing: Jacobian supports SLV calibration parameters only");
                        auto* parameter = model->Parameters()[found - names.begin()];
                        AAD::PutOnTape(*parameter);
                        selected[col].push_back(parameter);
                    }
                }
                AAD::NewRecording(*AAD::Tape());
            };
            data_->Samples<AAD::Number_>(
                data, initialize, [] { AAD::Mark(*AAD::Tape()); }, [] { AAD::RewindToMark(*AAD::Tape()); },
                [&](auto* values, const auto&, int, int) {
                    for (size_t row = 0; row < values->size(); ++row) {
#if defined(DAL_USE_XAD_AAD) || defined(DAL_USE_CODIPACK_AAD) || defined(DAL_USE_ADEPT_AAD)
                        AAD::ZeroAdjoints(*AAD::Tape());
#endif
                        AAD::Adjoint((*values)[row]) = 1.0;
                        AAD::PropagateToStart(*AAD::Tape());
                        for (size_t col = 0; col < labels.size(); ++col)
                            for (auto* parameter : selected[col]) {
                                result(row, col) += AAD::AdjointValue(*parameter) / data_->settings_.paths_;
#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
                                AAD::Adjoint(*parameter) = 0.0;
#endif
                            }
                    }
                });
            return result;
        }

    } // namespace GSRSLVPricingInternal

    Vector_<GSRMonteCarloPrice_>
    PriceGSRSLVEuropeanOptions(const GSRSLVModelData_& data, const Vector_<GSREuropeanOption_>& options, const GSRMonteCarloSettings_& settings) {
        return GSRSLVPricingInternal::PreparedPricer_(data, options, settings).Price(data);
    }
} // namespace Dal
