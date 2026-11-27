#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/math/aad/bumpoveraad.hpp>
#include <dal/math/aad/forwardoverreverse.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/integral/quadrature.hpp>
#include <dal/platform/platform.hpp>

#include "secondorderperf.hpp"

using namespace Dal;
using namespace Dal::AAD;

namespace {
    constexpr int BASKET_NODES = 48;
    constexpr double BASKET_RATE = 0.05;
    constexpr double BASKET_STRIKE = 110.0;
    constexpr double BS_TIME = 1.7;
    constexpr double BS_RATE = 0.03;
    constexpr double BS_DIVIDEND = 0.01;
    constexpr double BS_STRIKE = 100.0;
    constexpr int REFERENCE_STEP_MULTIPLIER = 4;
    constexpr int WARMUP_PAIRS = 3;
    constexpr int ROUNDS = 2;
    constexpr int PAIRS_PER_ROUND = 10;

    struct Case_ {
        const char* name_;
        Vector_<> point_;
        Matrix_<> directions_;
        Vector_<> steps_;
        int callsPerBatch_;
        double pointwiseTolerance_;
        double centeredTolerance_;
    };

    struct Reference_ {
        double price_;
        Matrix_<> products_;
        double stencilDisagreement_ = 0.0;
    };

    Matrix_<> Directions(std::initializer_list<double> mixed) {
        const auto n = static_cast<int>(mixed.size());
        Matrix_<> result(n + 1, n, 0.0);
        for (int column = 0; column < n; ++column) {
            result(column, column) = 1.0;
            result(n, column) = mixed.begin()[column];
        }
        return result;
    }

    double NormalCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

    template <class T_> T_ BlackCall(const T_& forward, const T_& strike, const T_& stdDev) {
        const T_ d1 = log(forward / strike) / stdDev + 0.5 * stdDev;
        const T_ d2 = d1 - stdDev;
        return forward * NCDF(d1) - strike * NCDF(d2);
    }

    struct BlackScholes_ {
        template <class T_> T_ operator()(RecordingScope_*, const Vector_<T_>& x) const {
            const double rootTime = std::sqrt(BS_TIME);
            const auto d1 = (log(x[0] / BS_STRIKE) + (BS_RATE - BS_DIVIDEND + 0.5 * x[1] * x[1]) * BS_TIME) / (x[1] * rootTime);
            return x[0] * std::exp(-BS_DIVIDEND * BS_TIME) * NCDF(d1) - BS_STRIKE * std::exp(-BS_RATE * BS_TIME) * NCDF(d1 - x[1] * rootTime);
        }
    };

    Reference_ BlackScholesReference(const Case_& shape) {
        const double spot = shape.point_[0], vol = shape.point_[1], rootTime = std::sqrt(BS_TIME);
        const double dividend = std::exp(-BS_DIVIDEND * BS_TIME);
        const double d1 = (std::log(spot / BS_STRIKE) + (BS_RATE - BS_DIVIDEND + 0.5 * vol * vol) * BS_TIME) / (vol * rootTime);
        const double d2 = d1 - vol * rootTime;
        const double density = std::exp(-0.5 * d1 * d1) / std::sqrt(2.0 * std::acos(-1.0));
        const double gamma = dividend * density / (spot * vol * rootTime);
        const double vanna = -dividend * density * d2 / vol;
        const double volga = spot * dividend * density * rootTime * d1 * d2 / vol;
        const double price = spot * dividend * NormalCdf(d1) - BS_STRIKE * std::exp(-BS_RATE * BS_TIME) * NormalCdf(d2);
        Reference_ result{price, Matrix_<>(shape.directions_.Rows(), 2)};
        for (int row = 0; row < result.products_.Rows(); ++row) {
            const double v0 = shape.directions_(row, 0), v1 = shape.directions_(row, 1);
            result.products_(row, 0) = gamma * v0 + vanna * v1;
            result.products_(row, 1) = vanna * v0 + volga * v1;
        }
        return result;
    }

    struct BasketNode_ {
        double z_;
        double weight_;
        bool linear_;
    };

