#include <algorithm>
#include <cfenv>
#include <cmath>
#include <cstring>

#include <dal/math/analytics/detail/conditionalbasketcallrisk.hpp>

namespace Dal::ConditionalBasketDetail {
    namespace {
        namespace Tape = AAD::SecondOrderDetail;
        using Number_ = AAD::SecondOrderNumber_;

        struct Input_ {
            double value_, dot_;
            Tape::Slot_ slot_;
        };

        inline Input_ Read(const Number_& number, const Tape::Arena_& arena) {
            const auto slot = Tape::Access_::Slot(number, arena);
            return {Tape::Access_::Value(number), Tape::Access_::Tangent<double>(number, arena, slot), slot};
        }

        struct BlackPoint_ {
            double forward_, strike_, deviation_, d1_, d2_;
        };

        inline bool NearCancellation(double sum, double scale) { return std::abs(sum) < 0x1p-28 * scale; }

        inline double AddProtected(double left, double right) {
            const double sum = left + right;
            const double scale = std::max(std::abs(left), std::abs(right));
            if (NearCancellation(sum, scale) || ((sum == left || sum == right) && std::min(std::abs(left), std::abs(right)) > 0x1p-42))
                Tape::Promote("Black local derivative cancellation");
            return sum;
        }

        inline double LinearTangent(double forward, double exercise) {
            const double difference = Tape::Add(forward, -exercise);
            // A difference of already-rounded leg products can hide their residuals.
            if (NearCancellation(difference, std::max(std::abs(forward), std::abs(exercise))))
                Tape::Promote("linear basket tangent cancellation");
            return difference;
        }

        struct QuadratureTangent_ {
            double correction_ = 0.0, largestTerm_ = 0.0;

            double Accumulate(double previous, double weight, double dot) {
                const double term = weight * dot;
                const double sum = Tape::Add(previous, term);
                const double fromTerm = sum - previous;
                const double additionError = (previous - (sum - fromTerm)) + (term - fromTerm);
                // The running low limb survives later cancellation of the high limb.
                correction_ += additionError + std::fma(weight, dot, -term);
                largestTerm_ = std::max(largestTerm_, std::abs(term));
                return sum;
            }

            double Discount(double high, double discount) const {
                // A small sum can also expose rounding inside a node's derivative.
                if (NearCancellation(high, largestTerm_))
                    Tape::Promote("basket quadrature tangent cancellation");
                return std::fma(discount, high, discount * correction_);
            }
        };

        struct Local_ {
            Tape::Dual_<double> result_;
            Tape::Weight_<double> forward_, strike_, deviation_;
        };

        Local_ Weights(BlackPoint_ p, const Input_& f, const Input_& k, const Input_& u) {
            constexpr double high = 0x1p20, low = 0x1p-20;
            if (!(p.forward_ >= low && p.forward_ <= high && p.strike_ >= low && p.strike_ <= high && p.deviation_ >= low && p.deviation_ <= high &&
                  std::abs(u.dot_) <= high && std::abs(p.d1_) <= 20.0 && std::abs(p.d2_) <= 20.0 && std::abs(f.dot_) <= high &&
                  std::abs(k.dot_) <= high))
                Tape::Promote("Black local derivative range");

            constexpr double invRootTwo = 0.7071067811865475244;
            constexpr double invRootTwoPi = 0.39894228040143267794;
            const double cdf1 = 0.5 * std::erfc(-invRootTwo * p.d1_);
            const double cdf2 = 0.5 * std::erfc(-invRootTwo * p.d2_);
            const double density = invRootTwoPi * std::exp((-0.5 * p.d1_) * p.d1_);
            const double vega = p.forward_ * density;
            const double relativeF = f.dot_ / f.value_;
            const double logDot = AddProtected(relativeF, -k.dot_ / k.value_);
            const double d1Dot = AddProtected(logDot, -p.d2_ * u.dot_) / p.deviation_;
            const double d2Dot = AddProtected(logDot, -p.d1_ * u.dot_) / p.deviation_;
            const double forwardDot = density * d1Dot;
            const double vegaDot = vega * AddProtected(relativeF, -p.d1_ * d1Dot);
            const double legDot = AddProtected(cdf1 * f.dot_, -cdf2 * k.dot_);
            const double tangent = AddProtected(legDot, vega * u.dot_);
            return {{p.forward_ * cdf1 - p.strike_ * cdf2, Tape::Guard(tangent)},
                    {cdf1, Tape::Guard(forwardDot)},
                    {-cdf2, Tape::Guard(-(vega / p.strike_) * d2Dot)},
                    {Tape::Guard(vega), Tape::Guard(vegaDot)}};
        }