    struct ConditionalBasket_ {
        Vector_<BasketNode_> nodes_;

        template <class T_> T_ operator()(RecordingScope_*, const Vector_<T_>& x) const {
            // One-year maturity with both dividend yields zero.
            const T_ driftA = BASKET_RATE - 0.5 * x[2] * x[2];
            const T_ projectionB = x[3] * x[4];
            const T_ driftB = BASKET_RATE - 0.5 * projectionB * projectionB;
            const T_ stdDev = x[3] * sqrt(1.0 - x[4] * x[4]);
            T_ sum(0.0);
            for (const auto& node : nodes_) {
                const T_ strike = 2.0 * BASKET_STRIKE - x[0] * exp(driftA + x[2] * node.z_);
                const T_ forwardB = x[1] * exp(driftB + projectionB * node.z_);
                const T_ price = node.linear_ ? T_(forwardB - strike) : BlackCall(forwardB, strike, stdDev);
                sum += node.weight_ * price;
            }
            return 0.5 * std::exp(-BASKET_RATE) * sum;
        }
    };

    ConditionalBasket_ MakeBasket(const Case_& shape, const Vector_<>& coordinateSteps) {
        Vector_<> radius(shape.point_.size(), 0.0);
        for (int row = 0; row < shape.directions_.Rows(); ++row)
            for (int column = 0; column < shape.directions_.Cols(); ++column)
                radius[column] = std::max(radius[column], coordinateSteps[column] + REFERENCE_STEP_MULTIPLIER * shape.steps_[row] *
                                                                                        std::abs(shape.directions_(row, column)));
        const auto& x = shape.point_;
        for (size_t column = 0; column < 4; ++column)
            REQUIRE(x[column] > radius[column], "second-order basket requires positive spots and vols throughout the stencil");
        REQUIRE(std::abs(x[4]) + radius[4] < 1.0, "second-order basket requires nonsingular correlation throughout the stencil");
        const double volLow = x[2] - radius[2], volHigh = x[2] + radius[2];
        const NormalExpectation_<> quadrature(BASKET_NODES);
        ConditionalBasket_ result;
        result.nodes_.reserve(BASKET_NODES);
        for (int i = 0; i < BASKET_NODES; ++i) {
            const double z = quadrature.Abscissa()[i];
            // Bound the stock over the entire box before fixing the node's payoff branch.
            const double projectionLow = volLow * z, projectionHigh = volHigh * z;
            const auto projection = std::minmax(projectionLow, projectionHigh);
            const double stockLow = (x[0] - radius[0]) * std::exp(BASKET_RATE - 0.5 * volHigh * volHigh + projection.first);
            const double stockHigh = (x[0] + radius[0]) * std::exp(BASKET_RATE - 0.5 * volLow * volLow + projection.second);
            REQUIRE(stockHigh < 2.0 * BASKET_STRIKE || stockLow > 2.0 * BASKET_STRIKE,
                    "second-order basket stencil crosses a conditional strike zero");
            result.nodes_.push_back({z, quadrature.Weight()[i], stockLow > 2.0 * BASKET_STRIKE});
        }
        return result;
    }

    double BasketScalarPrice(const ConditionalBasket_& basket, const Vector_<>& x) {
        const double conditionalStdDev = x[3] * std::sqrt((1.0 - x[4]) * (1.0 + x[4]));
        double price = 0.0;
        for (const auto& node : basket.nodes_) {
            const double terminalA = x[0] * std::exp(BASKET_RATE + x[2] * (node.z_ - 0.5 * x[2]));
            const double conditionalLogB = std::log(x[1]) + BASKET_RATE - 0.5 * x[3] * x[3] + x[3] * x[4] * node.z_;
            const double meanB = std::exp(conditionalLogB + 0.5 * conditionalStdDev * conditionalStdDev);
            const double strike = 2.0 * BASKET_STRIKE - terminalA;
            double call = meanB - strike;
            if (strike > 0.0) {
                const double d2 = (conditionalLogB - std::log(strike)) / conditionalStdDev;
                call = meanB * NormalCdf(d2 + conditionalStdDev) - strike * NormalCdf(d2);
            }
            price += node.weight_ * call;
        }
        return 0.5 * std::exp(-BASKET_RATE) * price;
    }

    double MixedPriceDifference(const ConditionalBasket_& basket, const Case_& shape, int row, int column, double h, double k) {
        Vector_<> upper = shape.point_, lower = shape.point_;
        for (int i = 0; i < shape.directions_.Cols(); ++i) {
            upper[i] += h * shape.directions_(row, i);
            lower[i] -= h * shape.directions_(row, i);
        }
        upper[column] += k;
        lower[column] += k;
        const double upperPlus = BasketScalarPrice(basket, upper), lowerPlus = BasketScalarPrice(basket, lower);
        upper[column] -= 2.0 * k;
        lower[column] -= 2.0 * k;
        return ((upperPlus - BasketScalarPrice(basket, upper)) - (lowerPlus - BasketScalarPrice(basket, lower))) / (4.0 * h * k);
    }

    Reference_ BasketReference(const ConditionalBasket_& basket, const Case_& shape, const Vector_<>& coordinateSteps) {
        Reference_ result{BasketScalarPrice(basket, shape.point_), Matrix_<>(shape.directions_.Rows(), shape.directions_.Cols())};
        for (int row = 0; row < result.products_.Rows(); ++row) {
            for (int column = 0; column < result.products_.Cols(); ++column) {
                const double h = REFERENCE_STEP_MULTIPLIER * shape.steps_[row], k = coordinateSteps[column];
                const double full = MixedPriceDifference(basket, shape, row, column, h, k);
                const double half = MixedPriceDifference(basket, shape, row, column, h / 2.0, k / 2.0);
                const double quarter = MixedPriceDifference(basket, shape, row, column, h / 4.0, k / 4.0);
                // Halving both axes cancels the leading squared-step error.
                const double coarse = (4.0 * half - full) / 3.0, refined = (4.0 * quarter - half) / 3.0;
                const double disagreement = std::abs(refined - coarse);
                REQUIRE(disagreement <= 0.5 * shape.pointwiseTolerance_ * std::max(1.0, std::abs(refined)),
                        "second-order basket scalar-price HVP reference did not stabilize");
                result.stencilDisagreement_ = std::max(result.stencilDisagreement_, disagreement);
                result.products_(row, column) = refined;
            }
        }
        return result;
    }

    Matrix_<> Prefix(const Matrix_<>& full, int rows) {
        Matrix_<> result(rows, full.Cols());
        for (int row = 0; row < rows; ++row)
            for (int column = 0; column < full.Cols(); ++column)
                result(row, column) = full(row, column);
        return result;
    }