        inline Tape::Weight_<double> ScaleQuadrature(Tape::Weight_<double> weight, double quadrature) {
            return {quadrature * weight.value_, quadrature * weight.tangent_};
        }

        inline BlackPoint_ ForwardPoint(double f, double k, double u) {
            const double d1 = std::log(f / k) / u + 0.5 * u;
            return {f, k, u, d1, d1 - u};
        }

        using LegInputs_ = std::array<Input_, 3>;

        struct BasketInputs_ {
            LegInputs_ first_, second_;
            Input_ deviation_;
        };

        struct Leg_ {
            Input_ output_;
            std::array<Tape::Weight_<double>, 3> weights_;
        };

        inline Leg_ Leg(const LegInputs_& inputs, double node) {
            const auto& spot = inputs[0];
            const auto& drift = inputs[1];
            const auto& projection = inputs[2];
            const double phase = drift.value_ + projection.value_ * node;
            const double exponential = Tape::Exponential<double>(phase);
            const double exponentialDot = exponential * AddProtected(drift.dot_, projection.dot_ * node);
            const double value = spot.value_ * exponential;
            const double dot = Tape::Guard(AddProtected(spot.dot_ * exponential, spot.value_ * exponentialDot));
            return {{value, dot, Tape::PASSIVE}, {{{exponential, exponentialDot}, {value, dot}, {value * node, dot * node}}}};
        }

        inline Tape::Weight_<double> ComposeLeg(Tape::Weight_<double> outer, Tape::Weight_<double> inner) {
            return {Tape::Guard(outer.value_ * inner.value_),
                    Tape::Guard(AddProtected(outer.tangent_ * inner.value_, outer.value_ * inner.tangent_))};
        }

        template <bool Trace_>
        Number_ AccumulateBasketNode(const Number_& sum,
                                     const BasketInputs_& inputs,
                                     double strike,
                                     double node,
                                     double quadrature,
                                     bool linear,
                                     QuadratureTangent_* tangent,
                                     double* trace) {
            auto& arena = *Tape::activeArena;
            if (arena.recordingFailure_)
                std::rethrow_exception(arena.recordingFailure_);
            if (arena.retry_)
                Tape::Promote();
            constexpr size_t temporary = sizeof(Tape::EdgeWriter_<double>) + sizeof(Local_) + 2 * sizeof(Leg_) + sizeof(BlackPoint_) +
                                         sizeof(Input_) + sizeof(Tape::Dual_<double>) + 7 * sizeof(Tape::Weight_<double>);
            try {
                arena.AccountTemporary(temporary);
                if (!(quadrature >= 0x1p-120 && quadrature <= 1.0))
                    Tape::Promote("Black quadrature weight range");
                const auto previous = Read(sum, arena);
                const auto first = Leg(inputs.first_, node), second = Leg(inputs.second_, node);
                const Input_ exercise{2.0 * strike - first.output_.value_, -first.output_.dot_, Tape::PASSIVE};
                const auto local = linear ? Local_{{second.output_.value_ - exercise.value_, LinearTangent(second.output_.dot_, exercise.dot_)},
                                                   {1.0, 0.0},
                                                   {-1.0, 0.0},
                                                   {0.0, 0.0}}
                                          : Weights(ForwardPoint(second.output_.value_, exercise.value_, inputs.deviation_.value_), second.output_,
                                                    exercise, inputs.deviation_);
                const Tape::Dual_<double> accumulated{previous.value_ + quadrature * local.result_.value_,
                                                      tangent->Accumulate(previous.dot_, quadrature, local.result_.tangent_)};
                const auto fWeight = ScaleQuadrature(local.forward_, quadrature), aWeight = ScaleQuadrature(local.strike_, -quadrature);
                const auto uWeight = ScaleQuadrature(local.deviation_, quadrature);
                std::array<Tape::Weight_<double>, 3> firstWeights, secondWeights;
                for (size_t i = 0; i < 3; ++i) {
                    firstWeights[i] = ComposeLeg(aWeight, first.weights_[i]);
                    secondWeights[i] = ComposeLeg(fWeight, second.weights_[i]);
                }
                const auto result = Tape::Append<double, 8>(arena, accumulated, temporary, [&](auto& edges) {
                    edges.AddEdge(previous.slot_, {1.0, 0.0});
                    for (size_t i = 0; i < 3; ++i)
                        edges.AddEdge(inputs.second_[i].slot_, secondWeights[i]);
                    for (size_t i = 0; i < 3; ++i)
                        edges.AddEdge(inputs.first_[i].slot_, firstWeights[i]);
                    if (!linear)
                        edges.AddEdge(inputs.deviation_.slot_, uWeight);
                });
                arena.AccountLogical<double>(false, temporary);
                if constexpr (Trace_)
                    *trace = local.result_.value_;
                return result;
            } catch (const Tape::Retry_&) {
                throw;
            } catch (...) {
                arena.RecordFailure();
            }
        }