    void
    Verify(const Case_& shape, const Reference_& ref, int count, const ForwardOverReverseResult_& pointwise, const BumpOverAADResult_& centered) {
        const auto& pointwiseWork = pointwise.Execution();
        const auto& centeredWork = centered.Execution();
        const auto& products = pointwise.HessianProducts();
        const auto n = static_cast<size_t>(count);
        REQUIRE(products.Rows() == count && centered.HessianProducts().Rows() == count && products.Cols() == ref.products_.Cols() &&
                    centered.HessianProducts().Cols() == ref.products_.Cols(),
                "second-order benchmark unexpected HVP shape");
        REQUIRE(pointwiseWork.recordings_ == n && pointwiseWork.reverseSweeps_ == n + 1 && centeredWork.gradientEvaluations_ == 1 + 2 * n &&
                    centeredWork.reverseSweeps_ == 1 + 2 * n,
                "second-order benchmark unexpected tape work");
        REQUIRE(std::abs(ref.price_ - pointwise.Value()) <= 1e-11 && std::abs(ref.price_ - centered.Value()) <= 1e-11,
                "second-order benchmark price differs from independent reference");
        REQUIRE(std::abs(ref.products_(0, 1)) > 1e-6, "second-order benchmark single direction needs a nonzero cross term");
        double pointwiseError = 0.0, centeredError = 0.0;
        for (int row = 0; row < products.Rows(); ++row) {
            for (int column = 0; column < products.Cols(); ++column) {
                const double expected = ref.products_(row, column);
                const double ad = products(row, column), bump = centered.HessianProducts()(row, column);
                const double adError = std::abs(ad - expected), bumpError = std::abs(bump - expected);
                std::cout << "hvp," << shape.name_ << ',' << n << ',' << row << ',' << column << ',' << centered.Steps()[row] << ',' << expected
                          << ',' << ad << ',' << bump << '\n';
                REQUIRE(adError <= shape.pointwiseTolerance_ * std::max(1.0, std::abs(expected)),
                        "second-order benchmark pointwise HVP differs from reference");
                REQUIRE(bumpError <= shape.centeredTolerance_ * std::max(1.0, std::abs(expected)),
                        "second-order benchmark centered HVP differs from reference");
                pointwiseError = std::max(pointwiseError, adError);
                centeredError = std::max(centeredError, bumpError);
            }
        }
        std::cout << "accuracy," << shape.name_ << ',' << n << ',' << ref.price_ << ',' << ref.stencilDisagreement_ << ',' << pointwiseError << ','
                  << centeredError << '\n'
                  << "work," << shape.name_ << ',' << n << ',' << pointwiseWork.recordings_ << ',' << pointwiseWork.reverseSweeps_ << ','
                  << pointwiseWork.peakTapeBytes_ << ',' << centeredWork.gradientEvaluations_ << ',' << centeredWork.reverseSweeps_ << ','
                  << centeredWork.peakTapeBytes_ << '\n';
    }

    size_t TapeCapacity() {
        TapeCapacityBudget_ budget(std::numeric_limits<size_t>::max());
        TapeCapacityScope_ scope(&budget);
        return budget.CapacityBytes();
    }

    template <class F_> double TimeBatch(const F_& evaluate, int calls) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < calls; ++i) {
            const auto result = evaluate();
            Bench::DoNotOptimize(&result);
        }
        return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / calls;
    }

    using Samples_ = std::array<double, ROUNDS * PAIRS_PER_ROUND>;

    std::array<double, 3> Summary(Samples_ samples) {
        std::sort(samples.begin(), samples.end());
        const size_t middle = samples.size() / 2;
        const size_t rank95 = (95 * samples.size() + 99) / 100;
        return {0.5 * (samples[middle - 1] + samples[middle]), samples[rank95 - 1], samples.front()};
    }

    template <class P_, class B_> void Measure(const Case_& shape, int directions, const P_& pointwise, const B_& centered, bool centeredFirst) {
        Clear(*Tape());
        for (int i = 0; i < WARMUP_PAIRS; ++i) {
            (void)TimeBatch(pointwise, shape.callsPerBatch_);
            (void)TimeBatch(centered, shape.callsPerBatch_);
        }
        // Both drivers rewind to an empty graph and retain the same warmed tape blocks.
        const size_t initialCapacity = TapeCapacity();
        Samples_ pointwiseSamples{}, centeredSamples{};
        for (size_t i = 0; i < pointwiseSamples.size(); ++i) {
            const bool bumpFirst = centeredFirst != (i % 2 != 0);
            if (bumpFirst) {
                centeredSamples[i] = TimeBatch(centered, shape.callsPerBatch_);
                pointwiseSamples[i] = TimeBatch(pointwise, shape.callsPerBatch_);
            } else {
                pointwiseSamples[i] = TimeBatch(pointwise, shape.callsPerBatch_);
                centeredSamples[i] = TimeBatch(centered, shape.callsPerBatch_);
            }
        }
        REQUIRE(TapeCapacity() == initialCapacity, "second-order benchmark tape grew after warmup");
        for (size_t i = 0; i < pointwiseSamples.size(); ++i)
            std::cout << "sample," << shape.name_ << ',' << directions << ',' << 1 + i / PAIRS_PER_ROUND << ',' << 1 + i % PAIRS_PER_ROUND << ','
                      << ((centeredFirst != (i % 2 != 0)) ? "centered" : "pointwise") << ',' << shape.callsPerBatch_ << ',' << pointwiseSamples[i]
                      << ',' << centeredSamples[i] << '\n';
        const auto [adMedian, adP95, adMin] = Summary(pointwiseSamples);
        const auto [bumpMedian, bumpP95, bumpMin] = Summary(centeredSamples);
        std::cout << "summary," << shape.name_ << ',' << directions << ',' << pointwiseSamples.size() << ',' << initialCapacity << ',' << adMedian
                  << ',' << adP95 << ',' << adMin << ',' << bumpMedian << ',' << bumpP95 << ',' << bumpMin << ',' << adMedian / bumpMedian << '\n';
    }

    template <class K_> void Run(const Case_& shape, const K_& kernel, const Reference_& ref, bool verifyOnly, bool centeredFirst) {
        const NativeDirectionalFunction_ pointwiseKernel = std::cref(kernel);
        const NativeScalarFunction_ centeredKernel = std::cref(kernel);
        for (int count : {1, shape.directions_.Rows()}) {
            ForwardOverReverseRequest_ adRequest;
            adRequest.directions_ = Prefix(shape.directions_, count);
            BumpOverAADRequest_ bumpRequest;
            bumpRequest.directions_ = adRequest.directions_;
            bumpRequest.steps_ = Vector_<>(shape.steps_.begin(), shape.steps_.begin() + count);
            const auto pointwise = [&]() { return EvaluateForwardOverReverse(pointwiseKernel, shape.point_, adRequest); };
            const auto centered = [&]() { return EvaluateBumpOverAAD(centeredKernel, shape.point_, bumpRequest); };
            Clear(*Tape());
            const auto ad = pointwise();
            Clear(*Tape());
            const auto bump = centered();
            Verify(shape, ref, count, ad, bump);
            if (!verifyOnly)
                Measure(shape, count, pointwise, centered, centeredFirst);
        }
    }
} // namespace

int RunSecondOrderBenchmarks(int argc, char** argv) {
    try {
        const std::string option = argc == 2 ? argv[1] : "";
        REQUIRE(argc == 1 || (argc == 2 && (option == "--verify-only" || option == "--centered-first")),
                "expected --second-order [--verify-only|--centered-first]");
        const bool verifyOnly = option == "--verify-only";
        const bool centeredFirst = option == "--centered-first";
        RegisterAll_::Init();
        const Case_ small{"black_scholes", {103.0, 0.24}, Directions({-2.0, 0.5}), {0.1, 0.0005, 0.0005}, 256, 1e-10, 2.5e-5};
        const Case_ basket{"conditional_basket",
                           {100.0, 120.0, 0.20, 0.30, 0.40},
                           Directions({-2.0, 0.75, 0.20, -0.15, 0.35}),
                           {0.05, 0.05, 0.0002, 0.0002, 0.0002, 0.0005},
                           16,
                           2e-6,
                           5e-5};
        const Vector_<> coordinateSteps{0.2, 0.2, 0.002, 0.002, 0.002};
        const auto conditional = MakeBasket(basket, coordinateSteps);
        const auto basketRef = BasketReference(conditional, basket, coordinateSteps);
        std::cout << std::setprecision(17) << "# hvp,case,directions,direction_index,coordinate_index,bump_step,reference,pointwise,centered\n"
                  << "# accuracy,case,directions,reference_price,reference_stencil_max_disagreement,pointwise_max_abs_error,centered_max_abs_error\n"
                  << "# "
                     "work,case,directions,pointwise_recordings,pointwise_sweeps,pointwise_peak_bytes,centered_recordings,centered_sweeps,centered_"
                     "peak_bytes\n"
                  << "# sample,case,directions,round,pair,first,calls_per_batch,pointwise_ns_per_call,centered_ns_per_call\n"
                  << "# summary,case,directions,pairs,warm_tape_bytes,pointwise_median_ns,pointwise_p95_ns,pointwise_min_ns,"
                     "centered_median_ns,centered_p95_ns,centered_min_ns,pointwise_over_centered_median\n";
        Run(small, BlackScholes_{}, BlackScholesReference(small), verifyOnly, centeredFirst);
        Run(basket, conditional, basketRef, verifyOnly, centeredFirst);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