        [[noreturn]] void Validation(Tape::Reason reason, const char* message) { Tape::Access_::Fail(reason, Tape::Stage::VALIDATION, {}, message); }

        void RequireNearest() {
            if (std::fegetround() != FE_TONEAREST)
                Validation(Tape::Reason::UNSUPPORTED_OPERATION, "fixed conditional basket requires round-to-nearest");
        }
    } // namespace

    FixedConditionalBasketCall_::FixedConditionalBasketCall_(double strike, double rate, const Nodes_& nodes, const Vector_<>& point)
        : strike_(strike), rate_(rate), nodes_(nodes), point_(point) {
        RequireNearest();
        if (point.size() != 5 || !std::isfinite(strike) || !std::isfinite(rate) ||
            !std::all_of(point.begin(), point.end(), [](double x) { return std::isfinite(x); }))
            Validation(Tape::Reason::INVALID_INPUT, "fixed conditional basket requires five finite inputs and finite contract terms");
        if (!(strike > 0.0 && point[0] > 0.0 && point[1] > 0.0 && point[2] > 0.0 && point[3] > 0.0 && std::abs(point[4]) < 1.0))
            Validation(Tape::Reason::OUTSIDE_DOMAIN, "fixed conditional basket requires smooth positive spots, vols and strike");
        discount_ = 0.5 * std::exp(-rate);
        if (!(discount_ > 0.0) || !std::isfinite(discount_))
            Validation(Tape::Reason::OUTSIDE_DOMAIN, "fixed conditional basket discount is outside the finite positive range");
        const double driftA = rate - 0.5 * point[2] * point[2], projectionB = point[3] * point[4];
        const double driftB = rate - 0.5 * projectionB * projectionB;
        const double deviation = point[3] * std::sqrt(1.0 - point[4] * point[4]);
        if (!(std::isfinite(deviation) && deviation > 0.0))
            Validation(Tape::Reason::OUTSIDE_DOMAIN, "fixed conditional basket requires positive conditional deviation");
        for (const auto& node : nodes_) {
            if (!(std::isfinite(node.z_) && node.weight_ >= 0x1p-120 && node.weight_ <= 1.0))
                Validation(Tape::Reason::OUTSIDE_DOMAIN, "unresolved fixed basket quadrature weight or node");
            const double exercise = 2.0 * strike - point[0] * std::exp(driftA + point[2] * node.z_);
            const double forward = point[1] * std::exp(driftB + projectionB * node.z_);
            if (!(std::isfinite(exercise) && std::isfinite(forward) && forward > 0.0 && (node.linear_ ? exercise < 0.0 : exercise > 0.0)))
                Validation(Tape::Reason::OUTSIDE_DOMAIN, "fixed conditional basket node does not match its smooth branch");
        }
    }

    template <bool TraceNodes_> Number_ FixedConditionalBasketCall_::Record(const Vector_<Number_>& x, Trace_* trace) const {
        const Number_ driftA = rate_ - 0.5 * x[2] * x[2];
        const Number_ projectionB = x[3] * x[4];
        const Number_ driftB = rate_ - 0.5 * projectionB * projectionB;
        const Number_ deviation = x[3] * sqrt(1.0 - x[4] * x[4]);
        auto& arena = *Tape::activeArena;
        Number_ sum(0.0);
        Number_ price;
        if (arena.wide_) {
            constexpr double invRootTwo = 0.7071067811865475244;
            const auto cdf = [](const Number_& z) -> Number_ { return 0.5 * erfc(-invRootTwo * z); };
            for (size_t i = 0; i < nodes_.size(); ++i) {
                const auto& node = nodes_[i];
                const Number_ exercise = 2.0 * strike_ - x[0] * exp(driftA + x[2] * node.z_);
                const Number_ forward = x[1] * exp(driftB + projectionB * node.z_);
                if (node.linear_) {
                    if constexpr (TraceNodes_)
                        trace->prices_[i] = Tape::Access_::Value(forward) - Tape::Access_::Value(exercise);
                    sum += node.weight_ * (forward - exercise);
                    continue;
                }
                const Number_ d1 = log(forward / exercise) / deviation + 0.5 * deviation;
                const Number_ d2 = d1 - deviation;
                const Number_ nodePrice = forward * cdf(d1) - exercise * cdf(d2);
                if constexpr (TraceNodes_)
                    trace->prices_[i] = Tape::Access_::Value(nodePrice);
                sum += node.weight_ * nodePrice;
            }
            price = discount_ * sum;
        } else {
            const Tape::Scratch_ storage(arena, sizeof(BasketInputs_) + sizeof(QuadratureTangent_));
            const BasketInputs_ inputs{{Read(x[0], arena), Read(driftA, arena), Read(x[2], arena)},
                                       {Read(x[1], arena), Read(driftB, arena), Read(projectionB, arena)},
                                       Read(deviation, arena)};
            QuadratureTangent_ tangent;
            for (size_t i = 0; i < nodes_.size(); ++i) {
                const auto& node = nodes_[i];
                double* nodePrice = nullptr;
                if constexpr (TraceNodes_)
                    nodePrice = &trace->prices_[i];
                sum = AccumulateBasketNode<TraceNodes_>(sum, inputs, strike_, node.z_, node.weight_, node.linear_, &tangent, nodePrice);
            }
            const double dot = tangent.Discount(Read(sum, arena).dot_, discount_);
            const Number_ discounted = discount_ * sum;
            // The passive discount's reverse weights do not use the tangent.
            price = Tape::Access_::Bind(arena, Tape::Access_::Slot(discounted, arena), Tape::Access_::Value(discounted), dot);
        }
        if constexpr (TraceNodes_) {
            trace->statements_ = arena.statements_;
            trace->edges_ = arena.edges_;
        }
        return price;
    }

    AAD::SecondOrderResult_ FixedConditionalBasketCall_::Evaluate(const Vector_<>& point,
                                                                  const Matrix_<>& directions,
                                                                  const AAD::SecondOrderOptions_& options,
                                                                  Trace_* trace) const {
        RequireNearest();
        if (directions.Rows() != 1 || directions.Cols() != 5 || point.size() != 5)
            Validation(Tape::Reason::INVALID_INPUT, "fixed conditional basket requires a five-coordinate point and one direction");
        if (std::memcmp(&point[0], &point_[0], 5 * sizeof(double)) != 0)
            Validation(Tape::Reason::OUTSIDE_DOMAIN, "fixed conditional basket point differs from the certified point");
        auto result = trace ? AAD::EvaluateSecondOrder([this, trace](const auto& x) { return Record<true>(x, trace); }, point, directions, options)
                            : AAD::EvaluateSecondOrder([this](const auto& x) { return Record<false>(x, nullptr); }, point, directions, options);
        Tape::Access_::SetMethod(&result, result.Execution().safePathActivations_ ? "NativeBasketScalarReplay" : "NativeBasketComposedAAD");
        return result;
    }
} // namespace Dal::ConditionalBasketDetail
